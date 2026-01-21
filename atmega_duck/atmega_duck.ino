/*
   This software is licensed under the MIT License. See the license file for details.
   Source: https://github.com/spacehuhntech/WiFiDuck
 */

#include "config.h"
#include "debug.h"

#include "keyboard.h"
#include "led.h"
#include "com.h"
#include "duckparser.h"
#include "serial_bridge.h"
#include "sdcard.h"
#include "script_runner.h"
#include <Mouse.h>

#ifdef USE_SD_CARD
    static uint8_t _gb[BUFFER_SIZE];
    static sdcard::SDStatus sdcardStatus;
#endif

// ===== SETUP ====== //
void setup() {
    debug_init();

    com::begin();
    led::begin();
    serial_bridge::begin();
    keyboard::begin();
    Mouse.begin();

    delay(1000);

    debugs("Started! ");
    debugln(VERSION);
    
    #ifdef USE_SD_CARD
        if (sdcard::begin()) {
            #ifdef AUTORUN_SCRIPT
                if(script_runner::start(AUTORUN_SCRIPT)) 
                    led::right(true);
            #endif
        }
    #endif

}

// ===== LOOOP ===== //
void loop() {

    #ifdef USE_SD_CARD
        sdcardStatus = sdcard::getStatus();

        if (sdcardStatus == sdcard::SD_EXECUTING) {
            uint8_t l;
            if (script_runner::getLine(_gb, &l)) {
                //for(uint8_t i=0; i<l; i++) debug((char)_gb[i]);
                duckparser::parse(_gb, l);
            }else {
                Mouse.release(MOUSE_LEFT);
                Mouse.release(MOUSE_RIGHT);
                Mouse.release(MOUSE_MIDDLE);
                delay(10);
                
                Mouse.move(0, 0, 0);
                delay(10);
                
                keyboard::release();
                delay(10);

                script_runner::stop();
                
                led::right(false);

                com::sendDone(); // Sends SD card completion status update
                
                return;
            }

        }
    #endif

    com::update();
    if (com::hasData()) {
        const buffer_t& buffer = com::getBuffer();

        #ifdef USE_SD_CARD
            if (sdcardStatus != sdcard::SD_IDLE && sdcard::SD_NOT_PRESENT && sdcard::SD_ERROR) {
                com::sendDone();
                return;
            }
        #endif 

        debugs("Interpreting: ");

        for (size_t i = 0; i<buffer.len; i++) debug(buffer.data[i]);

        duckparser::parse(buffer.data, buffer.len);

        com::sendDone();
    }
}