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
DEVICE_NAME = "LilyGo T5 ePaper Pro"
PROJECT_NAME = "t-paper"


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
    <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>
    <style>
      :root {{
        color-scheme: light;
        --bg: #f7f3ea;
        --panel: #fffdf8;
        --text: #16120c;
        --muted: #6e6252;
        --line: #d9cfbf;
        --accent: #16120c;
        --esp-tools-button-color: #16120c;
        --esp-tools-button-text-color: #fffdf8;
        --esp-tools-button-border-radius: 999px;
      }}

      * {{
        box-sizing: border-box;
      }}

      body {{
        margin: 0;
        min-height: 100vh;
        font-family: "Iowan Old Style", "Palatino Linotype", "Book Antiqua", serif;
        color: var(--text);
        background:
          radial-gradient(circle at top left, #efe4d0 0, transparent 28rem),
          linear-gradient(180deg, #fbf8f1 0%, var(--bg) 100%);
      }}

      main {{
        width: min(46rem, calc(100vw - 2rem));
        margin: 0 auto;
        padding: 3rem 0 4rem;
      }}

      .card {{
        background: color-mix(in srgb, var(--panel) 92%, white);
        border: 1px solid var(--line);
        border-radius: 1.5rem;
        box-shadow: 0 1rem 3rem rgba(22, 18, 12, 0.08);
        overflow: hidden;
      }}

      .hero {{
        padding: 2rem;
        border-bottom: 1px solid var(--line);
      }}

      .eyebrow {{
        margin: 0 0 0.75rem;
        font-size: 0.85rem;
        font-weight: 700;
        letter-spacing: 0.08em;
        text-transform: uppercase;
        color: var(--muted);
      }}

      h1 {{
        margin: 0;
        font-size: clamp(2.2rem, 6vw, 4rem);
        line-height: 0.95;
      }}

      .lede {{
        margin: 1rem 0 0;
        max-width: 34rem;
        font-size: 1.1rem;
        line-height: 1.6;
        color: var(--muted);
      }}

      .body {{
        display: grid;
        gap: 1.5rem;
        padding: 2rem;
      }}

      .meta {{
        display: flex;
        flex-wrap: wrap;
        gap: 0.75rem;
      }}

      .pill {{
        padding: 0.55rem 0.9rem;
        border: 1px solid var(--line);
        border-radius: 999px;
        font-size: 0.95rem;
        background: #fff;
      }}

      .actions {{
        display: flex;
        flex-wrap: wrap;
        gap: 1rem;
        align-items: center;
      }}

      .hint, .notes {{
        margin: 0;
        color: var(--muted);
        line-height: 1.6;
      }}

      .notes {{
        padding-left: 1.1rem;
      }}

      a {{
        color: inherit;
      }}

      @media (max-width: 640px) {{
        main {{
          padding-top: 1rem;
        }}

        .hero,
        .body {{
          padding: 1.25rem;
        }}
      }}
    </style>
  </head>
  <body>
    <main>
      <section class="card">
        <div class="hero">
          <p class="eyebrow">Browser Flasher</p>
          <h1>{PROJECT_NAME}</h1>
          <p class="lede">Flash the latest PlatformIO build for the LilyGo T5 ePaper Pro directly from Chrome or Edge with ESP Web Tools.</p>
        </div>
        <div class="body">
          <div class="meta">
            <span class="pill">Device: {DEVICE_NAME}</span>
            <span class="pill">Chip: {CHIP_FAMILY}</span>
            <span class="pill">Build: {safe_version}</span>
          </div>
          <div class="actions">
            <esp-web-install-button manifest="manifest.json"></esp-web-install-button>
            <a href="{safe_repo_url}">Repository</a>
          </div>
          <p class="hint">Use a USB data cable and open this page in a Web Serial capable browser.</p>
          <ol class="notes">
            <li>Put the board in bootloader mode if the browser cannot detect it.</li>
            <li>This page currently supports only the LilyGo T5 ePaper Pro build.</li>
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

    shutil.copy2(merged_firmware, output_dir / f"{PROJECT_NAME}-{args.version}.bin")


if __name__ == "__main__":
    main()
