#ifdef BOARD_TDECK

#include "ui_port.h"
#include "ui_screen_mgr.h"
#include "ui_theme.h"
#include "../board.h"
#include "../model.h"
#include "../nvs_param.h"
#include <SPI.h>
#include <esp_heap_caps.h>

namespace ui::port {

static int refresh_mode = UI_REFRESH_MODE_NORMAL;  // no-op on TFT but kept for API compat
static volatile bool touch_enabled = true;
static int backlight_mode = 1;
static const char* mode_names_bl[] = {"On"};
static int brightness = 2;
static const char* bright_names[] = {"Low", "Mid", "High"};
static const int bright_pwm[] = {50, 128, 255};

// ---------- ST7789 SPI driver ----------
// SPI at 80 MHz — matches trail-mate's proven T-Deck setup for fast flush.

static SPISettings spi_settings(80000000, MSBFIRST, SPI_MODE0);

static inline void cs_low()  { GPIO.out_w1tc = (1 << TDECK_DISP_CS); }  // fast GPIO
static inline void cs_high() { GPIO.out_w1ts = (1 << TDECK_DISP_CS); }
static inline void dc_cmd()  { GPIO.out_w1tc = (1 << TDECK_DISP_DC); }
static inline void dc_data() { GPIO.out_w1ts = (1 << TDECK_DISP_DC); }

static void write_cmd(uint8_t cmd) {
    cs_low();
    dc_cmd();
    SPI.transfer(cmd);
    cs_high();
}

static void write_cmd_data(uint8_t cmd, const uint8_t* data, size_t len) {
    cs_low();
    dc_cmd();
    SPI.transfer(cmd);
    if (len > 0) {
        dc_data();
        SPI.writeBytes(data, len);
    }
    cs_high();
}

static void st7789_init() {
    pinMode(TDECK_DISP_CS, OUTPUT);
    pinMode(TDECK_DISP_DC, OUTPUT);
    digitalWrite(TDECK_DISP_CS, HIGH);

    if (TDECK_DISP_RST >= 0) {
        pinMode(TDECK_DISP_RST, OUTPUT);
        digitalWrite(TDECK_DISP_RST, LOW);
        delay(20);
        digitalWrite(TDECK_DISP_RST, HIGH);
        delay(120);
    }

    SPI.beginTransaction(spi_settings);

    // Full ST7789 init sequence from trail-mate (tuned from LilyGo T-Deck TFT_eSPI)
    write_cmd(0x11);  // SLPOUT
    delay(120);
    write_cmd(0x13);  // NORON

    uint8_t madctl = 0x00;
    write_cmd_data(0x36, &madctl, 1);  // MADCTL (RGB order, will set rotation after)

    uint8_t colmod = 0x55;
    write_cmd_data(0x3A, &colmod, 1);  // COLMOD RGB565

    // Porch control
    uint8_t porctrl[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    write_cmd_data(0xB2, porctrl, 5);

    // Gate control
    uint8_t gctrl = 0x75;
    write_cmd_data(0xB7, &gctrl, 1);

    // VCOM setting
    uint8_t vcoms = 0x1A;
    write_cmd_data(0xBB, &vcoms, 1);

    // LCM control
    uint8_t lcmctrl = 0x2C;
    write_cmd_data(0xC0, &lcmctrl, 1);

    // VDV and VRH command enable
    uint8_t vdvvrhen = 0x01;
    write_cmd_data(0xC2, &vdvvrhen, 1);

    // VRH set
    uint8_t vrhs = 0x13;
    write_cmd_data(0xC3, &vrhs, 1);

    // VDV set
    uint8_t vdvset = 0x20;
    write_cmd_data(0xC4, &vdvset, 1);

    // Frame rate control
    uint8_t frctr2 = 0x0F;
    write_cmd_data(0xC6, &frctr2, 1);

    // Power control
    uint8_t pwctrl1[] = {0xA4, 0xA1};
    write_cmd_data(0xD0, pwctrl1, 2);

    // Positive gamma
    uint8_t pvgam[] = {0xD0, 0x0D, 0x14, 0x0D, 0x0D, 0x09, 0x38, 0x44, 0x4E, 0x3A, 0x17, 0x18, 0x2F, 0x30};
    write_cmd_data(0xE0, pvgam, 14);

    // Negative gamma
    uint8_t nvgam[] = {0xD0, 0x09, 0x0F, 0x08, 0x07, 0x14, 0x37, 0x44, 0x4D, 0x38, 0x15, 0x16, 0x2C, 0x3E};
    write_cmd_data(0xE1, nvgam, 14);

    write_cmd(0x21);  // INVON

    // Column address set: 0..239
    uint8_t caset[] = {0x00, 0x00, 0x00, 0xEF};
    write_cmd_data(0x2A, caset, 4);

    // Row address set: 0..319
    uint8_t raset[] = {0x00, 0x00, 0x01, 0x3F};
    write_cmd_data(0x2B, raset, 4);

    // Landscape rotation: MY=0, MX=1, MV=1 → 0x60
    uint8_t rot = 0x60;
    write_cmd_data(0x36, &rot, 1);

    write_cmd(0x29);  // DISPON
    delay(10);

    SPI.endTransaction();

    Serial.printf("ST7789 init: %dx%d @ 80MHz SPI\n", SCREEN_WIDTH, SCREEN_HEIGHT);
}

// ---------- 32-bit SWAR RGB565 byte swap ----------
// Processes 2 pixels per iteration using SIMD-Within-A-Register.
// 0xAABBCCDD → 0xBBAADDCC (swap bytes within each 16-bit half)
// ~2x faster than lv_draw_sw_rgb565_swap which does 1 pixel at a time.

static inline void swar_rgb565_swap(uint8_t *buf, uint32_t pixel_count) {
    uint32_t *buf32 = reinterpret_cast<uint32_t *>(buf);
    uint32_t len32 = pixel_count / 2;

    // Unrolled 8x loop — processes 16 pixels per iteration
    uint32_t i = 0;
    for (; i + 8 <= len32; i += 8) {
        buf32[i + 0] = ((buf32[i + 0] & 0xFF00FF00) >> 8) | ((buf32[i + 0] & 0x00FF00FF) << 8);
        buf32[i + 1] = ((buf32[i + 1] & 0xFF00FF00) >> 8) | ((buf32[i + 1] & 0x00FF00FF) << 8);
        buf32[i + 2] = ((buf32[i + 2] & 0xFF00FF00) >> 8) | ((buf32[i + 2] & 0x00FF00FF) << 8);
        buf32[i + 3] = ((buf32[i + 3] & 0xFF00FF00) >> 8) | ((buf32[i + 3] & 0x00FF00FF) << 8);
        buf32[i + 4] = ((buf32[i + 4] & 0xFF00FF00) >> 8) | ((buf32[i + 4] & 0x00FF00FF) << 8);
        buf32[i + 5] = ((buf32[i + 5] & 0xFF00FF00) >> 8) | ((buf32[i + 5] & 0x00FF00FF) << 8);
        buf32[i + 6] = ((buf32[i + 6] & 0xFF00FF00) >> 8) | ((buf32[i + 6] & 0x00FF00FF) << 8);
        buf32[i + 7] = ((buf32[i + 7] & 0xFF00FF00) >> 8) | ((buf32[i + 7] & 0x00FF00FF) << 8);
    }
    for (; i < len32; i++) {
        buf32[i] = ((buf32[i] & 0xFF00FF00) >> 8) | ((buf32[i] & 0x00FF00FF) << 8);
    }
    // Handle odd trailing pixel
    if (pixel_count & 1) {
        uint16_t *buf16 = reinterpret_cast<uint16_t *>(buf);
        uint16_t v = buf16[pixel_count - 1];
        buf16[pixel_count - 1] = (v << 8) | (v >> 8);
    }
}

// ---------- LVGL flush callback ----------
// Single SPI transaction: addr window + RAMWR + pixel data, CS held low throughout.

static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint16_t x1 = area->x1;
    uint16_t y1 = area->y1;
    uint16_t x2 = area->x2;
    uint16_t y2 = area->y2;
    uint32_t w = x2 - x1 + 1;
    uint32_t h = y2 - y1 + 1;
    uint32_t pixel_count = w * h;

    // 32-bit SWAR byte swap — 2x faster than lv_draw_sw_rgb565_swap
    swar_rgb565_swap(px_map, pixel_count);

    SPI.beginTransaction(spi_settings);
    cs_low();

    // CASET
    uint8_t ca[] = { (uint8_t)(x1 >> 8), (uint8_t)x1, (uint8_t)(x2 >> 8), (uint8_t)x2 };
    dc_cmd();
    SPI.transfer(0x2A);
    dc_data();
    SPI.writeBytes(ca, 4);

    // RASET
    uint8_t ra[] = { (uint8_t)(y1 >> 8), (uint8_t)y1, (uint8_t)(y2 >> 8), (uint8_t)y2 };
    dc_cmd();
    SPI.transfer(0x2B);
    dc_data();
    SPI.writeBytes(ra, 4);

    // RAMWR + pixel data
    dc_cmd();
    SPI.transfer(0x2C);
    dc_data();
    SPI.writeBytes(px_map, pixel_count * 2);

    cs_high();
    SPI.endTransaction();

    lv_display_flush_ready(disp);
}

// ---------- Touch input ----------

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    static int16_t x = 0, y = 0;
    static lv_indev_state_t last_state = LV_INDEV_STATE_RELEASED;
    static uint32_t next_poll = 0;

