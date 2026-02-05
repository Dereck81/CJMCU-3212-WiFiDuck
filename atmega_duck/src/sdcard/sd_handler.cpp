#include "../../include/config.h"
#include "../../include/debug.h"
#include "../com/com.h"

#include "../hid/keyboard.h"

#include "sd_handler.h"
#include "sdcard.h"
#include "script_runner.h"

#include <Mouse.h>

#ifdef USE_SD_CARD

/**
 * @brief Shorthand for com::getRawBuffer()
 *
 * Returns a writable pointer to com's internal data buffer. Used by sd_handler
 * as scratch space for building outgoing packets (file lists, read chunks) and
 * as the working buffer for script_runner.
 */
#define _gb (com::getRawBuffer())

/**
 * @brief Local copy of the SD card status, captured at the start of process()
 *
 * This is a snapshot taken from sdcard::getStatus() before dispatching commands.
 * It lets functions like ack_received() know which streaming operation is active
 * (listing, reading, writing) without querying the status module repeatedly.
 */
static sdcard::SDStatus sdcard_status;

/**
 * @brief Single-byte acknowledgment value sent back after WRITE chunks
 *
 * After sd_handler writes a chunk to the SD card, it sends this byte back to
 * signal the receiving device that it is ready for the next chunk.
 */
static uint8_t sd_ack_val = SD_ACK;

namespace sd_handler {

     /**
     * @brief Streams directory contents to the receiving device chunk by chunk
     *
     * Called once initially with a directory path, then repeatedly with a null
     * path (via ack_received) to send subsequent entries. Each entry is packed as:
     *   [4 bytes: file size, little-endian]
     *   [N bytes: filename, null-terminated]
     *
     * When no more entries exist, endList() is called and the operation finishes.
     *
     * @param path Directory path to list, or null to continue the current listing
     */
    void streamList(const char* path) {
        // If we are not already listing, start a new listing operation
        if (sdcard_status == sdcard::SD_IDLE || sdcard_status == sdcard::SD_ERROR) 
            if (!sdcard::beginList(path[0] ? path : "/")) return;
        
        // Verify we are actually in listing mode (beginList succeeded)
        if (sdcard::getStatus() != sdcard::SD_LISTING) return;

        uint32_t size;

        // Try to get the next file in the directory
        if (sdcard::getNextFile((char*)&_gb[4], BUFFER_SIZE - 5, &size)) {
            // Pack the size as a little-endian 32-bit value at the start
            _gb[0] = size; 
            _gb[1] = size >> 8; 
            _gb[2] = size >> 16; 
            _gb[3] = size >> 24;

            // Ensure the filename is null-terminated (it should be already, but be safe)
            _gb[BUFFER_SIZE - 1] = '\0';

            // Calculate the total packet length: 4 bytes size + filename length
            uint8_t nameLen = strlen((char*)&_gb[4]);
            
            // Send this entry to the receiving device
            com::sendSdData(_gb, nameLen + 4);
            return;
        }

        sdcard::endList();
    }

    /**
     * @brief Streams file contents to the receiving device chunk by chunk
     *
     * Called once initially with a file path, then repeatedly with a null path
     * (via ack_received) to send subsequent chunks. Each chunk is raw file data,
     * up to BUFFER_SIZE - 12 bytes (the -12 is headroom for framing overhead in
     * com.cpp's SD_SOT/SD_EOT wrappers).
     *
     * When EOF is reached, endFileRead() is called and the operation finishes.
     *
     * @param file File path to read, or null to continue reading the current file
     */
    void streamRead(const char* file) {
        // If we are not already reading, open the file
        if (sdcard_status == sdcard::SD_IDLE || sdcard_status == sdcard::SD_ERROR) {
            if (!sdcard::beginFileRead(file, nullptr)) return;
        }

        // Verify we are actually in reading mode (beginFileRead succeeded)
        if (sdcard::getStatus() != sdcard::SD_READING) return;

        int16_t read;

        // Try to read a chunk. The -12 headroom ensures com::sendSdData() has
        // room to add SD_SOT and SD_EOT framing bytes without overflow.
        if ((read = sdcard::readFileChunk(_gb, BUFFER_SIZE - 12)) > 0) {
            com::sendSdData(_gb, read);
            return;
        }

        // EOF or error — close the file
        sdcard::endFileRead();
    }

    /**
     * @brief Receives file data from the receiving device and writes it to SD
     *
     * The first packet contains:
     *   byte 0: append flag (0 = overwrite, 1 = append)
     *   byte 1+: filename (null-terminated)
     *
     * This opens the file. Subsequent packets contain raw file data. When the
     * receiving device is done it sends a zero-length chunk, which triggers
     * endFileWrite().
     *
     * After each chunk is written, SD_ACK is sent back to signal readiness for
     * the next one.
     *
     * @param data Packet data (either [append][filename] or raw file bytes)
     * @param len  Number of bytes in the packet
     */
    void streamWrite(uint8_t* data, size_t len) {
        // Opening a new file for writing
        if (sdcard_status == sdcard::SD_IDLE || sdcard_status == sdcard::SD_ERROR) {
            // First packet must be at least: [append][1 char][null] = 3 bytes,
            // but we require 5 to ensure a meaningful filename exists
            if (len < 5) return;
    
            bool append = data[0];
            
            // Filename must not exceed MAX_NAME
            if (len - 1 > MAX_NAME) return;
            
            // Null-terminate the filename (should already be, but be safe)
            data[len] = '\0';

            // Open the file. Filename starts at data[1], skipping the append byte.
            sdcard::beginFileWrite((char*)&data[1], append);

            return;
        }

        // Writing data to an already-open file
        if (sdcard::getStatus() != sdcard::SD_WRITING) return;

        // Write the chunk. If writeFileChunk returns 0, either an error occurred
        // or this was a zero-length packet signaling completion.
        if (sdcard::writeFileChunk(data, len) == 0)
            sdcard::endFileWrite();

        // Send acknowledgment back so the receiving device knows we are ready
        // for the next chunk
        com::sendSdData(sd_ack_val, 1);
    }

