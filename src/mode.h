#pragma once

#include <stdint.h>

#include "build_config.h"

namespace mode {

enum class Mode : uint8_t {
  kKeyboardMouse = 0,
  kGamepad = 1,  // Stadia Controller
#if FEATHER_ENABLE_DUALSHOCK4_MODE
  kDualShock4 = 2,
#endif
  kCount,
};

void Init();
Mode GetActive();

// Toggles the active mode, writes it to flash, and reboots the device.
// Does not return.
[[noreturn]] void ToggleAndReboot();

}  // namespace mode
