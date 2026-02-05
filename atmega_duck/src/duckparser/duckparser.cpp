/*
   Copyright (c) 2019 Stefan Kremser
   This software is licensed under the MIT License. See the license file for details.
   Source: github.com/spacehuhn/SimpleCLI

   Modified and adapted by:
    - Dereck81
 */

#include "duckparser.h"

#include "../../include/config.h"
// #include "../include/debug.h"

#include "../hid/keyboard.h"
#include "../led/led.h"

#include <Mouse.h>

extern "C" {
 #include "parser.h" // parse_lines
}

#define CASE_INSENSETIVE 0
#define CASE_SENSETIVE 1

namespace duckparser {
    // ====== PRIVATE ===== //

    // These persist across parse() calls. When a line is split
    // across buffer boundaries, the relevant flag stays set so
    // the next call knows to continue in the same mode.
    
    bool isStringln = false; // True if the current STRING is actually a STRINGLN (ENTER at the end)
    bool inString   = false; // True while we are in the middle of a STRING that spans multiple buffers
    bool inLString  = false; // True while we are inside an LSTRING_BEGIN...LSTRING_END block
    bool inComment  = false; // True while we are in the middle of a REM comment that spans multiple buffers
    bool inLoop     = false; // True while we are inside a LOOP_BEGIN...LOOP_END block


    /** Milliseconds to sleep after each command. Changed by DEFAULT_DELAY */
    unsigned int defaultDelay = 5;
    
    /**
     * @brief Pending repetition count, exposed via getRepeats()
     *
     * When REPEAT N is parsed this is set to N + 1. The +1 exists because the
     * post-command block at the bottom of the parse loop decrements it once
     * immediately (on the REPEAT line itself), bringing it down to N. From
     * that point, script_runner seeks back to the previous command and
     * re-parses it, decrementing once more each time. The net result is
     * exactly N re-executions of the previous command.
     */
    unsigned int repeatNum    = 0;

    /**
     * @brief Current loop iteration count, exposed via getLoops()
     *
     * Positive values are the number of iterations remaining. Zero means the
     * loop is finished. A value of -1 means the loop is infinite (any negative
     * number in the script is normalized to -1).
     */
    int loopNum = 0;

    unsigned long interpretTime  = 0; // millis() captured at the start of each parse() call
    unsigned long sleepStartTime = 0; // millis() at the moment the current sleep actually began
    unsigned long sleepTime      = 0; // How long (ms) the current sleep is supposed to last

    /**
     * @brief Types raw text directly via the keyboard
     *
     * Used for STRING and LSTRING content — characters are sent as-is,
     * not interpreted as command names.
     *
     * @param str Pointer to the text
     * @param len Number of bytes to type
     */
    void type(const char* str, size_t len) {
        keyboard::write(str, len);
    }

