/*
   Part of the code in this file is taken from the repository: https://github.com/spacehuhntech/usbnova
   
   Modified and adapted by:
    - Dereck81
*/

#include "../../include/debug.h"
#include "../../include/config.h"
#include "../duckparser/duckparser.h"
#include "../led/led.h"
#include "sdcard.h"
#include "script_runner.h"
#include <Arduino.h>

#ifdef USE_SD_CARD

/**
 * @file script_runner.cpp
 * @brief Script execution engine for DuckyScript files stored on SD card
 * 
 * This module handles reading and executing DuckyScript files line by line from
 * an SD card. It manages the execution flow, including repetitions, loops, and
 * multi-line string blocks, while properly handling UTF-8 encoded characters.
 */
namespace script_runner {

    /**
     * @brief Internal state structure to track script execution progress
     * 
     * This structure maintains all necessary information about the current
     * execution state, including file positions for seeking and flags for
     * tracking which parsing mode we're currently in.
     */
    static struct {
        uint32_t loop_pos;      // File position where LOOP_BEGIN was encountered
        uint32_t last_pos;      // Starting position of the last valid command line
        uint32_t after_repeat;  // Position immediately after a REPEAT command
        uint32_t cur_pos;       // Cursor position at the start of current read
        uint8_t  flags;         // State flags (running, in loop, in string block, etc.)
        uint8_t repeat_count;   // Number of repetitions remaining for current command
    } state;

    /**
     * @brief Pointer to the buffer where file data is read
     * 
     * This buffer is provided externally during start() and is used to store
     * each line before parsing. The buffer size is defined by BUFFER_SIZE.
     */
    static uint8_t* buffer = nullptr;

    /**
     * @brief Number of bytes read into buffer during last get_line() call
     * 
     * This value indicates how many valid bytes are in the buffer and should
     * be passed to the parser.
     */
    size_t read  = 0;
    
    // Flag bit definitions for state.flags
    #define FLAG_RUNNING          0x01  // !< Script execution is active
    #define FLAG_STOP_READING     0x02  // !< Stop reading immediately (newline or EOF found)
    #define FLAG_IN_LINE          0x04  // !< Currently reading a line that spans multiple buffers
    #define FLAG_IN_LSTRING_BLOCK 0x08  // !< Inside LSTRING_BEGIN...LSTRING_END block
    #define FLAG_IN_LOOP_BLOCK    0x10  // !< Inside LOOP_BEGIN...LOOP_END block
    #define FLAG_IN_LOOP_INFINITE 0x20  // !< Current loop is infinite (negative iteration count)

    // Macros for checking flag states
    #define IS_RUNNING            (state.flags & FLAG_RUNNING)
    #define IS_STOP_READING       (state.flags & FLAG_STOP_READING)
    #define IS_IN_LINE            (state.flags & FLAG_IN_LINE)
    #define IS_IN_LSTRING_BLOCK   (state.flags & FLAG_IN_LSTRING_BLOCK)
    #define IS_IN_LOOP_BLOCK      (state.flags & FLAG_IN_LOOP_BLOCK)
    #define IS_IN_LOOP_INFINITE   (state.flags & FLAG_IN_LOOP_INFINITE)

    // Macros for modifying flags
    #define SET_FLAG(f)           (state.flags |= (f))
    #define CLR_FLAG(f)           (state.flags &= ~(f))

