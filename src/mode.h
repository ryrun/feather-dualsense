#pragma once

#include <stdint.h>

namespace mode {

enum class Mode : uint8_t {
  kKeyboardMouse = 0,
  kGamepad = 1,
  kCount,
};

void Init();
Mode GetActive();

// Toggles the active mode, writes it to flash, and reboots the device.
// Does not return.
[[noreturn]] void ToggleAndReboot();

}  // namespace mode
