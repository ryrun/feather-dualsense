# Feather DualSense HID Remapper

Embedded firmware for the Adafruit Feather RP2040 USB Host, Type A, product 5723. The firmware accepts exactly one wired Sony DualSense or DualSense Edge controller on the USB host port and operates in one of two default modes:

- **KBM mode** – maps controller buttons to a USB HID keyboard and mouse
- **Gamepad mode** – emulates a Google Stadia Controller (VID `0x18D1`, PID `0x9400`) as a USB gamepad

An optional **DualShock 4 mode** can be compiled in for future testing. It emulates a Sony DualShock 4 v2 (VID `0x054C`, PID `0x09CC`) as a USB HID gamepad, but is disabled by default.

The active mode is persisted in flash and survives power cycles. Perform a **full-width touchpad swipe** (single finger, edge to edge) to cycle KBM → Gamepad → KBM. If DualShock 4 mode is compiled in, the cycle becomes KBM → Gamepad → DualShock 4 → KBM. The device saves the new mode and reboots.

There is no runtime configuration, UI, or configuration script. Mappings are compile-time tables in `src/mapping.h`.

## Motivation

The project started from the idea of making a wired DualSense or DualSense Edge feel like an Input Labs Alpakka-style gyro controller, using the Adafruit Feather RP2040 USB Host as a small standalone adapter. In this repository, "DualPakka" refers to that idea: PlayStation controller hardware with touch-activated gyro-to-mouse output and keyboard/mouse button mappings.

In KBM mode, touching the DualSense touchpad enables gyro aiming and sends gyro movement as relative mouse input. The rest of the controller is mapped to keyboard and mouse actions, so games see a regular keyboard and mouse instead of a controller. This avoids PC-side mapper software such as Steam Input or reWASD.

Responsiveness is a core design goal. The firmware forces the DualSense input endpoint to 1 ms and exposes 1 ms HID output reports, targeting a direct 1000 Hz input-to-output path.

A separate gamepad mode is included for games where native controller input works better and gyro is not useful, such as racing games. It emulates a Google Stadia Controller because that profile works reliably for the author's macOS cloud gaming setup with Shadow PC and GeForce Now. It avoids PlayStation-controller mapping issues in Shadow PC when USB forwarding is not used, and avoids XInput-style duplicate-controller detection through SDL2. Rumble and DualSense-specific features are intentionally out of scope.

## Hardware

- Target board: Adafruit Feather RP2040 USB Host, Type A, 5723
- Pico board variable: `PICO_BOARD=feather_host`
- Host wiring: PIO USB D+ on GPIO 16, D− on GPIO 17, VBUS enable on GPIO 18
- Input: wired USB controller on the Feather USB host port
- Output: HID composite device to the PC

## Supported Controllers

Only these VID/PID pairs are accepted:

| Controller | VID | PID |
| --- | --- | --- |
| Sony DualSense | `0x054C` | `0x0CE6` |
| Sony DualSense Edge | `0x054C` | `0x0CE7` |
| Sony DualSense Edge (alt) | `0x054C` | `0x0DF2` |

All other USB devices are ignored.

## HID Output

### KBM mode

The Feather enumerates as a composite HID device with two interfaces:

- **Keyboard**: 14KRO keyboard report
- **Mouse**: relative mouse with 16-bit signed X/Y axes

### Gamepad mode

The Feather enumerates as a single HID gamepad that mimics the Google Stadia Controller USB HID descriptor (VID `0x18D1`, PID `0x9400`). Analog sticks, D-pad hat, and all digital buttons are forwarded.

### DualShock 4 mode

DualShock 4 mode is optional and is not part of the default build. When `FEATHER_ENABLE_DUALSHOCK4_MODE=ON`, the Feather can enumerate as a single HID gamepad that mimics a Sony DualShock 4 v2 controller (VID `0x054C`, PID `0x09CC`). Sticks, D-pad, face buttons, shoulders, stick clicks, PS, touchpad click, and analog triggers are forwarded. The DualSense lightbar is purple while this mode is active.

## KBM Mapping

### DualSense (standard)

| Controller Input | Output |
| --- | --- |
| Cross | `F` |
| Circle | `V` |
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
| Touchpad click (left third) | `M` |
| Touchpad click (middle third) | Middle mouse button |
| Touchpad click (right third) | `N` |
| PS button | Enter |
| Mute | F12 |
| D-Pad Up | Arrow Up |
| D-Pad Right | Arrow Right |
| D-Pad Down | Arrow Down |
| D-Pad Left | Arrow Left |
| Left stick (WASD zone) | `W` / `A` / `S` / `D` |
| Left stick (inner ring) | `L` |
| Right stick | Numpad `1`–`8` |
| Gyro (while touching touchpad) | Relative mouse X/Y |
| Touchpad vertical swipe | Scroll wheel |
| Touchpad full-width swipe (left→right or right→left, single finger) | Cycle KBM → Gamepad mode |

