#include "ui_port.h"
#include "../board.h"
#include <epdiy.h>

namespace ui::port {

static int refresh_mode = UI_REFRESH_MODE_NORMAL;
static volatile bool touch_enabled = true;
static volatile bool flush_enabled = true;
static uint8_t* decodebuffer = NULL;

static inline void checkError(enum EpdDrawError err) {
    if (err != EPD_DRAW_SUCCESS) {
        Serial.printf("EPD draw error: %X\n", err);
    }
}

// v9 flush: px_map is RGB565 (2 bytes per pixel).
// We convert to RGB332 (1 byte per pixel) for epdiy — identical to v8 factory approach.
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (flush_enabled) {
        // epdiy uses 4-bit grayscale: 2 pixels per byte (high nibble + low nibble).
        // decode buffer size = w * h / 2 bytes.
        // Convert RGB565 → 4-bit grayscale pairs.
        int32_t w = lv_area_get_width(area);
        int32_t h = lv_area_get_height(area);
        uint16_t *src = (uint16_t *)px_map;

        for (int32_t i = 0; i < (w * h); i += 2) {
            // Pixel 1 → high nibble
            uint16_t px1 = src[i];
            uint8_t r1 = ((px1 >> 11) & 0x1F) << 3;
            uint8_t g1 = ((px1 >> 5) & 0x3F) << 2;
            uint8_t b1 = (px1 & 0x1F) << 3;
            uint8_t gray1 = (r1 * 66 + g1 * 129 + b1 * 25 + 128) >> 8;

            // Pixel 2 → low nibble
            uint16_t px2 = src[i + 1];
            uint8_t r2 = ((px2 >> 11) & 0x1F) << 3;
            uint8_t g2 = ((px2 >> 5) & 0x3F) << 2;
            uint8_t b2 = (px2 & 0x1F) << 3;
            uint8_t gray2 = (r2 * 66 + g2 * 129 + b2 * 25 + 128) >> 8;

            decodebuffer[i / 2] = (gray1 & 0xF0) | (gray2 >> 4);
        }
    }

    EpdRect rener_area = {
        .x = 0, .y = 0,
        .width = epd_rotated_display_width(),
        .height = epd_rotated_display_height(),
    };

    if (refresh_mode == UI_REFRESH_MODE_FAST) {
        epd_draw_rotated_image(rener_area, decodebuffer, epd_hl_get_framebuffer(&board::hl));
        epd_poweron();
        checkError(epd_hl_update_area(&board::hl, MODE_DU, epd_ambient_temperature(), rener_area));
        epd_poweroff();
    } else if (refresh_mode == UI_REFRESH_MODE_NORMAL) {
        epd_draw_rotated_image(rener_area, decodebuffer, epd_hl_get_framebuffer(&board::hl));
        epd_poweron();
        checkError(epd_hl_update_screen(&board::hl, MODE_GL16, epd_ambient_temperature()));
        epd_poweroff();
    } else if (refresh_mode == UI_REFRESH_MODE_NEAT) {
        epd_hl_set_all_white(&board::hl);
        epd_poweron();
        checkError(epd_hl_update_screen(&board::hl, MODE_GC16, epd_ambient_temperature()));
        epd_poweroff();
        epd_draw_rotated_image(rener_area, decodebuffer, epd_hl_get_framebuffer(&board::hl));
        epd_poweron();
        checkError(epd_hl_update_screen(&board::hl, MODE_GC16, epd_ambient_temperature()));
        epd_poweroff();
    }

    lv_display_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
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

static uint32_t tick_cb(void) {
    return millis();
}

void init() {
    lv_init();
    lv_tick_set_cb(tick_cb);

    int disp_w = epd_rotated_display_width();
    int disp_h = epd_rotated_display_height();
    size_t pixel_count = disp_w * disp_h;

    lv_display_t *disp = lv_display_create(disp_w, disp_h);
    lv_display_set_flush_cb(disp, disp_flush_cb);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

    // Same buffer layout as v8 factory:
    // LVGL draw buffer: pixel_count * 2 bytes (RGB565)
    // epdiy decode buffer: pixel_count / 2 bytes (RGB332, same as v8)
    size_t buf_size = pixel_count * sizeof(uint16_t);
    void *buf1 = ps_calloc(1, buf_size);
    void *buf2 = ps_calloc(1, buf_size);
    decodebuffer = (uint8_t *)ps_calloc(1, pixel_count / 2);  // 4-bit grayscale: 2 pixels per byte
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_FULL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    lv_indev_set_display(indev, disp);

    lv_theme_t *th = lv_theme_default_init(
        disp,
        lv_color_hex(0x000000),
        lv_color_hex(0x888888),
        false,
        &lv_font_montserrat_14
    );
    lv_display_set_theme(disp, th);
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
    int t = 12;
    epd_poweron();
    for (int i = 0; i < 10; i++) epd_push_pixels(epd_full_screen(), t, 0);
    for (int i = 0; i < 10; i++) epd_push_pixels(epd_full_screen(), t, 1);
    for (int i = 0; i < 2; i++)  epd_push_pixels(epd_full_screen(), t, 2);
    epd_poweroff();
}

void touch_enable()  { touch_enabled = true; }
void touch_disable() { touch_enabled = false; }

// Backlight: 0=Auto, 1=Off, 2=Low, 3=Mid, 4=High
static int backlight_level = 0;
static const char* bl_names[] = {"Auto", "Off", "Low", "Mid", "High"};
static const int bl_pwm[] = {0, 0, 50, 100, 230};

void set_backlight(int level) {
    if (level < 0) level = 0;
    if (level > 4) level = 4;
    backlight_level = level;
    // For Auto (0) and Off (1), turn off immediately. For fixed levels, set PWM.
    if (level <= 1) {
        analogWrite(BOARD_BL_EN, 0);
    } else {
        analogWrite(BOARD_BL_EN, bl_pwm[level]);
    }
}

bool is_backlight_auto() { return backlight_level == 0; }

int get_backlight() { return backlight_level; }
const char* get_backlight_name() { return bl_names[backlight_level]; }

} // namespace ui::port
