/*
   This software is licensed under the MIT License. See the license file for details.
   Source: https://github.com/spacehuhntech/WiFiDuck

   Modified and adapted by:
    - Dereck81
   
   Part of the code in this file is taken from the repository: https://github.com/spacehuhntech/usbnova
 */

#include "duckscript.h"

#include "config.h"
#include "debug.h"

#include "com.h"
#include "spiffs.h"
#include "sdcard.h"

/**
 * @file duckscript.cpp
 * @brief WiFiDuck script execution engine using SPIFFS filesystem
 *
 * This module is responsible for reading DuckyScript files stored in the SPIFFS
 * filesystem and sending their content line by line to a remote parser through
 * the com module (over WiFi). It does not interpret the commands itself — that
 * responsibility belongs to the device on the other end of the connection.
 *
 * The execution loop, delay handling, and command interpretation all happen 
 * remotely once com::send() delivers each line.
 *
 * The module supports three special constructs that require local state management:
 *   - REPEAT:       Re-sends the previous command N times by seeking back in the file
 *   - LSTRING:      Marks raw text blocks so the runner stops updating last_pos
 *   - LOOP:         Jumps the file cursor back to LOOP_BEGIN on each LOOP_END
 */
namespace duckscript {
    // ===== PRIVATE ===== //

    /**
     * @brief File handle for the currently active script
     *
     * This is the SPIFFS File object used for all read and seek operations.
     * It remains open for the entire duration of script execution and is
     * closed when stopAll() or stop() is called.
     */
    File f;

    /**
     * @brief Internal state structure to track script execution progress
     *
     * Holds all the file positions and flags needed to support REPEAT,
     * LSTRING, and LOOP features. Every field here exists to allow the
     * module to seek to specific points in the file without having to
     * re-read from the beginning.
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
     * @brief Buffer that holds the current line read from the file
     *
     * Sized according to BUFFER_SIZE defined in config.h. A null terminator
     * is always appended after the last byte read, making the buffer safe
     * to use as a C string when needed.
     */
    static char buffer[BUFFER_SIZE];
    
    /**
     * @brief Number of valid bytes in the buffer after the last get_line() call
     *
     * This is passed along with the buffer to com::send() so the remote
     * parser knows exactly how many bytes to process.
     */
    unsigned int read =  0;
    
    // Flag bit definitions for state.flags
    #define FLAG_RUNNING          0x01  // !< Script is currently active and being executed
    #define FLAG_STOP_READING     0x02  // !< Signals get_line() to stop (newline or EOF reached)
    #define FLAG_IN_LINE          0x04  // !< A line was too long for the buffer; more data remains
    #define FLAG_IN_LSTRING_BLOCK 0x08  // !< We are inside an LSTRING_BEGIN...LSTRING_END block
    #define FLAG_IN_LOOP_BLOCK    0x10  // !< We are inside a LOOP_BEGIN...LOOP_END block
    #define FLAG_IN_LOOP_INFINITE 0x20  // !< The current loop has no iteration limit

    // Flag query macros
    #define IS_RUNNING            (state.flags & FLAG_RUNNING)
    #define IS_STOP_READING       (state.flags & FLAG_STOP_READING)
    #define IS_IN_LINE            (state.flags & FLAG_IN_LINE)
    #define IS_IN_LSTRING_BLOCK   (state.flags & FLAG_IN_LSTRING_BLOCK)
    #define IS_IN_LOOP_BLOCK      (state.flags & FLAG_IN_LOOP_BLOCK)
    #define IS_IN_LOOP_INFINITE   (state.flags & FLAG_IN_LOOP_INFINITE)

    // Flag manipulation macros
    #define SET_FLAG(f)           (state.flags |= (f))
    #define CLR_FLAG(f)           (state.flags &= ~(f))

    // ===== PUBLIC ===== //

