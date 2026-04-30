#include "host_reader.h"

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

// Minimal PIO-USB endpoint access for interval patching.
// Full pio_usb_ll.h cannot be included in C++ (duplicate inline / enum issues).
extern "C" {
#include "usb_definitions.h"
extern endpoint_t pio_usb_ep_pool[32];  // PIO_USB_EP_POOL_CNT = 32
}

namespace {

constexpr uint kStatusLedPin = PICO_DEFAULT_LED_PIN;

struct ActiveController {
  bool active;
  bool allowed_device_mounted;
  uint8_t allowed_dev_addr;
  uint8_t dev_addr;
  uint8_t instance;
  const mapping::Action* actions;
  const mapping::Action* gamepad_actions;
  uint64_t buttons;
  int32_t gyro_mouse_x_remainder_q16;
  int32_t gyro_mouse_y_remainder_q16;
  // Gyro bias in Q8 (fractional precision for slow EMA updates).
  // Updated only when not touching and all 3 axes are still.
  int32_t gyro_bias_x_q8;
  int32_t gyro_bias_y_q8;
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
  // Full-width single-finger swipe gesture state (left→right or right→left).
  bool swipe_finger_down;
  uint16_t swipe_start_x;
  uint16_t swipe_current_x;
  bool keyboard_pending;
  bool mouse_pending;
  device_out::KeyboardReport keyboard;
  device_out::MouseReport mouse;
};

ActiveController g_controller = {};

struct TouchPoint {
  bool active;
  uint8_t id;
  uint16_t x;
  int16_t y;
};

struct TouchState {
  TouchPoint point[2];
  bool any_active;
};

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
  g_controller.gamepad_actions = nullptr;
  g_controller.buttons = 0;
  g_controller.gyro_mouse_x_remainder_q16 = 0;
  g_controller.gyro_mouse_y_remainder_q16 = 0;
  g_controller.gyro_bias_x_q8 = 0;
  g_controller.gyro_bias_y_q8 = 0;
  g_controller.touch0_active = false;
  g_controller.touch0_scroll_accum = 0;
  g_controller.touch1_active = false;
  g_controller.touch1_scroll_accum = 0;
  g_controller.touchpad_clicked = false;
  g_controller.touchpad_zone = 0;
  g_controller.swipe_finger_down = false;
  g_controller.keyboard_pending = false;
  g_controller.mouse_pending = false;
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

