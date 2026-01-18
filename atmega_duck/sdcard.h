#pragma once

#include <SdFat.h> //version: 2.3.0
#include <Arduino.h>
#include "config.h"

#ifdef USE_SD_CARD

namespace sdcard {

    bool begin();
    bool available();

    // read files
    bool beginFileRead(const char *filename, uint32_t *fileSize);
    uint16_t readFileChunk(uint8_t *buffer, uint16_t maxSize);
    void endFileRead();
    
    uint32_t tell();
    bool seek(uint32_t pos);

}

#endif