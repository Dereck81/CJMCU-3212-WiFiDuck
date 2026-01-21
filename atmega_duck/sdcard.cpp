#include "sdcard.h"
#include "config.h"
#include <SdFat.h> //version: 2.3.0

#ifdef USE_SD_CARD

namespace sdcard {
    static SdFat SD;
    static SdFile f;
    static bool r;
    static SDStatus currentStatus = SD_NOT_PRESENT;

    bool begin() {
        if(SD.begin(SD_CS_PIN, SD_SPEED)) {
            currentStatus = SD_IDLE;
            return true;
        }
        return false;
    }

    bool available() {
        if(SD.card() && SD.vol() && currentStatus != SD_NOT_PRESENT) 
            return true;
        currentStatus = SD_NOT_PRESENT;
        return false;
    }

    void setStatus(SDStatus s) {
        currentStatus = s;
    }

    SDStatus getStatus() {
        return currentStatus;
    }

    bool beginFileRead(const char* n, uint32_t* s) {
        if (r || !available()) return false;
        if (!f.open(n, O_RDONLY)) { 
            currentStatus = SD_ERROR;
            return false;
        }
        *s = f.fileSize();
        r = true;
        currentStatus = SD_READING;
        return true;
    }

    uint16_t readFileChunk(uint8_t* b, uint16_t m) {
        return r ? f.read(b, m) : 0;
    }

    void endFileRead() {
        if (r) {
            f.close();
            r = false;
            currentStatus = SD_IDLE;
        }
    }

    uint32_t tell() {
        return r ? f.curPosition() : 0;
    }
    
    bool seek(uint32_t p) {
        return r ? f.seekSet(p) : false;
    }
}

#endif