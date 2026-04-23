# Build and test instructions

## Prerequisites (Ubuntu)

```bash
sudo apt update
sudo apt install -y --no-install-recommends \
  gcc-arm-none-eabi libnewlib-arm-none-eabi srecord cmake build-essential
```

Fetch pico-sdk in one of two ways:

1) Recommended: submodule

```bash
git submodule update --init --recursive
```

2) Or let CMake fetch automatically (default `PICO_SDK_FETCH_FROM_GIT=ON`) if `./pico-sdk` is not present.

## Configure and build

```bash
mkdir -p build
cd build
PICO_BOARD=feather_host cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
```

Artifact:

- `build/feather_dualsense.uf2`

## Debug build with UART logging

```bash
mkdir -p build-debug
cd build-debug
PICO_BOARD=feather_host cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_DEBUG=ON
make -j"$(nproc)"
```

## Flashing

- Enter BOOTSEL mode on the Feather.
- Copy `.uf2` file to the mounted USB drive.

## Verify controller VID/PID with lsusb

```bash
lsusb
lsusb -d 054c:
```

Expected accepted devices:

- `054c:0ce6`
- `054c:0ce7`

Any other VID/PID is ignored.

## Input/output functional smoke test

1. Connect Feather USB device port to PC.
2. Connect DualSense/DualSense Edge (wired) to Feather host port.
3. Press mapped buttons and check keyboard/mouse events on PC.
4. Replace with non-whitelisted controller (for example Xbox) and confirm no forwarded key/mouse events.

## Linux debugging hints

### usbmon capture

```bash
sudo modprobe usbmon
sudo cat /sys/kernel/debug/usb/usbmon/1u
```

### usbhid-dump

```bash
sudo apt install -y usbhid-dump
sudo usbhid-dump -m 054c:0ce6 -es
```

Use debug build (`-DBUILD_DEBUG=ON`) to print report summary over UART while comparing host report activity.
