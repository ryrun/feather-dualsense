#pragma once

#include <stddef.h>
#include <stdint.h>

#include "usb_helpers.h"

enum action_type : uint8_t {
  ACTION_NONE = 0,
  ACTION_KEY = 1,
  ACTION_MODIFIER = 2,
  ACTION_MOUSE_BUTTON = 3,
};

struct action_entry {
  action_type type;
  uint16_t value;
};

enum controller_button : uint8_t {
  BTN_CROSS = 0,
  BTN_CIRCLE,
  BTN_SQUARE,
  BTN_TRIANGLE,
  BTN_L1,
  BTN_R1,
  BTN_OPTIONS,
  BTN_CREATE,
  BTN_DPAD_UP,
  BTN_DPAD_RIGHT,
  BTN_DPAD_DOWN,
  BTN_DPAD_LEFT,
  BTN_COUNT
};

struct device_mapping {
  const action_entry* entries;
  size_t count;
};

struct allowed_device {
  uint16_t vid;
  uint16_t pid;
  const char* name;
  device_mapping mapping;
};

constexpr action_entry k_default_mapping[BTN_COUNT] = {
    {ACTION_MOUSE_BUTTON, HID_MOUSE_BTN_LEFT},   // Cross
    {ACTION_MOUSE_BUTTON, HID_MOUSE_BTN_RIGHT},  // Circle
    {ACTION_KEY, HID_KEY_Q},                     // Square
    {ACTION_KEY, HID_KEY_E},                     // Triangle
    {ACTION_MODIFIER, HID_MOD_LSHIFT},           // L1
    {ACTION_MODIFIER, HID_MOD_LCTRL},            // R1
    {ACTION_KEY, HID_KEY_ESCAPE},                // Options
    {ACTION_KEY, HID_KEY_TAB},                   // Create/Share
    {ACTION_KEY, HID_KEY_UP},                    // D-pad up
    {ACTION_KEY, HID_KEY_RIGHT},                 // D-pad right
    {ACTION_KEY, HID_KEY_DOWN},                  // D-pad down
    {ACTION_KEY, HID_KEY_LEFT},                  // D-pad left
};

constexpr allowed_device k_allowed_devices[] = {
    {0x054C, 0x0CE6, "DualSense", {k_default_mapping, BTN_COUNT}},
    {0x054C, 0x0CE7, "DualSense Edge", {k_default_mapping, BTN_COUNT}},
};

static_assert((sizeof(k_allowed_devices) / sizeof(k_allowed_devices[0])) == 2,
              "Whitelist must include exactly two supported controllers in v1.");
