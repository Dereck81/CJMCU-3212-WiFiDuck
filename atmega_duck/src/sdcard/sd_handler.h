
#include "../../include/config.h"
#include <Arduino.h>

#ifdef USE_SD_CARD
namespace sd_handler {
    /**
     * @brief Processes an SD card command received from the receiving device
     *
     * This is the main entry point for all SD card operations initiated remotely.
     * The buffer should contain a command byte followed by optional arguments.
     * See sd_handler.cpp for the full list of supported commands (SD_CMD_LS,
     * SD_CMD_READ, SD_CMD_WRITE, etc.).
     *
     * This function should be called from the main loop when com::isSdPacket()
     * returns true, indicating that the received data is an SD card command
     * rather than a regular script command.
     *
     * @param buffer Pointer to the received packet data (command + arguments)
     * @param len    Number of valid bytes in the buffer
     */
    void process(uint8_t* buffer, size_t len);

    /**
     * @brief Executes one step of the currently running script
     *
     * This function should be called from the main loop every tick. If a script
     * is currently running (status == SD_EXECUTING), it executes the next line
     * via script_runner::execute_next_line(). When the script finishes, this
     * function releases all held keys and mouse buttons, stops the script runner,
     * and notifies com that the operation is complete.
     *
     * Safe to call even when no script is running — it returns immediately if
     * the SD card status is not SD_EXECUTING.
     *
     * @return true if the script is still running or no script is active,
     *         false if the script just finished on this call
     */
    bool run_script_step();

    /**
     * @brief Starts the autorun script if defined at compile time
     *
     * Checks if AUTORUN_SCRIPT is defined in config.h. If it is, starts executing
     * that script immediately. This is typically called once during setup() to
     * allow the device to run a default script on boot.
     *
     * If AUTORUN_SCRIPT is not defined, this function does nothing.
     *
     * Example config.h entry:
     *   #define AUTORUN_SCRIPT "AUTORUN.DS"
     */
    void autorun();
}
#endif
