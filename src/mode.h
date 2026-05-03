#pragma once

#include <stdint.h>

#include "build_config.h"

namespace mode {

enum class Mode : uint8_t {
  kKeyboardMouse = 0,
  kGamepad = 1,  // Stadia Controller
  kHybrid = 2,   // Stadia Controller + touch-activated gyro mouse
#if FEATHER_ENABLE_DUALSHOCK4_MODE
  kDualShock4 = 3,
#endif
  kCount,
};

void Init();
Mode GetActive();

// Toggles the active mode in RAM only and returns the new mode.
Mode ToggleRuntime();

// Toggles the active mode, writes it to flash, and returns the new mode.
Mode ToggleAndSave();

}  // namespace mode
