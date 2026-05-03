#include "device_out.h"

#include <string.h>
#include "bsp/board_api.h"
#include "mode.h"
#include "tusb.h"

namespace device_out {

namespace {

constexpr uint8_t kNoHidInstance = 0xff;

uint8_t KeyboardInstance() {
  switch (mode::GetActive()) {
    case mode::Mode::kKeyboardMouse:
    case mode::Mode::kHybrid:
      return 0;
    default:
      return kNoHidInstance;
  }
}

uint8_t MouseInstance() {
  switch (mode::GetActive()) {
    case mode::Mode::kKeyboardMouse:
    case mode::Mode::kHybrid:
      return 1;
    default:
      return kNoHidInstance;
  }
}

uint8_t GamepadInstance() {
  switch (mode::GetActive()) {
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
  board_init();
  tusb_init();
}

void Task() {
  tud_task();
}

bool HasKeyboard() {
  return KeyboardInstance() != kNoHidInstance;
}

bool HasMouse() {
  return MouseInstance() != kNoHidInstance;
}

bool HasGamepad() {
  return GamepadInstance() != kNoHidInstance;
}

bool SendKeyboard(const KeyboardReport& report) {
  const uint8_t instance = KeyboardInstance();
  if (instance == kNoHidInstance ||
      !tud_ready() ||
      !tud_hid_n_ready(instance)) {
    return false;
  }

  return tud_hid_n_report(instance, kReportIdKeyboard, &report, sizeof(report));
}

bool SendMouse(const MouseReport& report) {
  const uint8_t instance = MouseInstance();
  if (instance == kNoHidInstance ||
      !tud_ready() ||
      !tud_hid_n_ready(instance)) {
    return false;
  }

  return tud_hid_n_report(instance, kReportIdMouse, &report, sizeof(report));
}

bool SendGamepad(const GamepadReport& report) {
  const uint8_t instance = GamepadInstance();
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
