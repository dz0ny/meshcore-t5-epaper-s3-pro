#pragma once
// UI layout constants for LilyGo T-Deck (320x240)

#include "lvgl.h"

// ---------- Fonts ----------
#define UI_FONT_SMALL       &lv_font_montserrat_14
#define UI_FONT_BODY        &lv_font_montserrat_14
#define UI_FONT_TITLE       &lv_font_montserrat_16
#define UI_FONT_CLOCK_SM    &lv_font_montserrat_bold_30
#define UI_FONT_CLOCK_LG    &lv_font_montserrat_bold_30

// ---------- Shared components ----------

// Statusbar
#define UI_STATUSBAR_HEIGHT  25
#define UI_STATUSBAR_Y       2
#define UI_STATUSBAR_PAD     3
#define UI_STATUSBAR_COL_PAD 4

// Back button
#define UI_BACK_BTN_Y        25
#define UI_BACK_BTN_HEIGHT   35
#define UI_BACK_BTN_PAD_V    5
#define UI_BACK_BTN_COL_PAD  4

// Menu / list items
#define UI_MENU_ITEM_HEIGHT  48
#define UI_MENU_ITEM_PAD     10
#define UI_MENU_ITEM_INSET   8

// Text button
#define UI_TEXT_BTN_HEIGHT   44
#define UI_TEXT_BTN_PAD      8
#define UI_TEXT_BTN_RADIUS   6
#define UI_TEXT_BTN_BORDER   2

// Scroll list
#define UI_SCROLL_LIST_H     80
#define UI_SCROLL_LIST_Y    -5

// Extended click areas
#define UI_EXT_CLICK_BACK    20
#define UI_EXT_CLICK_ACTION  15
#define UI_EXT_CLICK_LIST    15

// Action button (inline with back button)
#define UI_ACTION_BTN_H      28
#define UI_ACTION_BTN_PAD_H  10
#define UI_ACTION_BTN_PAD_V  4
#define UI_ACTION_BTN_RADIUS 6
#define UI_ACTION_BTN_BORDER 2

// ---------- Home screen ----------
#define UI_HOME_NODE_Y       26
#define UI_HOME_CLOCK_Y      38
#define UI_HOME_DATE_Y       72
#define UI_HOME_MENU_Y       95
#define UI_HOME_MENU_SCROLL  1  // menu must scroll on 240px screen

// ---------- Lock screen ----------
#define UI_LOCK_NODE_Y       26
#define UI_LOCK_CLOCK_Y      40
#define UI_LOCK_DATE_Y       72

// ---------- Compose screen ----------
#define UI_COMPOSE_RECIPIENT_Y   60
#define UI_COMPOSE_RECIPIENT_H   55
#define UI_COMPOSE_LIST_Y        120
#define UI_COMPOSE_LIST_H        115
#define UI_COMPOSE_EDITOR_Y      120
#define UI_COMPOSE_EDITOR_H      45
#define UI_COMPOSE_TA_Y          125
#define UI_COMPOSE_TA_H          70
#define UI_COMPOSE_SEND_BOTTOM   -5
#define UI_COMPOSE_SHOW_KB       0

// ---------- Contact detail screen ----------
#define UI_CONTACT_HERO_Y    60
#define UI_CONTACT_HERO_H    65
#define UI_CONTACT_BADGE_SZ  40
#define UI_CONTACT_NAME_X    52
#define UI_CONTACT_TYPE_X    52
#define UI_CONTACT_TYPE_Y    28
#define UI_CONTACT_DETAIL_Y  130
#define UI_CONTACT_DETAIL_H  100

// ---------- Ping screen ----------
#define UI_PING_BTN_ROW_Y    160
#define UI_PING_BTN_ROW_H    45
