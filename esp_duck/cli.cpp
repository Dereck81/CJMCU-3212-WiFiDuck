/*!
   \file esp_duck/cli.cpp
   \brief Command line interface source
   \author Stefan Kremser (original)
   \author Dereck81 (modifications and adaptation)
   \copyright MIT License
 */

#include "cli.h"

// SimpleCLI library
#include <SimpleCLI.h>

// Get RAM (heap) usage
extern "C" {
#include "user_interface.h"
}

// Import modules used for different commands
#include "spiffs.h"
#include "duckscript.h"
#include "settings.h"
#include "com.h"
#include "config.h"
#include "sdcard.h"

/*! \brief Maximum size for shared buffer used in SD card operations */
#define SHARED_BUFFER_SIZE 1024

namespace cli {
    // ===== PRIVATE ===== //
    SimpleCLI cli;           // !< Instance of SimpleCLI library

    PrintFunction printfunc; // !< Function used to print output

    /*! \brief Shared buffer for SD card command assembly */
    static uint8_t shared_buffer[SHARED_BUFFER_SIZE];

    /*!
     * \brief List of blacklisted Duckyscript commands
     * 
     * These commands are not allowed to be executed directly via the CLI
     * as they require special handling.
     */
    static const char* duckycommands_blacklist[] = {
        "DELAY", 
        "DEFAULT_DELAY", 
        "REPEAT",
        "LOOP_BEGIN",
        "LOOP_END",
        "LSTRING_BEGIN",
        "LSTRING_END",
        "REM"
    };

    /*!
     * \brief Structure for raw CLI commands
     * 
     * Raw commands are processed before SimpleCLI parsing to allow
     * custom handling of payloads without argument parsing.
     */
    struct RawCmd {
        const char* name;
        void (*handler) (const char* input);
    };

    /*!
     * \brief Internal print function
     *
     * Outputs a c-string using the currently set printfunc.
     * Helps to keep code readable.
     * It's only defined in the scope of this file!
     *
     * \param s String to printed
     */
    inline void print(const String& s) {
        if (printfunc) printfunc(s.c_str());
    }

    /**
     * \brief Checks whether a key command is blacklisted.
     *
     * Compares the beginning of the given key string against a list of
     * blacklisted Duckyscript commands in a case-insensitive way.
     *
     * \param key The command string to evaluate.
     * \return true if the command is blacklisted, false otherwise.
     */
    static bool isBlackListed(String key) {
        const char* str = key.c_str();

        for (const char* blocked : duckycommands_blacklist){
            size_t len = strlen(blocked);
            if (strncasecmp(str, blocked, len) == 0)
                if (str[len] == ' ' || str[len] == '\n' || str[len] == '\r' || str[len] == '\0')
                    return true;
        }
        return false;
    }

    /*!
     * \brief Handles raw key command transmission
     * 
     * Processes and sends a key/Duckyscript command to the ATmega32u4.
     * Validates against blacklist and ensures proper line termination.
     * 
     * \param input Raw command string to send
     * \param ack   If true, wait for acknowledgment from ATmega
     */
    static void handleRawKey(const char* input, bool ack) {
        if (!input || !*input) {
            print("> empty key command");
            return;
        }

        String keyStr(input);
        keyStr.trim();

        if (isBlackListed(keyStr)) {
            print("> unsupported command");
            return;
        }

        if (!keyStr.endsWith("\r\n")) {
            keyStr += "\r\n";
        }

        com::send(keyStr.c_str(), keyStr.length(), ack);
        print("> key: " + keyStr);
    }

    /*!
     * \brief Handler for 'key' command (no acknowledgment)
     * 
     * Sends a key command without waiting for ACK from ATmega.
     * 
     * \param input Command payload
     */
    static void handleKey(const char* input) {
        if (duckscript::isRunning()) return;
        handleRawKey(input, false);
    }

