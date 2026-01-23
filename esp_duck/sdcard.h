#pragma once

#include <Arduino.h>
#include "config.h"

#ifdef USE_SD_CARD

namespace sdcard {

    enum SDStatus : uint8_t {
        SD_NOT_PRESENT = 0xA0,
        SD_IDLE        = 0xA1,
        SD_ERROR       = 0xA3,
        
        SD_READING     = 0xB1,
        SD_WRITING     = 0xB2,
        SD_EXECUTING   = 0xB3,
        SD_LISTING     = 0xB4,

        SD_READY       = 0xC0,
    };

}

#endif