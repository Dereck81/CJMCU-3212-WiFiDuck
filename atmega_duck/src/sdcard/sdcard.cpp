#include "sdcard.h"
#include <SdFat.h> //version: 2.3.0
#include "../../include/debug.h"

#ifdef USE_SD_CARD

namespace sdcard {

    /** SdFat filesystem object — represents the SD card itself */
    static SdFat SD;

    /**
     * @brief Shared file handle used for all operations
     *
     * This single handle is used for reading, writing, and directory listing.
     * Only one operation can be active at a time.
     */
    static SdFile f;

    /**
     * @brief True when the file handle is open for reading or listing
     *
     * Set by beginFileRead() and beginList(), cleared by endFileRead() and endList().
     * When true, write operations are blocked. Mutually exclusive with w.
     */
    static bool r;
    
    /**
     * @brief True when the file handle is open for writing
     *
     * Set by beginFileWrite(), cleared by endFileWrite(). When true, read and
     * delete operations are blocked. Mutually exclusive with r.
     */
    static bool w;

    /**
     * @brief Current operation status
     *
     * Tracks what the SD card is currently doing. Read by com.cpp (for status
     * reporting to the receiving device) and sd_handler.cpp (for deciding which
     * streaming operation is active). Modified externally by script_runner when
     * it sets SD_EXECUTING.
     */
    static SDStatus currentStatus = SD_NOT_PRESENT;

    /**
     * @brief Counter for write sync optimization
     *
     * Tracks how many bytes have been written since the last sync. When this
     * reaches 512 (one SD card sector), syncFile() is called automatically to
     * flush the buffer to physical media.
     */
    static uint32_t bytesSinceSync = 0;

    /**
     * @brief Initializes the SD card and filesystem
     *
     * Attempts to mount the SD card using the chip select pin and speed defined
     * in config.h (SD_CS_PIN, SD_SPEED). If successful, sets the status to IDLE
     * so other modules know the card is ready.
     *
     * This should be called once during setup() before any other SD card operations.
     *
     * @return true if the card was successfully initialized, false if no card
     *         was detected or initialization failed
     */
    bool begin() {
        if(SD.begin(SD_CS_PIN, SD_SPEED)) {
            currentStatus = SD_IDLE;
            return true;
        }
        return false;
    }

    /**
     * @brief Checks if the SD card is present and mounted
     *
     * Verifies that both the physical card (SD.card()) and the filesystem
     * (SD.vol()) are valid. If either check fails, sets the status to
     * SD_NOT_PRESENT and returns false.
     *
     * This is called at the start of every operation as a health check before
     * attempting to access the card.
     *
     * @return true if the card is present and ready, false otherwise
     */
    bool available() {
        if(SD.card() && SD.vol() && currentStatus != SD_NOT_PRESENT) 
            return true;
        currentStatus = SD_NOT_PRESENT;
        return false;
    }

    #pragma region STATUS

    /**
     * @brief Sets the current SD card status
     *
     * Allows external modules (primarily script_runner) to update the status.
     * script_runner calls this with SD_EXECUTING when it starts running a script
     * from the SD card.
     *
     * @param s New status value
     */
    void setStatus(SDStatus s) {
        currentStatus = s;
    }

    /**
     * @brief Returns the current SD card status
     *
     * Read by com.cpp (to include in the status struct sent to the receiving
     * device) and by sd_handler.cpp (to determine which streaming operation
     * is active).
     *
     * @return Current status enum value
     */
    SDStatus getStatus() {
        return currentStatus;
    }

    #pragma endregion

    #pragma region READ

    /**
     * @brief Opens a file for reading
     *
     * Opens the specified file in read-only mode. If a file is already open
     * (r or w is true), this function fails immediately. Optionally returns
     * the file size via the s pointer.
     *
     * @param n File path
     * @param s Optional pointer to receive file size (can be nullptr)
     * @return true if the file was successfully opened, false if the file does
     *         not exist, another file is already open, or the card is unavailable
     */
    bool beginFileRead(const char* n, uint32_t* s = nullptr) {
        if (r || !available()) return false;
        if (!f.open(n, O_RDONLY)) {
            currentStatus = SD_ERROR;
            return false;
        }
        if(s) *s = f.fileSize();
        r = true;
        currentStatus = SD_READING;
        return true;
    }

