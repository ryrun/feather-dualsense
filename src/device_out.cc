#include "device_out.h"

#include <string.h>
#include "bsp/board_api.h"
#include "mode.h"
#include "tusb.h"
#include "usb_layout.h"

namespace device_out {

namespace {

constexpr uint8_t kNoHidInstance = 0xff;
#if !FEATHER_GAMEPAD_BACKEND_DUALSHOCK4
constexpr uint8_t kStadiaConfigInterface = 1;
constexpr uint8_t kStadiaConfigReportId = 100;
constexpr uint16_t kStadiaConfigReportSize = 32;
#endif

uint8_t g_keyboard_instance = kNoHidInstance;
uint8_t g_mouse_instance = kNoHidInstance;
uint8_t g_gamepad_instance = kNoHidInstance;
#if FEATHER_STATUS_HID_ENABLE
uint8_t g_status_instance = kNoHidInstance;
#endif

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

#if FEATHER_STATUS_HID_ENABLE
uint8_t StatusInstanceForMode(mode::Mode active_mode) {
  switch (active_mode) {
    case mode::Mode::kKeyboardMouse:
      return usb_layout::kItfStatusKbm;
    case mode::Mode::kHybrid:
      return usb_layout::kItfStatusHybrid;
    case mode::Mode::kGamepad:
      return usb_layout::kItfStatusGamepad;
#if GYRO_STICK_PROFILE_ENABLE
    case mode::Mode::kGyroStick:
      return usb_layout::kItfStatusGamepad;
#endif
    default:
      return kNoHidInstance;
  }
}
#endif

#if !FEATHER_GAMEPAD_BACKEND_DUALSHOCK4
bool HasStadiaConfigInterface() {
  const mode::Mode active_mode = mode::GetActive();
  if (active_mode == mode::Mode::kGamepad) {
    return true;
  }
#if GYRO_STICK_PROFILE_ENABLE
  if (active_mode == mode::Mode::kGyroStick) {
    return true;
  }
#endif
  return false;
}
#endif

}  // namespace

void Init() {
  const mode::Mode active_mode = mode::GetActive();
  g_keyboard_instance = KeyboardInstanceForMode(active_mode);
  g_mouse_instance = MouseInstanceForMode(active_mode);
  g_gamepad_instance = GamepadInstanceForMode(active_mode);
#if FEATHER_STATUS_HID_ENABLE
  g_status_instance = StatusInstanceForMode(active_mode);
#endif

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

#if FEATHER_STATUS_HID_ENABLE
bool HasStatus() {
  return g_status_instance != kNoHidInstance;
}
#endif

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

#if FEATHER_STATUS_HID_ENABLE
bool SendStatus(uint8_t const* report, uint16_t len) {
  const uint8_t instance = g_status_instance;
  if (instance == kNoHidInstance ||
      !tud_ready() ||
      !tud_hid_n_ready(instance)) {
    return false;
  }

  return tud_hid_n_report(instance, kReportIdStatus, report, len);
}
#endif

}  // namespace device_out

extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance,
                                           uint8_t report_id,
                                           hid_report_type_t report_type,
                                           uint8_t* buffer,
                                           uint16_t reqlen) {
#if !FEATHER_GAMEPAD_BACKEND_DUALSHOCK4
  if (device_out::HasStadiaConfigInterface() &&
      instance == device_out::kStadiaConfigInterface &&
      report_id == device_out::kStadiaConfigReportId &&
      report_type == HID_REPORT_TYPE_FEATURE &&
      reqlen >= device_out::kStadiaConfigReportSize) {
    memset(buffer, 0, device_out::kStadiaConfigReportSize);
    return device_out::kStadiaConfigReportSize;
  }
#endif

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
