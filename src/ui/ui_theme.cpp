#include "ui_theme.h"
#include "../nvs_param.h"

namespace ui::theme {

namespace {

#ifdef BOARD_TDECK
const palette_t palettes[] = {
    {0xFFFFFF, 0x000000, 0x000000, 0xD0D7DE, 0x94A3B8, 0x000000, 0xFFFFFF},
    {0x000000, 0xFFFFFF, 0xFFFFFF, 0xD0D7DE, 0xCBD5E1, 0xFFFFFF, 0x000000},
    {0x1A0000, 0xFF5252, 0xFFEBEE, 0x8B0000, 0xC47A74, 0xFF5252, 0x000000},
    {0x001A12, 0x69F0AE, 0xE8F5E9, 0x00695C, 0x5E9F8E, 0x69F0AE, 0x000000},
    {0x00111C, 0x5C9FFF, 0xE3F2FD, 0x003366, 0x6F89A8, 0x5C9FFF, 0x000000},
    {0x140A19, 0xF957FF, 0xFFF0FF, 0x5F3376, 0x29D7FF, 0xF957FF, 0x140A19},
    {0x1F1206, 0xFF9A3C, 0xFFF4E8, 0xA8550C, 0xFFD089, 0xFF8A1C, 0x1C1003},
    {0x06161A, 0x4EDFEA, 0xE9FDFF, 0x11606C, 0x9AF7FF, 0x1BB7C7, 0x031214},
    {0x170D1D, 0xD49CFF, 0xFAF0FF, 0x714394, 0xF0C1FF, 0xB779FF, 0x15081C},
    {0x171602, 0xE6FF45, 0xFCFFE5, 0x7B8700, 0xFFF985, 0xD0EA00, 0x111200},
};

const char* names[] = {
    "Light",
    "Dark",
    "SAR Red",
    "SAR Green",
    "SAR Navy",
    "Neo Vice",
    "Signal Orange",
    "Lagoon",
    "Orchid",
    "Citrine",
};
#else
const palette_t palettes[] = {
    {0xFFFFFF, 0x000000, 0x000000, 0x333333, 0x91B821, 0x1E1E00, 0xFFFEE6},
};

const char* names[] = {
    "Mono",
};
#endif

theme_id current_theme = THEME_LIGHT;

} // namespace

void init() {
    uint8_t stored = nvs_param_get_u8(NVS_ID_UI_THEME);
    if (stored >= count()) {
        stored = 0;
    }
    current_theme = static_cast<theme_id>(stored);
}

uint8_t count() {
    return sizeof(palettes) / sizeof(palettes[0]);
}

theme_id current() {
    return current_theme;
}

bool set(theme_id id) {
    if (static_cast<uint8_t>(id) >= count()) {
        return false;
    }
    current_theme = id;
    return true;
}

theme_id next() {
    return static_cast<theme_id>((static_cast<uint8_t>(current_theme) + 1) % count());
}

const char* current_name() {
    return names[static_cast<uint8_t>(current_theme)];
}

const palette_t& colors() {
    return palettes[static_cast<uint8_t>(current_theme)];
}

} // namespace ui::theme