    /**
     * @brief Opens a script file and begins execution
     *
     * Resets the entire state machine and opens the specified file from SPIFFS.
     * If an SD card read is already in progress, execution is blocked to avoid
     * conflicts. Once the file is opened, FLAG_RUNNING is set and nextLine()
     * is called once to send the first line immediately.
     *
     * @param fileName Name of the script file stored in SPIFFS
     */
    void run(String fileName) {
        #ifdef USE_SD_CARD
        // Do not start if the SD card is already being read by another process
        if (com::get_sdcard_status() >= sdcard::SD_READING) return;
        #endif

        // Full state reset before opening a new file
        state.repeat_count = 0;
        state.after_repeat = 0;
        state.last_pos     = 0;
        state.loop_pos     = 0;
        state.flags        = 0;
        read               = 0;

        if (fileName.length() > 0) {
            debugf("Run file %s\n", fileName.c_str());
            f       = spiffs::open(fileName);
            state.flags = FLAG_RUNNING;

            // We send the CMD_PARSER_RESET command to reset all values 
            // ​​from previous executions and prevent conflicts. The next call executes nextLine().
            com::send(CMD_PARSER_RESET);
            //nextLine();
        }
    }

    /**
     * @brief Detects LSTRING_BEGIN and LSTRING_END in the current buffer
     *
     * LSTRING blocks contain raw literal text that should be typed exactly
     * as written, including newlines. While FLAG_IN_LSTRING_BLOCK is set,
     * the module stops updating last_pos, which prevents REPEAT from
     * accidentally targeting a line of raw text instead of a real command.
     *
     * This function must be called after every line is read, even during
     * repeat cycles, to keep the flag in sync.
     * 
     * It also helps to work with REPEAT, allowing you to repeat an LSTRING 
     * the specified number of times.
     */
    void check_lstring_block() {
        if (read >= 13 && !IS_IN_LSTRING_BLOCK &&  memcmp(buffer, "LSTRING_BEGIN", 13) == 0) {
            SET_FLAG(FLAG_IN_LSTRING_BLOCK);
        }
        
        else if (read >= 11 && IS_IN_LSTRING_BLOCK && memcmp(buffer, "LSTRING_END", 11) == 0) {
            CLR_FLAG(FLAG_IN_LSTRING_BLOCK);
        }
    }

     /**
     * @brief Detects LOOP_BEGIN and LOOP_END and manages the loop jump
     *
     * When LOOP_BEGIN is encountered, the current file position is saved
     * into state.loop_pos. The loop iteration count is read from com,
     * which already parsed the value on the line. If that count is zero
     * or negative, the loop is marked as infinite.
     *
     * When LOOP_END is encountered:
     *   - If the iteration count has reached zero and the loop is not
     *     infinite, the loop block flag is cleared and execution continues
     *     past LOOP_END normally.
     *   - Otherwise, the file cursor is jumped back to loop_pos so the
     *     block executes again from the top.
     *
     * Note: Nested loops are not supported. A second LOOP_BEGIN before a
     * LOOP_END is ignored.
     * 
     * It does not allow repetition like in LSTRING
     */
    void check_loop_block() {
        if (read >= 10 && !IS_IN_LSTRING_BLOCK && !IS_IN_LOOP_BLOCK &&  memcmp(buffer, "LOOP_BEGIN", 10) == 0) {

            SET_FLAG(FLAG_IN_LOOP_BLOCK);
            // Record where the loop body starts (right after this line)
            state.loop_pos = f.position();
            
            // If the remote parser reported zero or fewer iterations, treat as infinite
            if (com::get_loops() <= 0) SET_FLAG(FLAG_IN_LOOP_INFINITE);
        }
        
        else if (read >= 8 && !IS_IN_LSTRING_BLOCK && IS_IN_LOOP_BLOCK && memcmp(buffer, "LOOP_END", 8) == 0) {
            // Loop finished all iterations — exit the block
            if (com::get_loops() == 0 && !IS_IN_LOOP_INFINITE) CLR_FLAG(FLAG_IN_LOOP_BLOCK);

            // Still iterations remaining or infinite — jump back to loop body
            else f.seek(state.loop_pos, SeekSet);
        }
    }

