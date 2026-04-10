#pragma once

#include "lvgl.h"

// ---------- E-paper color palette ----------

#define EPD_COLOR_BG          0xFFFFFF
#define EPD_COLOR_FG          0x000000
#define EPD_COLOR_TEXT        0x000000
#define EPD_COLOR_BORDER      0xBBBBBB
#define EPD_COLOR_FOCUS       0x91B821
#define EPD_COLOR_PROMPT_BG   0x1E1E00
#define EPD_COLOR_PROMPT_TXT  0xFFFEE6

// ---------- Screen IDs ----------

enum screen_id {
    SCREEN_HOME     = 0,
    SCREEN_CONTACTS = 1,
    SCREEN_CHAT     = 2,
    SCREEN_SETTINGS = 3,
    SCREEN_GPS      = 4,
    SCREEN_BATTERY  = 5,
    SCREEN_MESH     = 6,
    SCREEN_STATUS   = 7,
    SCREEN_SET_DISPLAY = 8,
    SCREEN_SET_GPS     = 9,
    SCREEN_SET_MESH    = 10,
    SCREEN_DISCOVERY   = 11,
    SCREEN_LOCK        = 12,
};

// ---------- Font declarations ----------
// Fonts are compiled from the factory assets in src/fonts/

LV_FONT_DECLARE(Font_Mono_Bold_20);
LV_FONT_DECLARE(Font_Mono_Bold_25);
LV_FONT_DECLARE(Font_Mono_Bold_30);
LV_FONT_DECLARE(Font_Mono_Bold_90);
LV_FONT_DECLARE(Font_Geist_Bold_20);
LV_FONT_DECLARE(Font_Geist_Light_20);

// Noto Sans — readable body text with Latin Extended (čšćžđ)
LV_FONT_DECLARE(lv_font_noto_24);       // statusbar, sender names
LV_FONT_DECLARE(lv_font_noto_28);       // message text, values

// Montserrat Bold — menus, titles, clock
LV_FONT_DECLARE(lv_font_montserrat_bold_30);  // menus, titles, settings
LV_FONT_DECLARE(lv_font_montserrat_bold_80);  // big clock
