/* This software is licensed under the MIT License: https://github.com/spacehuhntech/usbnova 

   Modified and adapted by:
    - Dereck81
*/

#include "locale.h"

#include "mac/locale_be_mac.h"
#include "mac/locale_bg_mac.h"
#include "mac/locale_ca_fr_mac.h"
#include "mac/locale_ch_de_mac.h"
#include "mac/locale_ch_fr_mac.h"
#include "mac/locale_cz_mac.h"
#include "mac/locale_de_mac.h"
#include "mac/locale_dk_mac.h"
#include "mac/locale_ee_mac.h"
#include "mac/locale_es_mac.h"
#include "mac/locale_es_la_mac.h"
#include "mac/locale_fi_mac.h"
#include "mac/locale_fr_mac.h"
#include "mac/locale_gb_mac.h"
#include "mac/locale_gr_mac.h"
#include "mac/locale_hr_mac.h"
#include "mac/locale_hu_mac.h"
#include "mac/locale_in_mac.h"
#include "mac/locale_is_mac.h"
#include "mac/locale_it_mac.h"
#include "mac/locale_lt_mac.h"
#include "mac/locale_lv_mac.h"
#include "mac/locale_nl_mac.h"
#include "mac/locale_no_mac.h"
#include "mac/locale_pl_mac.h"
#include "mac/locale_pt_mac.h"
#include "mac/locale_pt_br_mac.h"
#include "mac/locale_ro_mac.h"
#include "mac/locale_ru_mac.h"
#include "mac/locale_se_mac.h"
#include "mac/locale_si_mac.h"
#include "mac/locale_sk_mac.h"
#include "mac/locale_tr_mac.h"
#include "mac/locale_ua_mac.h"
#include "mac/locale_us_mac.h"
#include "win/locale_be_win.h"
#include "win/locale_bg_win.h"
#include "win/locale_ca_cms_win.h"
#include "win/locale_ca_fr_win.h"
#include "win/locale_ch_de_win.h"
#include "win/locale_ch_fr_win.h"
#include "win/locale_cz_win.h"
#include "win/locale_de_win.h"
#include "win/locale_dk_win.h"
#include "win/locale_ee_win.h"
#include "win/locale_es_la_win.h"
#include "win/locale_es_win.h"
#include "win/locale_fi_win.h"
#include "win/locale_fr_win.h"
#include "win/locale_gb_win.h"
#include "win/locale_gr_win.h"
#include "win/locale_hr_win.h"
#include "win/locale_hu_win.h"
#include "win/locale_ie_win.h"
#include "win/locale_in_win.h"
#include "win/locale_is_win.h"
#include "win/locale_it_win.h"
#include "win/locale_lt_win.h"
#include "win/locale_lv_win.h"
#include "win/locale_nl_win.h"
#include "win/locale_no_win.h"
#include "win/locale_pl_win.h"
#include "win/locale_pt_br_win.h"
#include "win/locale_pt_win.h"
#include "win/locale_ro_win.h"
#include "win/locale_ru_win.h"
#include "win/locale_se_win.h"
#include "win/locale_si_win.h"
#include "win/locale_sk_win.h"
#include "win/locale_tr_win.h"
#include "win/locale_ua_win.h"
#include "win/locale_us_win.h"

#include <stddef.h>

extern "C" {
#include "../duckparser/parser.h"
}

#define CASE_INSENSETIVE 0
#define CASE_SENSETIVE 1

namespace locale {
    // ===== PUBLIC ===== //
    hid_locale_t* get_default() {
        return &locale_us_win;
    }
    
