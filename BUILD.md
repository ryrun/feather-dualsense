# Build and Test Notes

## Dependencies

Ubuntu:

```sh
sudo apt update
sudo apt install -y --no-install-recommends \
  gcc-arm-none-eabi \
  libnewlib-arm-none-eabi \
  libstdc++-arm-none-eabi-newlib \
  srecord \
  cmake \
  make \
  build-essential
```

## Pico SDK

Use one of these approaches:

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
```

or keep `pico-sdk` checked out at the repository root.

PIO USB host support also needs Pico-PIO-USB at TinyUSB's expected path:

```sh
mkdir -p pico-sdk/lib/tinyusb/hw/mcu/raspberry_pi
git clone --depth 1 https://github.com/sekigon-gonnoc/Pico-PIO-USB.git \
  pico-sdk/lib/tinyusb/hw/mcu/raspberry_pi/Pico-PIO-USB
```

## Release Build

```sh
mkdir -p build
cd build
PICO_BOARD=feather_host cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

The default build produces both gamepad backend variants:

- `build/feather_remapper_stadia.uf2`
- `build/feather_remapper_ds4.uf2`

LTO is disabled by default (`FEATHER_REMAPPER_ENABLE_LTO=OFF`) because `-flto` is incompatible with the pico-sdk's `--wrap` linker symbol mechanism.

The firmware supports KBM, gamepad, and hybrid keyboard+mouse+gamepad profiles by default. The optional gyro-stick profile can be enabled at configure time. The gamepad backend is selected by flashing either the Stadia Controller UF2 or the DualShock 4 UF2.

The optional gyro-stick profile is disabled by default. Enable it at configure time with `-DFEATHER_GYRO_STICK_PROFILE=ON`.

Profile switching writes the selected profile to flash and reboots the board. On the next boot, USB re-enumerates with that profile's HID interface set. Empty or invalid flash storage falls back to KBM profile.

On macOS, use the Stadia backend for Hybrid profile testing. The DualShock 4 backend builds, but its mixed mouse+gamepad Hybrid profile currently does not work reliably on macOS.

## macOS Local Toolchain

Homebrew's `arm-none-eabi-gcc` formula does not include `nosys.specs` or the ARM newlib specs. Install the official ARM GNU Toolchain via the Homebrew cask and run the pkg installer:

```sh
brew install --cask gcc-arm-embedded
sudo installer -pkg /opt/homebrew/Caskroom/gcc-arm-embedded/15.2.rel1/arm-gnu-toolchain-15.2.rel1-darwin-arm64-arm-none-eabi.pkg -target /
```

Then prepend the toolchain to your PATH (add to `~/.zshrc` to make permanent):

```sh
export PATH="/Applications/ArmGNUToolchain/15.2.rel1/arm-none-eabi/bin:$PATH"
```

Build normally:

```sh
rm -rf build && mkdir build && cd build
PICO_BOARD=feather_host cmake ..
make -j$(sysctl -n hw.ncpu)
```

## Smoke Test

1. Flash `build/feather_remapper_stadia.uf2` or `build/feather_remapper_ds4.uf2`. Use the Stadia UF2 when testing Hybrid profile on macOS.
2. Connect the Feather device USB port to the PC.
3. Connect a wired DualSense or DualSense Edge to the Feather USB host Type-A port.
4. Confirm the PC sees a keyboard and mouse.
5. Press Cross for `F`, Circle for `V`, Square for `R`, Triangle for `T`.
6. Press L2 for right mouse button and R2 for left mouse button.
7. Touch the touchpad and move the controller to confirm gyro mouse movement.
8. Perform a left-to-right full-width touchpad swipe and confirm the board reboots and re-enumerates as the gamepad profile.
9. Perform another left-to-right full-width touchpad swipe and confirm the board reboots into the purple hybrid profile, sending gamepad input plus touch-activated gyro mouse. Press Mute and confirm it sends `F12`.
10. Perform a right-to-left full-width touchpad swipe and confirm the device switches back to gamepad profile.
11. If `FEATHER_GYRO_STICK_PROFILE=ON` was configured, switch forward to the green gyro-stick profile and confirm it sends gamepad input plus touch-activated gyro right-stick output.

## Linux HID Inspection

```sh
lsusb
lsusb -v -d 18d1:9400  # Stadia backend
lsusb -v -d 054c:09cc  # DualShock 4 backend
sudo usbhid-dump
sudo modprobe usbmon
sudo wireshark
```

For latency checks, use a Release build and keep UART debug disabled.
