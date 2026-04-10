#pragma once

#include <stdint.h>
#include <string.h>

namespace ui::text {

// Strip emoji and other non-Latin Unicode from UTF-8 text in-place.
// Keeps ASCII + Latin Extended (č, ć, ž, đ, š etc).
inline void strip_emoji(char* text) {
    char* r = text;
    char* w = text;
    while (*r) {
        uint8_t c = (uint8_t)*r;
        if (c < 0x80) {
            // ASCII — keep
            *w++ = *r++;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte UTF-8 (U+0080..U+07FF) — Latin Extended, keep
            *w++ = *r++;
            if (*r) *w++ = *r++;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte UTF-8 (U+0800..U+FFFF)
            // Keep general punctuation and common symbols, skip emoji ranges
            uint16_t cp = ((c & 0x0F) << 12) | ((r[1] & 0x3F) << 6) | (r[2] & 0x3F);
            if (cp < 0x2600) {
                // Keep: CJK, general punctuation, arrows, math, etc
                *w++ = *r++; *w++ = *r++; *w++ = *r++;
            } else {
                // Skip: emoji, dingbats, misc symbols (U+2600+)
                r += 3;
            }
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte UTF-8 (U+10000+) — all emoji, skip
            r += 4;
        } else {
            r++; // invalid byte, skip
        }
    }
    *w = 0;
}

} // namespace ui::text