    uint32_t now = millis();
    if (now >= next_poll) {
        next_poll = now + 30;
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

// ---------- Keyboard input ----------

static void keyboard_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    static uint32_t last_key = 0;
    static uint32_t last_backspace_ms = 0;
    static uint8_t backspace_repeat_count = 0;
    static bool backspace_cleared = false;

    int c = board::keyboard_read_char();
    if (c > 0) {
        uint32_t now = millis();
        last_key = (uint32_t)c;
        data->state = LV_INDEV_STATE_PRESSED;

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

// ---------- Trackball → dual-zone navigation ----------
// Two independent zones: nav-band (Left/Right) and content (Up/Down).
// Left/Right always controls nav-band items, Up/Down always controls content items.
// When one axis is used, the other zone's focus is cleared so Click hits the right target.

static constexpr int TB_MAX_NAV_ITEMS = 16;
static constexpr int TB_MAX_CONTENT_ITEMS = 128;

// Current focused item per zone (NULL = nothing focused in that zone)
static lv_obj_t *tb_nav_focused = NULL;
static lv_obj_t *tb_content_focused = NULL;
static lv_obj_t *tb_last_screen = NULL;

// Focus/defocus sends LVGL events to trigger existing visual callbacks
// (on_row_focus_feedback, on_back_focus_feedback) that invert bg/text colors.
static void tb_apply_focus(lv_obj_t *obj) {
    if (!obj || !lv_obj_is_valid(obj)) return;
    lv_obj_send_event(obj, LV_EVENT_FOCUSED, NULL);
}

static void tb_remove_focus(lv_obj_t *obj) {
    if (!obj || !lv_obj_is_valid(obj)) return;
    lv_obj_send_event(obj, LV_EVENT_DEFOCUSED, NULL);
}

static bool tb_obj_is_in_nav_band(lv_obj_t *obj) {
    if (!obj || !lv_obj_is_valid(obj)) return false;
    // Nav-band items are not inside scrollable containers and are in the top bar area
    lv_obj_t *node = lv_obj_get_parent(obj);
    while (node) {
        if (lv_obj_has_flag(node, LV_OBJ_FLAG_SCROLLABLE)) return false;
        node = lv_obj_get_parent(node);
    }
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    return coords.y1 <= (UI_NAV_BAND_BOTTOM + 4);
}

static void tb_collect_clickables(lv_obj_t *parent, bool want_nav, lv_obj_t **items, int max_items, int *count) {
    if (!parent || !items || !count || *count >= max_items) return;
    if (lv_obj_has_flag(parent, LV_OBJ_FLAG_HIDDEN)) return;

    uint32_t cnt = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < cnt && *count < max_items; i++) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        if (!child) continue;
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) continue;

        if (lv_obj_has_flag(child, LV_OBJ_FLAG_CLICKABLE) && (tb_obj_is_in_nav_band(child) == want_nav)) {
            items[*count] = child;
            (*count)++;
        }

        tb_collect_clickables(child, want_nav, items, max_items, count);
    }
}