    /*!
     * \brief Handler for 'key_ack' command (with acknowledgment)
     * 
     * Sends a key command and waits for ACK from ATmega.
     * Used by WebSocket interface for flow control.
     * 
     * \param input Command payload
     */
    static void handleKeyAck(const char* input) {
        if (duckscript::isRunning()){
            print("KEY_ACK:ERROR");
            return;
        }
        handleRawKey(input, true);
    }

    #ifdef USE_SD_CARD

    /*!
     * \brief Handler for streaming data writes to SD card
     * 
     * Sends data chunks to the ATmega for writing to SD card.
     * Must be preceded by 'sd_stream_write_begin' command.
     * 
     * \param input Data to write (max SHARED_BUFFER_SIZE - 1 bytes)
     * 
     * \note Only works when SD write mode is active
     * \note Data is sent with SD_CMD_WRITE header byte
     */
    static void handleSDStreamWrite(const char* input) {
        if (com::get_mode() != sdcard::SD_WRITING) {
            print("SYS_ERROR: The write flow to SDCARD was not initiated.");
            return;
        }
        
        size_t len = strlen(input);
        if (len == 0) {
            print("SD_ERROR: There is no information to send");
            return;
        }
        
        uint8_t* buffer = (uint8_t*)shared_buffer;
        size_t maxDataSize = min((size_t)SHARED_BUFFER_SIZE - 1, (size_t)BUFFER_SIZE - 1);
        
        buffer[0] = SD_CMD_WRITE;

        size_t dataLen = min(len, maxDataSize);
        memcpy(&buffer[1], input, dataLen);

        print("> Sending data...");

        com::send_sd(buffer, dataLen + 1);
        
        return;
    }

    /*!
     * \brief Prepares SD card command buffer
     * 
     * Assembles a buffer with command byte + filename for SD operations.
     * 
     * \param cmd_byte Command byte (SD_CMD_READ, SD_CMD_RM, etc.)
     * \param filename Filename to include in command (max MAX_NAME chars)
     * 
     * \return Total buffer size (cmd_byte + filename + null terminator)
     * 
     * \note Buffer format: [cmd_byte][filename bytes][0x00]
     */
    static size_t prepareSDBuffer(uint8_t cmd_byte, const String& filename) {
        uint8_t* buffer = (uint8_t*)shared_buffer;
        buffer[0] = cmd_byte;

        size_t len = min((size_t)filename.length(), (size_t)MAX_NAME);
        memcpy(&buffer[1], filename.c_str(), len);

        buffer[len + 1] = '\0';
        
        return len + 2;
    }
    #else
    static void handleSDStreamWrite(const char* input) { }

    #endif

    /*!
     * \brief Table of raw commands processed before SimpleCLI
     * 
     * These commands bypass normal argument parsing to allow
     * custom payload handling (e.g., for WebSocket ACK flow control).
     */
    static const RawCmd rawCommands[] = {
        { "key_ack", handleKeyAck },
        { "key", handleKey },
        { "sd_stream_write", handleSDStreamWrite }
    };

    /*!
     * \brief Attempts to execute a raw command
     * 
     * Checks if input matches any raw command prefix and executes
     * its handler if found.
     * 
     * \param input Full command line input
     * \return true if a raw command was executed, false otherwise
     */
    static bool tryRawCommand(const char* input) {
        for (const auto& cmd : rawCommands) {
            size_t len = strlen(cmd.name);

            if (strncmp(input, cmd.name, len) == 0 &&
            (input[len] == ' ' || input[len] == '\n' || input[len] == '\0')) {

                const char* payload = input + len;
                if (*payload == ' ') payload++;

                cmd.handler(payload);
                return true;
            }
        }
        return false;
    }

    // ===== PUBLIC ===== //