    /**
     * @brief Initializes the script runner and begins reading from the SD card
     * 
     * This function prepares the script execution environment by opening the
     * specified file, initializing the state machine, and setting up hardware
     * indicators (LEDs). It must be called before any calls to execute_next_line().
     * 
     * @param filename Path to the script file on the SD card
     * @param buff Pointer to buffer where file chunks will be stored
     * @return true if file was successfully opened, false otherwise
     */
    bool start(const char* filename, uint8_t* buff) {
        // Attempt to open the file for reading
        if (!sdcard::beginFileRead(filename, nullptr)) return false;
        if (!buff) return false;

        // Store buffer pointer for later use
        buffer             = buff;
        
        // Initialize state machine
        state.flags        = FLAG_RUNNING;
        state.repeat_count = 0;
        state.after_repeat = 0;
        state.last_pos     = 0;
        state.loop_pos     = 0;
        read               = 0;

        // Update SD card status
        sdcard::setStatus(sdcard::SDStatus::SD_EXECUTING);

        led::left(true);

        // Reset parser state from any previous execution
        duckparser::reset();
        
        return true;
    }

    /**
     * @brief Halts execution, closes the file, and resets hardware indicators
     * 
     * This function performs cleanup when script execution is finished or stopped.
     * It closes the file handle, resets the state machine, and turns off the LED.
     */
    void stop() {
        // Clear all state flags
        state.flags = 0;

        // Close the file
        sdcard::endFileRead();

        // Update SD card status back to idle
        sdcard::setStatus(sdcard::SDStatus::SD_IDLE);

        led::left(false);
    }

    /**
     * @brief Reads a single line from the SD card into the buffer
     * 
     * This function is UTF-8 aware and uses a peek-ahead mechanism to ensure
     * multi-byte characters (emojis, accented characters, etc.) are not split
     * across buffer boundaries. It reads until a newline is encountered or the
     * buffer is full.
     * 
     * The function sets FLAG_IN_LINE if the line exceeds BUFFER_SIZE and needs
     * to be read in multiple chunks. It normalizes line endings by converting
     * carriage returns (\r) to newlines (\n) and skips redundant newlines.
     */
    void get_line() {
        uint8_t c;
        uint8_t need = 1; // Number of bytes needed for current character
        read = 0;
        while (read < BUFFER_SIZE - 1) {

            // Peek at next byte to determine character length
            if (sdcard::peek() >= 0) {
                c = sdcard::peek();

                // Determine UTF-8 character length from first byte
                if ((c & 0x80) == 0x00)      need = 1; // standard ASCII
                else if ((c & 0xE0) == 0xC0) need = 2; // 2-byte character (ñ, á, etc.)
                else if ((c & 0xF0) == 0xE0) need = 3; // 3-byte character (Asian characters)
                else if ((c & 0xF8) == 0xF0) need = 4; // 4-byte character (emojis)

                // If character won't fit in remaining buffer space, stop here
                // This prevents splitting a multi-byte character across reads
                if (read + need > BUFFER_SIZE -1) {
                    SET_FLAG(FLAG_IN_LINE); // Mark that line continues
                    break;
                }
            }

            CLR_FLAG(FLAG_STOP_READING);

            // Read all bytes for this character
            for (uint8_t i = 0; i < need; i++) {
                // Try to read one byte
                if (sdcard::readFileChunk(&c, 1) <= 0) {
                    CLR_FLAG(FLAG_RUNNING | FLAG_IN_LINE);
                    SET_FLAG(FLAG_STOP_READING);
                    buffer[read++] = '\n'; // Add newline to properly end last line
                    break;
                }

                // Normalize line endings: convert \r to \n
                if (c == '\r') c = '\n';
                    
                buffer[read++] = c;
                
                // If we hit a newline, we've completed the line
                if (c == '\n') {
                    // Skip any additional consecutive newlines
                    while(sdcard::peek() == '\n') sdcard::readFileChunk(&c, 1);

                    CLR_FLAG(FLAG_IN_LINE);       // Line is complete
                    SET_FLAG(FLAG_STOP_READING);  // Stop reading
                    break;
                }

            }

            // If we hit EOF or newline, exit the main loop
            if (IS_STOP_READING) break;

            // If we get here, we successfully read a character
            SET_FLAG(FLAG_IN_LINE);
            
        }
    }