static int tb_find_index(lv_obj_t **items, int count, lv_obj_t *obj) {
    for (int i = 0; i < count; i++) {
        if (items[i] == obj) return i;
    }
    return -1;
}

static void tb_reset_on_screen_change() {
    lv_obj_t *screen = lv_screen_active();
    if (screen == tb_last_screen) return;

    // Screen changed — clear old focus
    tb_remove_focus(tb_nav_focused);
    tb_remove_focus(tb_content_focused);
    tb_nav_focused = NULL;
    tb_content_focused = NULL;
    tb_last_screen = screen;
}

static void trackball_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    static uint32_t last_key = 0;
    static int pending_enter = 0;
    static bool release_next = false;

    if (ui::screen_mgr::top_id() == SCREEN_LOCK) {
        (void)board::trackball_read();
        pending_enter = 0;
        release_next = false;
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = last_key;
        return;
    }

    tb_reset_on_screen_change();

    if (release_next) {
        data->state = LV_INDEV_STATE_RELEASED;
        data->key = last_key;
        release_next = false;
        return;
    }

    if (pending_enter == 0) {
        board::TrackballState tb = board::trackball_read();
        lv_obj_t *screen = lv_screen_active();

        if (tb.dx != 0 || tb.dy != 0 || tb.clicked) {
            Serial.printf("TB: dx=%d dy=%d click=%d | ISR totals U=%lu D=%lu L=%lu R=%lu\n",
                tb.dx, tb.dy, tb.clicked,
                board::tb_isr_up_total, board::tb_isr_down_total, board::tb_isr_left_total, board::tb_isr_right_total);
        }

        // Up/Down: content navigation (clears nav focus)
        if (screen && tb.dy != 0) {
            // Clear nav-band focus
            tb_remove_focus(tb_nav_focused);
            tb_nav_focused = NULL;

            lv_obj_t *items[TB_MAX_CONTENT_ITEMS] = {};
            int count = 0;
            tb_collect_clickables(screen, false, items, TB_MAX_CONTENT_ITEMS, &count);

            if (count > 0) {
                int cur = tb_find_index(items, count, tb_content_focused);
                int next;
                if (cur < 0) {
                    next = (tb.dy > 0) ? 0 : count - 1;
                } else {
                    next = cur + (tb.dy > 0 ? 1 : -1);
                    if (next < 0) next = 0;
                    if (next >= count) next = count - 1;
                }

                if (items[next] != tb_content_focused) {
                    tb_remove_focus(tb_content_focused);
                    tb_content_focused = items[next];
                    tb_apply_focus(tb_content_focused);
                    lv_obj_scroll_to_view_recursive(tb_content_focused, LV_ANIM_OFF);
                } else if (!tb_content_focused) {
                    tb_content_focused = items[next];
                    tb_apply_focus(tb_content_focused);
                    lv_obj_scroll_to_view_recursive(tb_content_focused, LV_ANIM_OFF);
                }
                model::touch_activity();
            }
        }

        // Left/Right: nav-band navigation (clears content focus)
        if (screen && tb.dx != 0) {
            // Clear content focus
            tb_remove_focus(tb_content_focused);
            tb_content_focused = NULL;

            lv_obj_t *items[TB_MAX_NAV_ITEMS] = {};
            int count = 0;
            tb_collect_clickables(screen, true, items, TB_MAX_NAV_ITEMS, &count);

            if (count > 0) {
                int cur = tb_find_index(items, count, tb_nav_focused);
                int next;
                if (cur < 0) {
                    next = (tb.dx > 0) ? 0 : count - 1;
                } else {
                    next = cur + (tb.dx > 0 ? 1 : -1);
                    if (next < 0) next = 0;
                    if (next >= count) next = count - 1;
                }

                if (items[next] != tb_nav_focused) {
                    tb_remove_focus(tb_nav_focused);
                    tb_nav_focused = items[next];
                    tb_apply_focus(tb_nav_focused);
                } else if (!tb_nav_focused) {
                    tb_nav_focused = items[next];
                    tb_apply_focus(tb_nav_focused);
                }
                model::touch_activity();
            }
        }

        if (tb.clicked) {
            // Click the last-focused item from either zone
            lv_obj_t *target = tb_content_focused ? tb_content_focused : tb_nav_focused;
            if (target && lv_obj_is_valid(target)) {
                lv_obj_send_event(target, LV_EVENT_CLICKED, NULL);
                model::touch_activity();
            }
        }
    }

    data->state = LV_INDEV_STATE_RELEASED;
    data->key = last_key;
}

