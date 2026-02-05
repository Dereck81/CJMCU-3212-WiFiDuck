#include "../../include/config.h"
#include <SdFat.h> //version: 2.3.0
#include <Arduino.h>

#ifdef USE_SD_CARD

/**
 * @defgroup SD_Commands SD Card Remote Commands
 * @brief Command opcodes sent by the receiving device to control SD card operations
 *
 * These constants define the wire protocol used between sd_handler and the
 * receiving device. Each command is a single byte followed by optional arguments.
 * @{
 */
#define SD_CMD_LS         0x10   // !< List directory contents (followed by null-terminated directory path)
#define SD_CMD_READ       0x11   // !< Read file contents (followed by null-terminated file path)
#define SD_CMD_WRITE      0x12   // !< Write file contents (followed by append flag + filename, then data chunks)
#define SD_CMD_RM         0x13   // !< Remove (delete) a file (followed by null-terminated file path)
#define SD_CMD_RUN        0x14   // !< Execute a script from SD card (followed by null-terminated file path)
#define SD_CMD_STOP_RUN   0x15   // !< Stop script execution (no arguments)
#define SD_CMD_STOP       0x16   // !< Abort any ongoing SD operation (list/read/write) (no arguments)
/** @} */

/**
 * @brief Acknowledgment byte used in the streaming protocol
 *
 * For operations that return large amounts of data (SD_CMD_LS, SD_CMD_READ),
 * the receiving device sends this byte after processing each chunk to signal
 * that it is ready for the next one.
 *
 * For SD_CMD_WRITE, this device sends this byte after successfully writing
 * each chunk to signal readiness for the next one.
 */
#define SD_ACK        0x06  

namespace sdcard {

    /**
     * @brief SD card operation status
     *
     * This enum tracks what the SD card is currently doing. The status is read by:
     *   - com.cpp: included in the status struct sent to the receiving device
     *   - sd_handler.cpp: used to determine which streaming operation is active
     *
     * The status is managed internally by this module, except for SD_EXECUTING
     * which is set externally by script_runner when a script starts running.
     *
     * The enum values are deliberately chosen to avoid collision with other status
     * codes in the system:
     *   - 0xA0-0xAF: General states (not present, idle, error)
     *   - 0xB0-0xBF: Operation-specific states (reading, writing, executing, listing)
     */
    enum SDStatus : uint8_t {
        SD_NOT_PRESENT = 0xA0,
        SD_IDLE        = 0xA1,
        SD_ERROR       = 0xA2,
        
        SD_READING     = 0xB0,
        SD_WRITING     = 0xB1,
        SD_EXECUTING   = 0xB2,
        SD_LISTING     = 0xB3,
    };

    /**
     * @brief Initializes the SD card and mounts the filesystem
     *
     * Attempts to initialize the SD card using the chip select pin and speed
     * defined in config.h (SD_CS_PIN, SD_SPEED). Should be called once during
     * setup() before any other SD card operations.
     *
     * @return true if the card was successfully initialized, false if no card
     *         was detected or initialization failed
     */
    bool begin();

    /**
     * @brief Checks if the SD card is present and the filesystem is mounted
     *
     * This is a health check that verifies both the physical card and the
     * filesystem are valid. Called internally at the start of every operation.
     * If the check fails, the status is set to SD_NOT_PRESENT.
     *
     * @return true if the card is present and ready, false otherwise
     */
    bool available();

    /**
     * @brief Sets the current SD card status
     *
     * Allows external modules to update the status. Primarily used by
     * script_runner to set SD_EXECUTING when a script starts running.
     *
     * @param s New status value
     */
    void setStatus(SDStatus s);
    
    /**
     * @brief Returns the current SD card status
     *
     * Read by com.cpp (to include in the status struct sent to the receiving
     * device) and by sd_handler.cpp (to determine which streaming operation
     * is active).
     *
     * @return Current status enum value
     */
    SDStatus getStatus();

    /**
     * @brief Opens a file for reading
     *
     * Opens the specified file in read-only mode. Fails if another file is
     * already open (reading, writing, or listing). Optionally returns the
     * file size via the fileSize pointer.
     *
     * After opening, use readFileChunk() to read data, and optionally peek(),
     * tell(), and seek() for navigation. Call endFileRead() when done.
     *
     * @param filename Path to the file
     * @param fileSize Optional pointer to receive file size in bytes (can be nullptr)
     * @return true if the file was successfully opened, false if the file does
     *         not exist, another file is already open, or the card is unavailable
     */
    bool beginFileRead(const char *filename, uint32_t *fileSize);

    /**
     * @brief Reads a chunk of data from the currently open file
     *
     * Reads up to maxSize bytes into the provided buffer. Returns the actual
     * number of bytes read, which may be less than maxSize if EOF is reached.
     *
     * Only works if a file is currently open for reading.
     *
     * @param buffer Buffer to read into
     * @param maxSize Maximum number of bytes to read
     * @return Number of bytes actually read, or 0 if no file is open or EOF reached
     */
    int16_t readFileChunk(uint8_t *buffer, uint16_t maxSize);

    /**
     * @brief Returns the current file position
     *
     * Returns the byte offset from the start of the file. Only works if a file
     * is currently open for reading.
     *
     * Used by script_runner to save positions for REPEAT and LOOP operations.
     *
     * @return Current byte offset, or 0 if no file is open
     */
    uint32_t tell();

