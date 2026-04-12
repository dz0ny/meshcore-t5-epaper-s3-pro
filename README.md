# MeshCore T5 ePaper Pro

MeshCore firmware for the LilyGo T5 ePaper Pro, built on ESP32-S3 with an LVGL e-paper interface and MeshCore radio stack.

## Highlights

- Built for the LilyGo T5S3 4.7" e-paper Pro
- Dual-core split: MeshCore radio task on one core, LVGL UI on the other
- Reactive screen model with contacts, chat, discovery, settings, GPS, sensors, and status views
- Browser-based install flow via GitHub Pages and ESP Web Tools

## Hardware

- Board: LilyGo T5 ePaper Pro
- MCU: ESP32-S3 with PSRAM
- Display: 540x960 4.7" e-paper
- Radio: SX1262 LoRa
- Touch: GT911 capacitive touch
- GPS: L76K or u-blox M10Q
- Power: BQ25896 charger + BQ27220 fuel gauge
- Storage: SPIFFS and SD card

## Local Build

```bash
uvx platformio run -e t-paper
```

Flash over USB:

```bash
uvx platformio run -e t-paper -t upload
```

If the board does not connect, hold `BOOT` and tap `RESET`.

## Web Flasher

This repository includes a GitHub Pages workflow that publishes a simple browser flasher for the latest `t-paper` build using ESP Web Tools.

Once GitHub Pages is enabled for the repository, the install page is available at:

`https://dz0ny.github.io/meshcore-t5-epaepr-pro/`

Use Chrome or Edge with a USB data cable.

## Project Layout

```text
src/
  board.*              hardware bring-up
  model.*              reactive shared UI state
  mesh/                MeshCore bridge and radio task
  ui/                  LVGL task, components, and screens
lib/                   vendored dependencies
boards/                PlatformIO board definitions
tools/                 helper scripts, including web flasher packaging
```

## UI Notes

- E-paper first: no animation-heavy UI
- Minimum readable font size is 24pt
- Screens use a shared navigation pattern and top status bar
- Updates are optimized for partial refreshes

## Firmware Packaging

The web flasher build process:

1. Builds the `t-paper` PlatformIO environment
2. Merges the ESP32-S3 bootloader, partitions, boot app, and app binary
3. Generates a `manifest.json` and a minimal install page for GitHub Pages

The packaging script lives in [tools/build_web_flasher.py](/Users/dz0ny/t-paper/tools/build_web_flasher.py).

## Repository

- GitHub: [dz0ny/meshcore-t5-epaepr-pro](https://github.com/dz0ny/meshcore-t5-epaepr-pro)

