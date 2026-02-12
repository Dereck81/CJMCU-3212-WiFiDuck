#pragma once

#include <Arduino.h>
#include "config.h"

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
#define SD_ACK 0x06 

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

}

#endif