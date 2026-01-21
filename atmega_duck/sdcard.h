#pragma once

#include <SdFat.h> //version: 2.3.0
#include <Arduino.h>
#include "config.h"

#ifdef USE_SD_CARD

namespace sdcard {

    enum SDStatus : uint8_t {
        SD_NOT_PRESENT = 0xFF,
        SD_IDLE        = 0x00,
        SD_READING     = 0x01,
        SD_WRITING     = 0x02,
        SD_EXECUTING   = 0x03,
        SD_LISTING     = 0x04,
        SD_ERROR       = 0x0F
    };

    bool begin();
    bool available();

    void setStatus(SDStatus s);
    SDStatus getStatus();

    // read files
    bool beginFileRead(const char *filename, uint32_t *fileSize);
    uint16_t readFileChunk(uint8_t *buffer, uint16_t maxSize);
    void endFileRead();
    
    uint32_t tell();
    bool seek(uint32_t pos);

}

#endif