    /**
     * @brief Detects and manages LSTRING block boundaries
     * 
     * LSTRING (Long String) blocks are used for typing literal text that may
     * span multiple lines. Everything between LSTRING_BEGIN and LSTRING_END
     * is treated as raw text, including newlines and special characters.
     * 
     * This function checks if the current buffer contains either:
     * - LSTRING_BEGIN: Sets flag to start treating content as literal text
     * - LSTRING_END: Clears flag to resume normal command parsing
     * 
     * The flag prevents the runner from treating content inside these blocks
     * as commands and preserves the exact text for typing.
     * 
     * It also helps to work with REPEAT, allowing you to repeat an LSTRING 
     * the specified number of times.
     */
    void check_lstring_block() { 
        // Check for LSTRING_BEGIN (13 characters)
        if (read >= 13 && !IS_IN_LSTRING_BLOCK &&  memcmp((char*)buffer, "LSTRING_BEGIN", 13) == 0) {
            SET_FLAG(FLAG_IN_LSTRING_BLOCK);
        }
        
        // Check for LSTRING_END (11 characters)
        else if (read >= 11 && IS_IN_LSTRING_BLOCK && memcmp((char*)buffer, "LSTRING_END", 11) == 0) {
            CLR_FLAG(FLAG_IN_LSTRING_BLOCK);
        }
    }

    /**
     * @brief Manages LOOP block execution and file seeking
     * 
     * LOOP blocks allow repeating a section of code multiple times. The syntax is:
     *   LOOP_BEGIN N
     *   ... commands to repeat ...
     *   LOOP_END
     * 
     * Where N is the number of iterations:
     * - N > 0: Loop executes N times
     * - N <= 0: Loop runs infinitely
     * 
     * This function:
     * - On LOOP_BEGIN: Records the file position and checks for infinite loops
     * - On LOOP_END: Seeks back to loop start if iterations remain
     * 
     * The loop counter is managed by duckparser, which decrements it each iteration.
     * 
     * It does not allow repetition like in LSTRING
     * It does not support nested loops
     */
    void check_loop_block() { 
        // Check for LOOP_BEGIN (10 characters)
        if (read >= 10 && !IS_IN_LSTRING_BLOCK && !IS_IN_LOOP_BLOCK &&  memcmp((char*)buffer, "LOOP_BEGIN", 10) == 0) {
            SET_FLAG(FLAG_IN_LOOP_BLOCK);
            
            // Save current file position (right after LOOP_BEGIN line)
            state.loop_pos = sdcard::tell();

            // Check if this is an infinite loop
            if (duckparser::getLoops() <= 0) SET_FLAG(FLAG_IN_LOOP_INFINITE);
        }

        // Check for LOOP_END (8 characters)
        else if (read >= 8 && !IS_IN_LSTRING_BLOCK && IS_IN_LOOP_BLOCK && memcmp((char*)buffer, "LOOP_END", 8) == 0) {
            // If loop counter reached 0 and it's not infinite, exit the loop
            if (duckparser::getLoops() == 0 && !IS_IN_LOOP_INFINITE) CLR_FLAG(FLAG_IN_LOOP_BLOCK);

            // Otherwise, seek back to loop start to repeat
            else sdcard::seek(state.loop_pos);
        }
    }

