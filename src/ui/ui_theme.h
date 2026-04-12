#pragma once

#include "lvgl.h"

// ---------- Color palette ----------

#define EPD_COLOR_BG          0xFFFFFF
#define EPD_COLOR_FG          0x000000
#define EPD_COLOR_TEXT        0x000000
#define EPD_COLOR_BORDER      0x333333
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
    SCREEN_MAP            = 18,
    SCREEN_SENSORS        = 19,
    SCREEN_PING           = 20,
};

// ---------- Font declarations ----------

// Noto Sans Medium — body text with Latin Extended (čšćžđ) + symbols
LV_FONT_DECLARE(lv_font_noto_24);               // statusbar, sender names
LV_FONT_DECLARE(lv_font_noto_28);               // message text, values

// Montserrat Bold — menus, titles, clock
LV_FONT_DECLARE(lv_font_montserrat_bold_30);    // menus, titles, settings
LV_FONT_DECLARE(lv_font_montserrat_bold_80);    // lock screen clock
LV_FONT_DECLARE(lv_font_montserrat_bold_120);   // home screen clock

// ---------- UI Scale ----------
// Board-conditional layout constants for different screen sizes.
// E-paper: 540x960, T-Deck: 320x240

#if defined(BOARD_TDECK)

// Fonts — scaled down for 320x240 TFT
#define UI_FONT_SMALL       &lv_font_montserrat_14  // was noto_24
#define UI_FONT_BODY        &lv_font_montserrat_14  // was noto_28
#define UI_FONT_TITLE       &lv_font_montserrat_16  // was bold_30
#define UI_FONT_CLOCK_SM    &lv_font_montserrat_bold_30 // was bold_80
#define UI_FONT_CLOCK_LG    &lv_font_montserrat_bold_30 // was bold_120

// Layout sizes — roughly halved
#define UI_STATUSBAR_HEIGHT  25     // was 50
#define UI_STATUSBAR_Y       2      // was 5
#define UI_STATUSBAR_PAD     3      // was 5
#define UI_STATUSBAR_COL_PAD 4      // was 8
#define UI_BACK_BTN_Y        25     // was 50
#define UI_BACK_BTN_HEIGHT   35     // was 70
#define UI_BACK_BTN_PAD_V    5      // was 10
#define UI_BACK_BTN_COL_PAD  4      // was 8
#define UI_MENU_ITEM_HEIGHT  48     // was 85, needs to be tappable
#define UI_MENU_ITEM_PAD     10     // was 15
#define UI_MENU_ITEM_INSET   8      // was 15
#define UI_TEXT_BTN_HEIGHT   44     // was 80
#define UI_TEXT_BTN_PAD      8      // was 15
#define UI_TEXT_BTN_RADIUS   6      // was 12
#define UI_TEXT_BTN_BORDER   2      // was 3
#define UI_SCROLL_LIST_H     80     // percent, was 85
#define UI_SCROLL_LIST_Y    -5      // was -10
#define UI_EXT_CLICK_BACK    20     // bigger hit area on small screen
#define UI_EXT_CLICK_ACTION  15
#define UI_EXT_CLICK_LIST    15     // make list items easy to tap
#define UI_ACTION_BTN_H      28     // was 56
#define UI_ACTION_BTN_PAD_H  10     // was 18
#define UI_ACTION_BTN_PAD_V  4      // was 8
#define UI_ACTION_BTN_RADIUS 6      // was 12
#define UI_ACTION_BTN_BORDER 2      // was 3

#else // BOARD_EPAPER (default)

// Fonts — original e-paper sizes
#define UI_FONT_SMALL       &lv_font_noto_24
#define UI_FONT_BODY        &lv_font_noto_28
#define UI_FONT_TITLE       &lv_font_montserrat_bold_30
#define UI_FONT_CLOCK_SM    &lv_font_montserrat_bold_80
#define UI_FONT_CLOCK_LG    &lv_font_montserrat_bold_120

// Layout sizes — original e-paper sizes
#define UI_STATUSBAR_HEIGHT  50
#define UI_STATUSBAR_Y       5
#define UI_STATUSBAR_PAD     5
#define UI_STATUSBAR_COL_PAD 8
#define UI_BACK_BTN_Y        50
#define UI_BACK_BTN_HEIGHT   70
#define UI_BACK_BTN_PAD_V    10
#define UI_BACK_BTN_COL_PAD  8
#define UI_MENU_ITEM_HEIGHT  85
#define UI_MENU_ITEM_PAD     15
#define UI_MENU_ITEM_INSET   15
#define UI_TEXT_BTN_HEIGHT   80
#define UI_TEXT_BTN_PAD      15
#define UI_TEXT_BTN_RADIUS   12
#define UI_TEXT_BTN_BORDER   3
#define UI_SCROLL_LIST_H     85
#define UI_SCROLL_LIST_Y    -10
#define UI_EXT_CLICK_BACK    35
#define UI_EXT_CLICK_ACTION  25
#define UI_EXT_CLICK_LIST    25
#define UI_ACTION_BTN_H      56
#define UI_ACTION_BTN_PAD_H  18
#define UI_ACTION_BTN_PAD_V  8
#define UI_ACTION_BTN_RADIUS 12
#define UI_ACTION_BTN_BORDER 3

#endif
