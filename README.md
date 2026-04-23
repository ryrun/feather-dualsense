# feather-dualsense

Lean RP2040 firmware for **Adafruit Feather 2024 USB HOST A** that accepts only wired Sony DualSense controllers and remaps fixed button presses to USB HID Keyboard + Mouse output.

## Goals

- Single-board target: `PICO_BOARD=feather_host` (board definition provided in `boards/feather_host.*`).
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
git submodule update --init --recursive  # recommended
mkdir -p build && cd build
PICO_BOARD=feather_host cmake .. -DCMAKE_BUILD_TYPE=Release -DPICOTOOL_NO_LIBUSB=0
make -j"$(nproc)"
```

Expected output: `build/feather_dualsense.uf2`


On macOS, the device should enumerate as a USB HID composite device with one keyboard interface and one mouse interface.

The mouse HID report uses 16-bit relative axes for X/Y/Wheel/Pan.


## Host init notes (Feather USB host)

This firmware initializes RP2040 PIO USB host support and starts a 1 ms SOF timer (`pio_usb_host_frame()`) during startup, similar to the known working pattern used by hid-remapper on single-RP2040 host builds.

If your board revision requires explicit VBUS power switching, set `-DHOST_VBUS_EN_PIN=<gpio>` at CMake configure time. Default is `-1` (disabled).

If the pico-sdk used for build does not contain Pico-PIO-USB (`pio_usb.h`), the firmware build falls back to no PIO-host frame helper and host enumeration over PIO USB will not work until a compatible pico-sdk is used.

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


If `./pico-sdk` is absent, CMake can clone pico-sdk automatically (default `PICO_SDK_FETCH_FROM_GIT=ON`) using `--recurse-submodules` to pull required dependencies (TinyUSB and related components).

## Note

This project is also a vibecoding experiment built with ChatGPT Codex.

Picotool may print "compiled without USB support" if libusb development headers are missing; this does not block UF2 generation, but installing `libusb-1.0-0-dev` enables the USB-capable picotool build.


CI configures CMake with `-DPICOTOOL_NO_LIBUSB=0` and fails the job if picotool reports it was compiled without USB support.


CI now performs multiple USB-related checks: it builds a standalone host picotool with `-DPICOTOOL_NO_LIBUSB=0`, fails if that binary reports no USB support, and also validates firmware artifact generation (`.elf` + `.uf2`).
