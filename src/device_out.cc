#include "device_out.h"

#include <string.h>
#include "bsp/board_api.h"
#include "mode.h"
#include "tusb.h"

namespace device_out {

void Init() {
  board_init();
  tusb_init();
}

void Task() {
  tud_task();
}

bool SendKeyboard(const KeyboardReport& report) {
  if (!tud_ready() || !tud_hid_n_ready(0)) {
    return false;
  }

  return tud_hid_n_report(0, kReportIdKeyboard, &report, sizeof(report));
}

bool SendMouse(const MouseReport& report) {
  if (!tud_ready() || !tud_hid_n_ready(1)) {
    return false;
  }

  return tud_hid_n_report(1, kReportIdMouse, &report, sizeof(report));
}

bool SendGamepad(const GamepadReport& report) {
  if (!tud_ready() || !tud_hid_n_ready(0)) {
    return false;
  }

  return tud_hid_n_report(0, kReportIdGamepad, &report, sizeof(report));
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
