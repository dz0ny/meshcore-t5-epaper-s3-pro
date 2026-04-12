#!/usr/bin/env python3

from __future__ import annotations

import argparse
import html
import shutil
import subprocess
from pathlib import Path


BOOTLOADER_OFFSET = "0x0"
PARTITIONS_OFFSET = "0x8000"
BOOT_APP0_OFFSET = "0xe000"
APP_OFFSET = "0x10000"
FLASH_MODE = "dio"
FLASH_FREQ = "80m"
FLASH_SIZE = "16MB"
CHIP = "esp32s3"
CHIP_FAMILY = "ESP32-S3"
DEVICE_NAME = "LilyGo T5 ePaper S3 Pro"
PROJECT_NAME = "LilyGo T5 ePaper S3 Pro"
PROJECT_SLUG = "lilygo-t5-epaper-pro"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--repo-url", required=True)
    parser.add_argument("--boot-app0")
    return parser.parse_args()


def run(command: list[str]) -> None:
    subprocess.run(command, check=True)


def find_boot_app0(explicit_path: str | None) -> Path:
    if explicit_path:
        path = Path(explicit_path)
        if path.is_file():
            return path
        raise FileNotFoundError(path)

    platformio_core = Path.home() / ".platformio"
    path = platformio_core / "packages" / "framework-arduinoespressif32" / "tools" / "partitions" / "boot_app0.bin"
    if path.is_file():
        return path
    raise FileNotFoundError(path)


def build_page(version: str, repo_url: str) -> str:
    safe_version = html.escape(version)
    safe_repo_url = html.escape(repo_url, quote=True)
    return f"""<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>{PROJECT_NAME} Web Flasher</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=Montserrat:wght@400;500;600;700&display=swap" rel="stylesheet">
    <script src="https://cdn.tailwindcss.com"></script>
    <script>
      tailwind.config = {{
        theme: {{
          extend: {{
            fontFamily: {{
              sans: ["Montserrat", "sans-serif"]
            }},
            colors: {{
              paper: {{
                bg: "#efefec",
                panel: "#f8f8f5",
                line: "#d7d7d1",
                text: "#20201d",
                muted: "#6c6c67",
                accent: "#2b2b28"
              }}
            }}
          }}
        }}
      }};
    </script>
    <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>
    <style>
      :root {{
        --esp-tools-button-color: #2b2b28;
        --esp-tools-button-text-color: #f8f8f5;
        --esp-tools-button-border-radius: 0;
      }}

      body {{
        color-scheme: light;
        font-family: "Montserrat", sans-serif;
      }}

      esp-web-install-button::part(button) {{
        font-family: "Montserrat", sans-serif;
        font-weight: 600;
        letter-spacing: 0.01em;
      }}
    </style>
  </head>
  <body class="min-h-screen bg-gradient-to-b from-[#f4f4f1] to-paper-bg text-paper-text">
    <main class="mx-auto w-[min(46rem,calc(100vw-2rem))] py-10 max-sm:pt-4 max-sm:pb-8">
      <section class="overflow-hidden border border-paper-line bg-paper-panel">
        <div class="border-b border-paper-line p-7 max-sm:p-5">
          <p class="mb-3 text-xs font-semibold uppercase tracking-[0.08em] text-paper-muted">Browser Flasher</p>
          <h1 class="text-[clamp(2rem,6vw,3.2rem)] font-bold leading-none">{PROJECT_NAME}</h1>
          <p class="mt-4 max-w-[34rem] text-base leading-6 text-paper-muted">Flash the latest PlatformIO build for the LilyGo T5 ePaper S3 Pro directly from Chrome or Edge with ESP Web Tools.</p>
        </div>
        <div class="grid gap-5 p-7 max-sm:p-5">
          <div class="flex flex-wrap gap-2">
            <span class="border border-paper-line bg-[#fcfcfa] px-3 py-2 text-sm">Device: {DEVICE_NAME}</span>
            <span class="border border-paper-line bg-[#fcfcfa] px-3 py-2 text-sm">Chip: {CHIP_FAMILY}</span>
            <span class="border border-paper-line bg-[#fcfcfa] px-3 py-2 text-sm">Build: {safe_version}</span>
          </div>
          <div class="flex flex-wrap items-center gap-4">
            <esp-web-install-button manifest="manifest.json"></esp-web-install-button>
            <a class="text-paper-accent underline decoration-1 underline-offset-2" href="{safe_repo_url}">Repository</a>
            <a class="text-paper-accent underline decoration-1 underline-offset-2" href="https://lilygo.cc/en-us/products/t5-e-paper-s3-pro">Product Page</a>
          </div>
          <div class="grid gap-3 border border-paper-line bg-[#fcfcfa] p-4">
            <p class="text-sm font-semibold uppercase tracking-[0.08em] text-paper-muted">What You Get</p>
            <ul class="grid gap-2 text-sm leading-6 text-paper-muted">
              <li>Home screen with clock, unread state, and device overview</li>
              <li>Contacts, conversations, message detail, and compose screens</li>
              <li>Discovery view for recently heard mesh nodes</li>
              <li>Status, battery, GPS, sensors, and map screens</li>
              <li>Settings for mesh, display, BLE, GPS, and storage</li>
            </ul>
          </div>
          <div class="grid gap-3 border border-paper-line bg-[#fcfcfa] p-4">
            <p class="text-sm font-semibold uppercase tracking-[0.08em] text-paper-muted">Core Functions</p>
            <ul class="grid gap-2 text-sm leading-6 text-paper-muted">
              <li>Off-grid LoRa mesh messaging with an e-paper-first interface</li>
              <li>Explicit contact management and nearby node discovery</li>
              <li>Live battery, radio, and location status on-device</li>
              <li>Touch-driven navigation designed for calm, readable use outdoors</li>
            </ul>
          </div>
          <p class="text-sm leading-6 text-paper-muted">Use a USB data cable and open this page in a Web Serial capable browser.</p>
          <ol class="list-decimal space-y-1 pl-5 text-sm leading-6 text-paper-muted">
            <li>Put the board in bootloader mode if the browser cannot detect it.</li>
            <li>This page currently supports only the LilyGo T5 ePaper S3 Pro build.</li>
            <li>Choose erase when you want a clean install.</li>
          </ol>
        </div>
      </section>
    </main>
  </body>
</html>
"""


