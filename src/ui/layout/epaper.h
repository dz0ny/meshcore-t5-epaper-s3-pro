#pragma once
// UI layout constants for LilyGo T5 ePaper S3 Pro (540x960)

#include "lvgl.h"

// ---------- Fonts ----------
#define UI_FONT_SMALL       &lv_font_noto_24
#define UI_FONT_BODY        &lv_font_noto_28
#define UI_FONT_TITLE       &lv_font_montserrat_bold_30
#define UI_FONT_CLOCK_SM    &lv_font_montserrat_bold_80
#define UI_FONT_CLOCK_LG    &lv_font_montserrat_bold_120

// ---------- Shared components ----------

// Statusbar
#define UI_STATUSBAR_HEIGHT  50
#define UI_STATUSBAR_Y       5
#define UI_STATUSBAR_PAD     5
#define UI_STATUSBAR_COL_PAD 8

// Back button
#define UI_BACK_BTN_Y        50
#define UI_BACK_BTN_HEIGHT   70
#define UI_BACK_BTN_PAD_V    10
#define UI_BACK_BTN_COL_PAD  8

// Menu / list items
#define UI_MENU_ITEM_HEIGHT  85
#define UI_MENU_ITEM_PAD     15
#define UI_MENU_ITEM_INSET   15

// Text button
#define UI_TEXT_BTN_HEIGHT   80
#define UI_TEXT_BTN_PAD      15
#define UI_TEXT_BTN_RADIUS   12
#define UI_TEXT_BTN_BORDER   3

// Scroll list
#define UI_SCROLL_LIST_H     85
#define UI_SCROLL_LIST_Y    -10

// Extended click areas
#define UI_EXT_CLICK_BACK    35
#define UI_EXT_CLICK_ACTION  25
#define UI_EXT_CLICK_LIST    25

// Action button (inline with back button)
#define UI_ACTION_BTN_H      56
#define UI_ACTION_BTN_PAD_H  18
#define UI_ACTION_BTN_PAD_V  8
#define UI_ACTION_BTN_RADIUS 12
#define UI_ACTION_BTN_BORDER 3

// ---------- Home screen ----------
#define UI_HOME_NODE_Y       55
#define UI_HOME_CLOCK_Y      100
#define UI_HOME_DATE_Y       225
#define UI_HOME_MENU_Y       340
#define UI_HOME_MENU_SCROLL  0  // no scroll needed on 960px

// ---------- Lock screen ----------
#define UI_LOCK_NODE_Y       55
#define UI_LOCK_CLOCK_Y      100
#define UI_LOCK_DATE_Y       210

// ---------- Compose screen ----------
#define UI_COMPOSE_RECIPIENT_Y   130
#define UI_COMPOSE_RECIPIENT_H   110
#define UI_COMPOSE_LIST_Y        255
#define UI_COMPOSE_LIST_H        630
#define UI_COMPOSE_EDITOR_Y      255
#define UI_COMPOSE_EDITOR_H      260
#define UI_COMPOSE_TA_Y          305
#define UI_COMPOSE_TA_H          180
#define UI_COMPOSE_SEND_BOTTOM   -335
#define UI_COMPOSE_SHOW_KB       1

// ---------- Contact detail screen ----------
#define UI_CONTACT_HERO_Y    130
#define UI_CONTACT_HERO_H    150
#define UI_CONTACT_BADGE_SZ  84
#define UI_CONTACT_NAME_X    108
#define UI_CONTACT_TYPE_X    108
#define UI_CONTACT_TYPE_Y    58
#define UI_CONTACT_DETAIL_Y  295
#define UI_CONTACT_DETAIL_H  230

// ---------- Ping screen ----------
#define UI_PING_BTN_ROW_Y    360
#define UI_PING_BTN_ROW_H    90