    default:
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
  // Soft suppression: proportionally reduce values below threshold instead of
  // hard-cutting. output = raw * |raw| / threshold (quadratic fade-in).
  constexpr int16_t kSoftThreshold = 15;
  if (Abs32(raw) < kSoftThreshold) {
    raw = static_cast<int16_t>(
        (static_cast<int32_t>(raw) * Abs32(raw)) / kSoftThreshold);
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

TouchState ParseTouchState(uint8_t const* report, uint16_t len) {
  TouchState touch = {};
  const uint8_t report_base = (report[0] == 0x01) ? 1 : 0;
  constexpr uint8_t kTouchOffset = 32;
  constexpr uint8_t kTouchPointSize = 4;

  for (uint8_t i = 0; i < 2; ++i) {
    const uint8_t p = report_base + kTouchOffset + i * kTouchPointSize;
    if (len <= static_cast<uint16_t>(p + 3)) {
      continue;
    }

    TouchPoint& point = touch.point[i];
    point.active = (report[p] & 0x80) == 0;
    point.id = report[p] & 0x7F;
    if (!point.active) {
      continue;
    }

    point.x = static_cast<uint16_t>(report[p + 1]) |
              (static_cast<uint16_t>(report[p + 2] & 0x0F) << 8);
    point.y = static_cast<int16_t>(
        (static_cast<uint16_t>(report[p + 2] & 0xF0) >> 4) |
        (static_cast<uint16_t>(report[p + 3]) << 4));
    touch.any_active = true;
  }

  return touch;
}

// Updates tracking state for one touch point and accumulates Y delta.
void UpdateTouchPoint(const TouchPoint& point, bool* active, uint8_t* id,
                      int16_t* last_y, int16_t* accum) {
  if (!point.active) {
    *active = false;
    *accum = 0;
    return;
  }

  if (!*active || *id != point.id) {
    *active = true;
    *id = point.id;
    *last_y = point.y;
    *accum = 0;
    return;
  }

  *accum += point.y - *last_y;
  *last_y = point.y;
}

bool ParseTouchScroll(const TouchState& touch, int8_t* scroll) {
  *scroll = 0;
#if TOUCHPAD_SCROLL_ENABLE
  UpdateTouchPoint(touch.point[0],
                   &g_controller.touch0_active, &g_controller.touch0_id,
                   &g_controller.touch0_last_y, &g_controller.touch0_scroll_accum);
  UpdateTouchPoint(touch.point[1],
                   &g_controller.touch1_active, &g_controller.touch1_id,
                   &g_controller.touch1_last_y, &g_controller.touch1_scroll_accum);

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
  (void)touch;
#endif
  return false;
}

bool ParseGyroMouse(uint8_t const* report, uint16_t len, const TouchState& touch,
                    int16_t* mouse_x, int16_t* mouse_y) {
#if GYRO_MOUSE_ENABLE
  const uint8_t report_base = (report[0] == 0x01) ? 1 : 0;
  constexpr uint8_t kGyroOffset = 15;
  constexpr uint8_t kGyroX = kGyroOffset;
  constexpr uint8_t kGyroY = kGyroOffset + 2;
  constexpr uint8_t kGyroZ = kGyroOffset + 4;
  if (len <= report_base + kGyroZ + 1) {
    return false;
  }

  const int16_t gyro_raw_x = ReadLe16(&report[report_base + kGyroX]);
  const int16_t gyro_raw_y = ReadLe16(&report[report_base + kGyroY]);
  const int16_t gyro_raw_z = ReadLe16(&report[report_base + kGyroZ]);

  // Yaw → mouse X, pitch → mouse Y.
  const int16_t gyro_for_x = gyro_raw_y;
  const int16_t gyro_for_y = gyro_raw_x;

  const int16_t bias_x = static_cast<int16_t>(g_controller.gyro_bias_x_q8 >> 8);
  const int16_t bias_y = static_cast<int16_t>(g_controller.gyro_bias_y_q8 >> 8);
  const int16_t corrected_x = gyro_for_x - bias_x;
  const int16_t corrected_y = gyro_for_y - bias_y;
  const int16_t corrected_z = gyro_raw_z - bias_x;  // roll — no own bias, use x as proxy

  // Continuous bias update via slow EMA (alpha≈0.0025, ~0.4s time constant at
  // 1000 Hz) — only when NOT touching and all 3 axes are still. This prevents
  // bias drift during active aiming.
  constexpr int16_t kRestThreshold = 12;
  const bool touching = touch.any_active;
  if (!touching &&
      Abs32(corrected_x) < kRestThreshold &&
      Abs32(corrected_y) < kRestThreshold &&
      Abs32(corrected_z) < kRestThreshold) {
    g_controller.gyro_bias_x_q8 +=
        ((static_cast<int32_t>(gyro_for_x) << 8) - g_controller.gyro_bias_x_q8) / 400;
    g_controller.gyro_bias_y_q8 +=
        ((static_cast<int32_t>(gyro_for_y) << 8) - g_controller.gyro_bias_y_q8) / 400;
  }

  if (!touching) {
    g_controller.gyro_mouse_x_remainder_q16 = 0;
    g_controller.gyro_mouse_y_remainder_q16 = 0;
    return false;
  }

  // Apply axis scale factors in integer. X_FACTOR = 1.0 → no-op.
  // Y_FACTOR converted to Q16: (int64 * Q16_factor) >> 16 avoids software float.
  constexpr int32_t kYFactorQ16 =
      static_cast<int32_t>(GYRO_MOUSE_Y_FACTOR * 65536.0f);

  *mouse_x = ConsumeMouseDelta(
      ShapeGyroDeltaQ16(corrected_x),
      &g_controller.gyro_mouse_x_remainder_q16);
  *mouse_y = ConsumeMouseDelta(
      static_cast<int32_t>(
          (static_cast<int64_t>(ShapeGyroDeltaQ16(corrected_y)) * kYFactorQ16) >> 16),
      &g_controller.gyro_mouse_y_remainder_q16);
  return *mouse_x != 0 || *mouse_y != 0;
#else
  (void)report;
  (void)len;
  (void)touch;
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
  const int r2 = sx * sx + sy * sy;

  // Dead zone: normalized radius <= 6.35 → raw radius <= ~31 → r² <= 961.
  if (r2 <= 961) {
    return 0;
  }

  uint64_t buttons = 0;

  // Inner ring: raw radius < ~102 → r² < 10362. Outer ring otherwise.
  buttons |= mapping::ButtonMask(
      r2 < 10362 ? mapping::Button::kInnerRing : mapping::Button::kOuterRing);

  // 4-way WASD, each sector 135° wide (45° overlap into diagonals).
  // Boundaries at ±22.5° and ±67.5° from each axis.
  // tan(67.5°) ≈ 2.414 → approx 5/2. Condition: |minor| * 2 ≤ |major| * 5.
  const int ax = sx < 0 ? -sx : sx;
  const int ay = sy < 0 ? -sy : sy;
  if (sy < 0 && ax * 2 <= ay * 5) buttons |= mapping::ButtonMask(mapping::Button::kLStickN);
  if (sy > 0 && ax * 2 <= ay * 5) buttons |= mapping::ButtonMask(mapping::Button::kLStickS);
  if (sx > 0 && ay * 2 <= ax * 5) buttons |= mapping::ButtonMask(mapping::Button::kLStickE);
  if (sx < 0 && ay * 2 <= ax * 5) buttons |= mapping::ButtonMask(mapping::Button::kLStickW);

  return buttons;
}

uint64_t ParseRightStickButtons(uint8_t const* report, uint16_t len) {
  const uint8_t report_base = (report[0] == 0x01) ? 1 : 0;
  if (len < static_cast<uint16_t>(report_base + 4)) {
    return 0;
  }

  const int sx = static_cast<int>(report[report_base + 2]) - 128;
  const int sy = static_cast<int>(report[report_base + 3]) - 128;

  // Active threshold: corresponds to normalized radius >= RSTICK_NUMPAD_THRESHOLD.
  // raw_thresh = 26 + T * 101/127. Squared to avoid sqrt.
  constexpr int kRawThresh = 26 + RSTICK_NUMPAD_THRESHOLD * 101 / 127;
  if (sx * sx + sy * sy < kRawThresh * kRawThresh) {
    return 0;
  }

  // Exclusive 8-way sector detection using integer comparisons.
  // tan(22.5°) ≈ 0.414 ≈ 2/5. tan(67.5°) ≈ 2.414 ≈ 5/2.
  // "Mostly X": |sx|/|sy| > 5/2 → cardinal E/W.
  // "Mostly Y": |sy|/|sx| > 5/2 → cardinal N/S.
  // Otherwise: diagonal.
  const int ax = sx < 0 ? -sx : sx;
  const int ay = sy < 0 ? -sy : sy;

  if (ax * 2 > ay * 5) {
    return mapping::ButtonMask(sx > 0 ? mapping::Button::kRStick3   // E
                                      : mapping::Button::kRStick1); // W
  }
  if (ay * 2 > ax * 5) {
    return mapping::ButtonMask(sy < 0 ? mapping::Button::kRStick4   // N
                                      : mapping::Button::kRStick2); // S
  }
  if (sx > 0) {
    return mapping::ButtonMask(sy < 0 ? mapping::Button::kRStick7   // NE
                                      : mapping::Button::kRStick6); // SE
  }
  return mapping::ButtonMask(sy > 0 ? mapping::Button::kRStick5     // SW
                                    : mapping::Button::kRStick8);   // NW
}

// Detects a full-width single-finger swipe for mode switching.
// Trigger: one finger starts within SWIPE_GESTURE_EDGE_MARGIN px of either
// edge and ends past the opposite edge threshold. Second finger cancels.
// Calls mode::ToggleAndReboot() (does not return) on success.
void ParseSwipeGesture(const TouchState& touch) {
  if (touch.point[1].active) {
    g_controller.swipe_finger_down = false;
    return;
  }

  if (touch.point[0].active && !g_controller.swipe_finger_down) {
    g_controller.swipe_finger_down = true;
    g_controller.swipe_start_x = touch.point[0].x;
    g_controller.swipe_current_x = touch.point[0].x;
  } else if (touch.point[0].active) {
    g_controller.swipe_current_x = touch.point[0].x;
  } else if (!touch.point[0].active && g_controller.swipe_finger_down) {
    g_controller.swipe_finger_down = false;
    const uint16_t start = g_controller.swipe_start_x;
    const uint16_t end   = g_controller.swipe_current_x;
    constexpr uint16_t kLeft  = SWIPE_GESTURE_EDGE_MARGIN;
    constexpr uint16_t kRight = 1920 - SWIPE_GESTURE_EDGE_MARGIN;
    const bool left_to_right = (start < kLeft)  && (end > kRight);
    const bool right_to_left = (start > kRight) && (end < kLeft);
    if (left_to_right || right_to_left) {
      mode::ToggleAndReboot();
    }
  }
}

// Updates keyboard/mouse state for a touchpad click event.
// Returns mouse_changed. Sets *keyboard_changed. Caller is responsible for both sends.
bool ParseTouchpadClick(uint8_t const* report, uint16_t len,
                        const TouchState& touch, bool* keyboard_changed) {
  const uint8_t button_base = (report[0] == 0x01) ? 8 : 7;
  if (len <= static_cast<uint16_t>(button_base + 2)) {
    return false;
  }

  const bool clicked = (report[button_base + 2] & 0x02) != 0;
  if (clicked == g_controller.touchpad_clicked) {
    return false;
  }
  g_controller.touchpad_clicked = clicked;

  bool mouse_changed = false;

  if (clicked) {
    // Read X position of touch point 0 to determine zone (0-1919 → three equal thirds).
    uint16_t x = 960;  // default to middle
    if (touch.point[0].active) {
      x = touch.point[0].x;
    }

    if (x < 640) {
      g_controller.touchpad_zone = 1;
      *keyboard_changed |= AddKey(&g_controller.keyboard, usb::kKeyM);
    } else if (x < 1280) {
      g_controller.touchpad_zone = 2;
      const uint8_t before = g_controller.mouse.buttons;
      g_controller.mouse.buttons |= usb::kMouseButtonMiddle;
      mouse_changed |= before != g_controller.mouse.buttons;
    } else {
      g_controller.touchpad_zone = 3;
      *keyboard_changed |= AddKey(&g_controller.keyboard, usb::kKeyN);
    }
  } else {
    switch (g_controller.touchpad_zone) {
      case 1:
        *keyboard_changed |= RemoveKey(&g_controller.keyboard, usb::kKeyM);
        break;
      case 2: {
        const uint8_t before = g_controller.mouse.buttons;
        g_controller.mouse.buttons &= ~usb::kMouseButtonMiddle;
        mouse_changed |= before != g_controller.mouse.buttons;
        break;
      }
      case 3:
        *keyboard_changed |= RemoveKey(&g_controller.keyboard, usb::kKeyN);
        break;
    }
    g_controller.touchpad_zone = 0;
  }

  return mouse_changed;
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

// Returns mouse_changed. Sets *keyboard_changed. Caller is responsible for both sends.
bool ProcessButtons(uint64_t next_buttons, bool* keyboard_changed) {
  const uint64_t changed = g_controller.buttons ^ next_buttons;
  if (changed == 0) {
    return false;
  }

  bool mouse_changed = false;

  for (size_t i = 0; i < mapping::kButtonCount; ++i) {
    const uint64_t mask = 1ull << i;
    if ((changed & mask) == 0) {
      continue;
    }

    ApplyAction(g_controller.actions[i], (next_buttons & mask) != 0, keyboard_changed, &mouse_changed);
  }

  g_controller.buttons = next_buttons;
  return mouse_changed;
}

}  // namespace

// Builds a Stadia Controller HID report from a DualSense input.
static device_out::GamepadReport ParseForGamepad(uint8_t const* report,
                                                 uint16_t len,
                                                 uint64_t button_mask) {
  device_out::GamepadReport gp = {};
  gp.hat = 0x08;  // hat center (null state)
  gp.left_x = gp.left_y = gp.right_x = gp.right_y = 0x80;

  const uint8_t report_base = (report[0] == 0x01) ? 1 : 0;
  const uint8_t button_base = report_base + 7;

  if (len < static_cast<uint16_t>(button_base + 3)) {
    return gp;
  }

  // D-Pad → hat (direct, independent of mapping table)
  const uint8_t hat = report[button_base] & 0x0F;
  gp.hat = (hat > 7) ? 0x08 : hat;

  // Digital buttons via mapping table (button_mask passed in, no re-parse needed)
  const mapping::Action* actions = g_controller.gamepad_actions;
  if (actions) {
    for (size_t i = 0; i < mapping::kButtonCount; ++i) {
      if (!(button_mask & mapping::ButtonMask(static_cast<mapping::Button>(i)))) {
        continue;
      }
      const mapping::Action& a = actions[i];
      if (a.type != mapping::ActionType::kGamepadButton) continue;
      const uint16_t v = a.value;
      if (v < 8) {
        gp.buttons1 |= static_cast<uint8_t>(1u << v);
      } else {
        gp.buttons2 |= static_cast<uint8_t>(1u << (v - 8));
      }
    }
  }

  // Sticks and triggers (direct copy, sanitize sticks: Logical Min=1)
  if (len >= static_cast<uint16_t>(report_base + 6)) {
    auto sanitize = [](uint8_t v) -> uint8_t { return v == 0 ? 1 : v; };
    gp.left_x  = sanitize(report[report_base + 0]);
    gp.left_y  = sanitize(report[report_base + 1]);
    gp.right_x = sanitize(report[report_base + 2]);
    gp.right_y = sanitize(report[report_base + 3]);
    gp.brake   = report[report_base + 4];  // L2 analog
    gp.accel   = report[report_base + 5];  // R2 analog
  }

  gp.consumer = 0;
  return gp;
}

#if FEATHER_ENABLE_DUALSHOCK4_MODE
static device_out::DualShock4Report ParseForDualShock4(uint8_t const* report,
                                                       uint16_t len,
                                                       uint64_t button_mask) {
  device_out::DualShock4Report ds4 = {};

  const uint8_t report_base = (report[0] == 0x01) ? 1 : 0;
  const uint8_t button_base = report_base + 7;

  if (len > button_base) {
    const uint8_t hat = report[button_base] & 0x0F;
    ds4.hat_buttons = (hat > 7) ? 0x08 : hat;
  } else {
    ds4.hat_buttons = 0x08;
  }

  const auto pressed = [button_mask](mapping::Button button) {
    return (button_mask & mapping::ButtonMask(button)) != 0;
  };

  if (pressed(mapping::Button::kSquare)) ds4.hat_buttons |= 0x10;
  if (pressed(mapping::Button::kCross)) ds4.hat_buttons |= 0x20;
  if (pressed(mapping::Button::kCircle)) ds4.hat_buttons |= 0x40;
  if (pressed(mapping::Button::kTriangle)) ds4.hat_buttons |= 0x80;

  if (pressed(mapping::Button::kL1)) ds4.buttons1 |= 0x01;
  if (pressed(mapping::Button::kR1)) ds4.buttons1 |= 0x02;
  if (pressed(mapping::Button::kL2)) ds4.buttons1 |= 0x04;
  if (pressed(mapping::Button::kR2)) ds4.buttons1 |= 0x08;
  if (pressed(mapping::Button::kCreate)) ds4.buttons1 |= 0x10;   // Share
  if (pressed(mapping::Button::kOptions)) ds4.buttons1 |= 0x20;
  if (pressed(mapping::Button::kL3)) ds4.buttons1 |= 0x40;
  if (pressed(mapping::Button::kR3)) ds4.buttons1 |= 0x80;

  if (pressed(mapping::Button::kPs)) ds4.buttons2 |= 0x01;
  if (pressed(mapping::Button::kTouchpad)) ds4.buttons2 |= 0x02;

  if (len >= static_cast<uint16_t>(report_base + 6)) {
    ds4.left_x = report[report_base + 0];
    ds4.left_y = report[report_base + 1];
    ds4.right_x = report[report_base + 2];
    ds4.right_y = report[report_base + 3];
    ds4.left_trigger = report[report_base + 4];
    ds4.right_trigger = report[report_base + 5];
  } else {
    ds4.left_x = ds4.left_y = 0x80;
    ds4.right_x = ds4.right_y = 0x80;
  }

  return ds4;
}
#endif
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
  const mapping::Action* gamepad_actions = mapping::FindGamepadMapping(vid, pid);

  g_controller.active = true;
  g_controller.dev_addr = dev_addr;
  g_controller.instance = instance;
  g_controller.actions = actions;
  g_controller.gamepad_actions = gamepad_actions;
  g_controller.allowed_device_mounted = true;
  g_controller.allowed_dev_addr = dev_addr;
  SetControllerLed(true);

  if (mode::GetActive() == mode::Mode::kGamepad) {
    SendDualSenseLightbar(dev_addr, instance, 255, 200, 0);  // yellow
  }
#if FEATHER_ENABLE_DUALSHOCK4_MODE
  else if (mode::GetActive() == mode::Mode::kDualShock4) {
    SendDualSenseLightbar(dev_addr, instance, 160, 0, 255);  // purple
  }
#endif
  else {
    SendDualSenseLightbar(dev_addr, instance, 0, 0, 255);    // blue
  }
  // Force the DualSense interrupt IN endpoint to 1ms interval (1000 Hz).
  for (int i = 0; i < 32; i++) {  // 32 = PIO_USB_EP_POOL_CNT
    endpoint_t *ep = &pio_usb_ep_pool[i];
    if (ep->dev_addr == dev_addr &&
        (ep->ep_num & EP_IN) &&
        (ep->attr & 0x03) == EP_ATTR_INTERRUPT) {
      ep->interval = 1;
      ep->interval_counter = 0;
    }
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
      empty_gp.hat = 0x08;  // hat center (null state)
      empty_gp.left_x = empty_gp.left_y = 0x80;
      empty_gp.right_x = empty_gp.right_y = 0x80;
      empty_gp.brake = empty_gp.accel = 0;
      empty_gp.consumer = 0;
      device_out::SendGamepad(empty_gp);
#if FEATHER_ENABLE_DUALSHOCK4_MODE
    } else if (mode::GetActive() == mode::Mode::kDualShock4) {
      device_out::DualShock4Report empty_ds4 = {};
      empty_ds4.left_x = empty_ds4.left_y = 0x80;
      empty_ds4.right_x = empty_ds4.right_y = 0x80;
      empty_ds4.hat_buttons = 0x08;
      device_out::SendDualShock4(empty_ds4);
#endif
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

  const uint64_t raw_buttons = ParseDualSenseButtons(report, len);
  const TouchState touch = ParseTouchState(report, len);

  // Swipe gesture works in both modes; ToggleAndReboot() does not return.
  ParseSwipeGesture(touch);

  if (mode::GetActive() == mode::Mode::kGamepad) {
    device_out::SendGamepad(ParseForGamepad(report, len, raw_buttons));
    tuh_hid_receive_report(dev_addr, instance);
    return;
  }
#if FEATHER_ENABLE_DUALSHOCK4_MODE
  if (mode::GetActive() == mode::Mode::kDualShock4) {
    device_out::SendDualShock4(ParseForDualShock4(report, len, raw_buttons));
    tuh_hid_receive_report(dev_addr, instance);
    return;
  }
#endif

  bool keyboard_send = false;
  bool mouse_send = ProcessButtons(raw_buttons |
                 ParseLeftStickButtons(report, len)  |
                 ParseRightStickButtons(report, len),
                 &keyboard_send);

  mouse_send |= ParseTouchpadClick(report, len, touch, &keyboard_send);

  // Single keyboard send covers both button changes and touchpad click zones.
  // Keep absolute state pending until the device endpoint accepts it; otherwise
  // a busy IN endpoint could drop a key press or release permanently.
  if (keyboard_send) {
    g_controller.keyboard_pending = true;
  }
  if (g_controller.keyboard_pending &&
      device_out::SendKeyboard(g_controller.keyboard)) {
    g_controller.keyboard_pending = false;
  }

  int8_t scroll = 0;
  if (ParseTouchScroll(touch, &scroll)) {
    // Scroll and gyro are mutually exclusive; scroll takes priority.
    // Accumulate in case the endpoint wasn't ready last frame.
    const int16_t new_wheel =
        static_cast<int16_t>(g_controller.mouse.wheel) + scroll;
    g_controller.mouse.wheel = static_cast<int8_t>(
        new_wheel > 127 ? 127 : new_wheel < -127 ? -127 : new_wheel);
    mouse_send = true;
  } else {
    int16_t mouse_x = 0;
    int16_t mouse_y = 0;
    if (ParseGyroMouse(report, len, touch, &mouse_x, &mouse_y)) {
      // Accumulate across frames so pixels are never lost when the host
      // polls slower than 1000 Hz (e.g. macOS at 125 Hz).
      g_controller.mouse.x = ClampI16(
          static_cast<int32_t>(g_controller.mouse.x) + mouse_x);
      g_controller.mouse.y = ClampI16(
          static_cast<int32_t>(g_controller.mouse.y) + mouse_y);
      mouse_send = true;
    }
  }

  // Also flush any accumulated x/y/wheel from previously dropped frames.
  mouse_send |= (g_controller.mouse.x != 0 ||
                 g_controller.mouse.y != 0 ||
                 g_controller.mouse.wheel != 0);

  // Single consolidated mouse send per callback.
  // Mouse buttons are absolute state, so retry failed sends even when no
  // relative x/y/wheel data is pending.
  // Clear relative axes only after a successful send — if the endpoint is not
  // ready the values stay accumulated for the next callback.
  if (mouse_send) {
    g_controller.mouse_pending = true;
  }
  if (g_controller.mouse_pending && device_out::SendMouse(g_controller.mouse)) {
    g_controller.mouse_pending = false;
    g_controller.mouse.x = 0;
    g_controller.mouse.y = 0;
    g_controller.mouse.wheel = 0;
    g_controller.mouse.pan = 0;
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
