# feather-dualsense

Lean RP2040 firmware for **Adafruit Feather 2024 USB HOST A** that accepts only wired Sony DualSense controllers and remaps fixed button presses to USB HID Keyboard + Mouse output.

## Goals

- Single-board target: `PICO_BOARD=feather_host`.
- Strict compile-time whitelist of supported controllers.
- No runtime config UI, no web UI, and no Python configuration tools in this repository.
- Low-latency hot path with static memory and bitmask parsing.
- CI-generated `.uf2` artifact for flashing.

## Supported controller whitelist (runtime)

Only these VID/PID pairs are accepted:

- `0x054C:0x0CE6` — Sony DualSense (PS5)
- `0x054C:0x0CE7` — Sony DualSense Edge

Any other USB HID controller is ignored by host input processing.

## Default fixed mapping (`src/mapping.h`)

- Cross (X) → Mouse Left Click
- Circle (O) → Mouse Right Click
- Square → `Q`
- Triangle → `E`
- L1 → Left Shift modifier
- R1 → Left Ctrl modifier
- Options → `Esc`
- Create / Share → `Tab`
- D-pad Up/Right/Down/Left → Arrow Up/Right/Down/Left

Buttons not present in the mapping table are ignored.

## Build

See [docs/BUILD.md](docs/BUILD.md).

Quick local build:

```bash
git submodule update --init --recursive
mkdir -p build && cd build
PICO_BOARD=feather_host cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
```

Expected output: `build/feather_dualsense.uf2`

## Flash

1. Hold **BOOTSEL** on Feather and connect USB.
2. Copy `feather_dualsense.uf2` to the mounted mass-storage volume.
3. Reboot and connect controller to Feather host port; connect Feather device port to PC.

## How to update mapping / whitelist

- Edit static tables in `src/mapping.h`.
- Rebuild firmware; there is no runtime remap.

## Verification notes

Use `lsusb` to verify controller VID/PID before testing acceptance/ignore behavior.

Examples:

```bash
lsusb | grep -i sony
```

## Acknowledgements

Implementation patterns were inspired by TinyUSB host/device HID examples and jfedor2/hid-remapper structure ideas, but this codebase is a minimal re-implementation tailored to this project and does not copy large external files verbatim.
