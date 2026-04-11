#include <cstdio>
#include <esp_random.h>
#include "set_ble.h"
#include "../ui_theme.h"
#include "../ui_screen_mgr.h"
#include "../components/nav_button.h"
#include "../../mesh/mesh_task.h"
#include "../../nvs_param.h"

namespace ui::screen::set_ble {

static lv_obj_t* scr = NULL;
static lv_obj_t* lbl_ble = NULL;
static lv_obj_t* lbl_pin = NULL;

static void on_back(lv_event_t* e) { ui::screen_mgr::pop(true); }

static void on_ble_toggle(lv_event_t* e) {
    if (mesh::task::ble_is_enabled()) {
        mesh::task::ble_disable();
        nvs_param_set_u8(NVS_ID_BLE_ENABLED, 0);
        if (lbl_ble) lv_label_set_text(lbl_ble, "Off");
    } else {
        mesh::task::ble_enable();
        nvs_param_set_u8(NVS_ID_BLE_ENABLED, 1);
        if (lbl_ble) lv_label_set_text(lbl_ble, "On");
    }
}

static void on_pin_regen(lv_event_t* e) {
    uint32_t pin = esp_random() % 900000 + 100000;
    mesh::task::set_ble_pin(pin);
    if (lbl_pin) lv_label_set_text_fmt(lbl_pin, "%lu", (unsigned long)pin);
}

static void create(lv_obj_t* parent) {
    scr = parent;
    ui::nav::back_button(parent, "Bluetooth", on_back);

    lv_obj_t* list = ui::nav::scroll_list(parent);

    lbl_ble = ui::nav::toggle_item(list, "BLE", mesh::task::ble_is_enabled() ? "On" : "Off", on_ble_toggle, NULL);

    static char buf[32];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)mesh::task::get_ble_pin());
    lbl_pin = ui::nav::toggle_item(list, "PIN", buf, on_pin_regen, NULL);
}

static void entry() {}
static void exit_fn() {}
static void destroy() {
    scr = NULL;
    lbl_ble = lbl_pin = NULL;
}

screen_lifecycle_t lifecycle = { create, entry, exit_fn, destroy };

} // namespace ui::screen::set_ble