    /**
     * @brief Reads a single line from the file into the buffer
     *
     * The function is UTF-8 aware. Before reading each character it peeks at
     * the next byte to determine how many bytes that character occupies. If
     * the full character would not fit in the remaining buffer space, reading
     * stops and FLAG_IN_LINE is set so the caller knows to call get_line()
     * again to continue.
     *
     * Line ending handling:
     *   - Carriage returns (\r) are converted to newlines (\n)
     *   - Consecutive newlines are collapsed into one
     *   - A newline is appended artificially when EOF is reached mid-line
     *
     * A null terminator is written at buffer[read] after the loop finishes,
     * making the buffer usable as a standard C string.
     */
    void get_line() {
        uint8_t c;
        uint8_t need = 1; // How many bytes the current character requires
        read = 0;
        while (f.available() && read < BUFFER_SIZE - 1) {

            // Peek at the next byte to figure out character width
            if (f.peek() >= 0) {
                c = f.peek();

                if ((c & 0x80) == 0x00)      need = 1; // standard ASCII
                else if ((c & 0xE0) == 0xC0) need = 2; // 2-byte character (ñ, á, etc.)
                else if ((c & 0xF0) == 0xE0) need = 3; // 3-byte character (Asian characters)
                else if ((c & 0xF8) == 0xF0) need = 4; // 4-byte character (emojis)

                // Not enough room left for this character — stop without
                // reading it, so it will be picked up on the next call
                if (read + need > BUFFER_SIZE -1) {
                    SET_FLAG(FLAG_IN_LINE);
                    break;
                }
            }

            CLR_FLAG(FLAG_STOP_READING);

            // Read all bytes that make up this character
            for (uint8_t i = 0; i < need; i++) {

                if (f.available() <= 0) {
                    // EOF reached in the middle of reading — close out the line
                    CLR_FLAG(FLAG_RUNNING | FLAG_IN_LINE);
                    SET_FLAG(FLAG_STOP_READING);
                    buffer[read++] = '\n'; // Synthetic newline to properly terminate the last line
                    break;
                }

                c = f.read();

                // Normalize \r to \n so the parser always sees Unix-style endings
                if (c == '\r') c = '\n';
                    
                buffer[read++] = c;
                    
                if (c == '\n') {
                    // Skip over any immediately following newlines to avoid
                    // sending blank lines to the parser
                    while(f.peek() == '\n') f.read();

                    CLR_FLAG(FLAG_IN_LINE);       // This line is fully read
                    SET_FLAG(FLAG_STOP_READING);  // Exit the outer loop
                    break;
                }

            }

            // If we hit EOF or newline, exit the main loop
            if (IS_STOP_READING) break;

            // We finished reading a character but haven't hit a newline yet,
            // so the line is still in progress
            SET_FLAG(FLAG_IN_LINE);
            
        }

        // Null-terminate the buffer for C string compatibility
        buffer[read] = '\0';
    }

     /**
     * @brief Reads the next line from the file and sends it to the remote parser
     *
     * This is the main per-tick function. It is called repeatedly by the
     * external event loop — once per cycle — and handles exactly one line
     * (or one fragment of a long line) per call.
     *
     * The function performs the following steps:
     *   1. Guards: checks that the SD card is not busy, the file is valid,
     *      and data is still available.
     *   2. Captures the current file position (for future REPEAT seeking),
     *      but only when we are at the start of a new line.
     *   3. Reads a line via get_line().
     *   4. Sends it to the remote parser via com::send().
     *   5. If the line is a REPEAT command, saves the position immediately
     *      after it (after_repeat) and returns — the external loop will call
     *      repeat() for each repetition instead of nextLine().
     *   6. If the line is still fragmented (FLAG_IN_LINE), returns immediately
     *      so the next call finishes reading it.
     *   7. Updates last_pos and checks for LSTRING block boundaries.
     *
     * Note: LOOP block detection (check_loop_block) is handled on the remote
     * side through com, so it is not called here.
     */
    void nextLine() {
        #ifdef USE_SD_CARD
        // If the SD card has been claimed by another process while we were running,
        // abort immediately
        if (com::get_sdcard_status() >= sdcard::SD_READING && IS_RUNNING) {
            stopAll();
            return;
        }
        #endif

        // Nothing to do if the script is not active and we are not inside a loop
        if (!IS_RUNNING && !IS_IN_LOOP_BLOCK) return;

        // Validate the file handle — if it is gone, something went wrong
        if (!f) {
            debugln("File error");
            stopAll();
            return;
        }

        // No more data in the file — we have reached the end
        if (!f.available()) {
            debugln("Reached end of file");
            stopAll();
            return;
        }

        // Snapshot the file position before reading. This will become last_pos
        // once we confirm the line is complete and is a real command.
        if (!IS_IN_LINE) state.cur_pos = f.position();

        get_line();

        // Nothing was read — treat as end of file
        if (read == 0) {
            stopAll();
            return;
        }

        // Deliver the line to the remote parser
        com::send(buffer, read);

        // If this line is a REPEAT command, record where to resume after
        // all repetitions are done, then hand control to repeat()
        if(memcmp(buffer, "REPEAT", 6) == 0) {
            state.after_repeat = f.position();
            CLR_FLAG(FLAG_IN_LINE); // Force a clean start for the repeated line
            return;
        }

        // If we are still in the middle of a long line, return and wait for
        // the next call to finish reading the rest
        if (IS_IN_LINE) return;

        // The line is complete. Update last_pos so that a future REPEAT can
        // seek back to it. Skip the update if we are inside an LSTRING block
        // because those lines are raw text, not repeatable commands.
        if (!IS_IN_LSTRING_BLOCK) 
            state.last_pos = state.cur_pos;

        // Check whether this line opens or closes an LSTRING block
        check_lstring_block();

    }