    /**
     * @brief Resolves a single token and presses the corresponding key
     *
     * Resolution order:
     *   1. Single character  → pressed as a raw character
     *   2. Named key         → pressed via its HID scancode (ENTER, TAB, F1, etc.)
     *   3. Modifier key      → pressed as a modifier (CTRL, SHIFT, ALT, GUI)
     *   4. Anything else     → passed to keyboard::press() which treats it as a
     *                           UTF-8 character
     *
     * @param str Token string
     * @param len Token length
     */
    void press(const char* str, size_t len) {
        // character
        if (len == 1) keyboard::press(str);

        // Keys
        else if (compare(str, len, "ENTER", CASE_SENSETIVE)) keyboard::pressKey(KEY_ENTER);
        else if (compare(str, len, "MENU", CASE_SENSETIVE)) keyboard::pressKey(KEY_PROPS);
        else if (compare(str, len, "DELETE", CASE_SENSETIVE)) keyboard::pressKey(KEY_DELETE);
        else if (compare(str, len, "BACKSPACE", CASE_SENSETIVE)) keyboard::pressKey(KEY_BACKSPACE);
        else if (compare(str, len, "HOME", CASE_SENSETIVE)) keyboard::pressKey(KEY_HOME);
        else if (compare(str, len, "INSERT", CASE_SENSETIVE)) keyboard::pressKey(KEY_INSERT);
        else if (compare(str, len, "PAGEUP", CASE_SENSETIVE)) keyboard::pressKey(KEY_PAGEUP);
        else if (compare(str, len, "PAGEDOWN", CASE_SENSETIVE)) keyboard::pressKey(KEY_PAGEDOWN);
        else if (compare(str, len, "UP", CASE_SENSETIVE)) keyboard::pressKey(KEY_UP);
        else if (compare(str, len, "DOWN", CASE_SENSETIVE)) keyboard::pressKey(KEY_DOWN);
        else if (compare(str, len, "LEFT", CASE_SENSETIVE)) keyboard::pressKey(KEY_LEFT);
        else if (compare(str, len, "RIGHT", CASE_SENSETIVE)) keyboard::pressKey(KEY_RIGHT);
        else if (compare(str, len, "TAB", CASE_SENSETIVE)) keyboard::pressKey(KEY_TAB);
        else if (compare(str, len, "END", CASE_SENSETIVE)) keyboard::pressKey(KEY_END);
        else if (compare(str, len, "ESC", CASE_SENSETIVE)) keyboard::pressKey(KEY_ESC);
        else if (compare(str, len, "F1", CASE_SENSETIVE)) keyboard::pressKey(KEY_F1);
        else if (compare(str, len, "F2", CASE_SENSETIVE)) keyboard::pressKey(KEY_F2);
        else if (compare(str, len, "F3", CASE_SENSETIVE)) keyboard::pressKey(KEY_F3);
        else if (compare(str, len, "F4", CASE_SENSETIVE)) keyboard::pressKey(KEY_F4);
        else if (compare(str, len, "F5", CASE_SENSETIVE)) keyboard::pressKey(KEY_F5);
        else if (compare(str, len, "F6", CASE_SENSETIVE)) keyboard::pressKey(KEY_F6);
        else if (compare(str, len, "F7", CASE_SENSETIVE)) keyboard::pressKey(KEY_F7);
        else if (compare(str, len, "F8", CASE_SENSETIVE)) keyboard::pressKey(KEY_F8);
        else if (compare(str, len, "F9", CASE_SENSETIVE)) keyboard::pressKey(KEY_F9);
        else if (compare(str, len, "F10", CASE_SENSETIVE)) keyboard::pressKey(KEY_F10);
        else if (compare(str, len, "F11", CASE_SENSETIVE)) keyboard::pressKey(KEY_F11);
        else if (compare(str, len, "F12", CASE_SENSETIVE)) keyboard::pressKey(KEY_F12);
        else if (compare(str, len, "SPACE", CASE_SENSETIVE)) keyboard::pressKey(KEY_SPACE);
        else if (compare(str, len, "PAUSE", CASE_SENSETIVE) || compare(str, len, "BREAK", CASE_SENSETIVE)) keyboard::pressKey(KEY_PAUSE);
        else if (compare(str, len, "CAPSLOCK", CASE_SENSETIVE)) keyboard::pressKey(KEY_CAPSLOCK);
        else if (compare(str, len, "NUMLOCK", CASE_SENSETIVE)) keyboard::pressKey(KEY_NUMLOCK);
        else if (compare(str, len, "PRINTSCREEN", CASE_SENSETIVE)) keyboard::pressKey(KEY_SYSRQ);
        else if (compare(str, len, "SCROLLLOCK", CASE_SENSETIVE)) keyboard::pressKey(KEY_SCROLLLOCK);

        // NUMPAD KEYS
        else if (compare(str, len, "NUM_0", CASE_SENSETIVE)) keyboard::pressKey(KEY_KP0);
        else if (compare(str, len, "NUM_1", CASE_SENSETIVE)) keyboard::pressKey(KEY_KP1);
        else if (compare(str, len, "NUM_2", CASE_SENSETIVE)) keyboard::pressKey(KEY_KP2);
        else if (compare(str, len, "NUM_3", CASE_SENSETIVE)) keyboard::pressKey(KEY_KP3);
        else if (compare(str, len, "NUM_4", CASE_SENSETIVE)) keyboard::pressKey(KEY_KP4);
        else if (compare(str, len, "NUM_5", CASE_SENSETIVE)) keyboard::pressKey(KEY_KP5);
        else if (compare(str, len, "NUM_6", CASE_SENSETIVE)) keyboard::pressKey(KEY_KP6);
        else if (compare(str, len, "NUM_7", CASE_SENSETIVE)) keyboard::pressKey(KEY_KP7);
        else if (compare(str, len, "NUM_8", CASE_SENSETIVE)) keyboard::pressKey(KEY_KP8);
        else if (compare(str, len, "NUM_9", CASE_SENSETIVE)) keyboard::pressKey(KEY_KP9);
        else if (compare(str, len, "NUM_ASTERIX", CASE_SENSETIVE)) keyboard::pressKey(KEY_KPASTERISK);
        else if (compare(str, len, "NUM_ENTER", CASE_SENSETIVE)) keyboard::pressKey(KEY_KPENTER);
        else if (compare(str, len, "NUM_MINUS", CASE_SENSETIVE)) keyboard::pressKey(KEY_KPMINUS);
        else if (compare(str, len, "NUM_DOT", CASE_SENSETIVE)) keyboard::pressKey(KEY_KPDOT);
        else if (compare(str, len, "NUM_PLUS", CASE_SENSETIVE)) keyboard::pressKey(KEY_KPPLUS);

        // Modifiers
        else if (compare(str, len, "CTRL", CASE_SENSETIVE) || compare(str, len, "CONTROL", CASE_SENSETIVE)) keyboard::pressModifier(KEY_MOD_LCTRL);
        else if (compare(str, len, "SHIFT", CASE_SENSETIVE)) keyboard::pressModifier(KEY_MOD_LSHIFT);
        else if (compare(str, len, "ALT", CASE_SENSETIVE)) keyboard::pressModifier(KEY_MOD_LALT);
        else if (compare(str, len, "WINDOWS", CASE_SENSETIVE) || compare(str, len, "GUI", CASE_SENSETIVE)) keyboard::pressModifier(KEY_MOD_LMETA);

        // Utf8 character
        else keyboard::press(str);
    }

