#pragma once

#include <stdint.h>

#include "build_config.h"

namespace mode {

enum class Mode : uint8_t {
  kKeyboardMouse = 0,
  kGamepad = 1,  // Stadia Controller
  kHybrid = 2,   // Stadia Controller + touch-activated gyro mouse
  kCount,
};

void Init();
Mode GetActive();

// Toggles the active mode and returns the new mode. The profile is runtime-only.
Mode ToggleRuntime();

}  // namespace mode
