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

// Stadia Controller USB HID report.
// Byte layout matches the real Stadia Controller rev. A (18d1:9400):
//   buttons (uint16): 2 bytes — see ParseForGamepad() for bit assignment
//   hat     (uint8):  bits 0-3 = hat (0=N…7=NW, 0xF=center), bits 4-7 = 0
//   left_x, left_y, right_x, right_y (uint8, 0-255, 128=center): sticks
//   r2 (uint8, 0-255): right trigger — Z,  SDL2 axis 4
//   l2 (uint8, 0-255): left  trigger — Rz, SDL2 axis 5
struct GamepadReport {
  uint16_t buttons;
  uint8_t hat;
  uint8_t left_x;
  uint8_t left_y;
  uint8_t right_x;
  uint8_t right_y;
  uint8_t r2;   // Z  — right trigger, SDL2 axis 4
  uint8_t l2;   // Rz — left trigger,  SDL2 axis 5
} __attribute__((packed));

void Init();
void Task();
bool SendKeyboard(const KeyboardReport& report);
bool SendMouse(const MouseReport& report);
bool SendGamepad(const GamepadReport& report);

}  // namespace device_out


