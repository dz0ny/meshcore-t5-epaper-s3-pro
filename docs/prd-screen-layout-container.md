# PRD: Container-Based Screen Layout

## 1. Executive Summary

**Problem**: Screen layouts are fragile and inconsistent. Content positioning relies on hardcoded pixel offsets (`UI_BACK_BTN_Y + UI_BACK_BTN_HEIGHT`), percentage-based height (`UI_SCROLL_LIST_H = 85%`), and bottom-aligned placement (`LV_ALIGN_BOTTOM_MID`). This causes:
- Nav bar / content overlap on T-Deck (content area intrudes ~5px into nav touch zone)
- Screens that bypass the standard components (status, ping) compute their own offsets and drift out of sync
- Double nav bars on screens that call `back_button()` when `screen_mgr::rebuild_nav()` already adds one (msg_detail, compose on TDECK)
- Every new screen must know the exact pixel math to position content correctly

**Proposed Solution**: Replace the current absolute-positioning model with an LVGL flex-column container, analogous to HTML's `<nav> + <main>` pattern. The screen manager creates a vertical flex container per screen. The nav bar is the first child (fixed height, shrink=0). The content area is the second child (`flex_grow=1`, fills remaining space). Screens only interact with the content area — they never position anything relative to the nav bar.

**Success Criteria**:
- All 24 screens use the container model with zero hardcoded Y-offsets for content placement
- Nav bar touch targets are never overlapped by content on either e-paper (540x960) or T-Deck (320x240)
- `UI_SCROLL_LIST_H`, `UI_SCROLL_LIST_Y`, `UI_BACK_BTN_Y` defines are removed from both layout headers
- No screen calls `back_button()` directly — the screen manager owns the nav bar exclusively
- Existing screen appearance is visually identical (no layout regression)

---

## 2. User Experience & Functionality

### User Personas

- **Firmware developer** adding new screens — should not need to know pixel offsets or which `content_area` / `scroll_list` variant to call. Just receives a parent container and adds widgets.
- **End user** on e-paper or T-Deck — every screen has consistent, non-overlapping touch targets for the back button and content list items.

### User Stories

**US-1**: As a firmware developer, I want every screen's `create(parent)` to receive a content container (not the raw screen object) so that I never position content relative to the nav bar.
- **AC**: `create()` signature stays the same (`lv_obj_t* parent`), but `parent` is now the content container, not the screen root.
- **AC**: Screens that currently call `ui::nav::back_button()` or `ui::nav::scroll_list()` work unchanged — `scroll_list()` just creates a scrollable child inside the container it receives.
- **AC**: The nav bar is created and owned by `screen_mgr` before `create()` is called.

**US-2**: As an end user on T-Deck, I want the back button touch area to never overlap with the first list item so that taps always hit the intended target.
- **AC**: There is a minimum gap of `UI_NAV_CONTENT_GAP` (define) between the nav bar bottom and the first content widget.
- **AC**: On T-Deck (320x240), the gap is at least 4px. On e-paper (540x960), at least 10px.

**US-3**: As a firmware developer, I want screens with custom layouts (map, compose, ping, contact_detail) to still work inside the container model without special-casing.
- **AC**: `content_area()` is simplified to a transparent full-size child — screens that need a styled sub-container (e.g. map with borders) can use it; others use `parent` directly.
- **AC**: `scroll_list()` still exists but only creates the inner scrollable list (no longer wraps a `content_area`).
- **AC**: Map, compose, and ping screens use the same container — they just don't call `scroll_list()`.

### Non-Goals

- Changing the statusbar — it lives on the top layer and is unaffected.
- Changing the home or lock screens — they have `screen_has_nav() == false` and get the full screen as their container (with `pad_top = UI_OUTER_MARGIN_X`, not `UI_STATUSBAR_BOTTOM`).
- Adding animations or transitions.
- Changing the visual appearance of the nav bar or its action buttons.
- Refactoring the screen_mgr stack/push/pop logic.

---

## 3. Technical Specifications

### Critical Constraint: Statusbar Overlay

The statusbar lives on LVGL's **top layer**, not on the screen object. It is rendered as an overlay that covers the top portion of every screen:

| Platform | Statusbar Y | Statusbar H | Statusbar Bottom |
|----------|-------------|-------------|-----------------|
| e-paper  | 5           | 50          | 55px from edge  |
| T-Deck   | 0           | 18          | 18px from edge  |

The screen object's `pad_top` **must** push screen content below this overlay. In the old model, `UI_BACK_BTN_Y` served this purpose (50 on e-paper, 10 on T-Deck). In the new model, `pad_top = UI_STATUSBAR_BOTTOM` replaces it.

