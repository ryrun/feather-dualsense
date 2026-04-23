#pragma once

#include <stdint.h>
#include "build_config.h"

namespace device_out {

enum ReportId : uint8_t {
  kReportIdKeyboard = 1,
  kReportIdMouse = 2,
  kReportIdGamepad = 0,  // Switch Pro Controller: no Report ID in descriptor
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

// Nintendo Switch Pro Controller USB HID report (no Report ID prefix).
// Byte layout (7 bytes, matches kDescHidGamepad):
//   buttons (uint16): bits 0-13 active — see ParseForGamepad() for assignment
//   hat     (uint8):  bits 0-3 = hat (0=N…7=NW, 8=center), bits 4-7 = 0
//   left_x, left_y (uint8, 0-255, 128=center): left stick
//   right_x, right_y (uint8, 0-255, 128=center): right stick
//   L2/ZL and R2/ZR are digital buttons (bits 6 and 7), NOT separate axes.
//
// Button bit assignments (SDL2 GameControllerDB):
//   bit  0 = A        (Cross)       a:b0
//   bit  1 = B        (Circle)      b:b1
//   bit  2 = X        (Square)      x:b2
//   bit  3 = Y        (Triangle)    y:b3
//   bit  4 = L / ZL   (L1 / L2)    leftshoulder:b4 / lefttrigger:b6
//   bit  5 = R / ZR   (R1 / R2)    rightshoulder:b5 / righttrigger:b7
//   bit  6 = ZL       (L2 digital)
//   bit  7 = ZR       (R2 digital)
//   bit  8 = Minus    (Create)      back:b8
//   bit  9 = Plus     (Options)     start:b9
//   bit 10 = L3       (L3)          leftstick:b10
//   bit 11 = R3       (R3)          rightstick:b11
//   bit 12 = Home     (PS)          guide:b12
//   bit 13 = Capture  (Touchpad)    misc1:b13
struct GamepadReport {
  uint16_t buttons;  // bits 0-13 active, bits 14-15 padding (must be 0)
  uint8_t hat;       // bits 0-3: hat value, bits 4-7: 0
  uint8_t left_x;
  uint8_t left_y;
  uint8_t right_x;
  uint8_t right_y;
} __attribute__((packed));

void Init();
void Task();
bool SendKeyboard(const KeyboardReport& report);
bool SendMouse(const MouseReport& report);
bool SendGamepad(const GamepadReport& report);

}  // namespace device_out