    hid_locale_t* get(const char* name, size_t len) {
        /*
            These are commented out here to avoid excessive flash usage; 
            if you wish to include one, you can uncomment it and comment on another that you don't use much.
        */
        if (compare(name, len, "US", CASE_INSENSETIVE)) return &locale_us_win;
        //else if (compare(name, len, "BE_MAC", CASE_INSENSETIVE)) return &locale_be_mac;
        //else if (compare(name, len, "BG_MAC", CASE_INSENSETIVE)) return &locale_bg_mac;
        //else if ((compare(name, len, "CA-FR_MAC", CASE_INSENSETIVE)) || (compare(name, len, "CA_FR_MAC", CASE_INSENSETIVE))) return &locale_ca_fr_mac;
        //else if ((compare(name, len, "CH-DE_MAC", CASE_INSENSETIVE)) || (compare(name, len, "CH_DE_MAC", CASE_INSENSETIVE))) return &locale_ch_de_mac;
        //else if ((compare(name, len, "CH-FR_MAC", CASE_INSENSETIVE)) || (compare(name, len, "CH_FR_MAC", CASE_INSENSETIVE))) return &locale_ch_fr_mac;
        //else if (compare(name, len, "CZ_MAC", CASE_INSENSETIVE)) return &locale_cz_mac;
        //else if (compare(name, len, "DE_MAC", CASE_INSENSETIVE)) return &locale_de_mac;
        //else if (compare(name, len, "DK_MAC", CASE_INSENSETIVE)) return &locale_dk_mac;
        //else if (compare(name, len, "EE_MAC", CASE_INSENSETIVE)) return &locale_ee_mac;
        //else if (compare(name, len, "ES_MAC", CASE_INSENSETIVE)) return &locale_es_mac;
        else if ((compare(name, len, "ES-LA_MAC", CASE_INSENSETIVE)) || (compare(name, len, "ES_LA_MAC", CASE_INSENSETIVE))) return &locale_es_la_mac;
        //else if (compare(name, len, "FI_MAC", CASE_INSENSETIVE)) return &locale_fi_mac;
        //else if (compare(name, len, "FR_MAC", CASE_INSENSETIVE)) return &locale_fr_mac;
        //else if (compare(name, len, "GB_MAC", CASE_INSENSETIVE)) return &locale_gb_mac;
        //else if (compare(name, len, "GR_MAC", CASE_INSENSETIVE)) return &locale_gr_mac;
        //else if (compare(name, len, "HR_MAC", CASE_INSENSETIVE)) return &locale_hr_mac;
        //else if (compare(name, len, "HU_MAC", CASE_INSENSETIVE)) return &locale_hu_mac;
        //else if (compare(name, len, "IN_MAC", CASE_INSENSETIVE)) return &locale_in_mac;
        //else if (compare(name, len, "IS_MAC", CASE_INSENSETIVE)) return &locale_is_mac;
        //else if (compare(name, len, "IT_MAC", CASE_INSENSETIVE)) return &locale_it_mac;
        //else if (compare(name, len, "LT_MAC", CASE_INSENSETIVE)) return &locale_lt_mac;
        //else if (compare(name, len, "LV_MAC", CASE_INSENSETIVE)) return &locale_lv_mac;
        //else if (compare(name, len, "NL_MAC", CASE_INSENSETIVE)) return &locale_nl_mac;
        //else if (compare(name, len, "NO_MAC", CASE_INSENSETIVE)) return &locale_no_mac;
        //else if (compare(name, len, "PL_MAC", CASE_INSENSETIVE)) return &locale_pl_mac;
        //else if (compare(name, len, "PT_MAC", CASE_INSENSETIVE)) return &locale_pt_mac;
        //else if ((compare(name, len, "PT-BR_MAC", CASE_INSENSETIVE)) || (compare(name, len, "PT_BR_MAC", CASE_INSENSETIVE))) return &locale_pt_br_mac;
        //else if (compare(name, len, "RO_MAC", CASE_INSENSETIVE)) return &locale_ro_mac;
        //else if (compare(name, len, "RU_MAC", CASE_INSENSETIVE)) return &locale_ru_mac;
        //else if (compare(name, len, "SE_MAC", CASE_INSENSETIVE)) return &locale_se_mac;
        //else if (compare(name, len, "SI_MAC", CASE_INSENSETIVE)) return &locale_si_mac;
        //else if (compare(name, len, "SK_MAC", CASE_INSENSETIVE)) return &locale_sk_mac;
        //else if (compare(name, len, "TR_MAC", CASE_INSENSETIVE)) return &locale_tr_mac;
        //else if (compare(name, len, "UA_MAC", CASE_INSENSETIVE)) return &locale_ua_mac;
        //else if (compare(name, len, "US_MAC", CASE_INSENSETIVE)) return &locale_us_mac;
        //else if (compare(name, len, "BE", CASE_INSENSETIVE)) return &locale_be_win;
        //else if (compare(name, len, "BG", CASE_INSENSETIVE)) return &locale_bg_win;
        //else if ((compare(name, len, "CA-CMS", CASE_INSENSETIVE)) || (compare(name, len, "CA_CMS", CASE_INSENSETIVE))) return &locale_ca_cms_win;
        //else if ((compare(name, len, "CA-FR", CASE_INSENSETIVE)) || (compare(name, len, "CA_FR", CASE_INSENSETIVE))) return &locale_ca_fr_win;
        //else if ((compare(name, len, "CH-DE", CASE_INSENSETIVE)) || (compare(name, len, "CH_DE", CASE_INSENSETIVE))) return &locale_ch_de_win;
        //else if ((compare(name, len, "CH-FR", CASE_INSENSETIVE)) || (compare(name, len, "CH_FR", CASE_INSENSETIVE))) return &locale_ch_fr_win;
        //else if (compare(name, len, "CZ", CASE_INSENSETIVE)) return &locale_cz_win;
        //else if (compare(name, len, "DE", CASE_INSENSETIVE)) return &locale_de_win;
        //else if (compare(name, len, "DK", CASE_INSENSETIVE)) return &locale_dk_win;
        //else if (compare(name, len, "EE", CASE_INSENSETIVE)) return &locale_ee_win;
        else if ((compare(name, len, "ES-LA", CASE_INSENSETIVE)) || (compare(name, len, "ES_LA", CASE_INSENSETIVE))) return &locale_es_la_win;
        else if (compare(name, len, "ES", CASE_INSENSETIVE)) return &locale_es_win;
        //else if (compare(name, len, "FI", CASE_INSENSETIVE)) return &locale_fi_win;
        //else if (compare(name, len, "FR", CASE_INSENSETIVE)) return &locale_fr_win;
        //else if (compare(name, len, "GB", CASE_INSENSETIVE)) return &locale_gb_win;
        //else if (compare(name, len, "GR", CASE_INSENSETIVE)) return &locale_gr_win;
        //else if (compare(name, len, "HR", CASE_INSENSETIVE)) return &locale_hr_win;
        //else if (compare(name, len, "HU", CASE_INSENSETIVE)) return &locale_hu_win;
        //else if (compare(name, len, "IE", CASE_INSENSETIVE)) return &locale_ie_win;
        //else if (compare(name, len, "IN", CASE_INSENSETIVE)) return &locale_in_win;
        //else if (compare(name, len, "IS", CASE_INSENSETIVE)) return &locale_is_win;
        //else if (compare(name, len, "IT", CASE_INSENSETIVE)) return &locale_it_win;
        //else if (compare(name, len, "LT", CASE_INSENSETIVE)) return &locale_lt_win;
        //else if (compare(name, len, "LV", CASE_INSENSETIVE)) return &locale_lv_win;
        //else if (compare(name, len, "NL", CASE_INSENSETIVE)) return &locale_nl_win;
        //else if (compare(name, len, "NO", CASE_INSENSETIVE)) return &locale_no_win;
        //else if (compare(name, len, "PL", CASE_INSENSETIVE)) return &locale_pl_win;
        //else if ((compare(name, len, "PT-BR", CASE_INSENSETIVE)) || (compare(name, len, "PT_BR", CASE_INSENSETIVE))) return &locale_pt_br_win;
        //else if (compare(name, len, "PT", CASE_INSENSETIVE)) return &locale_pt_win;
        //else if (compare(name, len, "RO", CASE_INSENSETIVE)) return &locale_ro_win;
        //else if (compare(name, len, "RU", CASE_INSENSETIVE)) return &locale_ru_win;
        //else if (compare(name, len, "SE", CASE_INSENSETIVE)) return &locale_se_win;
        //else if (compare(name, len, "SI", CASE_INSENSETIVE)) return &locale_si_win;
        //else if (compare(name, len, "SK", CASE_INSENSETIVE)) return &locale_sk_win;
        //else if (compare(name, len, "TR", CASE_INSENSETIVE)) return &locale_tr_win;
        //else if (compare(name, len, "UA", CASE_INSENSETIVE)) return &locale_ua_win;
        else return get_default();
    }
}