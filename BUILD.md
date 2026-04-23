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

## macOS Local Toolchain

Homebrew's `arm-none-eabi-gcc` formula may not include `nosys.specs`. If that happens, use Arm's full GNU toolchain and point CMake at it:

```sh
mkdir -p .toolchains
curl -L --fail -o .toolchains/arm-gnu-toolchain.pkg \
  https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/arm-gnu-toolchain-15.2.rel1-darwin-arm64-arm-none-eabi.pkg
pkgutil --expand-full .toolchains/arm-gnu-toolchain.pkg .toolchains/arm-expanded

rm -rf build
mkdir build
cd build
PICO_BOARD=feather_host cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DFEATHER_REMAPPER_ENABLE_LTO=OFF \
  -DPICO_TOOLCHAIN_PATH="$PWD/../.toolchains/arm-expanded/Payload" \
  ..
make -j$(sysctl -n hw.ncpu)
```

`FEATHER_REMAPPER_ENABLE_LTO=OFF` is a local workaround for Arm GNU Toolchain 15.x on macOS producing unresolved Pico stdio wrapper symbols during LTO. CI keeps LTO enabled by default.

## Smoke Test

1. Flash `build/feather_remapper.uf2`.
2. Connect the Feather device USB port to the PC.
3. Connect a wired DualSense or DualSense Edge to the Feather USB host Type-A port.
4. Confirm the PC sees a keyboard and mouse.
5. Press Cross for left click, Circle for right click, Square for `Q`, Triangle for `E`.

## Linux HID Inspection

```sh
lsusb
lsusb -v -d cafe:4023
sudo usbhid-dump
sudo modprobe usbmon
sudo wireshark
```

For latency checks, use a Release build and keep UART debug disabled.
