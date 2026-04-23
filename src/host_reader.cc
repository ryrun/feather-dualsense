#include "host_reader.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "build_config.h"
#include "bsp/board_api.h"
#include "device_out.h"
#include "mapping.h"
#include "mode.h"
#include "pico/stdlib.h"
#include "tusb.h"

namespace {

constexpr uint kStatusLedPin = PICO_DEFAULT_LED_PIN;

struct ActiveController {
  bool active;
  bool allowed_device_mounted;
  uint8_t allowed_dev_addr;
  uint8_t dev_addr;
  uint8_t instance;
  const mapping::Action* actions;
  uint64_t buttons;
  int32_t gyro_mouse_x_remainder_q16;
  int32_t gyro_mouse_y_remainder_q16;
  bool touch0_active;
  uint8_t touch0_id;
  int16_t touch0_last_y;
  int16_t touch0_scroll_accum;
  bool touch1_active;
  uint8_t touch1_id;
  int16_t touch1_last_y;
  int16_t touch1_scroll_accum;
  bool touchpad_clicked;
  uint8_t touchpad_zone;
  device_out::KeyboardReport keyboard;
  device_out::MouseReport mouse;
};

ActiveController g_controller = {};

#if FEATHER_REMAPPER_DEBUG
void DebugPrintReport(uint8_t const* report, uint16_t len) {
  printf("hid report len=%u:", len);
  for (uint16_t i = 0; i < len; ++i) {
    printf(" %02x", report[i]);
  }
  printf("\n");
}
#else
void DebugPrintReport(uint8_t const*, uint16_t) {}
#endif

void ResetState() {
  memset(&g_controller, 0, sizeof(g_controller));
}

void ClearHidState() {
  g_controller.active = false;
  g_controller.dev_addr = 0;
  g_controller.instance = 0;
  g_controller.actions = nullptr;
  g_controller.buttons = 0;
  memset(&g_controller.keyboard, 0, sizeof(g_controller.keyboard));
  memset(&g_controller.mouse, 0, sizeof(g_controller.mouse));
}

void SetControllerLed(bool on) {
  board_led_write(on);
}

void RunLedSelfTest() {
#if STATUS_LED_SELFTEST_MS > 0
  SetControllerLed(true);
  sleep_ms(STATUS_LED_SELFTEST_MS);
  SetControllerLed(false);
#endif
}

bool AddKey(device_out::KeyboardReport* keyboard, uint8_t keycode) {
  if (keycode == 0) {
    return false;
  }

  for (uint8_t& slot : keyboard->keycode) {
    if (slot == keycode) {
      return false;
    }
  }

  for (uint8_t& slot : keyboard->keycode) {
    if (slot == 0) {
      slot = keycode;
      return true;
    }
  }

  return false;
}

bool RemoveKey(device_out::KeyboardReport* keyboard, uint8_t keycode) {
  for (uint8_t& slot : keyboard->keycode) {
    if (slot == keycode) {
      slot = 0;
      return true;
    }
  }

  return false;
}

void ApplyAction(const mapping::Action& action, bool pressed, bool* keyboard_changed, bool* mouse_changed) {
  switch (action.type) {
    case mapping::ActionType::kNone:
      return;

    case mapping::ActionType::kKey:
      if (pressed) {
        *keyboard_changed |= AddKey(&g_controller.keyboard, static_cast<uint8_t>(action.value));
      } else {
        *keyboard_changed |= RemoveKey(&g_controller.keyboard, static_cast<uint8_t>(action.value));
      }
      return;

    case mapping::ActionType::kModifier:
      if (pressed) {
        const uint8_t before = g_controller.keyboard.modifiers;
        g_controller.keyboard.modifiers |= static_cast<uint8_t>(action.value);
        *keyboard_changed |= before != g_controller.keyboard.modifiers;
      } else {
        const uint8_t before = g_controller.keyboard.modifiers;
        g_controller.keyboard.modifiers &= ~static_cast<uint8_t>(action.value);
        *keyboard_changed |= before != g_controller.keyboard.modifiers;
      }
      return;

    case mapping::ActionType::kMouseButton:
      if (pressed) {
        const uint8_t before = g_controller.mouse.buttons;
        g_controller.mouse.buttons |= static_cast<uint8_t>(action.value);
        *mouse_changed |= before != g_controller.mouse.buttons;
      } else {
        const uint8_t before = g_controller.mouse.buttons;
        g_controller.mouse.buttons &= ~static_cast<uint8_t>(action.value);
        *mouse_changed |= before != g_controller.mouse.buttons;
      }
      return;
  }
}

int16_t ReadLe16(uint8_t const* data) {
  return static_cast<int16_t>(static_cast<uint16_t>(data[0]) |
                              (static_cast<uint16_t>(data[1]) << 8));
}

int32_t Abs32(int32_t value) {
  return value < 0 ? -value : value;
}

int16_t ClampI16(int32_t value) {
  if (value > 32767) {
    return 32767;
  }
  if (value < -32768) {
    return -32768;
  }
  return static_cast<int16_t>(value);
}

int32_t ShapeGyroDeltaQ16(int16_t raw) {
  if (Abs32(raw) <= GYRO_MOUSE_DEADZONE) {
    return 0;
  }

  const int32_t scaled_q16 = static_cast<int32_t>(raw) * GYRO_MOUSE_SENSITIVITY_Q16;
  const int32_t abs_scaled_q16 = Abs32(scaled_q16);

  if (abs_scaled_q16 >= (1 << 16)) {
    return scaled_q16;
  }

  // Equivalent to sign(x) * (0.5 * |x|) / (1 - 0.5 * |x|), in Q16.
  const int32_t half_abs_q16 = abs_scaled_q16 >> 1;
  const int32_t denominator_q16 = (1 << 16) - half_abs_q16;
  if (denominator_q16 <= 0) {
    return scaled_q16;
  }

  const int32_t shaped_abs_q16 = (half_abs_q16 << 16) / denominator_q16;
  return scaled_q16 < 0 ? -shaped_abs_q16 : shaped_abs_q16;
}

int16_t ConsumeMouseDelta(int32_t delta_q16, int32_t* remainder_q16) {
  const int32_t accumulated_q16 = *remainder_q16 + delta_q16;
  const int32_t whole = accumulated_q16 / (1 << 16);
  const int16_t clamped = ClampI16(whole);
  *remainder_q16 = accumulated_q16 - (static_cast<int32_t>(clamped) << 16);
  return clamped;
}

bool ParseTouchActive(uint8_t const* report, uint16_t len) {
  // USB report ID 0x01 is followed by struct dualsense_input_report. Touch
  // points begin at struct offset 32; bit 7 in contact means inactive.
  const uint8_t report_base = (report[0] == 0x01) ? 1 : 0;
  constexpr uint8_t kTouchPointsOffset = 32;
  constexpr uint8_t kTouchPointSize = 4;
  const uint8_t point0 = report_base + kTouchPointsOffset;
  const uint8_t point1 = point0 + kTouchPointSize;
  if (len <= point1) {
    return false;
  }

  return ((report[point0] & 0x80) == 0) || ((report[point1] & 0x80) == 0);
}

// Updates tracking state for one touch point and accumulates Y delta.
void UpdateTouchPoint(uint8_t const* p, bool* active, uint8_t* id,
                      int16_t* last_y, int16_t* accum) {
  const bool finger_down = (p[0] & 0x80) == 0;
  const uint8_t touch_id = p[0] & 0x7F;

  if (!finger_down) {
    *active = false;
    *accum = 0;
    return;
  }

  const int16_t y = static_cast<int16_t>(
      (static_cast<uint16_t>(p[2] & 0xF0) >> 4) |
      (static_cast<uint16_t>(p[3]) << 4));

  if (!*active || *id != touch_id) {
    *active = true;
    *id = touch_id;
    *last_y = y;
    *accum = 0;
    return;
  }

  *accum += y - *last_y;
  *last_y = y;
}

bool ParseTouchScroll(uint8_t const* report, uint16_t len, int8_t* scroll) {
  *scroll = 0;
#if TOUCHPAD_SCROLL_ENABLE
  const uint8_t report_base = (report[0] == 0x01) ? 1 : 0;
  constexpr uint8_t kTouchOffset = 32;
  constexpr uint8_t kTouchPointSize = 4;
  const uint8_t p0 = report_base + kTouchOffset;
  const uint8_t p1 = p0 + kTouchPointSize;

  if (len > static_cast<uint16_t>(p0 + 3)) {
    UpdateTouchPoint(&report[p0],
                     &g_controller.touch0_active, &g_controller.touch0_id,
                     &g_controller.touch0_last_y, &g_controller.touch0_scroll_accum);
  }
  if (len > static_cast<uint16_t>(p1 + 3)) {
    UpdateTouchPoint(&report[p1],
                     &g_controller.touch1_active, &g_controller.touch1_id,
                     &g_controller.touch1_last_y, &g_controller.touch1_scroll_accum);
  }

  // Fire scroll if either finger exceeds the threshold; reset both to avoid double-firing.
  const int16_t accum0 = g_controller.touch0_scroll_accum;
  const int16_t accum1 = g_controller.touch1_scroll_accum;
  const int16_t trigger =
      (Abs32(accum0) >= TOUCHPAD_SCROLL_THRESHOLD) ? accum0 :
      (Abs32(accum1) >= TOUCHPAD_SCROLL_THRESHOLD) ? accum1 : 0;

  if (trigger != 0) {
    *scroll = (trigger > 0) ? -1 : 1;
    g_controller.touch0_scroll_accum = 0;
    g_controller.touch1_scroll_accum = 0;
    return true;
  }
#else
  (void)report;
  (void)len;
#endif
  return false;
}

bool ParseGyroMouse(uint8_t const* report, uint16_t len, int16_t* mouse_x, int16_t* mouse_y) {
#if GYRO_MOUSE_ENABLE
  const uint8_t report_base = (report[0] == 0x01) ? 1 : 0;
  constexpr uint8_t kGyroOffset = 15;
  constexpr uint8_t kGyroX = kGyroOffset;
  constexpr uint8_t kGyroY = kGyroOffset + 2;
  if (len <= report_base + kGyroY + 1) {
    return false;
  }

  if (!ParseTouchActive(report, len)) {
    g_controller.gyro_mouse_x_remainder_q16 = 0;
    g_controller.gyro_mouse_y_remainder_q16 = 0;
    return false;
  }

  // Use yaw for horizontal cursor movement and pitch for vertical movement.
  const int16_t gyro_for_x = ReadLe16(&report[report_base + kGyroY]);
  const int16_t gyro_for_y = ReadLe16(&report[report_base + kGyroX]);
  *mouse_x = ConsumeMouseDelta(
      static_cast<int32_t>(ShapeGyroDeltaQ16(gyro_for_x) * GYRO_MOUSE_X_FACTOR),
      &g_controller.gyro_mouse_x_remainder_q16);
  *mouse_y = ConsumeMouseDelta(
      static_cast<int32_t>(ShapeGyroDeltaQ16(gyro_for_y) * GYRO_MOUSE_Y_FACTOR),
      &g_controller.gyro_mouse_y_remainder_q16);
  return *mouse_x != 0 || *mouse_y != 0;
#else
  (void)report;
  (void)len;
  (void)mouse_x;
  (void)mouse_y;
  return false;
#endif
}

uint64_t ParseLeftStickButtons(uint8_t const* report, uint16_t len) {
  const uint8_t report_base = (report[0] == 0x01) ? 1 : 0;
  if (len < static_cast<uint16_t>(report_base + 2)) {
    return 0;
  }

  const int sx = static_cast<int>(report[report_base + 0]) - 128;
  const int sy = static_cast<int>(report[report_base + 1]) - 128;
  const float radius_raw = fminf(sqrtf((float)(sx * sx + sy * sy)), 127.0f);
  const float radius = (radius_raw < 26.0f)
                           ? 0.0f
                           : (radius_raw - 26.0f) * 127.0f / 101.0f;

  if (radius <= 6.35f) {
    return 0;
  }

  uint64_t buttons = 0;

  // Ring buttons.
  if (radius < 95.25f) {
    buttons |= mapping::ButtonMask(mapping::Button::kInnerRing);
  } else {
    buttons |= mapping::ButtonMask(mapping::Button::kOuterRing);
  }

  // Directional buttons: angle 0°=N, 90°=E, ±180°=S, -90°=W.
  // 4DIR overlap sectors (135° each, 45° overlap → diagonals press two keys).
  const float angle = atan2f((float)sx, -(float)sy) * (180.0f / 3.14159265f);
  if (fabsf(angle) <= 67.5f) {
    buttons |= mapping::ButtonMask(mapping::Button::kLStickN);
  }
  if (angle > 22.5f && angle < 157.5f) {
    buttons |= mapping::ButtonMask(mapping::Button::kLStickE);
  }
  if (fabsf(angle) >= 112.5f) {
    buttons |= mapping::ButtonMask(mapping::Button::kLStickS);
  }
  if (angle > -157.5f && angle < -22.5f) {
    buttons |= mapping::ButtonMask(mapping::Button::kLStickW);
  }

  return buttons;
}

uint64_t ParseRightStickButtons(uint8_t const* report, uint16_t len) {
  const uint8_t report_base = (report[0] == 0x01) ? 1 : 0;
  if (len < static_cast<uint16_t>(report_base + 4)) {
    return 0;
  }

  const int sx = static_cast<int>(report[report_base + 2]) - 128;
  const int sy = static_cast<int>(report[report_base + 3]) - 128;
  const float radius_raw = fminf(sqrtf((float)(sx * sx + sy * sy)), 127.0f);
  const float radius = (radius_raw < 26.0f)
                           ? 0.0f
                           : (radius_raw - 26.0f) * 127.0f / 101.0f;

  if (radius < RSTICK_NUMPAD_THRESHOLD) {
    return 0;
  }

  // Exclusive 8-way sectors, 45° each. Angle: 0°=N, 90°=E, ±180°=S, -90°=W.
  const float angle = atan2f((float)sx, -(float)sy) * (180.0f / 3.14159265f);

  // Each sector spans [-22.5°, +22.5°] around its centre direction.
  if (angle >= -22.5f  && angle <  22.5f)  return mapping::ButtonMask(mapping::Button::kRStick4);  // N → 4
  if (angle >=  22.5f  && angle <  67.5f)  return mapping::ButtonMask(mapping::Button::kRStick7);  // NE → 7
  if (angle >=  67.5f  && angle < 112.5f)  return mapping::ButtonMask(mapping::Button::kRStick3);  // E → 3
  if (angle >= 112.5f  && angle < 157.5f)  return mapping::ButtonMask(mapping::Button::kRStick6);  // SE → 6
  if (angle >= -67.5f  && angle < -22.5f)  return mapping::ButtonMask(mapping::Button::kRStick8);  // NW → 8
  if (angle >= -112.5f && angle < -67.5f)  return mapping::ButtonMask(mapping::Button::kRStick1);  // W → 1
  if (angle >= -157.5f && angle < -112.5f) return mapping::ButtonMask(mapping::Button::kRStick5);  // SW → 5
  /* |angle| >= 157.5° → S */               return mapping::ButtonMask(mapping::Button::kRStick2);  // S → 2
}

void ParseTouchpadClick(uint8_t const* report, uint16_t len) {
  const uint8_t button_base = (report[0] == 0x01) ? 8 : 7;
  if (len <= static_cast<uint16_t>(button_base + 2)) {
    return;
  }

  const bool clicked = (report[button_base + 2] & 0x02) != 0;
  if (clicked == g_controller.touchpad_clicked) {
    return;
  }
  g_controller.touchpad_clicked = clicked;

  bool keyboard_changed = false;
  bool mouse_changed = false;

  if (clicked) {
    // Read X position of touch point 0 to determine zone (0-1919 → three equal thirds).
    const uint8_t report_base = (report[0] == 0x01) ? 1 : 0;
    const uint8_t p = report_base + 32;
    uint16_t x = 960;  // default to middle
    if (len > static_cast<uint16_t>(p + 2) && (report[p] & 0x80) == 0) {
      x = static_cast<uint16_t>(report[p + 1]) |
          (static_cast<uint16_t>(report[p + 2] & 0x0F) << 8);
    }

    if (x < 640) {
      g_controller.touchpad_zone = 1;
      keyboard_changed |= AddKey(&g_controller.keyboard, usb::kKeyM);
    } else if (x < 1280) {
      g_controller.touchpad_zone = 2;
      const uint8_t before = g_controller.mouse.buttons;
      g_controller.mouse.buttons |= usb::kMouseButtonMiddle;
      mouse_changed |= before != g_controller.mouse.buttons;
    } else {
      g_controller.touchpad_zone = 3;
      keyboard_changed |= AddKey(&g_controller.keyboard, usb::kKeyN);
    }
  } else {
    switch (g_controller.touchpad_zone) {
      case 1:
        keyboard_changed |= RemoveKey(&g_controller.keyboard, usb::kKeyM);
        break;
      case 2: {
        const uint8_t before = g_controller.mouse.buttons;
        g_controller.mouse.buttons &= ~usb::kMouseButtonMiddle;
        mouse_changed |= before != g_controller.mouse.buttons;
        break;
      }
      case 3:
        keyboard_changed |= RemoveKey(&g_controller.keyboard, usb::kKeyN);
        break;
    }
    g_controller.touchpad_zone = 0;
  }

  if (keyboard_changed) {
    device_out::SendKeyboard(g_controller.keyboard);
  }
  if (mouse_changed) {
    g_controller.mouse.x = 0;
    g_controller.mouse.y = 0;
    g_controller.mouse.wheel = 0;
    g_controller.mouse.pan = 0;
    device_out::SendMouse(g_controller.mouse);
  }
}

uint64_t ParseDualSenseButtons(uint8_t const* report, uint16_t len) {
  if (len < 11) {
    return 0;
  }

  // Wired DualSense USB input reports normally start with report ID 0x01.
  // The relevant digital buttons begin at byte 8 for this report layout.
  const uint8_t button_base = (report[0] == 0x01) ? 8 : 7;
  if (len <= static_cast<uint16_t>(button_base + 2)) {
    return 0;
  }

  const uint8_t dpad_and_shapes = report[button_base];
  const uint8_t shoulders_share_options = report[button_base + 1];
  const uint8_t system_buttons = report[button_base + 2];

  uint64_t buttons = 0;

  const uint8_t hat = dpad_and_shapes & 0x0f;
  if (hat == 0 || hat == 1 || hat == 7) {
    buttons |= mapping::ButtonMask(mapping::Button::kDpadUp);
  }
  if (hat == 1 || hat == 2 || hat == 3) {
    buttons |= mapping::ButtonMask(mapping::Button::kDpadRight);
  }
  if (hat == 3 || hat == 4 || hat == 5) {
    buttons |= mapping::ButtonMask(mapping::Button::kDpadDown);
  }
  if (hat == 5 || hat == 6 || hat == 7) {
    buttons |= mapping::ButtonMask(mapping::Button::kDpadLeft);
  }

  if (dpad_and_shapes & 0x10) {
    buttons |= mapping::ButtonMask(mapping::Button::kSquare);
  }
  if (dpad_and_shapes & 0x20) {
    buttons |= mapping::ButtonMask(mapping::Button::kCross);
  }
  if (dpad_and_shapes & 0x40) {
    buttons |= mapping::ButtonMask(mapping::Button::kCircle);
  }
  if (dpad_and_shapes & 0x80) {
    buttons |= mapping::ButtonMask(mapping::Button::kTriangle);
  }

  if (shoulders_share_options & 0x01) {
    buttons |= mapping::ButtonMask(mapping::Button::kL1);
  }
  if (shoulders_share_options & 0x02) {
    buttons |= mapping::ButtonMask(mapping::Button::kR1);
  }
  if (shoulders_share_options & 0x04) {
    buttons |= mapping::ButtonMask(mapping::Button::kL2);
  }
  if (shoulders_share_options & 0x08) {
    buttons |= mapping::ButtonMask(mapping::Button::kR2);
  }
  if (shoulders_share_options & 0x10) {
    buttons |= mapping::ButtonMask(mapping::Button::kCreate);
  }
  if (shoulders_share_options & 0x20) {
    buttons |= mapping::ButtonMask(mapping::Button::kOptions);
  }
  if (shoulders_share_options & 0x40) {
    buttons |= mapping::ButtonMask(mapping::Button::kL3);
  }
  if (shoulders_share_options & 0x80) {
    buttons |= mapping::ButtonMask(mapping::Button::kR3);
  }

  if (system_buttons & 0x01) {
    buttons |= mapping::ButtonMask(mapping::Button::kPs);
  }
  if (system_buttons & 0x02) {
    buttons |= mapping::ButtonMask(mapping::Button::kTouchpad);
  }
  if (system_buttons & 0x04) {
    buttons |= mapping::ButtonMask(mapping::Button::kMute);
  }
  if (system_buttons & 0x10) {
    buttons |= mapping::ButtonMask(mapping::Button::kFn1);
  }
  if (system_buttons & 0x20) {
    buttons |= mapping::ButtonMask(mapping::Button::kFn2);
  }
  if (system_buttons & 0x40) {
    buttons |= mapping::ButtonMask(mapping::Button::kLeftPaddle);
  }
  if (system_buttons & 0x80) {
    buttons |= mapping::ButtonMask(mapping::Button::kRightPaddle);
  }

  return buttons;
}

void ProcessButtons(uint64_t next_buttons) {
  const uint64_t changed = g_controller.buttons ^ next_buttons;
  if (changed == 0) {
    return;
  }

  bool keyboard_changed = false;
  bool mouse_changed = false;

  for (size_t i = 0; i < mapping::kButtonCount; ++i) {
    const uint64_t mask = 1ull << i;
    if ((changed & mask) == 0) {
      continue;
    }

    ApplyAction(g_controller.actions[i], (next_buttons & mask) != 0, &keyboard_changed, &mouse_changed);
  }

  g_controller.buttons = next_buttons;

  if (keyboard_changed) {
    device_out::SendKeyboard(g_controller.keyboard);
  }
  if (mouse_changed) {
    g_controller.mouse.x = 0;
    g_controller.mouse.y = 0;
    g_controller.mouse.wheel = 0;
    g_controller.mouse.pan = 0;
    device_out::SendMouse(g_controller.mouse);
  }
}

}  // namespace