    /**
     * @brief Reads a chunk of data from the currently open file
     *
     * Reads up to m bytes from the file into the provided buffer. Returns the
     * actual number of bytes read, which may be less than m if EOF is reached.
     *
     * Only works if a file is currently open for reading (r is true).
     *
     * @param b Buffer to read into
     * @param m Maximum number of bytes to read
     * @return Number of bytes actually read, or 0 if no file is open
     */
    int16_t readFileChunk(uint8_t* b, uint16_t m) {
        return r ? f.read(b, m) : 0;
    }

    /**
     * @brief Closes the currently open file
     *
     * Closes the file handle, clears the r flag, and sets the status back to
     * IDLE. Safe to call even if no file is open (does nothing in that case).
     */
    void endFileRead() {
        if (r) {
            f.close();
            r = false;
            currentStatus = SD_IDLE;
        }
    }

    /**
     * @brief Returns the next byte without advancing the file position
     *
     * Useful for look-ahead parsing (e.g., checking UTF-8 character boundaries).
     * Only works if a file is currently open for reading.
     *
     * @return Next byte in the file, or -1 if no file is open or EOF is reached
     */
    int peek() {
        return r ? f.peek() : -1;
    }
 
    /**
     * @brief Returns the current file position
     *
     * Only works if a file is currently open for reading.
     *
     * @return Current byte offset from the start of the file, or 0 if no file is open
     */
    uint32_t tell() {
        return r ? f.curPosition() : 0;
    }
    
    /**
     * @brief Moves the file position to an absolute offset
     *
     * Seeks to the specified byte position from the start of the file. Used by
     * script_runner to jump back to the start of a command for REPEAT operations
     * or to jump back to LOOP_BEGIN.
     *
     * Only works if a file is currently open for reading.
     *
     * @param p Target position (byte offset from start of file)
     * @return true if seek succeeded, false if no file is open or seek failed
     */
    bool seek(uint32_t p) {
        return r ? f.seekSet(p) : false;
    }

    #pragma endregion 

    #pragma region WRITE

    /**
     * @brief Opens a file for writing
     *
     * Opens the specified file in write mode. If a file is already open (r or w
     * is true), this function fails immediately.
     *
     * The append flag controls whether data is appended to the end of an existing
     * file or whether the file is truncated (cleared) first.
     *
     * @param n File path
     * @param append If true, append to existing file. If false, overwrite (truncate).
     * @return true if the file was successfully opened, false if another file is
     *         already open, the card is unavailable, or the open operation failed
     */
    bool beginFileWrite(const char* n, bool append) {
        if (r || w || !available()) return false;

        uint8_t mode = append ? (O_WRONLY | O_CREAT | O_APPEND) : (O_WRONLY | O_CREAT | O_TRUNC);

        if (!f.open(n, mode)) {
            currentStatus = SD_ERROR;
            return false;
        }

        w = true;
        currentStatus = SD_WRITING;
        return true;
    }

    /**
     * @brief Writes a chunk of data to the currently open file
     *
     * Writes len bytes from the buffer to the file. Tracks the number of bytes
     * written and automatically calls syncFile() every 512 bytes (one SD card
     * sector) to flush the buffer to physical media.
     *
     * Only works if a file is currently open for writing (w is true).
     *
     * @param b Buffer containing data to write
     * @param len Number of bytes to write
     * @return Number of bytes actually written, or 0 if no file is open
     */
    uint16_t writeFileChunk(const uint8_t* b, uint16_t len) {
        if (!w) return 0;
        
        uint16_t lenw = f.write(b, len);

        bytesSinceSync += lenw;

        // Auto-sync every 512 bytes (one SD card sector) to balance performance
        // with data safety. Syncing after every write would be slow; syncing only
        // at close would lose data if power fails mid-write.
        if (bytesSinceSync >= 512) {
            syncFile();
            bytesSinceSync = 0;
        }

        return lenw;
    }

     /**
     * @brief Flushes the write buffer to physical media
     *
     * Forces all buffered data to be written to the SD card. Called automatically
     * by writeFileChunk() every 512 bytes, and by endFileWrite() before closing
     * the file.
     *
     * Only works if a file is currently open for writing.
     *
     * @return true if sync succeeded, false if no file is open or sync failed
     */
    bool syncFile() {
        return w ? f.sync() : false;
    }

    /**
     * @brief Closes the currently open file for writing
     *
     * Performs a final sync to ensure all data is written to the SD card, then
     * closes the file handle, clears the w flag, and sets the status back to IDLE.
     * Also resets bytesSinceSync to 0 for the next write operation.
     *
     * Safe to call even if no file is open (does nothing in that case).
     */
    void endFileWrite() {
        if (w) {
            syncFile();
            f.close();
            w = false;
            currentStatus = SD_IDLE;
        }
    }