    /**
     * @brief Releases all currently held keys
     *
     * Called at the end of every line that contains key presses, and after
     * sending a raw KEYCODE report.
     */
    void release() {
        keyboard::release();
    }

    /**
     * @brief Parses an unsigned integer from a string
     *
     * @param str Input string
     * @param len Length of the string
     * @return Parsed unsigned integer value, or 0 if str is null or empty
     */
    unsigned int toInt(const char* str, size_t len) {
        if (!str || (len == 0)) return 0;

        unsigned int val = 0;

        // HEX
        if ((len > 2) && (str[0] == '0') && (str[1] == 'x')) {
            for (size_t i = 2; i < len; ++i) {
                uint8_t b = str[i];

                if ((b >= '0') && (b <= '9')) b = b - '0';
                else if ((b >= 'a') && (b <= 'f')) b = b - 'a' + 10;
                else if ((b >= 'A') && (b <= 'F')) b = b - 'A' + 10;

                val = (val << 4) | (b & 0xF);
            }
        }
        // DECIMAL
        else {
            for (size_t i = 0; i < len; ++i) {
                if ((str[i] >= '0') && (str[i] <= '9')) {
                    val = val * 10 + (str[i] - '0');
                }
            }
        }

        return val;
    }

    /**
     * @brief Parses a signed integer from a string
     *
     * Handles an optional leading '-' character. Everything after the sign
     * is parsed by toInt(), so hex (0x) is supported here as well.
     *
     * @param str Input string
     * @param len Length of the string
     * @return Parsed signed integer value, or 0 if str is null or empty
     */
    int toSignedInt(const char* str, size_t len) {
        if (!str || len == 0) return 0;

        if (str[0] == '-' && len > 1) return toInt(str + 1, len - 1) * -1;
    
        return toInt(str, len);
    }

