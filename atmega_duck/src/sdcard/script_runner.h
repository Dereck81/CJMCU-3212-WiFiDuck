#pragma once

#include <stdint.h>
#include "../../include/config.h"

#ifdef USE_SD_CARD

namespace script_runner {

    /**
     * @brief Starts script execution from an SD card file
     *
     * Opens the specified script file, initializes internal execution state,
     * resets the parser, and prepares the runner for execution.
     *
     * Must be called once before execute_next_line().
     *
     * @param f    Path to the script file on the SD card
     * @param buff Pointer to a buffer of size BUFFER_SIZE used for reading lines
     * @return true if the file was opened successfully, false otherwise
     */
    bool start(const char* f, uint8_t* buff);
    
    /**
     * @brief Stops script execution and releases resources
     *
     * Terminates execution, closes the file, resets internal state,
     * and updates hardware indicators (e.g. LEDs).
     */
    void stop();

    /**
     * @brief Executes the next logical step of the script
     *
     * This function advances script execution by:
     *  - Reading the next line (or line fragment) from the SD card
     *  - Parsing and executing it
     *  - Handling REPEAT, LOOP, and LSTRING control flow
     *
     * It must be called repeatedly (e.g. in loop()) until it returns false.
     *
     * @return true if execution should continue, false if the script has finished
     */
    bool execute_next_line();
}

#endif