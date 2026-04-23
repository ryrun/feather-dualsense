# Feather DualSense HID Remapper

Embedded firmware for the Adafruit Feather RP2040 USB Host, Type A, product 5723. The firmware accepts exactly one wired Sony DualSense or DualSense Edge controller on the USB host port and maps selected buttons to a USB HID composite keyboard and mouse exposed to the PC.

There is no runtime configuration, UI, or configuration script. Mappings are compile-time tables in `src/mapping.h`.

## Hardware

- Target board: Adafruit Feather RP2040 USB Host, Type A, 5723
- Pico board variable: `PICO_BOARD=feather_host`
- Host wiring: PIO USB D+ on GPIO 16, D- on GPIO 17, VBUS enable on GPIO 18
- Input: wired USB controller on the Feather USB host port
- Output: HID composite device to the PC

## Supported Controllers

Only these VID/PID pairs are accepted:

| Controller | VID | PID |
| --- | --- | --- |
| Sony DualSense | `0x054C` | `0x0CE6` |
| Sony DualSense Edge | `0x054C` | `0x0CE7` |

All other USB devices are ignored.

## HID Output

The device enumerates as one HID composite USB device with two HID interfaces:

- Keyboard: custom 14KRO keyboard report
- Mouse: relative mouse report with 16-bit signed X/Y axes

The initial mapping only uses keyboard keys, modifiers, and mouse buttons. Mouse movement is intentionally not mapped yet.

## Default Mapping

| Controller Button | Output |
| --- | --- |
| Cross / X | `F` |
| Circle / O | `V` |
| Square | `R` |
| Triangle | `T` |
| L1 | `Q` |
| R1 | `E` |
| L2 | Right mouse button |
| R2 | Left mouse button |
| Options | Escape |
| Create / Share | Tab |
| L3 | `J` |
| R3 | `0` |
| Touchpad click | `M` |
| PS button | Enter |
| Mute button | F12 |
| DualSense Edge left rear paddle | Left Shift |
| DualSense Edge right rear paddle | Space |
| D-Pad Up | Arrow Up |
| D-Pad Right | Arrow Right |
| D-Pad Down | Arrow Down |
| D-Pad Left | Arrow Left |
| Gyro while touchpad is touched | Relative mouse X/Y |

Unmapped buttons are ignored.

Gyro mouse movement is gated by touchpad contact. With no active touch contact,
gyro motion is ignored and the fractional mouse accumulator is reset. The
default gyro path uses a raw deadzone of `10`, a base sensitivity of about
`-0.003`, and a fixed-point soft curve for sub-1-pixel movement.

## Local Build

Install the ARM toolchain, CMake, Make, the Pico SDK, and Pico-PIO-USB. Either set `PICO_SDK_PATH` or check out `pico-sdk` at the repository root. Pico-PIO-USB must be available at `pico-sdk/lib/tinyusb/hw/mcu/raspberry_pi/Pico-PIO-USB`.

```sh
git submodule update --init --recursive
mkdir -p pico-sdk/lib/tinyusb/hw/mcu/raspberry_pi
git clone --depth 1 https://github.com/sekigon-gonnoc/Pico-PIO-USB.git \
  pico-sdk/lib/tinyusb/hw/mcu/raspberry_pi/Pico-PIO-USB
mkdir build
cd build
PICO_BOARD=feather_host cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

The flashable artifact is:

```sh
build/feather_remapper.uf2
```

## Flash

Put the Feather into BOOTSEL mode and copy the UF2 file to the mounted RP2040 mass-storage volume.

## Verification

On Linux, check that the Feather enumerates as a composite HID keyboard and mouse:

```sh
lsusb
lsusb -v -d cafe:4023
```

Check the connected controller VID/PID before testing:

```sh
lsusb -d 054c:
```

Inspect HID traffic:

```sh
sudo usbhid-dump
sudo modprobe usbmon
sudo wireshark
```

The HID endpoint descriptors should show a 1 ms polling interval for the keyboard and mouse interrupt IN endpoints.

## Debug Build

UART debug logging is disabled by default. Enable it for development:

```sh
PICO_BOARD=feather_host cmake -DCMAKE_BUILD_TYPE=Debug -DFEATHER_REMAPPER_DEBUG=1 ..
make -j$(nproc)
```

Debug builds print raw HID reports to UART. Do not use debug logging for latency testing.

## Modify Mapping

Edit `src/mapping.h`. The mapping is a static compile-time table of `Action` entries indexed by logical controller buttons. No JSON, EEPROM, filesystem, or runtime configuration is used.

## Notes

The DualSense wired USB report parser targets the common `0x01` input report layout. If a future controller firmware changes button positions, the whitelist will still prevent unsupported devices, but the parser may need adjustment.