    /**
     * @brief Sleeps for a specified duration, adjusted for interpretation time
     *
     * A DELAY command says "wait X ms from the moment this line was supposed to
     * execute." By the time we get here some of that time has already passed
     * while parsing the line. This function subtracts that overhead so the
     * actual wall-clock delay matches what the script author intended.
     *
     * @param time Desired delay in milliseconds
     */
    void sleep(unsigned long time) {
        unsigned long offset = millis() - interpretTime;

        if (time > offset) {
            sleepStartTime = millis();
            sleepTime      = time - offset;

            delay(sleepTime);
        }
    }

    // ====== PUBLIC ===== //

    /**
     * @brief Parses and executes one chunk of DuckyScript
     *
     * This is the core of the entire system. The caller hands us a buffer that
     * may contain one line, multiple lines, or a fragment of a line. We run
     * parse_lines() to split it into a linked list, then walk through every
     * line and dispatch the appropriate action.
     *
     * ── Per-line variables ──────────────────────────────────────────────────
     *   cmd          — The first word on the line (the command keyword itself)
     *   line_str     — Everything after the command keyword (the arguments)
     *   line_str_len — Length of the argument portion
     *   line_end     — True if the line ends with \r or \n. This is the key
     *                  signal: if false, the line was cut short by the buffer
     *                  boundary and the next parse() call will continue it.
     *
     * ── Post-command actions (run after every line) ────────────────────────
     *   1. Default delay is applied unless the command set ignore_delay or we
     *      are inside a string or comment block.
     *   2. repeatNum is decremented if the line is complete and we are not
     *      inside an LSTRING block.
     *
     * ── Memory note ─────────────────────────────────────────────────────────
     *   When USE_SD_CARD is defined the line list points into the caller's
     *   buffer — no allocation, no cleanup needed. When it is not defined,
     *   parse_lines() allocates the list on the heap and we must free it.
     *
     * @param str Buffer containing the script data to parse
     * @param len Number of valid bytes in the buffer
     */
    void parse(const char* str, size_t len) {
        interpretTime = millis();

        // Split str into a list of lines
        line_list* l = parse_lines(str, len);

        // Go through all lines
        line_node* n = l->first;

        // Flag, no default delay after this command
        bool ignore_delay;

        while (n) {
            ignore_delay = false;

            #ifdef USE_SD_CARD
                word_list* wl  = &n->words;
            #else
                word_list* wl  = n->words;
            #endif

            word_node* cmd = wl->first;

            const char* line_str = cmd->str + cmd->len + 1;
            size_t line_str_len  = n->len - cmd->len - 1;

            char last_char = n->str[n->len];
            bool line_end  = last_char == '\r' || last_char == '\n';

            // LSTRING_??? (-> type each character including linebreaks until LSTRING_END) 
            if (inLString || compare(cmd->str, 8, "LSTRING_", CASE_SENSETIVE)) {
                if (!inLString && cmd->len >= 13 && compare(&cmd->str[8], 5, "BEGIN", CASE_SENSETIVE)) {
                    ignore_delay = true;
                    inLString    = true;
                }else if (inLString && compare(cmd->str, cmd->len, "LSTRING_END", CASE_SENSETIVE)) {
                    ignore_delay = true;
                    inLString    = false;
                }else if(inLString) {
                    type(n->str, n->len);
                    if (line_end) {
                        keyboard::pressKey(KEY_ENTER);
                        release();
                    }
                }
                
            }
            
            // STRING (-> type each character)
            else if (inString || compare(cmd->str, cmd->len, "STRING", CASE_SENSETIVE) || compare(cmd->str, cmd->len, "STRINGLN", CASE_SENSETIVE)) {
                if (inString) {
                    type(n->str, n->len);
                } else {
                    isStringln = cmd->str[cmd->len-1] == 'N' && cmd->str[cmd->len-2] == 'L';

                    #ifdef USE_SD_CARD
                        const char* text_ptr = n->str + (isStringln ? 9 : 7);
                        int text_len = (int)n->len - (isStringln ? 9 : 7);
                        if (text_len > 0) type(text_ptr, text_len);
                    #else
                        type(line_str, line_str_len);
                    #endif 

                }

                inString = !line_end;

                if (line_end && isStringln) {
                    isStringln = false;
                    keyboard::pressKey(KEY_ENTER);
                    release();
                }
                    
            }

            // REM (= Comment -> do nothing)
            else if (inComment || compare(cmd->str, cmd->len, "REM", CASE_SENSETIVE)) {
                inComment    = !line_end;
                ignore_delay = true;
            }

            // LOCALE (-> change keyboard layout)
            else if (compare(cmd->str, cmd->len, "LOCALE", CASE_SENSETIVE)) {
                word_node* w = cmd->next;

                keyboard::setLocale(locale::get(w->str, w->len));
                
                ignore_delay = true;
            }

            // DELAY (-> sleep for x ms)
            else if (compare(cmd->str, cmd->len, "DELAY", CASE_SENSETIVE)) {
                sleep(toInt(line_str, line_str_len));
                ignore_delay = true;
            }

            // DEFAULTDELAY/DEFAULT_DELAY (set default delay per command)
            else if (compare(cmd->str, cmd->len, "DEFAULT_DELAY", CASE_SENSETIVE)) {
                defaultDelay = toInt(line_str, line_str_len);
                ignore_delay = true;
            }

            // REPEAT (-> repeat last command n times)
            else if (compare(cmd->str, cmd->len, "REPEAT", CASE_SENSETIVE)) {
                repeatNum    = toInt(line_str, line_str_len) + 1;
                ignore_delay = true;
            }

            // LOOP_BEGIN (-> Start of loop; if you enter a negative value, 
            // the loop will be infinite; if it is 0, it will not execute.)
            else if (compare(cmd->str, cmd->len, "LOOP_BEGIN", CASE_SENSETIVE)) {
                if (!inLoop) {
                    loopNum      = toSignedInt(line_str, line_str_len);
                    inLoop       = true;
                    if (loopNum < 0) loopNum = -1;
                }
                ignore_delay = true;
            }

            // LOOP_END (-> End of loop)
            else if (compare(cmd->str, cmd->len, "LOOP_END", CASE_SENSETIVE)) {
                if (inLoop) {
                    if ((loopNum - 1) == 0) {
                        loopNum--;
                        inLoop = false;
                    } 
                    else if (loopNum <= 0) loopNum = -1;
                    else loopNum--;
                }
                ignore_delay = true;
            }

            // LED
            else if (compare(cmd->str, cmd->len, "LED", CASE_SENSETIVE)) {
                word_node* w = cmd->next;

                #ifdef LED_CJMCU3212
                    if (compare(w->str, w->len, "RIGHT", CASE_INSENSETIVE)) {
                        w = w->next;
                        led::right(toInt(w->str, w->len) == 0 ? false : true);
                    }
                    else if (compare(w->str, w->len, "LEFT", CASE_INSENSETIVE)) {
                        w = w->next;
                        led::left(toInt(w->str, w->len) == 0 ? false : true);
                    }
                #else
                    int c[3];

                    for (uint8_t i = 0; i<3; ++i) {
                        if (w) {
                            c[i] = toInt(w->str, w->len);
                            w    = w->next;
                        } else {
                            c[i] = 0;
                        }
                    }

                    led::setColor(c[0], c[1], c[2]);
                #endif
            }

            // MOUSE MOVE
            else if (compare(cmd->str, cmd->len, "M_MOVE", CASE_SENSETIVE)) {
                word_node *w = cmd->next;

                int x, y;

                x = toSignedInt(w->str, w->len);
                w = w->next;
                y = toSignedInt(w->str, w->len);

                Mouse.move(x, y);
            }

            // MOUSE CLICK
            else if (compare(cmd->str, cmd->len, "M_CLICK", CASE_SENSETIVE)) {
                word_node *w = cmd->next;

                int b = toInt(w->str, w->len);

                Mouse.click(b);
            }

            // MOUSE PRESS
            else if (compare(cmd->str, cmd->len, "M_PRESS", CASE_SENSETIVE)) {
                word_node *w = cmd->next;

                int b = toInt(w->str, w->len);

                Mouse.press(b);
            }

            // MOUSE RELEASE
            else if (compare(cmd->str, cmd->len, "M_RELEASE", CASE_SENSETIVE)) {
                word_node *w = cmd->next;

                int b = toInt(w->str, w->len);

                Mouse.release(b);
            }

            // MOUSE SCROLL
            else if (compare(cmd->str, cmd->len, "M_SCROLL", CASE_SENSETIVE)) {
                word_node *w = cmd->next;

                int y = toSignedInt(w->str, w->len);

                Mouse.move(0, 0, y);
            }

            // KEYCODE
            else if (compare(cmd->str, cmd->len, "KEYCODE", CASE_SENSETIVE)) {
                word_node* w = cmd->next;
                if (w) {
                    keyboard::report k;

                    k.modifiers = (uint8_t)toInt(w->str, w->len);
                    k.reserved  = 0;
                    w           = w->next;

                    for (uint8_t i = 0; i<6; ++i) {
                        if (w) {
                            k.keys[i] = (uint8_t)toInt(w->str, w->len);
                            w         = w->next;
                        } else {
                            k.keys[i] = 0;
                        }
                    }

                    keyboard::send(&k);
                    keyboard::release();
                }
            }

            // Otherwise go through words and look for keys to press
            else {
                word_node* w = wl->first;

                while (w) {
                    press(w->str, w->len);
                    w = w->next;
                }

                if (line_end) release();
            }

            n = n->next;

            if (!inLString && !isStringln && !inString && !inComment && !ignore_delay) sleep(defaultDelay);

            if (line_end && !inLString && (repeatNum > 0)) --repeatNum;

            interpretTime = millis();
        }

        #if !defined(USE_SD_CARD)
            line_list_destroy(l);
        #endif
    }