For screens without a nav bar (home, lock), `pad_top = UI_OUTER_MARGIN_X` is used instead — these screens account for the statusbar in their own Y values (e.g. `UI_HOME_NODE_Y = 55` on e-paper).

### Architecture: Before vs After

**Before** (current):
```
screen_obj (100% x 100%, pad_all=5, non-scrollable)
  +-- nav_obj (absolute pos: x=5, y=50, w=SIZE_CONTENT, h=70)    [managed by screen_mgr]
  +-- content_area (ALIGN_BOTTOM_MID, 95% w, 85% h, y=-10)       [created by each screen]
        +-- scroll_list (100% x 100%, flex column)                 [created by each screen]
              +-- menu_item ...
```
Problems:
- nav_obj and content_area are siblings with absolute positions — nothing enforces non-overlap
- On T-Deck: 86% height from bottom = 206px, content top = 34px, nav bottom = 38px → **4px overlap**
- `UI_BACK_BTN_Y` double-encodes both "below statusbar" and "nav position" into one value
- Each screen must independently compute content placement

**After** (proposed):
```
screen_obj (100% x 100%, FLEX_FLOW_COLUMN, non-scrollable)
  |  pad_top = UI_STATUSBAR_BOTTOM (55 epaper, 18 tdeck)
  |  pad_left/right/bottom = UI_OUTER_MARGIN_X
  |  pad_row = UI_NAV_CONTENT_GAP
  |
  +-- nav_container (100% w, SIZE_CONTENT h, shrink=0)            [managed by screen_mgr]
  |     +-- back_hit_row + action buttons (same as today)
  +-- content_container (100% w, flex_grow=1, non-scrollable)     [managed by screen_mgr, passed to create()]
        +-- [screen widgets go here]
        +-- e.g. scroll_list (100% x 100%, flex column)           [created by each screen]
              +-- menu_item ...
```

The flex column guarantees:
1. Nav starts below the statusbar overlay (via `pad_top`)
2. Content starts exactly where nav ends + gap (via `pad_row`)
3. Content fills all remaining vertical space (via `flex_grow=1`)

### Coordinate System Change

All screen-relative Y values must be converted to content-container-relative Y values.

**Offset formula**: `new_Y = old_Y - (old_pad_top + UI_BACK_BTN_Y + UI_BACK_BTN_HEIGHT - UI_STATUSBAR_BOTTOM - UI_BACK_BTN_HEIGHT - UI_NAV_CONTENT_GAP)`

Simplified per platform:

| Platform | old_pad | old_nav_bottom | new_content_start | Subtract |
|----------|---------|----------------|-------------------|----------|
| e-paper  | 5       | 5+50+70 = 125  | 55+70+10 = 135    | 130      |
| T-Deck   | 1       | 1+10+28 = 39   | 18+28+4 = 50      | 49       |

`new_Y = old_Y + old_pad - new_content_start` → `old_Y - 130` (epaper) or `old_Y - 49` (tdeck)

**Example** (e-paper compose picker card):
- Old: `UI_COMPOSE_PICKER_CARD_Y = 130`, absolute = 5+130 = 135px from edge
- New: `UI_COMPOSE_PICKER_CARD_Y = 0`, absolute = 135+0 = 135px from edge ✓

### Container Creation (screen_mgr changes)

```cpp
static lv_obj_t* create_default_screen(card_t* card) {
    lv_obj_t* obj = lv_obj_create(NULL);
    lv_obj_set_size(obj, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(obj, lv_color_hex(EPD_COLOR_BG), LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

    // Padding: statusbar clearance on top, margins elsewhere
    lv_obj_set_style_pad_left(obj, UI_OUTER_MARGIN_X, LV_PART_MAIN);
    lv_obj_set_style_pad_right(obj, UI_OUTER_MARGIN_X, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(obj, UI_OUTER_MARGIN_X, LV_PART_MAIN);
    if (screen_has_nav(card->id)) {
        lv_obj_set_style_pad_top(obj, UI_STATUSBAR_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_pad_row(obj, UI_NAV_CONTENT_GAP, LV_PART_MAIN);
    } else {
        // Home/lock manage their own statusbar offset via Y constants
        lv_obj_set_style_pad_top(obj, UI_OUTER_MARGIN_X, LV_PART_MAIN);
    }

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC
                          | LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);

    // ... init card fields ...

    rebuild_nav(card);  // nav_obj becomes first flex child

    // Content container — fills remaining space below nav
    lv_obj_t* content = lv_obj_create(obj);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_style_bg_opa(content, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_border_width(content, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(content, 0, LV_PART_MAIN);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    card->content_obj = content;

    creating_card = card;
    card->life->create(content);  // screen gets content container
    creating_card = NULL;
    return obj;
}
```

### Nav Bar Changes

