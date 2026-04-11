#include "ui_port.h"
#include "../board.h"
#include <epdiy.h>
#include <Wire.h>

namespace ui::port {

static int refresh_mode = UI_REFRESH_MODE_NORMAL;
static volatile bool touch_enabled = true;
static uint8_t* prev_frame = NULL;  // previous frame for change detection

static inline void checkError(enum EpdDrawError err) {
    if (err != EPD_DRAW_SUCCESS) {
        Serial.printf("EPD draw error: %X\n", err);
    }
}

// RENDER_MODE_DIRECT flush: px_map is the persistent full-screen L8 buffer,
// area identifies only the dirty region. L8 format means each pixel is already
// 8-bit grayscale — we just pack into 4-bit nibbles for epdiy.
//
// Rotation inlined for EPD_ROT_INVERTED_PORTRAIT:
//   physical_x = rotated_y
//   physical_y = epd_height() - 1 - rotated_x
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int32_t disp_w = epd_rotated_display_width();

    // Skip e-paper update if the dirty area pixels haven't actually changed
    if (prev_frame) {
        bool changed = false;
        for (int32_t ry = area->y1; ry <= area->y2 && !changed; ry++) {
            int32_t offset = ry * disp_w + area->x1;
            int32_t len = area->x2 - area->x1 + 1;
            if (memcmp(&px_map[offset], &prev_frame[offset], len) != 0) {
                changed = true;
            }
        }
        if (!changed) {
            lv_display_flush_ready(disp);
            return;
        }
        // Update prev_frame with new content for the dirty area
        for (int32_t ry = area->y1; ry <= area->y2; ry++) {
            int32_t offset = ry * disp_w + area->x1;
            int32_t len = area->x2 - area->x1 + 1;
            memcpy(&prev_frame[offset], &px_map[offset], len);
        }
    }

    uint8_t *fb = epd_hl_get_framebuffer(&board::hl);
    int32_t phys_w = epd_width();
    int32_t phys_h = epd_height();

    // Pack L8 (8-bit gray) → 4-bit nibbles in epdiy framebuffer with inline rotation
    // Optimized: for each row ry, px_x is constant (=ry), so precompute nibble mask/shift
    int32_t half_w = phys_w / 2;
    for (int32_t ry = area->y1; ry <= area->y2; ry++) {
        int32_t px_x = ry;
        int32_t byte_x = px_x / 2;
        bool is_odd = px_x & 1;
        const uint8_t *src_row = &px_map[ry * disp_w];

        if (is_odd) {
            for (int32_t rx = area->x1; rx <= area->x2; rx++) {
                uint8_t gray = src_row[rx];
                int32_t px_y = phys_h - 1 - rx;
                uint8_t *bp = &fb[px_y * half_w + byte_x];
                *bp = (*bp & 0x0F) | (gray & 0xF0);
            }
        } else {
            for (int32_t rx = area->x1; rx <= area->x2; rx++) {
                uint8_t gray = src_row[rx];
                int32_t px_y = phys_h - 1 - rx;
                uint8_t *bp = &fb[px_y * half_w + byte_x];
                *bp = (*bp & 0xF0) | (gray >> 4);
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
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_L8);

    // DIRECT mode with L8 (8-bit luminance): LVGL renders grayscale directly.
    // Double buffered in PSRAM — CPU renders to buf2 while buf1 flushes to e-paper.
    size_t buf_size = pixel_count;  // 1 byte per pixel for L8
    void *buf1 = ps_calloc(1, buf_size);
    void *buf2 = ps_calloc(1, buf_size);
    prev_frame = (uint8_t *)ps_calloc(1, buf_size);  // for change detection
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_DIRECT);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    lv_indev_set_display(indev, disp);

    // Disable scroll momentum/throw — instant stop on e-ink (100 = max deceleration)
    lv_indev_set_scroll_throw(indev, 100);

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

// Backlight mode: 0=Auto, 1=On, 2=Off
static int backlight_mode = 0;
static const char* mode_names_bl[] = {"Auto", "On", "Off"};

// Brightness: 0=Low, 1=Mid, 2=High
static int brightness = 1;  // default Mid
static const char* bright_names[] = {"Low", "Mid", "High"};
static const int bright_pwm[] = {50, 100, 230};

void set_backlight(int mode) {
    if (mode < 0) mode = 0;
    if (mode > 2) mode = 2;
    backlight_mode = mode;
    if (mode == 1) {
        analogWrite(BOARD_BL_EN, bright_pwm[brightness]);  // On at current brightness
    } else {
        analogWrite(BOARD_BL_EN, 0);  // Off or Auto (auto handled by ui_task)
    }
}

void set_brightness(int level) {
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    brightness = level;
}

void apply_backlight() {
    analogWrite(BOARD_BL_EN, bright_pwm[brightness]);
}

bool is_backlight_auto() { return backlight_mode == 0; }
int get_backlight() { return backlight_mode; }
const char* get_backlight_name() { return mode_names_bl[backlight_mode]; }
int get_brightness() { return brightness; }
const char* get_brightness_name() { return bright_names[brightness]; }
int get_brightness_pwm() { return bright_pwm[brightness]; }

} // namespace ui::port
