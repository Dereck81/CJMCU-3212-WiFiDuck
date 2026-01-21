#include "script_runner.h"
#include "sdcard.h"
#include "config.h"

#ifdef USE_SD_CARD

namespace script_runner {
    // Runner status
    static struct {
        uint32_t lastPos;      // Starting position of last valid line
        uint32_t afterRepeat;  // Position after REPEAT line
        uint16_t repeatCount;  // Remaining repetition counter
        uint8_t flags;         // bit0=running, bit1=inFragment, bit2=useAfterRepeat
    } state;
    
    static uint32_t fragmentPos;  // Position where the fragment ended up
    
    // Macros for flags
    #define FLAG_RUNNING       0x01
    #define FLAG_IN_FRAGMENT   0x02
    #define FLAG_USE_AFTER_REP 0x04
    
    bool start(const char* filename) {
        uint32_t size;
        if (!sdcard::beginFileRead(filename, &size)) return 0;
        
        state.lastPos = 0;
        state.afterRepeat = 0;
        state.repeatCount = 0;
        state.flags = FLAG_RUNNING;
        sdcard::setStatus(sdcard::SDStatus::SD_EXECUTING);
        
        return 1;
    }

    void stop() {
        if (state.flags & FLAG_RUNNING) {
            sdcard::endFileRead();
            state.flags = 0;
        }
        sdcard::setStatus(sdcard::SDStatus::SD_IDLE);
    }

    bool getLine(uint8_t* buffer, uint8_t* length) {
        // If it's not running and there are no pending reps, exit
        if (!(state.flags & FLAG_RUNNING) && state.repeatCount == 0) {
            return 0;
        }
        
        while (1) {
            // Manage repetitions
            if (state.repeatCount > 0) {
                state.repeatCount--;
                state.flags |= FLAG_RUNNING;
                sdcard::seek(state.lastPos);
                state.flags &= ~FLAG_IN_FRAGMENT;
            }

            // After completing all repetitions, go to the position after REPEAT
            else if (state.flags & FLAG_USE_AFTER_REP) {
                sdcard::seek(state.afterRepeat);
                state.flags &= ~(FLAG_USE_AFTER_REP | FLAG_IN_FRAGMENT);
            }
            
            // Determine line starting position
            uint32_t startPos;
            if (state.flags & FLAG_IN_FRAGMENT) {
                sdcard::seek(fragmentPos);
                startPos = state.lastPos;  // Maintain original starting position
            } else {
                startPos = sdcard::tell();
            }
            
            // Read characters until end of line or buffer is full
            uint8_t i = 0;
            uint8_t ch;
            bool endOfLine = false;
            
            while (i < BUFFER_SIZE - 1) {
                if (!sdcard::readFileChunk(&ch, 1)) {
                    state.flags &= ~FLAG_RUNNING;
                    break;
                }
                
                buffer[i++] = ch;
                
                if (ch == '\n') {
                    endOfLine = true;
                    break;
                }
            }
            
            // If we read something but didn't get to \n, mark it as a fragment
            if (i > 0 && !endOfLine) {
                endOfLine = true;  // Treat as end of line anyway
            }
            
            // If we didn't read anything
            if (i == 0) {
                if (state.repeatCount > 0) continue;  // There are repetitions, try again
                stop();
                return 0;
            }
            
            // If we complete a line
            if (endOfLine) {
                state.flags &= ~FLAG_IN_FRAGMENT;
                
                // Detect REPEAT command
                if (i >= 7 && buffer[0]=='R' && buffer[1]=='E' && buffer[2]=='P' && 
                    buffer[3]=='E' && buffer[4]=='A' && buffer[5]=='T') {
                    
                    // Parse number after REPEAT
                    state.repeatCount = 0;
                    for (uint8_t j=7; j<i; j++) {
                        if (buffer[j] >= '0' && buffer[j] <= '9') {
                            state.repeatCount = state.repeatCount * 10 + (buffer[j] - '0');
                        } else if (buffer[j] == ' ' || buffer[j] == '\t') {
                            continue;  // Ignore spaces
                        } else {
                            break;  // Another character, end parsing
                        }
                    }
                    
                    // Save position after REPEAT line
                    state.afterRepeat = sdcard::tell();
                    state.flags |= FLAG_USE_AFTER_REP;
                    
                    continue;  // Read next line
                }
                
                // Detect REM comment
                if (i >= 3 && buffer[0]=='R' && buffer[1]=='E' && buffer[2]=='M') {
                    continue;  // Ignore comment line
                }
                
                // Normal valid line
                state.lastPos = startPos;
                *length = i;
                return 1;
            }
            
            // If it's a fragment (very long line)
            state.flags |= FLAG_IN_FRAGMENT;
            fragmentPos = sdcard::tell();
            *length = i;
            return 1;
        }
        
        return 0;
    }
}

#endif