    /*!
     * \brief Initialize CLI and register all commands
     * 
     * Sets up error handling and registers all available CLI commands
     * including file operations, script execution, settings management,
     * and SD card operations (if enabled).
     * 
     * \note Must be called once during system initialization
     */
    void begin() {
        /**
         * \brief Set error callback.
         *
         * Prints 'ERROR: <error-message>'
         * And 'Did you mean "<command-help>"?'
         * if the command name matched, but the arguments didn't
         */
        cli.setOnError([](cmd_error* e) {
            CommandError cmdError(e); // Create wrapper object

            String res = "ERROR: " + cmdError.toString();

            if (cmdError.hasCommand()) {
                res += "\nDid you mean \"";
                res += cmdError.getCommand().toString();
                res += "\"?";
            }

            print(res);
        });

        /**
         * \brief Create help Command
         *
         * Prints all available commands with their arguments
         */
        cli.addCommand("help", [](cmd* c) {
            print(cli.toString());
        });

        /**
         * \brief Prints flash memory size information.
         *
         * Displays both the real flash chip size detected at runtime
         * and the flash size configured in the firmware.
         *
         * Output:
         *  - FlashChipRealSize
         *  - FlashChipSize
         */
        cli.addCommand("flash_size", [](cmd* c) {
            print("FlashChipRealSize: " + String(ESP.getFlashChipRealSize()) + "\n" + 
            "FlashChipSize: " + String(ESP.getFlashChipSize()));
        });

        /*!
         * \brief Sends a raw key command to the ATmega device with ACK.
         *
         * This command receives a single argument representing a key or
         * Duckyscript instruction and forwards it directly to the ATmega
         * over the communication interface, waiting for acknowledgment.
         *
         * The command is transmitted exactly as provided, without
         * further parsing or interpretation.
         *
         * Usage: key_ack <command>
         * Example: key_ack STRING Hello World
         * 
         * \note Blacklisted or unsupported commands will not be sent.
         * \note This is primarily used by the WebSocket interface for flow control
         */
        cli.addSingleArgCmd("key_ack", [](cmd* c) {
            Command  cmd { c };
            Argument arg { cmd.getArg(0) };
        });

        /*!
         * \brief Sends a raw key command to the ATmega device without ACK.
         *
         * Similar to key_ack but does not wait for acknowledgment.
         * Faster but no guarantee of delivery.
         *
         * Usage: key <command>
         * Example: key ENTER
         * 
         * \note Blacklisted or unsupported commands will not be sent.
         */
        cli.addSingleArgCmd("key", [](cmd* c) {
            Command  cmd { c };
            Argument arg { cmd.getArg(0) };
        });

        /**
         * \brief Create ram command
         *
         * Prints number of free bytes in the RAM
         */
        cli.addCommand("ram", [](cmd* c) {
            size_t freeRam = system_get_free_heap_size();
            String res     = String(freeRam) + " bytes available";
            print(res);
        });

        /**
         * \brief Create freq command
         *
         * Print the frequency at which the ESP is running
         */
        cli.addCommand("freq", [](cmd* c) {
            String res = String(ESP.getCpuFreqMHz()) + " MHz";
            print(res);
        });

        /**
         * \brief Create version command
         *
         * Prints the current version number
         */
        cli.addCommand("version", [](cmd* c) {
            String res = "Version " + String(VERSION) + " (ATmega: " + String(com::get_version()) + ", ESP: " + String(com::get_com_version())+ ")";
            print(res);
        });

        /**
         * \brief Create settings command
         *
         * Prints all settings with their values
         */
        cli.addCommand("settings", [](cmd* c) {
            settings::load();
            print(settings::toString());
        });

        /**
         * \brief Create set command
         *
         * Updates the value of a setting
         *
         * \param name name of the setting
         * \param vale new value for the setting
         */
        Command cmdSet {
            cli.addCommand("set", [](cmd* c) {
                Command  cmd { c };

                Argument argName { cmd.getArg(0) };
                Argument argValue { cmd.getArg(1) };

                String name { argName.getValue() };
                String value { argValue.getValue() };

                settings::set(name.c_str(), value.c_str());

                String response = "> set \"" + name + "\" to \"" + value + "\"";

                print(response);
            })
        };
        cmdSet.addPosArg("n/ame");
        cmdSet.addPosArg("v/alue");

        /**
         * \brief Create reset command
         *
         * Resets all settings and prints out the defaul values
         */
        cli.addCommand("reset", [](cmd* c) {
            settings::reset();
            print(settings::toString());
        });

        /*!
         * \brief Create status command
         *
         * Prints the current system status including:
         * - I2C connection to ATmega32u4
         * - Script execution state
         * - SD card operation state (if enabled)
         */
        cli.addCommand("status", [](cmd* c) {
            String response = "pre-if version=" + String(com::get_version()) + "\n";
            if (com::get_version() != com::get_com_version()) {
                response += "ERROR, COM_VERSION=" + String(com::get_com_version());
            }
            if (com::connected()) {
                #ifdef USE_SD_CARD
                uint8_t sdcard_status = com::get_sdcard_status();
                if (sdcard_status >= sdcard::SD_READING && sdcard_status <= sdcard::SD_LISTING) {
                    String s = "SD_STATUS: ";
                    if (sdcard_status == sdcard::SD_READING) s += "reading...";
                    else if (sdcard_status == sdcard::SD_WRITING) s += "writting...";
                    else if (sdcard_status == sdcard::SD_EXECUTING) s += "running...";
                    else if (sdcard_status == sdcard::SD_LISTING) s += "enumerating...";
                    print(s);
                    return;
                }
                #endif
                if (duckscript::isRunning()) {
                    String s = "running " + duckscript::currentScript();
                    print(s);
                } else {
                    print("connected");
                }
            } else {
                print("Internal connection problem\n" + response);
            }
        });

        /*!
         * \brief Create ls command
         *
         * Lists files and directories in SPIFFS filesystem
         *
         * Usage: ls <path>
         * Example: ls /scripts
         * 
         * \param * Path to directory (use "/" for root)
         */
        cli.addSingleArgCmd("ls", [](cmd* c) {
            Command  cmd { c };
            Argument arg { cmd.getArg(0) };

            String res = spiffs::listDir(arg.getValue());
            print(res);
        });

        /**
         * \brief Create mem command
         *
         * Prints memory usage of SPIFFS
         */
        cli.addCommand("mem", [](cmd* c) {
            String s = "";
            s.reserve(64);

            s += String(spiffs::size());
            s += " byte\n";
            s += String(spiffs::usedBytes());
            s += " byte used\n";
            s += String(spiffs::freeBytes());
            s += " byte free";

            print(s);
        });

        /**
         * \brief Create cat command
         *
         * Prints out a file from the SPIFFS
         *
         * \param * Path to file
         */
        cli.addSingleArgCmd("cat", [](cmd* c) {
            Command  cmd { c };
            Argument arg { cmd.getArg(0) };

            File f = spiffs::open(arg.getValue());

            char* buffer = (char*) shared_buffer;
            size_t buf_size = SHARED_BUFFER_SIZE;

            while (f && f.available()) {
                for (size_t i = 0; i<buf_size; ++i) {
                    if (!f.available() || (i == buf_size-1)) {
                        buffer[i] = '\0';
                        i         = buf_size;
                    } else {
                        buffer[i] = f.read();
                    }
                }
                print(buffer);
            }
        });

        /**
         * \brief Create run command
         *
         * Starts executing a ducky script
         *
         * \param * Path to script in SPIFFS
         */
        cli.addSingleArgCmd("run", [](cmd* c) {
            Command  cmd { c };
            Argument arg { cmd.getArg(0) };

            duckscript::run(arg.getValue());

            String response = "> started \"" + arg.getValue() + "\"";
            print(response);
        });

        /**
         * \brief Create stop command
         *
         * Stops executing a script
         *
         * \param * Path to specific ducky script to stop
         *          If no path is given, stop whatever script is active
         */
        cli.addSingleArgCmd("stop", [](cmd* c) {
            Command  cmd { c };
            Argument arg { cmd.getArg(0) };

            duckscript::stop(arg.getValue());

            String response = "> stopped " + arg.getValue();
            print(response);
        });

        /**
         * \brief Create create command
         *
         * Creates a file in the SPIFFS
         *
         * \param * Path with filename
         */
        cli.addSingleArgCmd("create", [](cmd* c) {
            Command  cmd { c };
            Argument arg { cmd.getArg(0) };

            spiffs::create(arg.getValue());

            String response = "> created file \"" + arg.getValue() + "\"";
            print(response);
        });

        /**
         * \brief Create remove command
         *
         * Removes file in SPIFFS
         *
         * \param * Path to file
         */
        cli.addSingleArgCmd("remove", [](cmd* c) {
            Command  cmd { c };
            Argument arg { cmd.getArg(0) };

            spiffs::remove(arg.getValue());

            String response = "> removed file \"" + arg.getValue() + "\"";
            print(response);
        });

        /**
         * \brief Create rename command
         *
         * Renames a file in SPIFFS
         *
         * \param fileA Old path with filename
         * \param fileB New path with filename
         */
        Command cmdRename {
            cli.addCommand("rename", [](cmd* c) {
                Command  cmd { c };

                Argument argA { cmd.getArg(0) };
                Argument argB { cmd.getArg(1) };

                String fileA { argA.getValue() };
                String fileB { argB.getValue() };

                spiffs::rename(fileA, fileB);

                String response = "> renamed \"" + fileA + "\" to \"" + fileB + "\"";
                print(response);
            })
        };
        cmdRename.addPosArg("fileA,a");
        cmdRename.addPosArg("fileB,b");

        /**
         * \brief Create write command
         *
         * Appends string to a file in SPIFFS
         *
         * \param file    Path to file
         * \param content String to write
         */
        Command cmdWrite {
            cli.addCommand("write", [](cmd* c) {
                Command  cmd { c };

                Argument argFileName { cmd.getArg(0) };
                Argument argContent { cmd.getArg(1) };

                String fileName { argFileName.getValue() };
                String content { argContent.getValue() };

                spiffs::write(fileName, (uint8_t*)content.c_str(), content.length());

                String response = "> wrote to file \"" + fileName + "\"";
                print(response);
            })
        };
        cmdWrite.addPosArg("f/ile");
        cmdWrite.addPosArg("c/ontent");

        /**
         * \brief Create format command
         *
         * Formats SPIFFS
         */
        cli.addCommand("format", [](cmd* c) {
            spiffs::format();
            print("Formatted SPIFFS");
        });

        /**
         * \brief Create stream command
         *
         * Opens stream to a file in SPIFFS.
         * Whatever is parsed to the CLI is written into the strem.
         * Only close and read are commands will be executed.
         *
         * \param * Path to file
         */
        cli.addSingleArgCmd("stream", [](cmd* c) {
            Command  cmd { c };
            Argument arg { cmd.getArg(0) };

            spiffs::streamOpen(arg.getValue());

            String response = "> opened stream \"" + arg.getValue() + "\"";
            print(response);
        });

        /**
         * \brief Create close command
         *
         * Closes file stream
         */
        cli.addCommand("close", [](cmd* c) {
            spiffs::streamClose();
            print("> closed stream");
        });

        /**
         * \brief Create read command
         *
         * Reads from file stream (1024 characters)
         */
        cli.addCommand("read", [](cmd* c) {
            if (spiffs::streamAvailable()) {
                char* buffer = (char*) shared_buffer;

                size_t len = SHARED_BUFFER_SIZE - 1;

                size_t read = spiffs::streamRead(buffer, len);

                buffer[read] = '\0';

                print(buffer);
            } else {
                print("> END");
            }
        });

        /**
         * \brief Create duckparser_reset command
         *
         * Restart the duckparser
         */
        cli.addCommand("duckparser_reset", [](cmd* c) {
            com::send(CMD_PARSER_RESET);

            print("Duckparser reset");
        });

        #ifdef USE_SD_CARD

        /*!
         * \brief Create sd_ls command
         * 
         * Lists files on the SD card connected to the ATmega32u4.
         * Only shows .ds and .txt files with names ≤ 32 characters.
         * Currently only reads root directory (/).
         * 
         * \note Requires SD card to be inserted and initialized
         * \note Only available if USE_SD_CARD is defined
         */
        cli.addCommand("sd_ls", [](cmd* c) {            
            com::set_mode(sdcard::SD_LISTING);

            uint8_t* buffer = (uint8_t*) shared_buffer;
            buffer[0] = SD_CMD_LS;
            buffer[1] = '/';
            buffer[2] = '\0';
            
            com::send_sd(buffer, 3);

            print("> Requesting list for: /\n");
        });

        /*!
         * \brief Create sd_cat command
         * 
         * Reads and displays the contents of a file from the SD card.
         * 
         * \param filename Name of file on SD card (max MAX_NAME characters)
         * 
         * \note File must exist on SD card
         * \note Content is streamed back over I2C
         */
        cli.addSingleArgCmd("sd_cat", [](cmd* c) {
            Command cmd{c};
            Argument arg{cmd.getArg(0)};
            
            String file = arg.getValue();

            if (file.length() == 0) {
                print("SD_ERROR: No name was specified");
                return;
            }

            com::set_mode(sdcard::SD_READING);
            
            size_t size = prepareSDBuffer(SD_CMD_READ, file);
            
            com::send_sd((uint8_t*)shared_buffer, size);
            
            print("> Reading file " + file);
        });

        /*!
         * \brief Create sd_rm command
         * 
         * Deletes a file from the SD card.
         * 
         * \param filename Name of file to delete (max MAX_NAME characters)
         * 
         * \warning This operation cannot be undone!
         */
        cli.addSingleArgCmd("sd_rm", [](cmd* c) {
            Command cmd{c};
            Argument arg{cmd.getArg(0)};
            
            String file = arg.getValue();

            if (file.length() == 0) {
                print("SD_ERROR: No name was specified");
                return;
            }

            size_t size = prepareSDBuffer(SD_CMD_RM, file);

            com::send_sd((uint8_t*)shared_buffer, size);
            
            print("> Removing file " + file);
        });

        /*!
         * \brief Create sd_run command
         * 
         * Executes a Duckyscript from the SD card.
         * 
         * \param filename Name of script file on SD card
         * 
         * \note Script must be valid Duckyscript format
         * \note Execution happens on the ATmega32u4
         */
        cli.addSingleArgCmd("sd_run", [](cmd* c) {
            Command cmd{c};
            Argument arg{cmd.getArg(0)};
            
            String file = arg.getValue();

            if (file.length() == 0) {
                print("SD_ERROR: No name was specified");
                return;
            }

            com::set_mode(sdcard::SD_EXECUTING);
            
            size_t size = prepareSDBuffer(SD_CMD_RUN, file);

            com::send_sd((uint8_t*)shared_buffer, size);
            
            print("> Run script " + file);
        });

        /*!
         * \brief Create sd_stop_run command
         * 
         * Stops the currently executing script on the SD card.
         * 
         * \note Only affects scripts running from SD card (not SPIFFS)
         */
        cli.addCommand("sd_stop_run", [](cmd* c) {
            uint8_t stop_cmd = SD_CMD_STOP_RUN;

            com::send_sd(&stop_cmd, 1);
            
            print("Stopping script execution on SD card...");
        });

        /*!
         * \brief sd_stream_write command (internal handler)
         * 
         * Sends data chunks to be written to an SD card file.
         * This command is only functional after sd_stream_write_begin.
         * 
         * \note Maximum 126 bytes per transmission
         * \note Must call sd_stream_write_begin first
         * \note Processed as raw command (bypasses SimpleCLI)
         */
        cli.addCommand("sd_stream_write", [](cmd* c) {
            Command  cmd { c };
            Argument arg { cmd.getArg(0) };
            // Nothing
        }).setDescription("Sends data to be written to the SD card; this is only available if you start with sd_stream_write_begin");

         /*!
         * \brief Create sd_stream_write_begin command
         * 
         * Initiates a streaming write session to an SD card file.
         * After calling this, use sd_stream_write to send data chunks.
         * 
         * \param filename Name of file to create/write on SD card
         * 
         * \note File will be created if it doesn't exist
         * \note Maximum transmission size: 126 bytes per chunk
         * \note Remember to call sd_stop when finished
         */
        cli.addSingleArgCmd("sd_stream_write_begin", [](cmd* c) {
            Command  cmd { c };
            Argument arg { cmd.getArg(0) };

            String file = arg.getValue();

            if (file.length() == 0) {
                print("SD_ERROR: No name was specified");
                return;
            }

            com::set_mode(sdcard::SD_WRITING);
        
            uint8_t* buffer = (uint8_t*)shared_buffer;
            buffer[0] = SD_CMD_WRITE;
            buffer[1] = 0;
            
            size_t len = min((size_t)file.length(), (size_t)MAX_NAME);

            memcpy(&buffer[2], file.c_str(), len);

            buffer[len + 2] = '\0';
            
            com::send_sd(buffer, len + 3);
            print("> Starting the write flow to the SD card. Maximum transmission of 126 bytes");
        });

        /*!
         * \brief Create sd_stop command
         * 
         * Stops any ongoing SD card operation and returns to idle state.
         * Use this to finalize write streams or abort long operations.
         * 
         * \note Safe to call even if no operation is active
         */
        cli.addCommand("sd_stop", [](cmd* c) {
            uint8_t stop_cmd = SD_CMD_STOP;

            com::send_sd(&stop_cmd, 1);
            
            print("Stopping sdcard...");
        });

        /*!
         * \brief Create sd_status command
         *
         * Prints the current SD card operation status code.
         * 
         */
        cli.addCommand("sd_status", [](cmd* c) {
            String res = String(com::get_sdcard_status());
            print(res);
        });

        #endif
    }