    /**
     * @brief Processes the next logical step of the script execution
     * 
     * This is the main execution function that should be called repeatedly
     * until it returns false. It implements the core logic for handling
     * repeats, reading new lines, parsing commands, and managing execution flow.
     * 
     * Execution logic:
     * 
     * 1. REPEAT handling:
     *    When a REPEAT command is executed, repeat_count is set to N.
     *    For each repetition, we seek back to last_pos and re-read/re-execute
     *    the previous command. This avoids storing commands in RAM.
     * 
     * 2. Normal line execution:
     *    - Read a new line from SD card
     *    - Parse and execute it via duckparser
     *    - Wait for any delays to complete
     *    - Check if this command should be repeated
     * 
     * 3. Fragment handling:
     *    If a line is too long for the buffer (FLAG_IN_LINE is set), we process
     *    it in chunks. The parser accumulates these chunks until the line ends.
     * 
     * 4. Position tracking:
     *    - last_pos: Updated when a complete, non-REPEAT command finishes
     *    - after_repeat: Set to position after REPEAT for resuming execution
     *    - loop_pos: Set at LOOP_BEGIN for jumping back on LOOP_END
     * 
     * @return true if execution should continue, false if script finished or stopped
     */
    bool execute_next_line() {
        // Exit if not running and no pending work
        if (!IS_RUNNING && !IS_IN_LOOP_BLOCK && state.repeat_count == 0) return false;

        // ===== REPEAT EXECUTION BRANCH =====
        // If we have repetitions pending, re-execute the previous command
        if (state.repeat_count > 0) {
            // We search again for the last valid command position 
            // (unless we are in the middle of the line and are inside an LSTRING)
            if (!IS_IN_LINE && !IS_IN_LSTRING_BLOCK) sdcard::seek(state.last_pos);
            
            // Read the line again
            get_line();
            
            // Debug output (commented in production)
            //debug("Read line: ");
            //for(size_t i = 0; i < read; i++) debug((char)buffer[i]);
            //debugln("");

            // Parse and execute the line
            duckparser::parse(buffer, read);

            // Check if this line changes LSTRING block state
            check_lstring_block();

            // Update the repetition counter upon completion of the line
            // (Only decrement after reading the entire line and not being inside an LSTRING)
            if (!IS_IN_LINE && !IS_IN_LSTRING_BLOCK) 
                state.repeat_count = (uint8_t)(duckparser::getRepeats() > 255 ? 255 : duckparser::getRepeats());

            // WHere it checks if the repetitions have finished; if so, 
            // it skips the REPEAT command, going to the position after the REPEAT.
            if (state.repeat_count == 0) sdcard::seek(state.after_repeat);
            
            return true;
        }

        // ===== NORMAL EXECUTION BRANCH =====
        
        // Save current file position before reading
        // (This will become last_pos after the line is fully processed)       
        if (!IS_IN_LINE) state.cur_pos = sdcard::tell();

        // Read next line from file
        get_line();

        // If we read nothing, we've reached the end of the file
        if (read == 0) {
            stop();
            return false;
        }

        // Parse and execute the line
        duckparser::parse(buffer, read);

        // Wait for any delays to complete before continuing
        // This ensures proper timing between commands
        while (duckparser::getDelayTime() != 0) delay(5);

        // Check if the last valid command before REPEAT should be repeated
        // The parser sets the number of repetitions when it encounters a REPEAT command
        state.repeat_count = (uint8_t)(duckparser::getRepeats() > 255 ? 255 : duckparser::getRepeats());

        // If we need to repeat this command
        if (state.repeat_count > 0) {
            // Save position after REPEAT for later resumption
            state.after_repeat = sdcard::tell();

            // Clear the IN_LINE flag to force a fresh read on next iteration
            CLR_FLAG(FLAG_IN_LINE);

            return true;
        } 

        // ===== POST-PROCESSING FOR COMPLETE LINES =====
        
        // If we're still reading fragments of a long line, just continue
        // No need to update positions or check for blocks yet
        if (IS_IN_LINE) return true;

        // ===== LINE COMPLETED - UPDATE STATE =====
        
        // Update last_pos to point to this command for potential REPEAT
        // Exception: Don't update if we're inside an LSTRING block, because
        // LSTRING content is not a command and shouldn't be repeated
        if (!IS_IN_LSTRING_BLOCK) state.last_pos = state.cur_pos;
        
        // Check if this line starts or ends an LSTRING block
        check_lstring_block();

        // Check if this line starts or ends a LOOP block
        check_loop_block();

        return true;

    }


}

#endif