    /**
     * @brief Re-sends the previous command to satisfy a REPEAT request
     *
     * This function is called externally by the event loop once per tick
     * while repetitions are still pending. On each call it:
     *   1. Seeks back to last_pos (the start of the command to repeat),
     *      unless we are mid-fragment or inside an LSTRING block.
     *   2. Reads that line again via get_line().
     *   3. Sends it to the remote parser via com::send().
     *   4. Queries com::get_repeats() to see how many repetitions the
     *      remote side still expects. When that value reaches zero, it
     *      seeks forward to after_repeat so that nextLine() resumes from
     *      the correct position after the REPEAT command.
     *
     * The repeat count is owned and decremented by the remote parser (com).
     * This function simply reads that count and reacts when it hits zero.
     */
    void repeat() {
        // Returns to the command that needs to be repeated, 
        // but only if we are not reading a fragmented line or are inside an LSTRING
        if (!IS_IN_LINE && !IS_IN_LSTRING_BLOCK) 
            f.seek(state.last_pos, SeekSet);
            
        get_line();

        // Save the value of get_repeats before sending the command.
        int repeats = com::get_repeats() - 1;

        // Send the line again
        com::send(buffer, read);

        // Keep LSTRING state in sync even during repeats
        check_lstring_block();

        // If this was the last repetition and the line is complete and we are not in an LSTRING, 
        // skip the REPEAT command to resume normal execution.
        if (!IS_IN_LINE && !IS_IN_LSTRING_BLOCK && (repeats == 0))
            f.seek(state.after_repeat, SeekSet);
        
    }

    /**
     * @brief Unconditionally stops execution and closes the file
     *
     * Closes the file handle if it is open, clears FLAG_RUNNING, and then
     * zeros out the entire flags field. This is the hard stop — it does
     * not care which script is running or why.
     */
    void stopAll() {
        if (IS_RUNNING) {
            if (f) f.close();
            CLR_FLAG(FLAG_RUNNING);
            debugln("Stopped script");
        }
        state.flags = 0;
    }

    /**
     * @brief Stops execution, optionally targeting a specific script by name
     *
     * If fileName is empty, this behaves identically to stopAll(). If a name
     * is provided, the file is only closed if it matches the currently running
     * script. This prevents one part of the system from accidentally killing
     * a script that was started by another.
     *
     * @param fileName Name of the script to stop, or an empty string to stop
     *                 whatever is currently running
     */
    void stop(String fileName) {
        if (fileName.length() == 0) stopAll();
        else {
            if (IS_RUNNING && f && (fileName == currentScript())) {
                f.close();
                CLR_FLAG(FLAG_RUNNING);
                debugln("Stopped script");
            }
        }
        state.flags = 0;
    }

    /**
     * @brief Returns whether a script is currently executing
     *
     * @return true if FLAG_RUNNING is set, false otherwise
     */
    bool isRunning() {
        return IS_RUNNING;
    }

    /**
     * @brief Returns the name of the script that is currently running
     *
     * If no script is active, returns an empty String.
     *
     * @return The file name of the active script, or an empty String
     */
    String currentScript() {
        if (!IS_RUNNING) return String();
        return String(f.name());
    }
}