// Builds a Stadia-compatible gamepad report from a raw DualSense input report.
//
// Stadia button layout (SDL2 DB: 18d19400,Stadia Controller rev. A):
//   bit  0 = A        (Cross)      a:b0
//   bit  1 = B        (Circle)     b:b1
//   bit  2 = X        (Square)     x:b2
//   bit  3 = Y        (Triangle)   y:b3
//   bit  4 = unused   (Assistant on real Stadia)
//   bit  5 = Guide    (PS)         guide:b5
//   bit  6 = Menu     (Options)    start:b6
//   bit  7 = L3                    leftstick:b7
//   bit  8 = R3                    rightstick:b8
//   bit  9 = L1                    leftshoulder:b9
//   bit 10 = R1                    rightshoulder:b10
//   bit 11 = Capture  (Create)     back:b11 / misc1:b11
//   bits 12-15 = unused
//
// Triggers (analog axes only, no digital button bits):
//   r2 = Z   → SDL2 axis 4 (righttrigger:a4)
//   l2 = Rz  → SDL2 axis 5 (lefttrigger:a5)
//
// Edge paddles: right paddle → A (bit 0), left paddle → L3 (bit 7).
static device_out::GamepadReport ParseForGamepad(uint8_t const* report,
                                                 uint16_t len) {
  device_out::GamepadReport gp = {};
  const uint8_t report_base = (report[0] == 0x01) ? 1 : 0;
  const uint8_t button_base = report_base + 7;

  if (len < static_cast<uint16_t>(button_base + 3)) {
    return gp;
  }

  const uint8_t dpad_shapes    = report[button_base + 0];
  const uint8_t shoulders      = report[button_base + 1];
  const uint8_t system_buttons = report[button_base + 2];

  uint16_t b = 0;
  if (dpad_shapes  & 0x20) b |= (1u << 0);   // Cross     → A
  if (dpad_shapes  & 0x40) b |= (1u << 1);   // Circle    → B
  if (dpad_shapes  & 0x10) b |= (1u << 2);   // Square    → X
  if (dpad_shapes  & 0x80) b |= (1u << 3);   // Triangle  → Y
  // bit 4 unused (Stadia Assistant)
  if (system_buttons & 0x01) b |= (1u << 5);  // PS        → Guide
  if (shoulders    & 0x20) b |= (1u << 6);   // Options   → Menu
  if (shoulders    & 0x40) b |= (1u << 7);   // L3
  if (shoulders    & 0x80) b |= (1u << 8);   // R3
  if (shoulders    & 0x01) b |= (1u << 9);   // L1
  if (shoulders    & 0x02) b |= (1u << 10);  // R1
  if (shoulders    & 0x10) b |= (1u << 11);  // Create    → Capture
  if (system_buttons & 0x80) b |= (1u << 0);  // Right paddle → A
  if (system_buttons & 0x40) b |= (1u << 7);  // Left paddle  → L3
  gp.buttons = b;

  const uint8_t hat = dpad_shapes & 0x0F;
  gp.hat = (hat > 7) ? 0x0F : hat;

  if (len >= static_cast<uint16_t>(report_base + 6)) {
    auto scale = [](uint8_t raw) -> int16_t {
      return static_cast<int16_t>((static_cast<int32_t>(raw) - 128) * 256);
    };
    gp.left_x  = scale(report[report_base + 0]);
    gp.left_y  = scale(report[report_base + 1]);
    gp.right_x = scale(report[report_base + 2]);
    gp.right_y = scale(report[report_base + 3]);
    gp.r2 = report[report_base + 5];  // R2 = Z   = SDL2 axis 4
    gp.l2 = report[report_base + 4];  // L2 = Rz  = SDL2 axis 5
  }

  return gp;
}
// Layout mirrors struct dualsense_output_report from hid-playstation.c:
//   byte  1 = valid_flag1 (0x04 = lightbar control enable)
//   byte 44 = lightbar_red
//   byte 45 = lightbar_green
//   byte 46 = lightbar_blue
// Total: 63 bytes of data + 1 byte report ID = 64 bytes.
static void SendDualSenseLightbar(uint8_t dev_addr, uint8_t instance,
                                  uint8_t r, uint8_t g, uint8_t b) {
  uint8_t report[63] = {};
  report[1]  = 0x04;  // valid_flag1: lightbar_control_enable
  report[44] = r;
  report[45] = g;
  report[46] = b;
  tuh_hid_send_report(dev_addr, instance, 0x02, report, sizeof(report));
}

