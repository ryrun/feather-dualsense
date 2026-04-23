#pragma once

#include <stdint.h>

// Minimal USB HID helper constants used by this project.
constexpr uint8_t HID_MOD_LCTRL = 0x01;
constexpr uint8_t HID_MOD_LSHIFT = 0x02;

constexpr uint8_t HID_KEY_Q = 0x14;
constexpr uint8_t HID_KEY_E = 0x08;
constexpr uint8_t HID_KEY_ESCAPE = 0x29;
constexpr uint8_t HID_KEY_TAB = 0x2B;
constexpr uint8_t HID_KEY_RIGHT = 0x4F;
constexpr uint8_t HID_KEY_LEFT = 0x50;
constexpr uint8_t HID_KEY_DOWN = 0x51;
constexpr uint8_t HID_KEY_UP = 0x52;

constexpr uint8_t HID_MOUSE_BTN_LEFT = 0x01;
constexpr uint8_t HID_MOUSE_BTN_RIGHT = 0x02;