### DualSense Edge (additional / different)

| Controller Input | Output |
| --- | --- |
| Left rear paddle | Left Shift |
| Right rear paddle | Space |
| Fn1 | `X` |
| Fn2 | `Y` |

## Gamepad Mapping

Controller buttons are translated to the equivalent Stadia Controller buttons:

| DualSense | Stadia |
| --- | --- |
| Cross | A |
| Circle | B |
| Square | X |
| Triangle | Y |
| L1 | LB |
| R1 | RB |
| L2 | LT |
| R2 | RT |
| L3 | L3 |
| R3 | R3 |
| Create | Back |
| Options | Options |
| PS | Guide |
| D-Pad | D-Pad |
| Left stick | Left stick |
| Right stick | Right stick |
| Edge Fn1 | Assistant |
| Edge Fn2 | Capture |

Mute, Touchpad click, and Touchpad touch are not forwarded in gamepad mode.

## Gyro Mouse (KBM mode)

Gyro movement is mapped to relative mouse X/Y while at least one finger touches the touchpad. When no touch is active the gyro output is suppressed and the sub-pixel accumulator is reset.

**Bias correction** — While the controller is held still (all three gyro axes below threshold) and the touchpad is **not** being touched, a per-axis bias estimate is updated via an EMA (α ≈ 0.0025, ~0.4 s time constant at 1000 Hz). This continuously compensates for sensor drift without interfering with aiming.

**Soft suppression** — Instead of a hard deadzone, small residual values after bias subtraction are shaped with a quadratic fade-in (`output = v × |v| / threshold`, threshold = 15 raw units). Micro-movements are preserved rather than cut off.

**1000 Hz polling** — On mount, the firmware patches the DualSense interrupt IN endpoint interval to 1 ms (overriding the 4 ms default). The sensitivity constant is tuned accordingly.

Base sensitivity: `GYRO_MOUSE_SENSITIVITY_Q16 = -171` (~−0.0026 per raw unit at 1000 Hz).  
Axis scale factors: X = 1.0, Y = 0.7.

## Mode Switch

Perform a **full-width touchpad swipe** (single finger from one edge to the other, ≥ ~80 % of pad width) to cycle between KBM and Gamepad mode. The device saves the new mode to flash and reboots.

If the optional DualShock 4 mode is enabled at compile time, the switch cycle includes it as the third mode. If a firmware build without DualShock 4 support boots with an old saved DualShock 4 mode value in flash, it treats that value as invalid and falls back to KBM mode.

The swipe gesture works in all modes. A second finger on the pad at any point during the swipe cancels it.

## Local Build

Install the ARM toolchain, CMake, Make, the Pico SDK, and Pico-PIO-USB. Either set `PICO_SDK_PATH` or check out `pico-sdk` at the repository root. Pico-PIO-USB must be available at `pico-sdk/lib/tinyusb/hw/mcu/raspberry_pi/Pico-PIO-USB`.

**macOS:** Homebrew's `arm-none-eabi-gcc` does not include `nosys.specs`. Install the official ARM toolchain instead:

```sh
brew install --cask gcc-arm-embedded
sudo installer -pkg /opt/homebrew/Caskroom/gcc-arm-embedded/15.2.rel1/arm-gnu-toolchain-15.2.rel1-darwin-arm64-arm-none-eabi.pkg -target /
export PATH="/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/bin:$PATH"
# Add the export to ~/.zshrc to make it permanent
```

```sh
git submodule update --init --recursive
mkdir -p pico-sdk/lib/tinyusb/hw/mcu/raspberry_pi
git clone --depth 1 https://github.com/sekigon-gonnoc/Pico-PIO-USB.git \
  pico-sdk/lib/tinyusb/hw/mcu/raspberry_pi/Pico-PIO-USB
mkdir build
cd build
PICO_BOARD=feather_host cmake ..
make -j$(nproc)   # Linux
make -j$(sysctl -n hw.logicalcpu)   # macOS
```

Optional DualShock 4 output mode can be included with:

```sh
PICO_BOARD=feather_host cmake -DFEATHER_ENABLE_DUALSHOCK4_MODE=ON ..
```

The flashable artifact is:

```sh
build/feather_remapper.uf2
```

## Flash

Put the Feather into BOOTSEL mode and copy the UF2 file to the mounted RP2040 mass-storage volume.

## Verification

Check that the Feather enumerates correctly:

```sh
# KBM mode  — expect composite keyboard + mouse
lsusb -v -d cafe:4023

# Gamepad mode — expect Stadia Controller
lsusb -v -d 18d1:9400
```

Check the connected controller:

```sh
lsusb -d 054c:
```

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
