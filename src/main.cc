#include "bsp/board_api.h"
#include "device_out.h"
#include "host_reader.h"
#include "mode.h"

// Hold BOOTSEL for this many milliseconds to switch modes and reboot.
static constexpr uint32_t kBootselHoldMs = 1000;

int main() {
  // Mode must be read before device_out::Init() triggers USB enumeration.
  mode::Init();
  device_out::Init();
  host_reader::Init();

  uint32_t boot_press_start = 0;
  bool boot_was_pressed = false;

  while (true) {
    device_out::Task();
    host_reader::Task();

    const bool boot_pressed = mode::ReadBootselButton();
    if (boot_pressed && !boot_was_pressed) {
      boot_press_start = board_millis();
    }
    if (boot_pressed && board_millis() - boot_press_start >= kBootselHoldMs) {
      mode::ToggleAndReboot();
    }
    boot_was_pressed = boot_pressed;
  }
}