    /*!
     * \brief Parse and execute a CLI command
     * 
     * Main entry point for processing CLI input. Handles:
     * - Raw commands (key, key_ack, sd_stream_write)
     * - SD card session management
     * - File streaming mode
     * - Standard SimpleCLI commands
     * 
     * \param input      Command string to parse
     * \param printfunc  Function pointer for output (e.g., Serial.print)
     * \param echo       If true, echo the input command before executing
     * 
     * \note File streaming mode intercepts all input except 'close' and 'read'
     * \note SD operations are blocked if a transfer is in progress
     */
    void parse(const char* input, PrintFunction printfunc, bool echo) {
        cli::printfunc = printfunc;

        #ifdef USE_SD_CARD

        if (strncmp(input, "sd_", 3) == 0 && duckscript::isRunning()) {
            print("SYS_BUSY: A script is being executed from SPIFFS");
            return;
        }

        // Prevent SD command conflicts during active transfers
        if (com::is_session_active() && strncmp(input, "sd_", 3) == 0 && strncmp(input, "sd_stop", 7) != 0) {
            if (tryRawCommand(input)) return;

            if (com::get_mode() >= sdcard::SD_READING) {
                print("SYS_BUSY: SD Transfer in progress. Wait for SD_END");
                return;
            }
        }

        #else
        // Reject SD commands if SD support is not compiled
        if (strncmp(input, "sd_", 3) == 0) {
            print("SD_END:ERROR\n");
            print("SYS_ERROR: Unsupported command.");
            return;
        }
        #endif

        // Try raw commands first (bypasses argument parsing)
        if (tryRawCommand(input)) return;

         // File streaming mode: write to stream instead of parsing
        if (spiffs::streaming() &&
            (strcmp(input, "close\n") != 0) &&
            (strcmp(input, "read\n") != 0)) {
            spiffs::streamWrite(input, strlen(input));
            print("> Written data to file");
        } else {
            // Normal command execution
            if (echo) {
                String s = "# " + String(input);
                print(s);
            }

            cli.parse(input);
        }
    }
}