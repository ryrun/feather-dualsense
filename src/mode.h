#pragma once

#include <stdint.h>

#include "build_config.h"

namespace mode {

enum class Mode : uint8_t {
  kKeyboardMouse = 0,
  kGamepad = 1,    // Gamepad backend
  kHybrid = 2,     // Gamepad backend + touch-activated gyro mouse
#if GYRO_STICK_PROFILE_ENABLE
  kGyroStick = 3,  // Gamepad backend + touch-activated gyro right stick
#endif
  kCount,
};

void Init();
Mode GetActive();

// Toggles the active mode and returns the new mode. The profile is runtime-only.
Mode ToggleRuntime();

}  // namespace mode
