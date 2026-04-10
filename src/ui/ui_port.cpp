#include "ui_port.h"
#include "../board.h"
#include <epdiy.h>

namespace ui::port {

static int refresh_mode = UI_REFRESH_MODE_NORMAL;
static volatile bool touch_enabled = true;

static inline void checkError(enum EpdDrawError err) {
    if (err != EPD_DRAW_SUCCESS) {
        Serial.printf("EPD draw error: %X\n", err);
    }
}

// RENDER_MODE_DIRECT flush: px_map is the persistent full-screen buffer,
// area identifies only the dirty region. We convert just those pixels
// and do a partial e-paper update.
//
// Rotation is inlined for EPD_ROT_INVERTED_PORTRAIT (the only rotation used):
//   physical_x = rotated_y
//   physical_y = epd_height() - 1 - rotated_x
// This avoids per-pixel function call overhead of epd_draw_pixel().
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int32_t disp_w = epd_rotated_display_width();
    uint8_t *fb = epd_hl_get_framebuffer(&board::hl);
    uint16_t *src = (uint16_t *)px_map;
    int32_t phys_w = epd_width();  // physical width (960 for landscape panel)
    int32_t phys_h = epd_height(); // physical height (540)

    // Convert dirty area: RGB565 → 4-bit grayscale, inline rotation to physical coords
    for (int32_t ry = area->y1; ry <= area->y2; ry++) {
        for (int32_t rx = area->x1; rx <= area->x2; rx++) {
            // RGB565 → grayscale
            uint16_t px = src[ry * disp_w + rx];
            uint8_t gray = ((((px >> 11) & 0x1F) << 3) * 66 +
                            (((px >> 5) & 0x3F) << 2) * 129 +
                            ((px & 0x1F) << 3) * 25 + 128) >> 8;

            // Inline EPD_ROT_INVERTED_PORTRAIT rotation
            int32_t px_x = ry;
            int32_t px_y = phys_h - 1 - rx;

            // Write 4-bit grayscale to framebuffer (2 pixels per byte)
            uint8_t *buf_ptr = &fb[px_y * phys_w / 2 + px_x / 2];
            if (px_x & 1) {
                *buf_ptr = (*buf_ptr & 0x0F) | (gray & 0xF0);
            } else {
                *buf_ptr = (*buf_ptr & 0xF0) | (gray >> 4);
            }
        }
    }

    // Partial area rect in rotated coordinates — epdiy handles rotation internally
    EpdRect update_rect = {
        .x = (int)area->x1,
        .y = (int)area->y1,
        .width = (int)lv_area_get_width(area),
        .height = (int)lv_area_get_height(area),
    };

    if (refresh_mode == UI_REFRESH_MODE_FAST) {
        epd_poweron();
        checkError(epd_hl_update_area(&board::hl, MODE_DU, epd_ambient_temperature(), update_rect));
        epd_poweroff();
    } else if (refresh_mode == UI_REFRESH_MODE_NORMAL) {
        epd_poweron();
        checkError(epd_hl_update_area(&board::hl, MODE_GL16, epd_ambient_temperature(), update_rect));
        epd_poweroff();
    } else if (refresh_mode == UI_REFRESH_MODE_NEAT) {
        // NEAT: full white clear then full redraw for best quality
        epd_hl_set_all_white(&board::hl);
        epd_poweron();
        checkError(epd_hl_update_screen(&board::hl, MODE_GC16, epd_ambient_temperature()));
        epd_poweroff();
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

    // DIRECT mode: LVGL keeps a persistent framebuffer and only redraws dirty areas.
    // Two full-screen buffers in PSRAM for double-buffered DIRECT rendering.
    // No intermediate decode buffer — we write directly to epdiy's framebuffer.
    size_t buf_size = pixel_count * sizeof(uint16_t);
    void *buf1 = ps_calloc(1, buf_size);
    void *buf2 = ps_calloc(1, buf_size);
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_DIRECT);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    lv_indev_set_display(indev, disp);

    lv_theme_t *th = lv_theme_simple_init(disp);
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
