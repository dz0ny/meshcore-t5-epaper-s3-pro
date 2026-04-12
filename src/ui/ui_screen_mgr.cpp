#include "ui_screen_mgr.h"
#include "ui_theme.h"
#include <cstring>

// Ported from factory scr_mrg.cpp — kept as C-style linked list, works well.

namespace ui::screen_mgr {

enum card_state {
    STATE_IDLE = 0,
    STATE_DESTROYED,
    STATE_CREATED,
    STATE_INACTIVE,
    STATE_ACTIVE_BG,
    STATE_ACTIVE,
};

struct card_t {
    int id;
    lv_obj_t* obj;
    card_state st;
    screen_lifecycle_t* life;
    char nav_title[32];
    card_t* next;
    card_t* prev;
};

static card_t* head = NULL;
static card_t* top_card = NULL;
static card_t* stack_root = NULL;
static card_t* stack_top = NULL;

// No animations on e-paper — partial redraws cause visual artifacts
static lv_screen_load_anim_t anim_sw   = LV_SCR_LOAD_ANIM_NONE;
static lv_screen_load_anim_t anim_push = LV_SCR_LOAD_ANIM_NONE;
static lv_screen_load_anim_t anim_pop  = LV_SCR_LOAD_ANIM_NONE;
static uint32_t anim_time = 0;

// ---------- Internal ----------

static const char* default_nav_title(int id) {
    switch (id) {
        case SCREEN_HOME: return "Home";
        case SCREEN_CONTACTS: return "Contacts";
        case SCREEN_CHAT: return "Messages";
        case SCREEN_SETTINGS: return "Settings";
        case SCREEN_GPS: return "GPS";
        case SCREEN_BATTERY: return "Battery";
        case SCREEN_MESH: return "Mesh";
        case SCREEN_STATUS: return "Status";
        case SCREEN_SET_DISPLAY: return "Display";
        case SCREEN_SET_GPS: return "GPS Settings";
        case SCREEN_SET_MESH: return "Mesh Config";
        case SCREEN_DISCOVERY: return "Discovery";
        case SCREEN_LOCK: return "Lock";
        case SCREEN_CONTACT_DETAIL: return "Contact";
        case SCREEN_MSG_DETAIL: return "Message";
        case SCREEN_SET_BLE: return "Bluetooth";
        case SCREEN_SET_STORAGE: return "Storage";
        case SCREEN_COMPOSE: return "Compose";
        case SCREEN_MAP: return "Map";
        case SCREEN_SENSORS: return "Sensors";
        case SCREEN_PING: return "Ping";
        default: return "";
    }
}

static lv_obj_t* create_default_screen(card_t* card) {
    lv_obj_t* obj = lv_obj_create(NULL);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(obj, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(obj, 5, LV_PART_MAIN);

    card->life->create(obj);
    return obj;
}

static card_t* find_by_id(int id) {
    card_t* p = head->next;
    while (p) {
        if (p->id == id) return p;
        p = p->next;
    }
    return NULL;
}

static void activate(card_t* card) {
    if (card->st == STATE_DESTROYED) {
        card->obj = create_default_screen(card);
        card->life->entry();
        card->st = STATE_ACTIVE;
    } else if (card->st == STATE_CREATED || card->st == STATE_INACTIVE) {
        card->life->entry();
        card->st = STATE_ACTIVE;
    }
}

static void deactivate(card_t* card) {
    if (card->st > STATE_INACTIVE) {
        card->life->exit();
        card->st = STATE_INACTIVE;
    }
}

static void remove_card(card_t* card) {
    if (card->st > STATE_INACTIVE) {
        card->life->exit();
        card->life->destroy();
        card->st = STATE_IDLE;
    } else if (card->st > STATE_DESTROYED) {
        card->life->destroy();
        card->st = STATE_IDLE;
    }
}

// ---------- Public API ----------

void init() {
    head = (card_t*)lv_malloc(sizeof(card_t));
    head->id = -1;
    head->obj = NULL;
    head->st = (card_state)-1;
    head->life = NULL;
    head->nav_title[0] = 0;
    head->next = NULL;
    head->prev = NULL;
    top_card = head;
    stack_root = NULL;
    stack_top = NULL;
}

bool register_screen(int id, screen_lifecycle_t* life) {
    if (find_by_id(id)) return false;

    card_t* c = (card_t*)lv_malloc(sizeof(card_t));
    c->id = id;
    c->obj = NULL;
    c->st = STATE_IDLE;
    c->life = life;
    c->nav_title[0] = 0;
    c->next = NULL;
    c->prev = top_card;
    top_card->next = c;
    top_card = c;
    return true;
}

bool switch_to(int id, bool anim) {
    card_t* tgt = find_by_id(id);
    if (!tgt) return false;

    lv_obj_t* curr_obj = NULL;
    card_t* s = NULL;

    // Clear stack
    if (stack_top) {
        curr_obj = stack_top->obj;
        s = stack_top->prev;
        remove_card(stack_top);
        lv_free(stack_top);
        stack_top = s;
    }
    while (stack_top) {
        s = stack_top->prev;
        remove_card(stack_top);
        lv_free(stack_top);
        stack_top = s;
    }

    // Create new stack entry
    s = (card_t*)lv_malloc(sizeof(card_t));
    s->id = tgt->id;
    s->obj = create_default_screen(tgt);
    s->st = STATE_CREATED;
    s->life = tgt->life;
    s->nav_title[0] = 0;
    s->prev = NULL;
    s->next = NULL;
    stack_root = s;
    stack_top = s;

    activate(s);

    if (anim_sw != LV_SCR_LOAD_ANIM_NONE && anim) {
        lv_screen_load_anim(s->obj, anim_sw, anim_time, 0, true);
    } else {
        lv_screen_load(s->obj);
        if (curr_obj) lv_obj_delete(curr_obj);
    }
    return true;
}

bool push(int id, bool anim) {
    card_t* tgt = find_by_id(id);
    if (!tgt) return false;
    if (stack_top && tgt->id == stack_top->id) return false;

    card_t* s = (card_t*)lv_malloc(sizeof(card_t));
    s->id = tgt->id;
    s->obj = create_default_screen(tgt);
    s->st = STATE_CREATED;
    s->life = tgt->life;
    s->nav_title[0] = 0;

    if (!stack_top) {
        s->prev = NULL;
        s->next = NULL;
        stack_root = s;
        stack_top = s;
    } else {
        deactivate(stack_top);
        stack_top->next = s;
        s->prev = stack_top;
        s->next = NULL;
        stack_top = s;
    }

    activate(s);

    if (anim_push != LV_SCR_LOAD_ANIM_NONE && anim) {
        lv_screen_load_anim(s->obj, anim_push, anim_time, 0, false);
    } else {
        lv_screen_load(s->obj);
    }
    return true;
}

bool pop(bool anim) {
    if (!stack_top || stack_top == stack_root) return false;

    lv_obj_t* cur_obj = stack_top->obj;
    card_t* dst = stack_top->prev;
    remove_card(stack_top);
    lv_free(stack_top);
    stack_top = dst;

    activate(dst);

    if (anim_pop != LV_SCR_LOAD_ANIM_NONE && anim) {
        lv_screen_load_anim(dst->obj, anim_pop, anim_time, 0, true);
    } else {
        lv_screen_load(dst->obj);
        if (cur_obj) lv_obj_delete(cur_obj);
    }
    return true;
}

void set_nav_title(const char* title) {
    if (!stack_top || !title) return;

    strncpy(stack_top->nav_title, title, sizeof(stack_top->nav_title) - 1);
    stack_top->nav_title[sizeof(stack_top->nav_title) - 1] = 0;
}

const char* previous_nav_title(const char* fallback) {
    if (!stack_top || !stack_top->prev) return fallback ? fallback : "";

    if (stack_top->prev->nav_title[0] != 0) return stack_top->prev->nav_title;

    return default_nav_title(stack_top->prev->id);
}

int top_id() {
    return stack_top ? stack_top->id : -1;
}

lv_obj_t* top_obj() {
    return stack_top ? stack_top->obj : NULL;
}

} // namespace ui::screen_mgr
