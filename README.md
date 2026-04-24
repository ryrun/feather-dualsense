# Feather DualSense HID Remapper

Embedded firmware for the Adafruit Feather RP2040 USB Host, Type A, product 5723. The firmware accepts exactly one wired Sony DualSense or DualSense Edge controller on the USB host port and operates in one of two modes:

- **KBM mode** – maps controller buttons to a USB HID keyboard and mouse
- **Gamepad mode** – emulates a Google Stadia Controller (VID `0x18D1`, PID `0x9400`) as a USB gamepad

The active mode is persisted in flash and survives power cycles. Press the **BOOT** button on the Feather board to toggle between modes (the device reboots).

There is no runtime configuration, UI, or configuration script. Mappings are compile-time tables in `src/mapping.h`.

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
| Touchpad full-width swipe (left→right or right→left, single finger) | Toggle KBM ↔ Gamepad mode |

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

Press the **BOOT** button on the Feather board, or perform a **full-width touchpad swipe** (single finger from one edge to the other, ≥ ~80 % of pad width) to toggle between KBM and Gamepad mode. The device saves the new mode to flash and reboots.

The swipe gesture works in both modes. A second finger on the pad at any point during the swipe cancels it.

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
