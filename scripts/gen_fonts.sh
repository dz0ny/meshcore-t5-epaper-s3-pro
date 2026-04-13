#!/bin/bash
# Regenerate LVGL font files for LilyGo T5 ePaper Pro
# Usage: ./scripts/gen_fonts.sh
#
# Prerequisites:
#   npm install -g lv_font_conv
#   Run `uvx platformio run` once so LVGL is fetched (FA5 font lives in .pio/libdeps/)
#
# Font sources are in assets/fonts/:
#   NotoSans-Medium.ttf, Montserrat-Bold.ttf, NotoSansSymbols2.ttf
# FontAwesome5 is bundled with LVGL

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
OUT_DIR="$PROJECT_DIR/src/fonts"

# Font sources (TTFs stored in assets/, FA5 bundled with LVGL)
NOTO="$PROJECT_DIR/assets/fonts/NotoSans-Medium.ttf"
MONTSERRAT="$PROJECT_DIR/assets/fonts/Montserrat-Bold.ttf"
SYMBOLS="$PROJECT_DIR/assets/fonts/NotoSansSymbols2.ttf"
FA5="$PROJECT_DIR/.pio/libdeps/lilygo-t5-epaper-pro/lvgl/scripts/built_in_font/FontAwesome5-Solid+Brands+Regular.woff"

# Verify font files exist
for f in "$NOTO" "$MONTSERRAT" "$SYMBOLS" "$FA5"; do
    if [ ! -f "$f" ]; then
        echo "ERROR: Missing font: $f"
        exit 1
    fi
done

# Common character ranges
LATIN="0x20-0x7F,0xA0-0x24F,0x2000-0x206F"

# FontAwesome5 glyphs used in the UI:
#   0xF005 = star (solid)
#   0xF00C = check/ok
#   0xF00D = close/x
#   0xF053 = chevron-up
#   0xF054 = chevron-down
#   0xF071 = warning triangle
#   0xF0E7 = bolt/lightning
#   0xF11C = keyboard
#   0xF124 = location-arrow (GPS)
#   0xF240-0xF244 = battery levels
#   0xF293 = bluetooth
#   0xF294 = bluetooth-b (brand)
#   0xF3C5 = map-marker-alt (location pin)
#   0xF55A = backspace
#   0xF689 = satellite
#   0xF8A2 = enter/new-line
FA5_GLYPHS="0xF005,0xF00C,0xF00D,0xF053,0xF054,0xF071,0xF0E7,0xF11C,0xF124,0xF240,0xF241,0xF242,0xF243,0xF244,0xF293,0xF294,0xF3C5,0xF55A,0xF689,0xF8A2"

# NotoSansSymbols2 glyphs:
#   0x2605 = ★ black star
#   0x2606 = ☆ white star
SYMBOL_GLYPHS="0x2605,0x2606"

echo "Generating fonts..."

# noto_24 — statusbar, secondary info, sender names
echo "  noto_24.c"
npx lv_font_conv --bpp 4 --size 24 \
    --font "$NOTO" -r "$LATIN" \
    --font "$FA5" -r "$FA5_GLYPHS" \
    --font "$SYMBOLS" -r "$SYMBOL_GLYPHS" \
    --format lvgl --lv-font-name lv_font_noto_24 \
    -o "$OUT_DIR/noto_24.c" --no-compress

# noto_28 — message text, body text, values
echo "  noto_28.c"
npx lv_font_conv --bpp 4 --size 28 \
    --font "$NOTO" -r "$LATIN" \
    --font "$FA5" -r "$FA5_GLYPHS" \
    --font "$SYMBOLS" -r "$SYMBOL_GLYPHS" \
    --format lvgl --lv-font-name lv_font_noto_28 \
    -o "$OUT_DIR/noto_28.c" --no-compress

# montserrat_bold_30 — menus, titles, settings labels, keyboard
echo "  montserrat_bold_30.c"
npx lv_font_conv --bpp 4 --size 30 \
    --font "$MONTSERRAT" -r "$LATIN" \
    --font "$FA5" -r "$FA5_GLYPHS" \
    --font "$SYMBOLS" -r "$SYMBOL_GLYPHS" \
    --format lvgl --lv-font-name lv_font_montserrat_bold_30 \
    -o "$OUT_DIR/montserrat_bold_30.c" --no-compress

# montserrat_bold_80 — splash screen (needs letters for "MeshCore", "Power Off", "Sleep", etc.)
echo "  montserrat_bold_80.c"
npx lv_font_conv --bpp 4 --size 80 \
    --font "$MONTSERRAT" -r "0x20-0x7E" \
    --format lvgl --lv-font-name lv_font_montserrat_bold_80 \
    -o "$OUT_DIR/montserrat_bold_80.c" --no-compress

# montserrat_bold_120 — home/lock screen clock (digits, colon, space only)
echo "  montserrat_bold_120.c"
npx lv_font_conv --bpp 4 --size 120 \
    --font "$MONTSERRAT" -r "0x20-0x3A" \
    --format lvgl --lv-font-name lv_font_montserrat_bold_120 \
    -o "$OUT_DIR/montserrat_bold_120.c" --no-compress

# ---------- T-Deck fonts (smaller sizes with same glyph coverage) ----------

# noto_14 — T-Deck statusbar, secondary info (replaces built-in montserrat_14)
echo "  noto_14.c"
npx lv_font_conv --bpp 4 --size 14 \
    --font "$NOTO" -r "$LATIN" \
    --font "$FA5" -r "$FA5_GLYPHS" \
    --font "$SYMBOLS" -r "$SYMBOL_GLYPHS" \
    --format lvgl --lv-font-name lv_font_noto_14 \
    -o "$OUT_DIR/noto_14.c" --no-compress

# noto_16 — T-Deck titles, menu labels (replaces built-in montserrat_16)
echo "  noto_16.c"
npx lv_font_conv --bpp 4 --size 16 \
    --font "$NOTO" -r "$LATIN" \
    --font "$FA5" -r "$FA5_GLYPHS" \
    --font "$SYMBOLS" -r "$SYMBOL_GLYPHS" \
    --format lvgl --lv-font-name lv_font_noto_16 \
    -o "$OUT_DIR/noto_16.c" --no-compress

# Fix include paths (lv_font_conv generates "lvgl/lvgl.h", we need "lvgl.h")
echo "Fixing include paths..."
sed -i '' 's|#include "lvgl/lvgl.h"|#include "lvgl.h"|g' "$OUT_DIR"/*.c

echo "Done. Generated:"
ls -la "$OUT_DIR"/*.c
