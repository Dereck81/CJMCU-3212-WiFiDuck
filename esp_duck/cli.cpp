/*!
   \file esp_duck/cli.cpp
   \brief Command line interface source
   \author Stefan Kremser
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

const char* duckycommands_blacklist[] = {
    "DELAY", 
    "DEFAULT_DELAY", 
    "REPEAT", 
    "REPLAY",
    "REM"
};

namespace cli {
    // ===== PRIVATE ===== //
    SimpleCLI cli;           // !< Instance of SimpleCLI library

    PrintFunction printfunc; // !< Function used to print output

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
    bool isBlackListed(String key) {
        const char* str = key.c_str();
        for (const char* blocked : duckycommands_blacklist){
            size_t len = strlen(blocked);
            if (strncasecmp(str, blocked, len) == 0)
                if (str[len] == ' ' || str[len] == '\n' || str[len] == '\r' || str[len] == '\0')
                    return true;
        }
        return false;
    }

    // ===== PUBLIC ===== //
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

        /**
         * \brief Sends a raw key command to the ATmega device.
         *
         * This command receives a single argument representing a key or
         * Duckyscript instruction and forwards it directly to the ATmega
         * over the communication interface.
         *
         * The command is then transmitted exactly as provided, without
         * further parsing or interpretation.
         *
         * \note Blacklisted or unsupported commands will not be sent.
         *
         * Usage:
         *   key <command>
         */
        cli.addSingleArgCmd("key", [](cmd* c) {
            Command  cmd { c };
            Argument arg { cmd.getArg(0) };

            String keyStr = arg.getValue();

            if (isBlackListed(keyStr)) {
                print("> unsupported command");
                return;
            }
            
            // If empty, ignore
            if (keyStr.length() == 0) {
                print("> empty key command");
                return;
            }
            
            keyStr.trim();

            // Add newline if not present
            if (!keyStr.endsWith("\r\n")) {
                if (keyStr.endsWith("\n") || keyStr.endsWith("\r")) {
                    keyStr.remove(keyStr.length() - 1);
                }
                keyStr += "\r\n";
            }
            
            // Send directly to ATmega via com
            com::send(keyStr.c_str(), keyStr.length());
            
            String response = "> key: " + keyStr;
            print(response);
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
         * \brief Create version command
         *
         * Prints the current version number
         */
        cli.addCommand("version", [](cmd* c) {
            String res = "Version " + String(VERSION) + " (ATmega: " + String(com::getVersion()) + ", ESP: " + String(com::getComVersion())+ ")";
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

        /**
         * \brief Create status command
         *
         * Prints status of i2c connection to atmega32u4:
         * running <script>
         * connected
         * i2c connection problem
         */
        cli.addCommand("status", [](cmd* c) {
            String response = "pre-if version=" + String(com::getVersion()) + "\n";
            if (com::getVersion() != com::getComVersion()) {
                response += "ERROR, COM_VERSION=" + String(com::getComVersion());
            }
            if (com::connected()) {
                #ifdef USE_SD_CARD
                    uint8_t sdcard_status = com::getSdcardStatus();
                    if (sdcard_status >= sdcard::SD_READING && sdcard_status <= sdcard::SD_LISTING) {
                        String s = "[SDCARD!] ";
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

        /**
         * \brief Create ls command
         *
         * Prints a list of files inside of a given directory
         *
         * \param * Path to directory
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

            int buf_size { 256 };
            char buffer[buf_size];

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
                size_t len = 1024;

                char buffer[len];

                size_t read = spiffs::streamRead(buffer, len);

                print(buffer);
            } else {
                print("> END");
            }
        });
    }

    void parse(const char* input, PrintFunction printfunc, bool echo) {
        cli::printfunc = printfunc;

        if (spiffs::streaming() &&
            (strcmp(input, "close\n") != 0) &&
            (strcmp(input, "read\n") != 0)) {
            spiffs::streamWrite(input, strlen(input));
            print("> Written data to file");
        } else {
            if (echo) {
                String s = "# " + String(input);
                print(s);
            }

            cli.parse(input);
        }
    }
}