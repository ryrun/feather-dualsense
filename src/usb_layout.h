#pragma once

#include <stdint.h>
#include "build_config.h"

namespace usb_layout {

constexpr uint8_t kItfKeyboard = 0;
constexpr uint8_t kItfMouse = 1;
constexpr uint8_t kItfGamepad = 2;
constexpr uint8_t kItfGamepadConfig = 1;

#if FEATHER_STATUS_HID_ENABLE
constexpr uint8_t kItfStatusKbm = 2;
constexpr uint8_t kItfStatusGamepad =
#if FEATHER_GAMEPAD_BACKEND_DUALSHOCK4
    1;
#else
    2;
#endif
constexpr uint8_t kItfStatusHybrid = 3;
#endif

constexpr uint8_t kItfKbmTotal = 2 + FEATHER_STATUS_HID_ENABLE;
constexpr uint8_t kItfGamepadBaseTotal =
#if FEATHER_GAMEPAD_BACKEND_DUALSHOCK4
    1;
#else
    2;
#endif
constexpr uint8_t kItfGamepadTotal =
    kItfGamepadBaseTotal + FEATHER_STATUS_HID_ENABLE;
constexpr uint8_t kItfHybridTotal = 3 + FEATHER_STATUS_HID_ENABLE;

}  // namespace usb_layout