`create_back_hit_row()`:
- Remove `lv_obj_set_pos(hit, UI_OUTER_MARGIN_X, UI_BACK_BTN_Y)` — flex flow handles position
- Keep `lv_obj_set_size(hit, LV_SIZE_CONTENT, UI_BACK_BTN_HEIGHT)` — height stays fixed

`rebuild_nav()`:
- Replace `lv_obj_move_foreground(nav_obj)` with `lv_obj_move_to_index(nav_obj, 0)` — keeps nav as first flex child even when rebuilt during `create()`

### scroll_list() Changes

```cpp
// Before: creates content_area wrapper + inner scrollable list
lv_obj_t* scroll_list(lv_obj_t* parent) {
    lv_obj_t* area = content_area(parent);  // absolute positioned wrapper
    lv_obj_t* list = lv_obj_create(area);   // inner scroll
    // ...
}

// After: just creates the scrollable list — parent IS the content container
lv_obj_t* scroll_list(lv_obj_t* parent) {
    lv_obj_t* list = lv_obj_create(parent);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    // ... same scroll/flex styling as today ...
    return list;
}
```

`content_area()` is **simplified** to a transparent 100%x100% child (for screens like map that need a styled sub-container). Screens that don't need a sub-container use `parent` directly.

### Layout Defines: Removed vs Kept vs New

| Define | Status | Reason |
|--------|--------|--------|
| `UI_BACK_BTN_Y` | **Remove** | Flex flow + `pad_top` handles position |
| `UI_SCROLL_LIST_H` | **Remove** | `flex_grow=1` handles height |
| `UI_SCROLL_LIST_Y` | **Remove** | No bottom-alignment offset needed |
| `UI_BACK_BTN_HEIGHT` | **Keep** | Nav bar still needs a fixed height |
| `UI_BACK_BTN_PAD_*` | **Keep** | Internal nav bar padding |
| `UI_MENU_ITEM_HEIGHT` | **Keep** | List item sizing unchanged |
| `UI_EXT_CLICK_*` | **Keep** | Touch target extensions unchanged |
| `UI_STATUSBAR_BOTTOM` | **New** | `UI_STATUSBAR_Y + UI_STATUSBAR_HEIGHT` — screen `pad_top` for nav screens |
| `UI_NAV_CONTENT_GAP` | **New** | Flex row gap between nav and content (epaper: 10px, tdeck: 4px) |
| `UI_NAV_BAND_BOTTOM` | **New** | `UI_STATUSBAR_BOTTOM + UI_BACK_BTN_HEIGHT` — nav band detection for T-Deck keyboard navigation |

### Y Value Adjustments (Compose & Ping)

Screens with absolute Y positioning need values adjusted to content-container coordinates.

**E-paper** (subtract 130):

| Define | Old | New | Notes |
|--------|-----|-----|-------|
| `UI_COMPOSE_PICKER_CARD_Y` | 130 | 0 | First widget at content top |
| `UI_COMPOSE_FILTERS_Y` | 255 | 125 | |
| `UI_COMPOSE_PICKER_LIST_Y` | 335 | 205 | |
| `UI_COMPOSE_EDITOR_CARD_Y` | 130 | 0 | First widget at content top |
| `UI_COMPOSE_MESSAGE_Y` | 235 | 105 | |
| `UI_COMPOSE_TA_Y` | 305 | 175 | |
| `UI_PING_BTN_ROW_Y` | 360 | 230 | |
| `UI_MAP_Y` | 130 | 0 | Unused in code but updated for consistency |

**T-Deck** (subtract 49, clamp to 0):

| Define | Old | New | Notes |
|--------|-----|-----|-------|
| `UI_COMPOSE_RECIPIENT_Y` | 36 | 0 | Was overlapping nav by 2px — now fixed |
| `UI_COMPOSE_LIST_Y` | 66 | 28 | Below recipient (H=28) |
| `UI_COMPOSE_EDITOR_Y` | 66 | 28 | |
| `UI_COMPOSE_TA_Y` | 84 | 46 | |
| `UI_PING_BTN_ROW_Y` | 160 | 111 | |
| `UI_MAP_Y` | 33 | 0 | |

### Height Calculations

Screens that compute available height from `lv_display_get_vertical_resolution()` must switch to `lv_obj_get_content_height(parent)` since the content container is smaller than the full screen.

Affected functions in compose.cpp:
- `compose_send_top()` — use `lv_obj_get_content_height(scr)` instead of display resolution
- `compose_picker_list_height()` — same change
- Call `lv_obj_update_layout(scr)` before querying height to ensure flex layout is computed

### Screen-by-Screen Migration

#### Group A: Simple list screens (16 screens — zero code changes)

