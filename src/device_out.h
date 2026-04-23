#pragma once

#include <stdint.h>
#include "build_config.h"

namespace device_out {

enum ReportId : uint8_t {
  kReportIdKeyboard = 1,
  kReportIdMouse = 2,
  kReportIdGamepad = 1,  // gamepad mode uses instance 0 with its own descriptor
};

struct KeyboardReport {
  uint8_t modifiers;
  uint8_t reserved;
  uint8_t keycode[KEYBOARD_ROLLOVER_KEYS];
} __attribute__((packed));

struct MouseReport {
  uint8_t buttons;
  int16_t x;
  int16_t y;
  int8_t wheel;
  int8_t pan;
} __attribute__((packed));

// Stadia-compatible gamepad report.
// Layout: 14 buttons (bits 0-13) + 2 padding bits, 4-bit hat + 4 padding bits,
//         left X/Y (int16), right X/Y (int16), L2/R2 (uint8).
struct GamepadReport {
  uint16_t buttons;  // bits 0-13 active, 14-15 padding
  uint8_t hat;       // bits 0-3: 0=N,1=NE,…,7=NW, 0xF=centered; bits 4-7: 0
  int16_t left_x;
  int16_t left_y;
  int16_t right_x;
  int16_t right_y;
  uint8_t l2;
  uint8_t r2;
} __attribute__((packed));

void Init();
void Task();
bool SendKeyboard(const KeyboardReport& report);
bool SendMouse(const MouseReport& report);
bool SendGamepad(const GamepadReport& report);

}  // namespace device_out


