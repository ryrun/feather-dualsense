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
Mode Next(Mode current);
Mode Previous(Mode current);

// Switches the active mode in RAM and returns the new mode.
Mode NextRuntime();
Mode PreviousRuntime();

// Persists a mode to flash and reboots so USB re-enumerates with the
// selected profile's HID interface set.
void PersistAndReboot(Mode mode);

}  // namespace mode
