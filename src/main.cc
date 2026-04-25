#include "bsp/board_api.h"
#include "device_out.h"
#include "host_reader.h"
#include "mode.h"

// A press shorter than this threshold triggers a mode switch on release.
// Presses at or above the threshold are treated as BOOTSEL holds and ignored.
static constexpr uint32_t kBootselShortPressMaxMs = 500;

int main() {
  // Mode must be read before device_out::Init() triggers USB enumeration.
  mode::Init();
  device_out::Init();
  host_reader::Init();

  uint32_t boot_press_start = 0;
  bool boot_was_pressed = false;
  uint32_t last_bootsel_check = 0;

  while (true) {
    device_out::Task();
    host_reader::Task();

    // ReadBootselButton() disables all interrupts for ~24 µs (QSPI CS tri-state
    // settle delay). Throttle to every 10 ms to keep the interrupt blackout
    // budget negligible relative to the 1 ms USB frame period.
    const uint32_t now = board_millis();
    if (now - last_bootsel_check < 10) {
      continue;
    }
    last_bootsel_check = now;

    const bool boot_pressed = mode::ReadBootselButton();
    if (boot_pressed && !boot_was_pressed) {
      boot_press_start = now;
    }
    if (!boot_pressed && boot_was_pressed) {
      if (now - boot_press_start < kBootselShortPressMaxMs) {
        mode::ToggleAndReboot();
      }
    }
    boot_was_pressed = boot_pressed;
  }
}

