/*!
    \file esp_duck/com.h
    \brief Communication Module header
    \author Stefan Kremser
    \author Dereck81 (modifications and adaptation)
    \copyright MIT License
 */

#pragma once
/*! \typedef com_callback
 *  \brief Callback function to react on different responses
 */
typedef void (* com_callback)();

typedef void (*print_callback)(const char* str);

#include "sdcard.h"
#include "config.h"

/*! \namespace com
 *  \brief Communication module
 */
namespace com {
    /*! Initializes the communication module */
    void begin();

    /*! Updates the communication module */
    void update();

    /*! Transmits string */
    unsigned int send(char str);
    unsigned int send(const char* str);
    unsigned int send(const char* str, unsigned int len, bool waiting_ack = false);

    /*! Sets callback for status done */
    void onDone(com_callback c);

    /*! Sets callback for status error */
    void onError(com_callback c);

    /*! Sets callback for status repeat */
    void onRepeat(com_callback c);

    /*! Sets callback for status loop */
    void onLoop(com_callback c);

    /*! Returns state of connection */
    bool connected();

    void set_print_callback(print_callback cb);
    
    #ifdef USE_SD_CARD
    /*! Sets the SD card operating mode */
    void set_mode(sdcard::SDStatus);

    /*! Returns the current SD card status */
    int get_sdcard_status();

    /*! Returns the current SD card mode */
    sdcard::SDStatus get_mode();

    /*!  Sends raw data from SD card */
    unsigned int send_sd(const uint8_t* data, size_t len);

    /*! Checks if an SD card session is active*/
    bool is_session_active();
    #endif

    /*! Returns the current loop counter */
    int8_t get_loops();

    /*! Returns the repeat counter */
    int get_repeats();

    /*! Returns the communication protocol version */
    int get_com_version();

    /*! Returns the firmware version */
    int get_version();
}