namespace host_reader {

void Init() {
  gpio_init(kStatusLedPin);
  gpio_set_dir(kStatusLedPin, GPIO_OUT);
  SetControllerLed(false);
  RunLedSelfTest();
  ResetState();
}

void Task() {
  tuh_task();
}

}  // namespace host_reader

extern "C" void tuh_hid_mount_cb(uint8_t dev_addr,
                                  uint8_t instance,
                                  uint8_t const* desc_report,
                                  uint16_t desc_len) {
  (void)desc_report;
  (void)desc_len;

  uint16_t vid = 0;
  uint16_t pid = 0;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  const mapping::Action* actions = mapping::FindMapping(vid, pid);
  if (actions == nullptr || g_controller.active) {
    return;
  }

  g_controller.active = true;
  g_controller.dev_addr = dev_addr;
  g_controller.instance = instance;
  g_controller.actions = actions;
  g_controller.allowed_device_mounted = true;
  g_controller.allowed_dev_addr = dev_addr;
  SetControllerLed(true);

  if (mode::GetActive() == mode::Mode::kGamepad) {
    SendDualSenseLightbar(dev_addr, instance, 255, 200, 0);  // yellow
  } else {
    SendDualSenseLightbar(dev_addr, instance, 0, 0, 255);    // blue
  }
  tuh_hid_receive_report(dev_addr, instance);
}

