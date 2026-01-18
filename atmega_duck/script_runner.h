#pragma once

#include <stdint.h>
#include "config.h"

#ifdef USE_SD_CARD

namespace script_runner {
    bool start(const char* f);
    void stop();
    bool getLine(uint8_t* b, uint8_t* l);
}

#endif