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

  while (true) {
    device_out::Task();
    host_reader::Task();

    const bool boot_pressed = mode::ReadBootselButton();
    if (boot_pressed && !boot_was_pressed) {
      // Leading edge: record press start time.
      boot_press_start = board_millis();
    }
    if (!boot_pressed && boot_was_pressed) {
      // Trailing edge: switch only if it was a short press.
      const uint32_t held_ms = board_millis() - boot_press_start;
      if (held_ms < kBootselShortPressMaxMs) {
        mode::ToggleAndReboot();
      }
    }
    boot_was_pressed = boot_pressed;
  }
}

