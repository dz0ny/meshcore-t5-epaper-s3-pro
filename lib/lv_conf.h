/**
 * @file lv_conf.h
 * Configuration file for LVGL v9.5 — t-paper e-ink device
 */

#if 1 /*Set it to "1" to enable content*/

#ifndef LV_CONF_H
#define LV_CONF_H

/*====================
   COLOR SETTINGS
 *====================*/

/** Color depth: 16 (RGB565) — same as v8, epdiy handles conversion */
#define LV_COLOR_DEPTH 16

/*=========================
   STDLIB WRAPPER SETTINGS
 *=========================*/

/* Use standard C stdlib — ESP32 with PSRAM will use PSRAM for large allocations */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

/*====================
   HAL SETTINGS
 *====================*/

#define LV_DEF_REFR_PERIOD 33     /**< [ms] — responsive touch */

/* Tick provided via lv_tick_set_cb() in ui_port.cpp */

/*====================
   OPERATING SYSTEM
 *====================*/

#define LV_USE_OS   LV_OS_NONE

/*====================
   FEATURE CONFIG
 *====================*/

/** Disable scroll animations for e-paper */
#define LV_ANIM_DEF_TIME 0

#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_OBJ           0

/** Draw buffer count (1 or 2). 2 allows double buffering. */
#define LV_DRAW_BUF_ALIGN 4

/*====================
   FONT USAGE
 *====================*/

#define LV_FONT_MONTSERRAT_14 1   /* Default font */
#define LV_FONT_MONTSERRAT_18 1   /* Message sender names */
#define LV_FONT_MONTSERRAT_20 1   /* Menu arrows with symbols */
#define LV_FONT_MONTSERRAT_22 1   /* Message text + contacts — has full Unicode */
#define LV_FONT_MONTSERRAT_24 1   /* Statusbar — readable on e-ink */
#define LV_FONT_MONTSERRAT_28 1   /* Message text — Unicode + readable */

/** Default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
   THEME USAGE
 *====================*/

#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    #define LV_THEME_DEFAULT_DARK 0     /* light mode for e-paper */
    #define LV_THEME_DEFAULT_GROW 0
    #define LV_THEME_DEFAULT_TRANSITION_TIME 0  /* no animations on e-paper */
#endif

#define LV_USE_THEME_SIMPLE 1

/*====================
   LAYOUTS
 *====================*/

#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/*====================
   WIDGETS
 *====================*/

#define LV_USE_ANIMIMG    0
#define LV_USE_ARC        0
#define LV_USE_BAR        1
#define LV_USE_BUTTON     1
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR   0
#define LV_USE_CANVAS     0
#define LV_USE_CHART      0
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMAGE      1
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD   0
#define LV_USE_LABEL      1
#define LV_USE_LED        0
#define LV_USE_LINE       1
#define LV_USE_LIST       1
#define LV_USE_MENU       0
#define LV_USE_MSGBOX     0
#define LV_USE_ROLLER     0
#define LV_USE_SCALE      0
#define LV_USE_SLIDER     1
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    0
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   0
#define LV_USE_TABLE      0
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/*====================
   OTHERS
 *====================*/

#define LV_USE_SNAPSHOT   0
#define LV_USE_SYSMON     0
#define LV_USE_PROFILER   0
#define LV_USE_MONKEY     0
#define LV_USE_GRIDNAV    0
#define LV_USE_FRAGMENT   0
#define LV_USE_OBSERVER   0
#define LV_USE_IME_PINYIN 0
#define LV_USE_FILE_EXPLORER 0

#define LV_BUILD_EXAMPLES 0

#endif /*LV_CONF_H*/

#endif /*Enable content*/
