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
static lv_display_t* epaper_disp = NULL;
static uint8_t gray_to_lo[256];
static uint8_t gray_to_hi[256];
static bool gray_tables_ready = false;
static uint8_t* packed_prev_frame = NULL;  // packed 4-bit rotated framebuffer shadow
static int32_t* rotated_row_offsets = NULL;
static bool cycle_force_full_refresh = false;
static bool cycle_had_updates = false;
static bool cycle_panel_powered = false;
static int cycle_temperature = 0;
struct FullRefreshStamp {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    bool valid;
};
static FullRefreshStamp last_full_refresh = {};
static lv_obj_t *keyboard_screen_root = NULL;
static bool keyboard_focus_dirty = true;
static const size_t MAX_KEYBOARD_FOCUSABLES = 1024;
static lv_obj_t *keyboard_focusables[MAX_KEYBOARD_FOCUSABLES] = {};
static size_t keyboard_focusable_count = 0;

static inline void checkError(enum EpdDrawError err) {
    if (err != EPD_DRAW_SUCCESS) {
        Serial.printf("EPD draw error: %X\n", err);
    }
}

static void sync_packed_prev_frame(uint8_t *fb, int32_t phys_w, int32_t phys_h) {
    if (!packed_prev_frame || !fb) {
        return;
    }
    memcpy(packed_prev_frame, fb, (size_t)(phys_w / 2) * phys_h);
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

static void render_start_cb(lv_event_t *event) {
    (void)event;
    cycle_force_full_refresh = should_do_hourly_full_refresh();
    cycle_had_updates = false;
    if (!cycle_panel_powered) {
        epd_poweron();
        cycle_panel_powered = true;
    }
    cycle_temperature = epd_ambient_temperature();
}

static void render_ready_cb(lv_event_t *event) {
    (void)event;
    if (cycle_force_full_refresh && cycle_had_updates) {
        checkError(epd_hl_update_screen(&board::hl, MODE_GC16, cycle_temperature));
        note_full_refresh_done();
        sync_packed_prev_frame(epd_hl_get_framebuffer(&board::hl), epd_width(), epd_height());
    }
    if (cycle_panel_powered) {
        epd_poweroff();
        cycle_panel_powered = false;
    }
    cycle_force_full_refresh = false;
    cycle_had_updates = false;
}

static void round_invalidate_area_cb(lv_event_t *event) {
    lv_area_t *area = (lv_area_t *)lv_event_get_param(event);
    if (!area || !epaper_disp) {
        return;
    }

    if (area->y1 < 0) {
        area->y1 = 0;
    }
    if (area->y1 & 1) {
        area->y1 -= 1;
    }

    lv_coord_t max_y = lv_display_get_vertical_resolution(epaper_disp) - 1;
    if ((area->y2 & 1) == 0) {
        area->y2 += 1;
    }
    if (area->y2 > max_y) {
        area->y2 = max_y;
    }
    if (area->y1 > area->y2) {
        area->y1 = area->y2;
    }
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

    cycle_had_updates = true;

    // Partial area rect in rotated coordinates — epdiy handles rotation internally
    EpdRect update_rect = {
        .x = (int)area->x1,
        .y = (int)area->y1,
        .width = (int)lv_area_get_width(area),
        .height = (int)lv_area_get_height(area),
    };

    if (!cycle_panel_powered) {
        epd_poweron();
        cycle_panel_powered = true;
        cycle_temperature = epd_ambient_temperature();
    }

    if (!cycle_force_full_refresh && refresh_mode == UI_REFRESH_MODE_FAST) {
        checkError(epd_hl_update_area(&board::hl, MODE_DU, cycle_temperature, update_rect));
    } else if (!cycle_force_full_refresh && refresh_mode == UI_REFRESH_MODE_NORMAL) {
        checkError(epd_hl_update_area(&board::hl, MODE_GL16, cycle_temperature, update_rect));
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

static bool obj_accepts_group_focus(lv_obj_t *obj) {
    return obj && (lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE) || lv_obj_check_type(obj, &lv_textarea_class));
}

static bool obj_is_focus_candidate_on_screen(lv_obj_t *obj, lv_obj_t *screen) {
    if (!obj || !screen || !lv_obj_is_valid(obj) || !lv_obj_is_visible(obj)) return false;
    if (lv_obj_get_screen(obj) != screen) return false;
    return obj_accepts_group_focus(obj);
}

static void compact_keyboard_focus_registry() {
    size_t write = 0;
    for (size_t i = 0; i < keyboard_focusable_count; i++) {
        lv_obj_t *obj = keyboard_focusables[i];
        if (!obj || !lv_obj_is_valid(obj)) continue;

        bool duplicate = false;
        for (size_t j = 0; j < write; j++) {
            if (keyboard_focusables[j] == obj) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        keyboard_focusables[write++] = obj;
    }
    for (size_t i = write; i < keyboard_focusable_count; i++) {
        keyboard_focusables[i] = NULL;
    }
    keyboard_focusable_count = write;
}

static void add_registered_group_targets(lv_group_t *group, lv_obj_t *screen) {
    if (!group || !screen) return;

    for (size_t i = 0; i < keyboard_focusable_count; i++) {
        lv_obj_t *obj = keyboard_focusables[i];
        if (!obj_is_focus_candidate_on_screen(obj, screen)) continue;

        if (lv_obj_get_group(obj) != group) {
            lv_group_add_obj(group, obj);
        }
        lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    }
}

static lv_obj_t *find_first_registered_focus_target(lv_obj_t *screen) {
    if (!screen) return NULL;

    for (size_t i = 0; i < keyboard_focusable_count; i++) {
        lv_obj_t *obj = keyboard_focusables[i];
        if (obj_is_focus_candidate_on_screen(obj, screen)) {
            return obj;
        }
    }
    return NULL;
}

static void ensure_keyboard_focus_target() {
    lv_group_t *group = lv_group_get_default();
    if (!group) return;

    lv_obj_t *screen = lv_screen_active();
    if (keyboard_screen_root != screen) {
        keyboard_screen_root = screen;
        keyboard_focus_dirty = true;
    }

    lv_obj_t *focused = lv_group_get_focused(group);
    if (!keyboard_focus_dirty &&
        (!focused || !lv_obj_is_valid(focused) || !lv_obj_is_visible(focused) ||
         lv_obj_get_screen(focused) != screen || !obj_accepts_group_focus(focused))) {
        keyboard_focus_dirty = true;
    }

    if (!keyboard_focus_dirty) {
        return;
    }

    compact_keyboard_focus_registry();
    lv_group_remove_all_objs(group);

    if (screen) {
        add_registered_group_targets(group, screen);
    }

    lv_obj_t *target = obj_is_focus_candidate_on_screen(focused, screen) ?
                       focused :
                       find_first_registered_focus_target(screen);
    if (target) {
        lv_group_focus_obj(target);
        lv_obj_scroll_to_view_recursive(target, LV_ANIM_OFF);
    }

    keyboard_focus_dirty = false;
}

static void keyboard_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    static uint32_t last_key = 0;
    static uint32_t last_backspace_ms = 0;
    static uint8_t backspace_repeat_count = 0;
    static bool backspace_cleared = false;

    ensure_keyboard_focus_target();

    int c = board::keyboard_read_char();
    if (c > 0) {
        uint32_t now = millis();
        last_key = (uint32_t)c;
        data->state = LV_INDEV_STATE_PRESSED;
        model::touch_activity();

        switch (c) {
            case 0x08:
                if (last_backspace_ms == 0 || (now - last_backspace_ms) > 250) {
                    backspace_repeat_count = 1;
                    backspace_cleared = false;
                } else {
                    backspace_repeat_count++;
                }
                last_backspace_ms = now;

                if (!backspace_cleared && backspace_repeat_count >= 4) {
                    lv_group_t *group = lv_group_get_default();
                    lv_obj_t *focused = group ? lv_group_get_focused(group) : NULL;
                    if (focused && lv_obj_check_type(focused, &lv_textarea_class)) {
                        lv_textarea_set_text(focused, "");
                        backspace_cleared = true;
                    }
                }

                data->key = LV_KEY_BACKSPACE;
                break;
            case 0x0D: data->key = LV_KEY_ENTER; break;
            case 0x1B: data->key = LV_KEY_ESC; break;
            case 0x09: data->key = LV_KEY_NEXT; break;
            case 0xF1: data->key = LV_KEY_DOWN; break;
            case 0xF2: data->key = LV_KEY_UP; break;
            case 0xF3: data->key = LV_KEY_LEFT; break;
            case 0xF4: data->key = LV_KEY_RIGHT; break;
            default:
                last_backspace_ms = 0;
                backspace_repeat_count = 0;
                backspace_cleared = false;
                data->key = (uint32_t)c;
                break;
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = last_key;
    }
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
    epaper_disp = disp;
    lv_display_set_flush_cb(disp, disp_flush_cb);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_L8);
    lv_display_add_event_cb(disp, round_invalidate_area_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    lv_display_add_event_cb(disp, render_start_cb, LV_EVENT_RENDER_START, NULL);
    lv_display_add_event_cb(disp, render_ready_cb, LV_EVENT_REFR_READY, NULL);

    // DIRECT mode with L8 (8-bit luminance): LVGL renders grayscale directly.
    // Single buffered in PSRAM to reduce memory pressure on the S3.
    size_t buf_size = pixel_count;  // 1 byte per pixel for L8
    void *buf1 = ps_calloc(1, buf_size);
    lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_DIRECT);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    lv_indev_set_display(indev, disp);

    // Disable scroll momentum/throw — instant stop on e-ink (100 = max deceleration)
    lv_indev_set_scroll_throw(indev, 100);

    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);
    keyboard_focus_invalidate();

    if (board::peri_status[E_PERI_KEYBOARD]) {
        lv_indev_t *kb_indev = lv_indev_create();
        lv_indev_set_type(kb_indev, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(kb_indev, keyboard_read_cb);
        lv_indev_set_display(kb_indev, disp);
        lv_indev_set_group(kb_indev, g);
    }

}

void set_refresh_mode(int mode) { refresh_mode = mode; }
int  get_refresh_mode() { return refresh_mode; }

void full_refresh() {
    epd_hl_set_all_white(&board::hl);
    epd_poweron();
    checkError(epd_hl_update_screen(&board::hl, MODE_GL16, epd_ambient_temperature()));
    epd_poweroff();
    sync_packed_prev_frame(epd_hl_get_framebuffer(&board::hl), epd_width(), epd_height());
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

void keyboard_focus_invalidate() {
    keyboard_focus_dirty = true;
    keyboard_screen_root = NULL;
}

void keyboard_focus_register(lv_obj_t* obj) {
    if (!obj || !lv_obj_is_valid(obj)) return;

    for (size_t i = 0; i < keyboard_focusable_count; i++) {
        if (keyboard_focusables[i] == obj) {
            keyboard_focus_dirty = true;
            return;
        }
    }

    if (keyboard_focusable_count >= MAX_KEYBOARD_FOCUSABLES) {
        compact_keyboard_focus_registry();
        if (keyboard_focusable_count >= MAX_KEYBOARD_FOCUSABLES) {
            return;
        }
    }

    keyboard_focusables[keyboard_focusable_count++] = obj;
    keyboard_focus_dirty = true;
}

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
