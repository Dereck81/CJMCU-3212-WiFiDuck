/*
   Copyright (c) 2019 Stefan Kremser
   This software is licensed under the MIT License. See the license file for details.
   Source: github.com/spacehuhn/SimpleCLI

    Modified and adapted by:
    - Dereck81
 */

#pragma once

#include <stddef.h> // size_t

namespace duckparser {
    /**
     * @brief Parses and executes a block of text in DuckyScript format.
     * * Processes the input buffer line by line. This function is designed to
     * handle fragmented commands if the buffer ends in the middle of a line,
     * maintaining the state of strings, comments, and multiline blocks.
     * * @param str Pointer to the character buffer containing the script data.
     * @param len Length of the data in the buffer.
     */
    void parse(const char* str, size_t len);
    
    /**
     * @brief Completely resets the parser state.
     * * Clears all control flags (inString, inComment, inLoop, etc.) and
     * restores default repetition counters and delays. This should be 
     * called before starting the execution of a new script.
     */
    void reset();

    /**
     * @brief Returns the current loop iteration count.
     * * @return int Number of remaining iterations.
     */
    int getLoops();

    /**
     * @brief Returns the number of pending command repetitions.
     * * Reports how many times the last instruction will be re-executed 
     * due to a REPEAT command.
     * * @return unsigned int Number of repetitions remaining.
     */
    unsigned int getRepeats();

    /**
     * @brief Calculates the remaining sleep time for a delay operation.
     * * Useful for external monitoring (e.g., via a status UI) to determine
     * how much time remains before the device is ready for new commands.
     * * @return unsigned int Remaining time in milliseconds. 0 if no delay is active.
     */
    unsigned int getDelayTime();
};