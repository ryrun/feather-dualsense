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

LTO is disabled by default (`FEATHER_REMAPPER_ENABLE_LTO=OFF`) because `-flto` is incompatible with the pico-sdk's `--wrap` linker symbol mechanism.

This composite HID experiment supports KBM, Stadia gamepad, and hybrid gamepad+gyro-mouse profiles. DualShock 4 output mode is intentionally disabled on this branch.

Profile persistence is temporarily disabled on this branch while runtime switching is being tested. The saved flash profile is still read on boot, but swipe changes are RAM-only.

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

1. Flash `build/feather_remapper.uf2`.
2. Connect the Feather device USB port to the PC.
3. Connect a wired DualSense or DualSense Edge to the Feather USB host Type-A port.
4. Confirm the PC sees a keyboard and mouse.
5. Press Cross for `F`, Circle for `V`, Square for `R`, Triangle for `T`.
6. Press L2 for right mouse button and R2 for left mouse button.
7. Touch the touchpad and move the controller to confirm gyro mouse movement.
8. Perform a full-width touchpad swipe and confirm the device switches to gamepad profile without USB re-enumeration.
9. Perform a second full-width touchpad swipe and confirm the purple hybrid profile sends gamepad input plus touch-activated gyro mouse.

## Linux HID Inspection

```sh
lsusb
lsusb -v -d 18d1:9400
sudo usbhid-dump
sudo modprobe usbmon
sudo wireshark
```

For latency checks, use a Release build and keep UART debug disabled.
