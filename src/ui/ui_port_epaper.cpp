#ifdef BOARD_EPAPER

#include "ui_port.h"
#include "../board.h"
#include "../model.h"
#include "../nvs_param.h"
#include <epdiy.h>
#include <Wire.h>

namespace ui::port {

static int refresh_mode = UI_REFRESH_MODE_NORMAL;
static volatile bool touch_enabled = true;
static int backlight_mode = 0;
static const char* mode_names_bl[] = {"Auto", "On", "Off"};
static int brightness = 1;  // default Mid
static const char* bright_names[] = {"Low", "Mid", "High"};
static const int bright_pwm[] = {50, 100, 230};
static const int32_t CHANGE_DETECT_MAX_PIXELS = 4096;
static uint8_t gray_to_lo[256];
static uint8_t gray_to_hi[256];
static bool gray_tables_ready = false;
static uint8_t* packed_prev_frame = NULL;  // packed 4-bit rotated framebuffer shadow
static int32_t* rotated_row_offsets = NULL;
struct FullRefreshStamp {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    bool valid;
};
static FullRefreshStamp last_full_refresh = {};

static inline void checkError(enum EpdDrawError err) {
    if (err != EPD_DRAW_SUCCESS) {
        Serial.printf("EPD draw error: %X\n", err);
    }
}

static void init_gray_tables() {
    if (gray_tables_ready) {
        return;
    }
    for (int i = 0; i < 256; i++) {
        gray_to_lo[i] = (uint8_t)(i >> 4);
        gray_to_hi[i] = (uint8_t)(i & 0xF0);
    }
    gray_tables_ready = true;
}

static void init_rotated_row_offsets(int32_t phys_h, int32_t half_w) {
    if (rotated_row_offsets) {
        return;
    }
    rotated_row_offsets = (int32_t *)ps_malloc(sizeof(int32_t) * phys_h);
    if (!rotated_row_offsets) {
        return;
    }
    for (int32_t rx = 0; rx < phys_h; rx++) {
        rotated_row_offsets[rx] = (phys_h - 1 - rx) * half_w;
    }
}

static inline int32_t rotated_offset(int32_t rx, int32_t phys_h, int32_t half_w) {
    if (rotated_row_offsets) {
        return rotated_row_offsets[rx];
    }
    return (phys_h - 1 - rx) * half_w;
}

static FullRefreshStamp current_refresh_stamp() {
    return {
        .year = model::clock.year,
        .month = model::clock.month,
        .day = model::clock.day,
        .hour = model::clock.hour,
        .valid = true,
    };
}

static bool should_do_hourly_full_refresh() {
    FullRefreshStamp now = current_refresh_stamp();
    if (!last_full_refresh.valid) {
        last_full_refresh = now;
        return false;
    }
    return now.year != last_full_refresh.year ||
           now.month != last_full_refresh.month ||
           now.day != last_full_refresh.day ||
           now.hour != last_full_refresh.hour;
}

static void note_full_refresh_done() {
    last_full_refresh = current_refresh_stamp();
}

static inline void pack_single_row(uint8_t *fb, const uint8_t *prev_fb, const uint8_t *src_row,
                                   int32_t phys_h, int32_t half_w, int32_t ry, int32_t x1, int32_t x2,
                                   bool track_dirty_area, bool *changed) {
    int32_t px_x = ry;
    int32_t byte_x = px_x / 2;
    bool is_odd = px_x & 1;

    if (is_odd) {
        for (int32_t rx = x1; rx <= x2; rx++) {
            int32_t offset = rotated_offset(rx, phys_h, half_w) + byte_x;
            uint8_t packed = (uint8_t)((prev_fb[offset] & 0x0F) | gray_to_hi[src_row[rx]]);
            if (track_dirty_area && packed != prev_fb[offset]) {
                *changed = true;
            }
            fb[offset] = packed;
        }
    } else {
        for (int32_t rx = x1; rx <= x2; rx++) {
            int32_t offset = rotated_offset(rx, phys_h, half_w) + byte_x;
            uint8_t packed = (uint8_t)((prev_fb[offset] & 0xF0) | gray_to_lo[src_row[rx]]);
            if (track_dirty_area && packed != prev_fb[offset]) {
                *changed = true;
            }
            fb[offset] = packed;
        }
    }
}

static inline void pack_row_pair(uint8_t *fb, const uint8_t *prev_fb, const uint8_t *src_even,
                                 const uint8_t *src_odd, int32_t phys_h, int32_t half_w, int32_t even_ry,
                                 int32_t x1, int32_t x2,
                                 bool track_dirty_area, bool *changed) {
    int32_t byte_x = even_ry / 2;
    for (int32_t rx = x1; rx <= x2; rx++) {
        int32_t offset = rotated_offset(rx, phys_h, half_w) + byte_x;
        uint8_t packed = (uint8_t)(gray_to_hi[src_odd[rx]] | gray_to_lo[src_even[rx]]);
        if (track_dirty_area && packed != prev_fb[offset]) {
            *changed = true;
        }
        fb[offset] = packed;
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
    init_gray_tables();

    int32_t area_w = lv_area_get_width(area);
    int32_t area_h = lv_area_get_height(area);
    int32_t area_px = area_w * area_h;

    uint8_t *fb = epd_hl_get_framebuffer(&board::hl);
    int32_t phys_w = epd_width();
    int32_t phys_h = epd_height();
    int32_t disp_w = epd_rotated_display_width();

    // Pack L8 (8-bit gray) → 4-bit nibbles in epdiy framebuffer with inline rotation.
    // Two adjacent source rows map to the same destination byte, so pack row pairs together
    // to avoid the read-modify-write on every pixel.
    int32_t half_w = phys_w / 2;
    init_rotated_row_offsets(phys_h, half_w);
    if (!packed_prev_frame) {
        packed_prev_frame = (uint8_t *)ps_malloc((size_t)half_w * phys_h);
        if (packed_prev_frame) {
            memcpy(packed_prev_frame, fb, (size_t)half_w * phys_h);
        }
    }

    bool changed = false;
    bool track_dirty_area = packed_prev_frame && rotated_row_offsets && area_px <= CHANGE_DETECT_MAX_PIXELS;
    int32_t ry = area->y1;
    if (ry & 1) {
        const uint8_t *src_row = &px_map[ry * disp_w];
        pack_single_row(fb, packed_prev_frame ? packed_prev_frame : fb, src_row, phys_h, half_w, ry,
                        area->x1, area->x2, track_dirty_area, &changed);
        ry++;
    }
    for (; ry < area->y2; ry += 2) {
        const uint8_t *src_even = &px_map[ry * disp_w];
        const uint8_t *src_odd = &px_map[(ry + 1) * disp_w];
        pack_row_pair(fb, packed_prev_frame ? packed_prev_frame : fb, src_even, src_odd, phys_h, half_w, ry,
                      area->x1, area->x2, track_dirty_area, &changed);
    }
    if (ry == area->y2) {
        const uint8_t *src_row = &px_map[ry * disp_w];
        pack_single_row(fb, packed_prev_frame ? packed_prev_frame : fb, src_row, phys_h, half_w, ry,
                        area->x1, area->x2, track_dirty_area, &changed);
    }

    if (packed_prev_frame) {
        int32_t packed_y1 = phys_h - 1 - area->x2;
        int32_t packed_y2 = phys_h - 1 - area->x1;
        int32_t packed_x1 = area->y1 / 2;
        int32_t packed_x2 = area->y2 / 2;
        int32_t packed_len = packed_x2 - packed_x1 + 1;
        for (int32_t py = packed_y1; py <= packed_y2; py++) {
            int32_t offset = py * half_w + packed_x1;
            memcpy(&packed_prev_frame[offset], &fb[offset], packed_len);
        }
    }

    if (track_dirty_area && !changed) {
        lv_display_flush_ready(disp);
        return;
    }

    // Partial area rect in rotated coordinates — epdiy handles rotation internally
    EpdRect update_rect = {
        .x = (int)area->x1,
        .y = (int)area->y1,
        .width = (int)lv_area_get_width(area),
        .height = (int)lv_area_get_height(area),
    };

    bool do_hourly_full_refresh = should_do_hourly_full_refresh();

    if (do_hourly_full_refresh) {
        epd_poweron();
        checkError(epd_hl_update_screen(&board::hl, MODE_GC16, epd_ambient_temperature()));
        epd_poweroff();
        note_full_refresh_done();
    } else if (refresh_mode == UI_REFRESH_MODE_FAST) {
        epd_poweron();
        checkError(epd_hl_update_area(&board::hl, MODE_DU, epd_ambient_temperature(), update_rect));
        epd_poweroff();
    } else if (refresh_mode == UI_REFRESH_MODE_NORMAL) {
        epd_poweron();
        checkError(epd_hl_update_area(&board::hl, MODE_GL16, epd_ambient_temperature(), update_rect));
        epd_poweroff();
    }

    lv_display_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    static int16_t x = 0, y = 0;
    static lv_indev_state_t last_state = LV_INDEV_STATE_RELEASED;
    static uint32_t next_poll = 0;

    // Throttle I2C touch reads to ~20Hz — faster than e-paper refresh, but still light on the bus
    uint32_t now = millis();
    if (now >= next_poll) {
        next_poll = now + 50;
        if (board::touch.isPressed() && touch_enabled) {
            if (board::touch.getPoint(&x, &y, 1)) {
                last_state = LV_INDEV_STATE_PRESSED;
            }
        } else {
            last_state = LV_INDEV_STATE_RELEASED;
        }
    }
    data->state = last_state;
    data->point.x = x;
    data->point.y = y;
}

static uint32_t tick_cb(void) {
    return millis();
}

void init() {
    lv_init();
    lv_tick_set_cb(tick_cb);

    int mode = nvs_param_get_u8(NVS_ID_REFRESH_MODE);
    if (mode < UI_REFRESH_MODE_NORMAL || mode > UI_REFRESH_MODE_FAST) {
        mode = UI_REFRESH_MODE_NORMAL;
    }
    refresh_mode = mode;

    int stored_brightness = nvs_param_get_u8(NVS_ID_BRIGHTNESS);
    if (stored_brightness < 0 || stored_brightness > 2) {
        stored_brightness = 1;
    }
    brightness = stored_brightness;

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
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_DIRECT);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    lv_indev_set_display(indev, disp);

    // Disable scroll momentum/throw — instant stop on e-ink (100 = max deceleration)
    lv_indev_set_scroll_throw(indev, 100);

}

void set_refresh_mode(int mode) { refresh_mode = mode; }
int  get_refresh_mode() { return refresh_mode; }

void full_refresh() {
    epd_hl_set_all_white(&board::hl);
    epd_poweron();
    checkError(epd_hl_update_screen(&board::hl, MODE_GL16, epd_ambient_temperature()));
    epd_poweroff();
    note_full_refresh_done();
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

#endif // BOARD_EPAPER