    /**
     * @brief Resets all parser state to initial values
     *
     * Called by script_runner at the start of every new script. Clears all
     * continuation flags, resets the counters, and restores the default delay.
     */
    void reset() {
        isStringln = false;
        inString   = false;
        inLString  = false;
        inComment  = false;
        inLoop     = false;

        defaultDelay = 5;
        repeatNum    = 0;
        loopNum      = 0;
    }

    /**
     * @brief Returns the current repeat counter
     *
     * Read by com.cpp (for the status struct) and by script_runner (to decide
     * whether to seek back and re-execute the previous command). The value is
     * N after the REPEAT line has been processed, where N is the number the
     * script specified.
     *
     * @return Number of repetitions still pending
     */
    unsigned int getRepeats() {
        return repeatNum;
    }

    /**
     * @brief Returns the current loop iteration counter
     *
     * Read by com.cpp (for the status struct) and by script_runner (to decide
     * whether to seek back to LOOP_BEGIN). Positive means iterations remaining.
     * Zero means the loop is done. -1 means infinite.
     *
     * @return Current loop counter value
     */
    int getLoops() {
        return loopNum;
    }

    /**
     * @brief Returns how many milliseconds remain on the current sleep
     *
     * Read by com.cpp to populate the `wait` field in the status struct sent
     * back to the receiving device. When this returns 0 the device is free to
     * accept new data.
     *
     * @return Remaining sleep time in milliseconds, or 0 if no sleep is active
     */
    unsigned int getDelayTime() {
        unsigned long finishTime  = sleepStartTime + sleepTime;
        unsigned long currentTime = millis();
        
        if (currentTime > finishTime) {
            return 0;
        } else {
            unsigned long remainingTime = finishTime - currentTime;
            return (unsigned int)remainingTime;
        }
    }
}
