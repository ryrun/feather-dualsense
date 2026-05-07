#include "status_hid.h"

#if FEATHER_STATUS_HID_ENABLE

#include <string.h>

#include "device_out.h"
#include "pico/stdlib.h"

namespace status_hid {
namespace {

constexpr uint32_t kReportIntervalUs = 1000000u / FEATHER_STATUS_HID_RATE_HZ;

Report g_report = {};
uint32_t g_last_send_us = 0;

int16_t ReadLe16(uint8_t const* data) {
  return static_cast<int16_t>(static_cast<uint16_t>(data[0]) |
                              (static_cast<uint16_t>(data[1]) << 8));
}

void ResetPayload() {
  memset(&g_report, 0, sizeof(g_report));
  g_report.version = kReportVersion;
}

}  // namespace

void Init() {
  ResetPayload();
}

void Task() {
  const uint32_t now = time_us_32();
  if (static_cast<uint32_t>(now - g_last_send_us) < kReportIntervalUs) {
    return;
  }

  Report report = g_report;
  report.sequence = static_cast<uint16_t>(report.sequence + 1);
  if (device_out::SendStatus(reinterpret_cast<uint8_t const*>(&report),
                             sizeof(report))) {
    g_report.sequence = report.sequence;
    g_last_send_us = now;
  }
}

void SetConnected(ControllerType type) {
  ResetPayload();
  g_report.controller_type = static_cast<uint8_t>(type);
  g_report.flags = kFlagControllerConnected;
}

void SetDisconnected() {
  ResetPayload();
}

void UpdateFromInput(uint8_t const* report, uint16_t len, uint64_t buttons,
                     const TouchPoint& touch0, const TouchPoint& touch1,
                     bool gyro_mouse_active, bool gyro_stick_active) {
  if (len == 0) {
    return;
  }

  g_report.buttons = buttons;

  const uint8_t report_base = (report[0] == 0x01) ? 1 : 0;
  if (len >= static_cast<uint16_t>(report_base + 6)) {
    g_report.left_x = report[report_base + 0];
    g_report.left_y = report[report_base + 1];
    g_report.right_x = report[report_base + 2];
    g_report.right_y = report[report_base + 3];
    g_report.l2 = report[report_base + 4];
    g_report.r2 = report[report_base + 5];
  }

  uint16_t flags = kFlagControllerConnected;
  if (touch0.active) {
    flags |= kFlagTouch0Active;
  }
  if (touch1.active) {
    flags |= kFlagTouch1Active;
  }
  if (gyro_mouse_active) {
    flags |= kFlagGyroMouseActive;
  }
  if (gyro_stick_active) {
    flags |= kFlagGyroStickActive;
  }
  g_report.flags = flags;

  g_report.touch0_id = touch0.id;
  g_report.touch1_id = touch1.id;
  g_report.touch0_x = touch0.x;
  g_report.touch0_y = touch0.y;
  g_report.touch1_x = touch1.x;
  g_report.touch1_y = touch1.y;

  constexpr uint8_t kGyroOffset = 15;
  constexpr uint8_t kAccelOffset = 21;
  constexpr uint8_t kGyroZ = kGyroOffset + 4;
  constexpr uint8_t kAccelZ = kAccelOffset + 4;
  if (len > report_base + kGyroZ + 1) {
    g_report.gyro_x = ReadLe16(&report[report_base + kGyroOffset + 0]);
    g_report.gyro_y = ReadLe16(&report[report_base + kGyroOffset + 2]);
    g_report.gyro_z = ReadLe16(&report[report_base + kGyroOffset + 4]);
  }
  if (len > report_base + kAccelZ + 1) {
    g_report.accel_x = ReadLe16(&report[report_base + kAccelOffset + 0]);
    g_report.accel_y = ReadLe16(&report[report_base + kAccelOffset + 2]);
    g_report.accel_z = ReadLe16(&report[report_base + kAccelOffset + 4]);
  }
}

}  // namespace status_hid

#endif  // FEATHER_STATUS_HID_ENABLE
