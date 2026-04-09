#include "ui_port.h"
#include "../board.h"
#include <epdiy.h>

namespace ui::port {

// ---------- Static state ----------

static int refresh_mode = UI_REFRESH_MODE_NORMAL;
static volatile bool touch_enabled = true;
static volatile bool flush_enabled = true;
static uint8_t* decodebuffer = NULL;

#define DISP_BUF_SIZE (epd_rotated_display_width() * epd_rotated_display_height())

// ---------- Display flush callback ----------

static inline void checkError(enum EpdDrawError err) {
    if (err != EPD_DRAW_SUCCESS) {
        Serial.printf("EPD draw error: %X\n", err);
    }
}

// Exact copy of factory disp_flush — proven to work on this hardware.
static void disp_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    if (flush_enabled) {
        uint16_t w = lv_area_get_width(area) / 2;
        uint16_t h = lv_area_get_height(area);
        lv_color32_t *t32 = (lv_color32_t *)color_p;

        for (int i = 0; i < (w * h); i++) {
            lv_color8_t ret;
            LV_COLOR_SET_R8(ret, LV_COLOR_GET_R(*t32) >> 5);
            LV_COLOR_SET_G8(ret, LV_COLOR_GET_G(*t32) >> 5);
            LV_COLOR_SET_B8(ret, LV_COLOR_GET_B(*t32) >> 6);
            decodebuffer[i] = ret.full;
            t32++;
        }
    }

    EpdRect rener_area = {
        .x = 0,
        .y = 0,
        .width = epd_rotated_display_width(),
        .height = epd_rotated_display_height(),
    };

    if (refresh_mode == UI_REFRESH_MODE_FAST)
    {
        epd_draw_rotated_image(rener_area, decodebuffer, epd_hl_get_framebuffer(&board::hl));
        epd_poweron();
        checkError(epd_hl_update_area(&board::hl, MODE_DU, epd_ambient_temperature(), rener_area));
        epd_poweroff();
    }
    else if (refresh_mode == UI_REFRESH_MODE_NORMAL)
    {
        epd_draw_rotated_image(rener_area, decodebuffer, epd_hl_get_framebuffer(&board::hl));
        epd_poweron();
        checkError(epd_hl_update_screen(&board::hl, MODE_GL16, epd_ambient_temperature()));
        epd_poweroff();
    }
    else if (refresh_mode == UI_REFRESH_MODE_NEAT)
    {
        // Full white first, then draw
        epd_hl_set_all_white(&board::hl);
        epd_poweron();
        checkError(epd_hl_update_screen(&board::hl, MODE_GC16, epd_ambient_temperature()));
        epd_poweroff();

        epd_draw_rotated_image(rener_area, decodebuffer, epd_hl_get_framebuffer(&board::hl));
        epd_poweron();
        checkError(epd_hl_update_screen(&board::hl, MODE_GC16, epd_ambient_temperature()));
        epd_poweroff();
    }

    lv_disp_flush_ready(disp);
}

// ---------- Touch input callback ----------

static void touch_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    static int16_t x = 0, y = 0;

    if (board::touch.isPressed() && touch_enabled) {
        if (board::touch.getPoint(&x, &y, 1)) {
            data->state = LV_INDEV_STATE_PRESSED;
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->point.x = x;
    data->point.y = y;
}

// ---------- Public API ----------

void init() {
    lv_init();

    static lv_disp_draw_buf_t draw_buf;
    lv_color_t* buf1 = (lv_color_t*)ps_calloc(sizeof(lv_color_t), DISP_BUF_SIZE);
    lv_color_t* buf2 = (lv_color_t*)ps_calloc(sizeof(lv_color_t), DISP_BUF_SIZE);
    decodebuffer = (uint8_t*)ps_calloc(sizeof(uint8_t), DISP_BUF_SIZE / 2);
    memset(decodebuffer, 0xFF, DISP_BUF_SIZE / 2); // white
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = epd_rotated_display_width();
    disp_drv.ver_res = epd_rotated_display_height();
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    lv_indev_drv_register(&indev_drv);

    // Apply light theme — white backgrounds, black text, no color accents.
    // This ensures all lv_obj_create() children default to e-paper-friendly colors.
    lv_theme_t *th = lv_theme_default_init(
        lv_disp_get_default(),
        lv_color_hex(0x000000),   // primary color: black
        lv_color_hex(0x888888),   // secondary color: gray
        false,                     // light mode (not dark)
        &lv_font_montserrat_14
    );
    lv_disp_set_theme(lv_disp_get_default(), th);
}

void set_refresh_mode(int mode) { refresh_mode = mode; }
int  get_refresh_mode() { return refresh_mode; }

void full_refresh() {
    epd_hl_set_all_white(&board::hl);
    epd_poweron();
    checkError(epd_hl_update_screen(&board::hl, MODE_GL16, epd_ambient_temperature()));
    epd_poweroff();
}

void full_clean() {
    int refresh_timer = 12;
    epd_poweron();
    for (int i = 0; i < 10; i++) epd_push_pixels(epd_full_screen(), refresh_timer, 0);
    for (int i = 0; i < 10; i++) epd_push_pixels(epd_full_screen(), refresh_timer, 1);
    for (int i = 0; i < 2; i++)  epd_push_pixels(epd_full_screen(), refresh_timer, 2);
    epd_poweroff();
}

void touch_enable()  { touch_enabled = true; }
void touch_disable() { touch_enabled = false; }

// Backlight — 4 levels matching factory firmware
static int backlight_level = 0;
static const char* bl_names[] = {"Off", "Low", "Mid", "High"};
static const int bl_pwm[] = {0, 50, 100, 230};

void set_backlight(int level) {
    if (level < 0) level = 0;
    if (level > 3) level = 3;
    backlight_level = level;
    analogWrite(BOARD_BL_EN, bl_pwm[level]);
}

int get_backlight() { return backlight_level; }

const char* get_backlight_name() { return bl_names[backlight_level]; }

} // namespace ui::port