extern "C" void tuh_mount_cb(uint8_t dev_addr) {
  uint16_t vid = 0;
  uint16_t pid = 0;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  if (mapping::FindMapping(vid, pid) == nullptr || g_controller.allowed_device_mounted) {
    return;
  }

  g_controller.allowed_device_mounted = true;
  g_controller.allowed_dev_addr = dev_addr;
  SetControllerLed(true);
}

extern "C" void tuh_umount_cb(uint8_t dev_addr) {
  if (g_controller.allowed_device_mounted &&
      g_controller.allowed_dev_addr == dev_addr) {
    ResetState();
    SetControllerLed(false);
  }
}

extern "C" void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  if (g_controller.active &&
      g_controller.dev_addr == dev_addr &&
      g_controller.instance == instance) {
    if (mode::GetActive() == mode::Mode::kGamepad) {
      device_out::GamepadReport empty_gp = {};
      device_out::SendGamepad(empty_gp);
    } else {
      device_out::KeyboardReport empty_keyboard = {};
      device_out::MouseReport empty_mouse = {};
      device_out::SendKeyboard(empty_keyboard);
      device_out::SendMouse(empty_mouse);
    }
    ClearHidState();
  }
}

extern "C" void tuh_hid_report_received_cb(uint8_t dev_addr,
                                            uint8_t instance,
                                            uint8_t const* report,
                                            uint16_t len) {
  if (!g_controller.active ||
      g_controller.dev_addr != dev_addr ||
      g_controller.instance != instance) {
    return;
  }

  DebugPrintReport(report, len);

  if (mode::GetActive() == mode::Mode::kGamepad) {
    device_out::SendGamepad(ParseForGamepad(report, len));
    tuh_hid_receive_report(dev_addr, instance);
    return;
  }

  ProcessButtons(ParseDualSenseButtons(report, len) |
                 ParseLeftStickButtons(report, len)  |
                 ParseRightStickButtons(report, len));
  ParseTouchpadClick(report, len);
  int8_t scroll = 0;
  if (ParseTouchScroll(report, len, &scroll)) {
    g_controller.mouse.x = 0;
    g_controller.mouse.y = 0;
    g_controller.mouse.wheel = scroll;
    g_controller.mouse.pan = 0;
    device_out::SendMouse(g_controller.mouse);
    g_controller.mouse.wheel = 0;
  }
  int16_t mouse_x = 0;
  int16_t mouse_y = 0;
  if (ParseGyroMouse(report, len, &mouse_x, &mouse_y)) {
    g_controller.mouse.x = mouse_x;
    g_controller.mouse.y = mouse_y;
    g_controller.mouse.wheel = 0;
    g_controller.mouse.pan = 0;
    device_out::SendMouse(g_controller.mouse);
  }
  tuh_hid_receive_report(dev_addr, instance);
}

extern "C" void tuh_hid_report_sent_cb(uint8_t dev_addr, uint8_t instance,
                                        uint8_t const* report, uint16_t len) {
  (void)dev_addr;
  (void)instance;
  (void)report;
  (void)len;
}
