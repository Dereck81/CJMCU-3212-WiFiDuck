/*
   This software is licensed under the MIT License. See the license file for details.
   Source: https://github.com/spacehuhntech/WiFiDuck

   Modified and adapted by:
    - Dereck81
 */

#pragma once

#include "../../include/config.h"

namespace led {

    void begin();
    
    #if defined(LED_CJMCU3212)
    void left(bool active);
    void right(bool active);
    #else
    void setColor(int r, int g, int b);
    #endif
}