| settings | settings_preferences | settings_debug | settings_device |
|----------|---------------------|---------------|----------------|
| set_ble | set_gps | set_mesh | set_display |
| set_storage | mesh_settings | gps | battery |
| contacts | discovery | sensors | contact_detail |

All call `scroll_list(parent)` and nothing else. Works as-is.

#### Group B: Screens using `content_area()` (3 screens)

| Screen | Change |
|--------|--------|
| chat | Remove `content_area(parent)`, create msg_list on `parent` directly |
| msg_detail | Remove `back_button()` + `content_area()`, set flex on `parent` directly |
| map | Keep `content_area(parent)` — map needs a styled sub-container with border/radius |

#### Group C: Screens with manual nav offset (2 screens)

| Screen | Change |
|--------|--------|
| status | Replace manual container with `scroll_list(parent)` |
| ping | Change `lv_obj_align(..., 0, UI_BACK_BTN_Y + UI_BACK_BTN_HEIGHT)` to `lv_obj_align(..., 0, 0)` |

#### Group D: Screens with duplicate back_button calls (2 screens)

| Screen | Change |
|--------|--------|
| compose (TDECK + EPAPER) | Remove `ui::nav::back_button(parent, "Compose", on_back)` |
| msg_detail | Remove `ui::nav::back_button(parent, "Message", on_back)` |

#### Group E: Full-screen / no nav (2 screens — zero changes)

| home | lock |
|------|------|

`screen_has_nav()` returns false → no nav created, `pad_top = UI_OUTER_MARGIN_X`. These screens manage statusbar offset via their own Y constants.

### Integration Points

- **Statusbar**: Unaffected. Lives on LVGL's top layer, independent of screen objects. Screen `pad_top = UI_STATUSBAR_BOTTOM` pushes content below it.
- **Toast**: Unaffected. Also uses top layer overlay.
- **screen_mgr nav rebuilds**: `rebuild_nav()` deletes and recreates nav_obj. Uses `lv_obj_move_to_index(nav_obj, 0)` to maintain flex order (replaces `lv_obj_move_foreground`).
- **T-Deck keyboard nav**: `ui_port_tft.cpp` uses `UI_NAV_BAND_BOTTOM` (replaces `UI_BACK_BTN_Y + UI_BACK_BTN_HEIGHT`) to detect nav-band widgets.

### Data Flow

```
screen_mgr::push(id)
  -> create_default_screen(card)
       -> create screen_obj (flex column, pad_top = UI_STATUSBAR_BOTTOM)
       -> rebuild_nav(card)         // creates nav as first flex child
       -> create content container  // second flex child, flex_grow=1
       -> card->life->create(content)  // screen only sees content area
  -> activate(card)
       -> card->life->entry()
```

### Security & Privacy

No changes — this is a layout-only refactor with no data flow implications.

---

## 4. Risks & Roadmap

### Phased Rollout

**Phase 1 — Infrastructure + all migrations** (single pass, all screens):
1. Add `UI_STATUSBAR_BOTTOM`, `UI_NAV_CONTENT_GAP`, `UI_NAV_BAND_BOTTOM` to both layout headers
2. Remove `UI_BACK_BTN_Y`, `UI_SCROLL_LIST_H`, `UI_SCROLL_LIST_Y`
3. Adjust compose/ping Y defines per offset table above
4. Add `content_obj` to `card_t` struct
5. Change `create_default_screen()` to flex column + statusbar-aware padding + content container
6. Update `rebuild_nav()` to use `lv_obj_move_to_index` instead of `lv_obj_move_foreground`
7. Update `create_back_hit_row()` — remove absolute positioning
8. Simplify `scroll_list()` — no longer wraps `content_area()`
9. Simplify `content_area()` — transparent 100% child (for map etc.)
10. Migrate Group B/C/D screens (remove back_button, content_area, manual offsets)
11. Update compose height calculations to use parent dimensions
12. Update `ui_port_tft.cpp` nav band detection

### Technical Risks

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Flex layout performance on ESP32-S3 | Low | Medium | LVGL flex is lightweight; only computed on create, not per-frame |
| Compose screen keyboard layout breaks | Medium | High | Compose has the most complex layout — test picker/editor/keyboard modes separately |
| Nav action buttons misaligned in flex | Low | Medium | Action buttons are inside nav_obj, which is a single flex child — internal layout unchanged |
| Home/lock screen regression | Low | High | These skip the container path entirely (`screen_has_nav() == false`) — add explicit test |
| Map screen sizing wrong | Medium | Medium | Map uses `lv_obj_update_layout()` + `lv_obj_get_content_height()` after flex — verify buffer allocation |
| Statusbar overlap on nav-less screens | Low | High | Home/lock keep `pad_top = UI_OUTER_MARGIN_X` and encode statusbar offset in their own Y constants (unchanged from today) |
