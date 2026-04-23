#pragma once

#include <stdint.h>
#include "class/hid/hid.h"

namespace usb {

constexpr uint8_t kMouseButtonLeft = 0x01;
constexpr uint8_t kMouseButtonRight = 0x02;
constexpr uint8_t kMouseButtonMiddle = 0x04;

constexpr uint8_t kKeyNone = 0x00;
constexpr uint8_t kKeyQ = HID_KEY_Q;
constexpr uint8_t kKeyR = HID_KEY_R;
constexpr uint8_t kKeyF = HID_KEY_F;
constexpr uint8_t kKeyE = HID_KEY_E;
constexpr uint8_t kKeyV = HID_KEY_V;
constexpr uint8_t kKeyT = HID_KEY_T;
constexpr uint8_t kKeyJ = HID_KEY_J;
constexpr uint8_t kKey0 = HID_KEY_0;
constexpr uint8_t kKeyM = HID_KEY_M;
constexpr uint8_t kKeyN = HID_KEY_N;
constexpr uint8_t kKeyW = HID_KEY_W;
constexpr uint8_t kKeyA = HID_KEY_A;
constexpr uint8_t kKeyS = HID_KEY_S;
constexpr uint8_t kKeyD = HID_KEY_D;
constexpr uint8_t kKeyL = HID_KEY_L;
constexpr uint8_t kKeyX = HID_KEY_X;
constexpr uint8_t kKeyY = HID_KEY_Y;
constexpr uint8_t kKey1 = HID_KEY_1;
constexpr uint8_t kKey2 = HID_KEY_2;
constexpr uint8_t kKey3 = HID_KEY_3;
constexpr uint8_t kKey4 = HID_KEY_4;
constexpr uint8_t kKey5 = HID_KEY_5;
constexpr uint8_t kKey6 = HID_KEY_6;
constexpr uint8_t kKey7 = HID_KEY_7;
constexpr uint8_t kKey8 = HID_KEY_8;
constexpr uint8_t kKeyEnter = HID_KEY_ENTER;
constexpr uint8_t kKeyEscape = HID_KEY_ESCAPE;
constexpr uint8_t kKeyTab = HID_KEY_TAB;
constexpr uint8_t kKeyF12 = HID_KEY_F12;
constexpr uint8_t kKeyArrowRight = HID_KEY_ARROW_RIGHT;
constexpr uint8_t kKeyArrowLeft = HID_KEY_ARROW_LEFT;
constexpr uint8_t kKeyArrowDown = HID_KEY_ARROW_DOWN;
constexpr uint8_t kKeyArrowUp = HID_KEY_ARROW_UP;
constexpr uint8_t kKeySpace = HID_KEY_SPACE;

constexpr uint8_t kModLeftCtrl = KEYBOARD_MODIFIER_LEFTCTRL;
constexpr uint8_t kModLeftShift = KEYBOARD_MODIFIER_LEFTSHIFT;

}  // namespace usb
