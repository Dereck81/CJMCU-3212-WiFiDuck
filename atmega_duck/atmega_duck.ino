/*
   This software is licensed under the MIT License. See the license file for details.
   Source: https://github.com/spacehuhntech/WiFiDuck
   
   Modified and adapted by:
    - Dereck81
 */

#include "include/config.h"
#include "include/debug.h"

#include "src/hid/keyboard.h"
#include "src/led/led.h"
#include "src/com/com.h"
#include "src/duckparser/duckparser.h"
#include "src/serial_bridge/serial_bridge.h"
#include "src/sdcard/sdcard.h"
#include "src/sdcard/sd_handler.h"

#include <Mouse.h>


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
    if (sdcard::begin()) sd_handler::autorun();
    #endif

}

// ===== LOOOP ===== //
void loop() {

    #ifdef USE_SD_CARD
    if (!sd_handler::run_script_step()) return;
    #endif

    com::update();
    if (com::hasData()) {
        const buffer_t& buffer = com::getBuffer();

        #ifdef USE_SD_CARD
            if (com::isSdPacket()) {
                //debugs("SD CMD: ");

                //debugln(buffer.data[0], HEX);

                sd_handler::process(buffer.data, buffer.len);

            } 
            
            else if (sdcard::getStatus() >= sdcard::SD_READING) {
                // This condition prevents the interpretation of any value 
                // other than an SD_CMD_ sent from the ESP when the SDCARD is operational.
                // Notify the status again with sendDone.
                com::sendDone();
                return;

            } 
            
            else if(buffer.len == 1 && buffer.data[0] == CMD_PARSER_RESET) {
                //debugln("Duckparser reset");

                duckparser::reset();      
            }
            
            else {
                //debugs("Interpreting: ");

                //for (size_t i = 0; i<buffer.len; i++) debug(buffer.data[i]);

                duckparser::parse(buffer.data, buffer.len);

            }
        #else 

            if (buffer.len == 1 && buffer.data[0] == CMD_PARSER_RESET) {
                //debugln("Duckparser reset");
                
                duckparser::reset();
            } else {
                //debugs("Interpreting: ");

                //for (size_t i = 0; i<buffer.len; i++) debug(buffer.data[i]);

                duckparser::parse(buffer.data, buffer.len);
            }
            
        #endif

        com::sendDone();
    }
}