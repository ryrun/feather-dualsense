#pragma once

#include <stdint.h>
#include "build_config.h"

namespace device_out {

enum ReportId : uint8_t {
  kReportIdKeyboard = 1,
  kReportIdMouse = 2,
  kReportIdGamepad = 3,  // Stadia Controller: Report ID 3
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

// Stadia Controller Rev. A USB HID report (Report ID 3, 10-byte payload).
// Byte layout matches kDescHidGamepad:
//   hat      (uint8):  bits 0-3 = hat (0=N…7=NW, 8=center/neutral), bits 4-7 padding
//   buttons1 (uint8):  bit0=Y(Triangle), bit1=X(Square), bit2=B(Circle), bit3=A(Cross),
//                      bit4=LB(L1), bit5=LT(L2-digital), bit6=RB(R1), bit7=RT(R2-digital)
//   buttons2 (uint8):  bit0=Menu(Options), bit1=Guide(PS), bit2=Assistant, bit3=Capture(Touchpad),
//                      bit4=L3, bit5=R3, bit6=Back(Create), bit7=padding
//   left_x, left_y, right_x, right_y (uint8, 1-255, 128=center; 0 is sanitized to 1)
//   brake    (uint8):  L2 analog, 0-255
//   accel    (uint8):  R2 analog, 0-255
//   consumer (uint8):  bit0=VolUp, bit1=VolDown, bit2=PlayPause, bits3-7=padding
struct GamepadReport {
  uint8_t hat;
  uint8_t buttons1;
  uint8_t buttons2;
  uint8_t left_x;
  uint8_t left_y;
  uint8_t right_x;
  uint8_t right_y;
  uint8_t brake;
  uint8_t accel;
  uint8_t consumer;
} __attribute__((packed));

void Init();
void Task();
bool SendKeyboard(const KeyboardReport& report);
bool SendMouse(const MouseReport& report);
bool SendGamepad(const GamepadReport& report);

}  // namespace device_out


