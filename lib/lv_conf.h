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

/** Color depth: 8 — matches LV_COLOR_FORMAT_L8 for e-ink grayscale */
#define LV_COLOR_DEPTH 8

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

#define LV_DEF_REFR_PERIOD 100    /**< [ms] — e-ink doesn't need fast refresh, saves CPU */

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

#define LV_USE_ASSERT_NULL          0
#define LV_USE_ASSERT_MALLOC        0
#define LV_USE_ASSERT_OBJ           0

/** Draw buffer count (1 or 2). 2 allows double buffering. */
#define LV_DRAW_BUF_ALIGN 4

/*====================
   FONT USAGE
 *====================*/

#define LV_FONT_MONTSERRAT_14 1   /* Default font (required by LVGL internals) */
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_28 0

/** Default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
   THEME USAGE
 *====================*/

#define LV_USE_THEME_DEFAULT 0      /* disabled — simple theme is lighter weight */
#define LV_USE_THEME_SIMPLE 1

/*====================
   LAYOUTS
 *====================*/

#define LV_USE_FLEX 1
#define LV_USE_GRID 0

/*====================
   WIDGETS
 *====================*/

#define LV_USE_ANIMIMG    0
#define LV_USE_ARC        0
#define LV_USE_BAR        0
#define LV_USE_BUTTON     1
#define LV_USE_BUTTONMATRIX 1   /* required by keyboard */
#define LV_USE_CALENDAR   0
#define LV_USE_CANVAS     0
#define LV_USE_CHART      0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMAGE      1
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD   1
#define LV_USE_LABEL      1
#define LV_USE_LED        0
#define LV_USE_LINE       1
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_MSGBOX     0
#define LV_USE_ROLLER     0
#define LV_USE_SCALE      0
#define LV_USE_SLIDER     0
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    0
#define LV_USE_SWITCH     0
#define LV_USE_TEXTAREA   1
#define LV_USE_TABLE      0
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/*====================
   DRAWING / RENDERING
 *====================*/

/** Enable complex drawing */
#define LV_USE_DRAW_SW_COMPLEX 1

/** Disable vector graphics engines — not needed for e-ink UI */
#define LV_USE_VECTOR_GRAPHIC 0
#define LV_USE_THORVG 0
#define LV_USE_DRAW_VG_LITE 0

/** Disable font engines we don't use (we use pre-compiled .c fonts) */
#define LV_USE_FREETYPE 0
#define LV_USE_TINY_TTF 0

/** Disable BiDi and Arabic shaping — saves RAM and CPU */
#define LV_USE_BIDI 0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/** Disable logging in release builds */
#define LV_USE_LOG 0

/** Image decoders — only need built-in */
#define LV_USE_LIBPNG 0
#define LV_USE_LIBJPEG_TURBO 0
#define LV_USE_BMP 0
#define LV_USE_GIF 0
#define LV_USE_QRCODE 0
#define LV_USE_BARCODE 0
#define LV_USE_RLE 0
#define LV_USE_LZ4 0

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
