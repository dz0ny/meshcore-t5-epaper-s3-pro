# t-paper

MeshCore mesh radio firmware with LVGL e-paper UI for LilyGo T5S3 4.7" e-paper PRO (ESP32-S3).

## Build & Flash

```bash
uvx platformio run              # build
uvx platformio run -t upload    # flash (hold BOOT+RESET if device won't connect)
```

## Architecture

- **Core 0**: MeshCore companion radio (mesh networking, BLE companion app)
- **Core 1**: LVGL v9 UI (e-paper display, touch input)
- **Bridge**: FreeRTOS queues between cores (mesh_bridge.h)
- **Model**: Reactive data model (model.h) — single source of truth for all UI screens

## Key Directories

- `src/` — application code
  - `board.h/cpp` — hardware init (screen, touch, PMU, GPS, RTC, SD)
  - `model.h/cpp` — reactive data model updated every 2s
  - `main.cpp` — entry point: board::init() → mesh::task::start(0) → ui::task::start(1)
  - `mesh/` — MeshCore task, bridge queues, companion radio integration
  - `mesh/companion/` — MeshCore companion radio files (MyMesh, DataStore, target)
  - `ui/` — LVGL screens, components, port layer
  - `ui/components/` — reusable UI: statusbar, nav_button, msg_list
  - `ui/screens/` — screen implementations (home, contacts, chat, discovery, etc.)
- `lib/` — vendored libraries (MeshCore, epdiy, lvgl config, SensorLib, BQ27220, etc.)
- `boards/` — PlatformIO board definition

## UI Design Rules

- **Minimum font size 24pt** — anything smaller is unreadable on e-ink
- **Never use dim/gray colors** — only EPD_COLOR_TEXT (0x000000 black) and EPD_COLOR_BG (0xFFFFFF white)
- **Namespace-based components** — no OOP classes for UI, use C++ namespaces
- **All screens get a statusbar** — added automatically by screen manager
- **No animations** — e-paper can't handle them
- Use `lv_font_montserrat_24` for text needing Unicode (contacts, messages) — has Latin Extended chars
- Use `Font_Mono_Bold_25/30/90` for titles and values (ASCII only)
- Use `ui::nav::toggle_item()` for settings rows, `ui::nav::menu_item()` for navigation

## Display

- LVGL v9.5 with RGB565 color format
- Flush converts RGB565 → 4-bit grayscale pairs for epdiy
- E-paper refresh modes: Normal (GL16), Fast (DU), Neat (GC16 with white clear)
- Screen invalidation every 2s from UI task forces periodic redraw

## MeshCore Integration

- Companion radio protocol via BLE (SerialBLEInterface)
- DataStore persists contacts/channels/prefs to SPIFFS
- Identity persists across reboots (SPIFFS /identity/_main.id)
- Discovery screen shows recently heard nodes (getRecentlyHeard)
- Auto-add disabled — contacts added explicitly from Discovery
- Radio params (freq, BW, SF, CR, TX power) configurable from Settings > Mesh

## Sleep

- Configurable timeout: Off, 1, 2, 5, 15, 30 min
- Lock screen shows clock, unread messages, battery
- ESP32-S3 light sleep with wake on LoRa DIO1 (GPIO 10) + touch INT (GPIO 3)
- Mesh mutex grabbed before sleep to ensure radio is idle

## Hardware

- Board: LilyGo T5S3 4.7" e-paper PRO
- MCU: ESP32-S3 with PSRAM
- Display: 540x960 4.7" e-paper via epdiy
- Touch: GT911 capacitive (I2C)
- Radio: SX1262 LoRa (shared SPI with SD card)
- GPS: L76K or u-blox M10Q (auto-detected)
- PMU: BQ25896 (charger) + BQ27220 (fuel gauge)
- RTC: PCF8563
- Storage: SPIFFS (identity/prefs), SD card (optional)
