# Feather DualSense HID Remapper

Embedded firmware for the Adafruit Feather RP2040 USB Host, Type A, product 5723. The firmware accepts exactly one wired Sony DualSense or DualSense Edge controller on the USB host port and exposes a profile-specific USB HID device to the PC:

- USB HID keyboard
- USB HID mouse
- Compile-time selected USB HID gamepad backend: Google Stadia Controller by default (VID `0x18D1`, PID `0x9400`) or DualShock 4 (VID `0x054C`, PID `0x09CC`)

The active logical profile controls which reports are sent:

- **KBM profile** – maps controller buttons to keyboard and mouse reports
- **Gamepad profile** – forwards controller state as gamepad reports
- **Hybrid profile** – forwards gamepad reports and adds touch-activated gyro mouse
- **Gyro Stick profile** – optional profile that forwards gamepad reports and maps touch-activated gyro to the right stick

Perform a **full-width touchpad swipe** (single finger, edge to edge) to switch profiles: left→right selects the next profile, right→left selects the previous profile. The default profile order is KBM → Gamepad → Hybrid → KBM. If `FEATHER_GYRO_STICK_PROFILE=ON` is configured, the order is KBM → Gamepad → Hybrid → Gyro Stick → KBM. Profile switches are written to flash and reboot the board so USB re-enumerates with the selected profile's HID interface set. Empty or invalid flash storage falls back to KBM profile.

There is no runtime configuration, UI, or configuration script. Mappings are compile-time tables in `src/mapping.h`.

## Motivation

The project started from the idea of making a wired DualSense or DualSense Edge feel like an Input Labs Alpakka-style gyro controller, using the Adafruit Feather RP2040 USB Host as a small standalone adapter. In this repository, "DualPakka" refers to that idea: PlayStation controller hardware with touch-activated gyro-to-mouse output and keyboard/mouse button mappings.

In KBM mode, touching the DualSense touchpad enables gyro aiming and sends gyro movement as relative mouse input. The rest of the controller is mapped to keyboard and mouse actions, so games see a regular keyboard and mouse instead of a controller. This avoids PC-side mapper software such as Steam Input or reWASD.

Responsiveness is a core design goal. The firmware forces the DualSense input endpoint to 1 ms and exposes 1 ms HID output reports, targeting a direct 1000 Hz input-to-output path.

A separate gamepad mode is included for games where native controller input works better and gyro is not useful, such as racing games. By default, it emulates a Google Stadia Controller because that profile works reliably for the author's macOS cloud gaming setup with Shadow PC and GeForce Now. It avoids PlayStation-controller mapping issues in Shadow PC when USB forwarding is not used, and avoids XInput-style duplicate-controller detection through SDL2. A DualShock 4 backend can be selected at compile time for testing. Rumble and DualSense-specific features are intentionally out of scope.

## Hardware

- Target board: Adafruit Feather RP2040 USB Host, Type A, 5723
- Pico board variable: `PICO_BOARD=feather_host`
- Host wiring: PIO USB D+ on GPIO 16, D− on GPIO 17, VBUS enable on GPIO 18
- Input: wired USB controller on the Feather USB host port
- Output: profile-specific HID device to the PC

## Supported Controllers

Only these VID/PID pairs are accepted:

| Controller | VID | PID |
| --- | --- | --- |
| Sony DualSense | `0x054C` | `0x0CE6` |
| Sony DualSense Edge | `0x054C` | `0x0CE7` |
| Sony DualSense Edge (alt) | `0x054C` | `0x0DF2` |

All other USB devices are ignored.

## HID Output

### KBM profile

The Feather enumerates keyboard and mouse HID interfaces:

- **Keyboard**: 14KRO keyboard report
- **Mouse**: relative mouse with 16-bit signed X/Y axes

### Gamepad profile

The Feather enumerates only a gamepad HID interface that mimics the selected gamepad backend. Stadia Controller is the default backend; DualShock 4 can be selected at compile time. Analog sticks, D-pad hat, and all digital buttons are forwarded.

### Hybrid profile

Hybrid profile enumerates keyboard, mouse, and gamepad HID interfaces. It uses the same gamepad mapping as Gamepad profile and additionally sends touch-activated gyro movement as relative mouse X/Y. Pressing the Mute button sends `F12` in this profile for screenshot shortcuts. Other controller buttons are not mapped to keyboard actions yet, but the keyboard interface is present for future hybrid mappings.

On macOS, Hybrid profile currently works with the Stadia backend only. The DualShock 4 firmware can still be built, but its mixed mouse+gamepad Hybrid profile does not work reliably on macOS at this time.

### Gyro Stick profile

Gyro Stick profile is enabled with `-DFEATHER_GYRO_STICK_PROFILE=ON`, which sets `GYRO_STICK_PROFILE_ENABLE` at compile time. It uses the same gamepad mapping as Gamepad profile. While the touchpad is touched, gyro movement is mapped to the right analog stick instead of mouse X/Y. When the touchpad is not touched, the physical right stick is forwarded normally.

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
| Touchpad full-width swipe left→right / right→left (single finger) | Next / previous profile |

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

Perform a **full-width touchpad swipe** (single finger from one edge to the other, ≥ ~80 % of pad width) to switch profiles. Left→right selects the next profile; right→left selects the previous profile. The default order is KBM, Gamepad, and Hybrid. If `FEATHER_GYRO_STICK_PROFILE` is enabled at configure time, Gyro Stick profile is added after Hybrid. The selected profile is saved to flash and the board reboots so the USB HID interfaces match the profile.

The swipe gesture works in all profiles. A second finger on the pad at any point during the swipe cancels it.

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

The build produces one flashable artifact per gamepad backend:

```sh
build/feather_remapper_stadia.uf2
build/feather_remapper_ds4.uf2
```

## Flash

Put the Feather into BOOTSEL mode and copy the UF2 file to the mounted RP2040 mass-storage volume.

## Verification

Check that the Feather enumerates correctly:

```sh
# Stadia backend — expect profile-dependent HID interfaces
lsusb -v -d 18d1:9400

# DualShock 4 backend — expect profile-dependent HID interfaces
lsusb -v -d 054c:09cc
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