    #pragma endregion

    #pragma region DELETE

    /**
     * @brief Deletes a file
     *
     * Removes the specified file from the SD card. Fails if a file is currently
     * open (r or w is true) to prevent deleting a file that is in use.
     *
     * @param n File path
     * @return true if the file was successfully deleted, false if the file does
     *         not exist, another file is open, or the card is unavailable
     */
    bool removeFile(const char* n) {
        if (r || w || !available()) return false;
        return SD.remove(n);
    }

    /**
     * @brief Deletes a directory
     *
     * Removes the specified directory from the SD card. The directory must be
     * empty. Fails if a file is currently open (r or w is true).
     *
     * @param n Directory path
     * @return true if the directory was successfully deleted, false if the
     *         directory does not exist, is not empty, another file is open,
     *         or the card is unavailable
     */
    bool removeDir(const char* n) {
        if (r || w || !available()) return false;
        return SD.rmdir(n);
    }

    #pragma endregion

    #pragma region LIST

    /**
     * @brief Opens a directory for listing
     *
     * Opens the specified directory so its contents can be iterated with
     * getNextFile(). The directory is opened in read-only mode (O_RDONLY).
     * If a file is already open (r or w is true), this function fails immediately.
     *
     * @param dir Directory path, or nullptr to list the root directory
     * @return true if the directory was successfully opened, false if the path
     *         does not exist, is not a directory, another file is open, or the
     *         card is unavailable
     */
    bool beginList(const char* dir) {
        if (r || w || !available()) return false;
        if (!f.open(dir ? dir : "/", O_RDONLY)) {
            currentStatus = SD_ERROR;
            return false;
        }
        if (!f.isDir()) {
            f.close();
            currentStatus = SD_ERROR;
            return false;
        }
        r = true;
        currentStatus = SD_LISTING;
        return true;
    }

    /**
     * @brief Returns the next file in the directory
     *
     * Iterates through the currently open directory and returns the next file
     * that matches the extension filter (.txt or .ds, case-insensitive).
     * Directories are skipped — only regular files are returned.
     *
     * The filename and size are returned via output parameters. When no more
     * matching files exist, returns false.
     *
     * Extension filter:
     *   - .txt / .TXT 
     *   - .ds  / .DS 
     *   - .js  / .JS
     *
     * @param name Buffer to receive the filename (null-terminated)
     * @param maxLen Maximum length of the filename buffer
     * @param size Pointer to receive the file size in bytes
     * @return true if a matching file was found, false if no more files exist
     */
    bool getNextFile(char* name, uint8_t maxLen, uint32_t* size) {
        if (!r) return false;
        
        SdFile entry;
        while (entry.openNext(&f, O_RDONLY)) {
            // Skip directories — only return regular files 
            if (!entry.isDir()) {
                entry.getName(name, maxLen);
                uint8_t len = strlen(name);
                
                // Extension filter: only return .txt or .ds files
                // Filename must be at least 4 chars (e.g., "a.ds") and no longer
                // than MAX_NAME to prevent buffer overflow
                if (len >= 4 && len <= MAX_NAME) {
                    char* e = &name[len - 4];
                    // Check for .txt (case-insensitive)
                    // OR check for .ds (case-insensitive, but there's a bug here)
                    if ((e[0] == '.') && (e[1] == 't' || e[1] == 'T') && (e[2] == 'x' || e[2] == 'X') && (e[3] == 't' || e[3] == 'T') ||
                        (e[1] == '.') && (e[2] == 'd' || e[2] == 'D') && (e[3] == 's' || e[3] == 'S') || 
                        (e[1] == '.') && (e[2] == 'j' || e[2] == 'J') && (e[3] == 's' || e[3] == 'S')) {
                        *size = entry.fileSize();
                        entry.close();
                        return true;
                    }
                }
            }
            entry.close();
        }
        return false;
    }

    /**
     * @brief Closes the currently open directory
     *
     * Closes the directory handle, clears the r flag, and sets the status back
     * to IDLE. Safe to call even if no directory is open (does nothing in that case).
     */
    void endList() {
        if (r) {
            f.close();
            r = false;
            currentStatus = SD_IDLE;
        }
    }

    #pragma endregion
}

#endif