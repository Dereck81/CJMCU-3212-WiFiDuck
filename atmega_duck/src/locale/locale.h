/* This software is licensed under the MIT License: https://github.com/spacehuhntech/usbnova 

   Modified and adapted by:
    - Dereck81
*/

#pragma once

#include "usb_hid_keys.h"
#include "locale_types.h"

namespace locale {
    hid_locale_t* get_default();
    hid_locale_t* get(const char* name, size_t len);
}