// ---------- Tick ----------

static uint32_t tick_cb(void) {
    return millis();
}

// ---------- Init ----------

void init() {
    st7789_init();

    lv_init();
    lv_tick_set_cb(tick_cb);

    lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, disp_flush_cb);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

    // Full-screen double buffer in PSRAM — plenty of PSRAM available (8MB),
    // keeps internal DRAM free for stack, heap, and DMA peripherals.
    size_t buf_size = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color16_t);
    void *buf1 = ps_malloc(buf_size);
    void *buf2 = ps_malloc(buf_size);
    Serial.printf("LVGL: PSRAM buffers %u bytes each\n", (unsigned)buf_size);
    lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Touch
    if (board::peri_status[E_PERI_TOUCH]) {
        lv_indev_t *touch_indev = lv_indev_create();
        lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(touch_indev, touch_read_cb);
        lv_indev_set_display(touch_indev, disp);
        lv_indev_set_scroll_throw(touch_indev, 100);  // disable momentum
    }

    // Keyboard
    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);

    if (board::peri_status[E_PERI_KEYBOARD]) {
        lv_indev_t *kb_indev = lv_indev_create();
        lv_indev_set_type(kb_indev, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(kb_indev, keyboard_read_cb);
        lv_indev_set_display(kb_indev, disp);
        lv_indev_set_group(kb_indev, g);
    }

    // Trackball — polls trackball_read_cb which directly manages focus
    if (board::peri_status[E_PERI_TRACKBALL]) {
        lv_indev_t *tb_indev = lv_indev_create();
        lv_indev_set_type(tb_indev, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(tb_indev, trackball_read_cb);
        lv_indev_set_display(tb_indev, disp);
        lv_indev_set_group(tb_indev, g);
    }

    int stored_brightness = nvs_param_get_u8(NVS_ID_BRIGHTNESS);
    if (stored_brightness < 0 || stored_brightness > 2) {
        stored_brightness = 1;
    }
    brightness = stored_brightness;

    apply_backlight();
}

// ---------- Refresh mode (no-ops on TFT) ----------

void set_refresh_mode(int mode) { refresh_mode = mode; }
int  get_refresh_mode() { return refresh_mode; }

void full_refresh() {}
void full_clean()   {}

void touch_enable()  { touch_enabled = true; }
void touch_disable() { touch_enabled = false; }
void keyboard_focus_invalidate() {}
void keyboard_focus_register(lv_obj_t* obj) { (void)obj; }

// ---------- Backlight ----------

void set_backlight(int mode) {
    (void)mode;
    backlight_mode = 1;
    analogWrite(TDECK_DISP_BL, bright_pwm[brightness]);
}

void set_brightness(int level) {
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    brightness = level;
    analogWrite(TDECK_DISP_BL, bright_pwm[brightness]);
}

void apply_backlight() {
    analogWrite(TDECK_DISP_BL, bright_pwm[brightness]);
}

bool is_backlight_auto() { return false; }
int get_backlight() { return backlight_mode; }
const char* get_backlight_name() { return mode_names_bl[0]; }
int get_brightness() { return brightness; }
const char* get_brightness_name() { return bright_names[brightness]; }
int get_brightness_pwm() { return bright_pwm[brightness]; }

} // namespace ui::port

#endif // BOARD_TDECK