    /**
     * @brief Moves the file position to an absolute offset
     *
     * Seeks to the specified byte position from the start of the file. Only
     * works if a file is currently open for reading.
     *
     * Used by script_runner to jump back for REPEAT commands and to return
     * to LOOP_BEGIN on each iteration.
     *
     * @param pos Target position (byte offset from start of file)
     * @return true if seek succeeded, false if no file is open or seek failed
     */
    bool seek(uint32_t pos);

    /**
     * @brief Returns the next byte without advancing the file position
     *
     * Useful for look-ahead parsing, such as checking UTF-8 character boundaries
     * before deciding how many bytes to read. Only works if a file is currently
     * open for reading.
     *
     * Used by script_runner's get_line() to avoid splitting multi-byte characters
     * across buffer boundaries.
     *
     * @return Next byte in the file, or -1 if no file is open or EOF is reached
     */
    int peek();

    /**
     * @brief Closes the currently open file
     *
     * Closes the file handle and sets the status back to IDLE. Safe to call
     * even if no file is open (does nothing in that case).
     */
    void endFileRead();
    
    /**
     * @brief Opens a file for writing
     *
     * Opens the specified file in write mode. Fails if another file is already
     * open (reading, writing, or listing).
     *
     * The append flag controls whether data is appended to the end of an existing
     * file (append=true) or whether the file is truncated first (append=false).
     *
     * After opening, use writeFileChunk() to write data. The module automatically
     * syncs to physical media every 512 bytes. Call endFileWrite() when done to
     * perform a final sync and close the file.
     *
     * @param n File path
     * @param append If true, append to existing file. If false, overwrite (truncate).
     * @return true if the file was successfully opened, false if another file is
     *         already open, the card is unavailable, or the open operation failed
     */
    bool beginFileWrite(const char* n, bool append);

    /**
     * @brief Writes a chunk of data to the currently open file
     *
     * Writes len bytes from the buffer to the file. The module automatically
     * calls syncFile() every 512 bytes to flush buffered data to physical media.
     *
     * Only works if a file is currently open for writing.
     *
     * @param b Buffer containing data to write
     * @param len Number of bytes to write
     * @return Number of bytes actually written, or 0 if no file is open
     */
    uint16_t writeFileChunk(const uint8_t* b, uint16_t len);
    
    /**
     * @brief Flushes the write buffer to physical media
     *
     * Forces all buffered data to be written to the SD card. Called automatically
     * by writeFileChunk() every 512 bytes, and by endFileWrite() before closing.
     *
     * Can be called manually if you need to ensure data is physically written
     * at a specific point (e.g., before entering a long operation where power
     * might fail).
     *
     * Only works if a file is currently open for writing.
     *
     * @return true if sync succeeded, false if no file is open or sync failed
     */
    bool syncFile();

     /**
     * @brief Closes the currently open file for writing
     *
     * Performs a final sync to ensure all data is written, then closes the file
     * and sets the status back to IDLE. Safe to call even if no file is open
     * (does nothing in that case).
     */
    void endFileWrite();

    /**
     * @brief Deletes a file
     *
     * Removes the specified file from the SD card. Fails if a file is currently
     * open (reading, writing, or listing) to prevent deleting a file that is in use.
     *
     * @param n File path
     * @return true if the file was successfully deleted, false if the file does
     *         not exist, another file is open, or the card is unavailable
     */
    bool removeFile(const char* n);

    /**
     * @brief Deletes a directory
     *
     * Removes the specified directory from the SD card. The directory must be
     * empty. Fails if a file is currently open (reading, writing, or listing).
     *
     * @param n Directory path
     * @return true if the directory was successfully deleted, false if the
     *         directory does not exist, is not empty, another file is open,
     *         or the card is unavailable
     */
    bool removeDir(const char* n);

    /**
     * @brief Opens a directory for listing
     *
     * Opens the specified directory so its contents can be iterated with
     * getNextFile(). Fails if a file is already open (reading, writing, or listing).
     *
     * After opening, call getNextFile() repeatedly to iterate through files.
     * Call endList() when done.
     *
     * @param dir Directory path, or nullptr to list the root directory
     * @return true if the directory was successfully opened, false if the path
     *         does not exist, is not a directory, another file is open, or the
     *         card is unavailable
     */
    bool beginList(const char* dir);

    /**
     * @brief Returns the next file in the directory
     *
     * Iterates through the currently open directory and returns the next file
     * that matches the extension filter. Only files with .txt or .ds extensions
     * (case-insensitive) are returned. Directories are skipped.
     *
     * The filename and size are returned via output parameters. Returns false
     * when no more matching files exist.
     *
     * @param name Buffer to receive the filename (null-terminated)
     * @param maxLen Maximum length of the filename buffer
     * @param size Pointer to receive the file size in bytes
     * @return true if a matching file was found, false if no more files exist
     */
    bool getNextFile(char* name, uint8_t maxLen, uint32_t* size);

    /**
     * @brief Closes the currently open directory
     *
     * Closes the directory handle and sets the status back to IDLE. Safe to
     * call even if no directory is open (does nothing in that case).
     */
    void endList();

}

#endif