     /**
     * @brief Starts the autorun script if defined at compile time
     *
     * Called once during setup (not from remote commands). If AUTORUN_SCRIPT
     * is defined in config.h, that script is started immediately when the
     * device boots.
     */
    void autorun() {
        #ifdef AUTORUN_SCRIPT
        script_runner::start(AUTORUN_SCRIPT, _gb);
        #endif
    }

    /**
     * @brief Drives script execution one line per tick, and cleans up at the end
     *
     * Called from the main loop every cycle while a script is running. Each call
     * executes one line (or one fragment of a long line). When the script finishes,
     * this function releases all held keys and mouse buttons to prevent them from
     * getting stuck, stops the script runner, and signals com that the operation
     * is complete.
     *
     * @return true if the script is still running, false if it just finished
     */
    bool run_script_step() {

        // Nothing to do if no script is running
        if (sdcard::getStatus() != sdcard::SD_EXECUTING) return true;

        // Execute the next line. If it returns true, there is more to do.
        if (script_runner::execute_next_line()) return true;

        // Script finished — release all held mouse buttons
        Mouse.release(MOUSE_LEFT);
        Mouse.release(MOUSE_RIGHT);
        Mouse.release(MOUSE_MIDDLE);
        delay(10);
        
        // Reset mouse position delta to zero (stops any drift)
        Mouse.move(0, 0, 0);
        delay(10);
        
        // Release all held keyboard keys
        keyboard::release();
        delay(10);
        
        // Stop the script runner and reset its state
        script_runner::stop();
        
        // Notify com that the operation is done so it sends a fresh status
        // back to the receiving device
        com::sendDone();
        
        return false;
        
    }

    /**
     * @brief Aborts any ongoing SD card operation (list, read, write)
     *
     * Called when the receiving device sends SD_CMD_STOP, or when the main loop
     * needs to forcibly halt an operation (e.g., because a new command arrived).
     * Calls the appropriate end function based on the current status.
     */
    inline void stop() {
        if (sdcard_status == sdcard::SD_READING) sdcard::endFileRead();
        else if (sdcard_status == sdcard::SD_WRITING) sdcard::endFileWrite();
        else if (sdcard_status == sdcard::SD_LISTING) sdcard::endList();
    }

    /**
     * @brief Handles acknowledgment (SD_ACK) from the receiving device
     *
     * When the receiving device sends SD_ACK it means it has processed the last
     * chunk and is ready for the next one. This function checks the current status
     * and calls the appropriate stream function with a null path to continue the
     * operation.
     */
    inline void ack_received() {
        if (sdcard_status == sdcard::SD_LISTING) streamList(0x00);
        else if(sdcard_status == sdcard::SD_READING) streamRead(0x00);
    }

    /**
     * @brief Main entry point — dispatches SD card commands from the receiving device
     *
     * Called by the main loop when com::isSdPacket() returns true. The buffer
     * contains a command byte followed by optional arguments. This function reads
     * the current SD card status, extracts the command, and dispatches to the
     * appropriate handler.
     *
     * Command format:
     *   byte 0:    Command opcode (SD_CMD_LS, SD_CMD_READ, etc.)
     *   byte 1+:   Arguments (typically a null-terminated file/directory path)
     *
     * Supported commands:
     *   SD_ACK          — Receiving device is ready for the next chunk
     *   SD_CMD_LS       — List directory contents
     *   SD_CMD_READ     — Read file contents
     *   SD_CMD_WRITE    — Write file contents
     *   SD_CMD_RM       — Remove (delete) a file
     *   SD_CMD_RUN      — Execute a script from the SD card
     *   SD_CMD_STOP_RUN — Stop script execution
     *   SD_CMD_STOP     — Abort any ongoing SD operation
     *
     * @param buffer Packet data received from com
     * @param len    Number of valid bytes in the buffer
     */
    void process(uint8_t* buffer, size_t len) {
        // Ignore empty packets or packets that arrive when SD card is not available
        if (!len || !sdcard::available()) return;

        // Capture the current status. This is a snapshot — the status may change
        // during the function as operations open/close.
        sdcard_status = sdcard::getStatus();
        
        // Extract the command byte
        uint8_t cmd = buffer[0];

        // Arguments start immediately after the command byte
        char* args = (char*)&buffer[1];

        // Special handling for ACK — it is not a command, it is a response
        if (cmd == SD_ACK) {
            ack_received();
            return;
        }

        // Dispatch based on the command opcode
        switch (cmd) {
            case SD_CMD_LS:
                streamList(args);
                break;

            case SD_CMD_READ:
                streamRead(args);
                break;

            case SD_CMD_WRITE:
                streamWrite(&buffer[1], len - 1);
                break;

            case SD_CMD_RM:
                sdcard::removeFile(args);
                break;

            case SD_CMD_RUN:
                script_runner::start(args, _gb);
                break;

            case SD_CMD_STOP_RUN:
                script_runner::stop();
                break;
            
            case SD_CMD_STOP:
                stop();
                break;
        }
    }
}
#endif