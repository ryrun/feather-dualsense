#pragma once

#include <stdint.h>
#include "build_config.h"

namespace status_hid {

constexpr uint8_t kReportId = 0x7e;
constexpr uint8_t kReportVersion = 1;
constexpr uint16_t kVendorUsagePage = 0xff42;
constexpr uint8_t kReportSize = 63;
constexpr uint8_t kPacketSize = 64;
constexpr uint8_t kVendorUsagePageLow = kVendorUsagePage & 0xff;
constexpr uint8_t kVendorUsagePageHigh = kVendorUsagePage >> 8;

static_assert(FEATHER_STATUS_HID_RATE_HZ > 0,
              "FEATHER_STATUS_HID_RATE_HZ must be greater than zero");

enum class ControllerType : uint8_t {
  kUnknown = 0,
  kDualSense = 1,
  kDualSenseEdge = 2,
};

enum Flags : uint16_t {
  kFlagControllerConnected = 1u << 0,
  kFlagTouch0Active = 1u << 1,
  kFlagTouch1Active = 1u << 2,
  kFlagGyroMouseActive = 1u << 3,
  kFlagGyroStickActive = 1u << 4,
};

struct TouchPoint {
  bool active;
  uint8_t id;
  uint16_t x;
  uint16_t y;
};

struct Report {
  uint8_t version;
  uint8_t controller_type;
  uint16_t sequence;
  uint64_t buttons;
  uint8_t left_x;
  uint8_t left_y;
  uint8_t right_x;
  uint8_t right_y;
  uint8_t l2;
  uint8_t r2;
  uint16_t flags;
  uint8_t touch0_id;
  uint8_t touch1_id;
  uint16_t touch0_x;
  uint16_t touch0_y;
  uint16_t touch1_x;
  uint16_t touch1_y;
  int16_t gyro_x;
  int16_t gyro_y;
  int16_t gyro_z;
  int16_t accel_x;
  int16_t accel_y;
  int16_t accel_z;
  uint8_t reserved[21];
} __attribute__((packed));

static_assert(sizeof(Report) == kReportSize,
              "Status HID payload must fit in one 64-byte packet with report ID");
static_assert(sizeof(Report) + 1 == kPacketSize,
              "Status HID report ID plus payload must be one 64-byte packet");

#if FEATHER_STATUS_HID_ENABLE
void Init();
void Task();
void SetConnected(ControllerType type);
void SetDisconnected();
void UpdateFromInput(uint8_t const* report, uint16_t len, uint64_t buttons,
                     const TouchPoint& touch0, const TouchPoint& touch1,
                     bool gyro_mouse_active, bool gyro_stick_active);
#else
inline void Init() {}
inline void Task() {}
inline void SetConnected(ControllerType) {}
inline void SetDisconnected() {}
inline void UpdateFromInput(uint8_t const*, uint16_t, uint64_t,
                            const TouchPoint&, const TouchPoint&, bool, bool) {}
#endif

}  // namespace status_hid
