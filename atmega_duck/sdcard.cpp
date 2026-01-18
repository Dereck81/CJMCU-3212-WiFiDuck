#include "sdcard.h"
#include "config.h"
#include <SdFat.h> //version: 2.3.0

#ifdef USE_SD_CARD

namespace sdcard {
    static SdFat SD;
    static SdFile f;
    static bool r;

    bool begin() {
        return SD.begin(SD_CS_PIN, SD_SPEED);
    }

    bool available() {
        return SD.card() && SD.vol();
    }

    bool beginFileRead(const char* n, uint32_t* s) {
        if (r || !available()) return false;
        if (!f.open(n, O_RDONLY)) return false;
        *s = f.fileSize();
        r = true;
        return true;
    }

    uint16_t readFileChunk(uint8_t* b, uint16_t m) {
        return r ? f.read(b, m) : 0;
    }

    void endFileRead() {
        if (r) {
            f.close();
            r = false;
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