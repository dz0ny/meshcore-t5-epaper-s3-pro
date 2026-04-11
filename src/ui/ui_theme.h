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
    SCREEN_CONTACT_DETAIL = 13,
    SCREEN_MSG_DETAIL     = 14,
    SCREEN_SET_BLE        = 15,
    SCREEN_SET_STORAGE    = 16,
    SCREEN_COMPOSE        = 17,
};

// ---------- Font declarations ----------

// Noto Sans Medium — body text with Latin Extended (čšćžđ) + symbols
LV_FONT_DECLARE(lv_font_noto_24);               // statusbar, sender names
LV_FONT_DECLARE(lv_font_noto_28);               // message text, values

// Montserrat Bold — menus, titles, clock
LV_FONT_DECLARE(lv_font_montserrat_bold_30);    // menus, titles, settings
LV_FONT_DECLARE(lv_font_montserrat_bold_80);    // lock screen clock
LV_FONT_DECLARE(lv_font_montserrat_bold_120);   // home screen clock
LV_FONT_DECLARE(lv_font_montserrat_bold_120);  // home screen clock
