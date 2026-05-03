#include "device_out.h"

#include <string.h>
#include "bsp/board_api.h"
#include "mode.h"
#include "tusb.h"

namespace device_out {

namespace {

constexpr uint8_t kNoHidInstance = 0xff;

uint8_t g_keyboard_instance = kNoHidInstance;
uint8_t g_mouse_instance = kNoHidInstance;
uint8_t g_gamepad_instance = kNoHidInstance;

uint8_t KeyboardInstanceForMode(mode::Mode active_mode) {
  switch (active_mode) {
    case mode::Mode::kKeyboardMouse:
    case mode::Mode::kHybrid:
      return 0;
    default:
      return kNoHidInstance;
  }
}

uint8_t MouseInstanceForMode(mode::Mode active_mode) {
  switch (active_mode) {
    case mode::Mode::kKeyboardMouse:
    case mode::Mode::kHybrid:
      return 1;
    default:
      return kNoHidInstance;
  }
}

uint8_t GamepadInstanceForMode(mode::Mode active_mode) {
  switch (active_mode) {
    case mode::Mode::kGamepad:
      return 0;
    case mode::Mode::kHybrid:
      return 2;
#if GYRO_STICK_PROFILE_ENABLE
    case mode::Mode::kGyroStick:
      return 0;
#endif
    default:
      return kNoHidInstance;
  }
}

}  // namespace

void Init() {
  const mode::Mode active_mode = mode::GetActive();
  g_keyboard_instance = KeyboardInstanceForMode(active_mode);
  g_mouse_instance = MouseInstanceForMode(active_mode);
  g_gamepad_instance = GamepadInstanceForMode(active_mode);

  board_init();
  tusb_init();
}

void Task() {
  tud_task();
}

bool HasKeyboard() {
  return g_keyboard_instance != kNoHidInstance;
}

bool HasMouse() {
  return g_mouse_instance != kNoHidInstance;
}

bool HasGamepad() {
  return g_gamepad_instance != kNoHidInstance;
}

bool SendKeyboard(const KeyboardReport& report) {
  const uint8_t instance = g_keyboard_instance;
  if (instance == kNoHidInstance ||
      !tud_ready() ||
      !tud_hid_n_ready(instance)) {
    return false;
  }

  return tud_hid_n_report(instance, kReportIdKeyboard, &report, sizeof(report));
}

bool SendMouse(const MouseReport& report) {
  const uint8_t instance = g_mouse_instance;
  if (instance == kNoHidInstance ||
      !tud_ready() ||
      !tud_hid_n_ready(instance)) {
    return false;
  }

  return tud_hid_n_report(instance, kReportIdMouse, &report, sizeof(report));
}

bool SendGamepad(const GamepadReport& report) {
  const uint8_t instance = g_gamepad_instance;
  if (instance == kNoHidInstance ||
      !tud_ready() ||
      !tud_hid_n_ready(instance)) {
    return false;
  }

  return tud_hid_n_report(instance, kReportIdGamepad, &report, sizeof(report));
}

}  // namespace device_out

extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance,
                                           uint8_t report_id,
                                           hid_report_type_t report_type,
                                           uint8_t* buffer,
                                           uint16_t reqlen) {
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;
  return 0;
}

extern "C" void tud_hid_set_report_cb(uint8_t instance,
                                       uint8_t report_id,
                                       hid_report_type_t report_type,
                                       uint8_t const* buffer,
                                       uint16_t bufsize) {
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)bufsize;
}