def main() -> None:
    args = parse_args()
    build_dir = Path(args.build_dir)
    output_dir = Path(args.output_dir)
    bootloader = build_dir / "bootloader.bin"
    partitions = build_dir / "partitions.bin"
    firmware = build_dir / "firmware.bin"
    boot_app0 = find_boot_app0(args.boot_app0)

    for path in (bootloader, partitions, firmware, boot_app0):
        if not path.is_file():
            raise FileNotFoundError(path)

    output_dir.mkdir(parents=True, exist_ok=True)
    merged_firmware = output_dir / "merged-firmware.bin"

    run(
        [
            "python",
            "-m",
            "esptool",
            "--chip",
            CHIP,
            "merge-bin",
            "-o",
            str(merged_firmware),
            "--flash-mode",
            FLASH_MODE,
            "--flash-freq",
            FLASH_FREQ,
            "--flash-size",
            FLASH_SIZE,
            BOOTLOADER_OFFSET,
            str(bootloader),
            PARTITIONS_OFFSET,
            str(partitions),
            BOOT_APP0_OFFSET,
            str(boot_app0),
            APP_OFFSET,
            str(firmware),
        ]
    )

    manifest = f"""{{
  "name": "{PROJECT_NAME}",
  "version": "{args.version}",
  "new_install_prompt_erase": true,
  "builds": [
    {{
      "chipFamily": "{CHIP_FAMILY}",
      "parts": [
        {{
          "path": "merged-firmware.bin",
          "offset": 0
        }}
      ]
    }}
  ]
}}
"""

    (output_dir / "manifest.json").write_text(manifest, encoding="utf-8")
    (output_dir / "index.html").write_text(build_page(args.version, args.repo_url), encoding="utf-8")

    shutil.copy2(merged_firmware, output_dir / f"{PROJECT_SLUG}-{args.version}.bin")


if __name__ == "__main__":
    main()
