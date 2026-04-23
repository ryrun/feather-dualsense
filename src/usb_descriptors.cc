#include <stdint.h>
#include <string.h>

#include "bsp/board_api.h"
#include "build_config.h"
#include "class/hid/hid.h"
#include "device_out.h"
#include "mode.h"
#include "pico/unique_id.h"
#include "tusb.h"

namespace {

// ── Keyboard/mouse mode ────────────────────────────────────────────────────

enum InterfaceNumberKbm : uint8_t {
  kItfKeyboard = 0,
  kItfMouse,
  kItfKbmTotal,
};

enum EndpointAddressKbm : uint8_t {
  kEpnKeyboard = 0x81,
  kEpnMouse = 0x82,
};

constexpr uint16_t kUsbVid    = 0xCafe;
constexpr uint16_t kUsbPidKbm = 0x4023;
// Google VID + Stadia PID: recognized natively by macOS, Chrome and SDL2.
constexpr uint16_t kUsbVidGp  = 0x18D1;
constexpr uint16_t kUsbPidGp  = 0x9400;
constexpr uint16_t kUsbBcd    = 0x0100;

uint8_t const kDescHidKeyboard[] = {
    0x05, 0x01,                                  // Usage Page (Generic Desktop)
    0x09, 0x06,                                  // Usage (Keyboard)
    0xA1, 0x01,                                  // Collection (Application)
    0x85, device_out::kReportIdKeyboard,         // Report ID
    0x05, 0x07,                                  // Usage Page (Keyboard)
    0x19, 0xE0,                                  // Usage Minimum (Keyboard LeftControl)
    0x29, 0xE7,                                  // Usage Maximum (Keyboard Right GUI)
    0x15, 0x00,                                  // Logical Minimum (0)
    0x25, 0x01,                                  // Logical Maximum (1)
    0x75, 0x01,                                  // Report Size (1)
    0x95, 0x08,                                  // Report Count (8)
    0x81, 0x02,                                  // Input (Data, Variable, Absolute)
    0x95, 0x01,                                  // Report Count (1)
    0x75, 0x08,                                  // Report Size (8)
    0x81, 0x03,                                  // Input (Constant, Variable, Absolute)
    0x05, 0x07,                                  // Usage Page (Keyboard)
    0x19, 0x00,                                  // Usage Minimum (Reserved)
    0x29, 0xFF,                                  // Usage Maximum (255)
    0x15, 0x00,                                  // Logical Minimum (0)
    0x26, 0xFF, 0x00,                            // Logical Maximum (255)
    0x75, 0x08,                                  // Report Size (8)
    0x95, KEYBOARD_ROLLOVER_KEYS,                // Report Count (14)
    0x81, 0x00,                                  // Input (Data, Array, Absolute)
    0xC0,                                        // End Collection
};

uint8_t const kDescHidMouse[] = {
    0x05, 0x01,                         // Usage Page (Generic Desktop)
    0x09, 0x02,                         // Usage (Mouse)
    0xA1, 0x01,                         // Collection (Application)
    0x85, device_out::kReportIdMouse,   // Report ID
    0x09, 0x01,                         // Usage (Pointer)
    0xA1, 0x00,                         // Collection (Physical)
    0x05, 0x09,                         // Usage Page (Button)
    0x19, 0x01,                         // Usage Minimum (1)
    0x29, 0x05,                         // Usage Maximum (5)
    0x15, 0x00,                         // Logical Minimum (0)
    0x25, 0x01,                         // Logical Maximum (1)
    0x95, 0x05,                         // Report Count (5)
    0x75, 0x01,                         // Report Size (1)
    0x81, 0x02,                         // Input (Data, Variable, Absolute)
    0x95, 0x01,                         // Report Count (1)
    0x75, 0x03,                         // Report Size (3)
    0x81, 0x03,                         // Input (Constant, Variable, Absolute)
    0x05, 0x01,                         // Usage Page (Generic Desktop)
    0x09, 0x30,                         // Usage (X)
    0x09, 0x31,                         // Usage (Y)
    0x16, 0x01, 0x80,                   // Logical Minimum (-32767)
    0x26, 0xFF, 0x7F,                   // Logical Maximum (32767)
    0x75, 0x10,                         // Report Size (16)
    0x95, 0x02,                         // Report Count (2)
    0x81, 0x06,                         // Input (Data, Variable, Relative)
    0x09, 0x38,                         // Usage (Wheel)
    0x15, 0x81,                         // Logical Minimum (-127)
    0x25, 0x7F,                         // Logical Maximum (127)
    0x75, 0x08,                         // Report Size (8)
    0x95, 0x01,                         // Report Count (1)
    0x81, 0x06,                         // Input (Data, Variable, Relative)
    0x05, 0x0C,                         // Usage Page (Consumer)
    0x0A, 0x38, 0x02,                   // Usage (AC Pan)
    0x15, 0x81,                         // Logical Minimum (-127)
    0x25, 0x7F,                         // Logical Maximum (127)
    0x75, 0x08,                         // Report Size (8)
    0x95, 0x01,                         // Report Count (1)
    0x81, 0x06,                         // Input (Data, Variable, Relative)
    0xC0,                               // End Collection
    0xC0,                               // End Collection
};

uint8_t const kDescConfigKbm[] = {
    TUD_CONFIG_DESCRIPTOR(1, kItfKbmTotal, 0,
                          TUD_CONFIG_DESC_LEN + 2 * TUD_HID_DESC_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(kItfKeyboard, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(kDescHidKeyboard),
                       kEpnKeyboard, CFG_TUD_HID_EP_BUFSIZE, REPORT_INTERVAL_MS),
    TUD_HID_DESCRIPTOR(kItfMouse, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(kDescHidMouse),
                       kEpnMouse, CFG_TUD_HID_EP_BUFSIZE, REPORT_INTERVAL_MS),
};

// ── Gamepad mode ───────────────────────────────────────────────────────────

enum InterfaceNumberGp : uint8_t {
  kItfGamepad = 0,
  kItfGpTotal,
};

enum EndpointAddressGp : uint8_t {
  kEpnGamepad = 0x81,
};

// Stadia-compatible gamepad:
//   16 buttons (2 bytes), hat switch (4 bits + 4 pad = 1 byte),
//   LX/LY/RX/RY/R2/L2 (6 × uint8 = 6 bytes, 0-255)
//   = 9 bytes payload.
uint8_t const kDescHidGamepad[] = {
    0x05, 0x01,                         // Usage Page (Generic Desktop)
    0x09, 0x05,                         // Usage (Gamepad)
    0xA1, 0x01,                         // Collection (Application)
    0x85, device_out::kReportIdGamepad, // Report ID
    // 16 buttons (no padding needed — 16 bits fill exactly 2 bytes)
    0x05, 0x09,                         // Usage Page (Button)
    0x19, 0x01,                         // Usage Minimum (1)
    0x29, 0x10,                         // Usage Maximum (16)
    0x15, 0x00,                         // Logical Minimum (0)
    0x25, 0x01,                         // Logical Maximum (1)
    0x75, 0x01,                         // Report Size (1)
    0x95, 0x10,                         // Report Count (16)
    0x81, 0x02,                         // Input (Data, Variable, Absolute)
    // Hat switch
    0x05, 0x01,                         // Usage Page (Generic Desktop)
    0x09, 0x39,                         // Usage (Hat Switch)
    0x15, 0x00,                         // Logical Minimum (0)
    0x25, 0x07,                         // Logical Maximum (7)
    0x35, 0x00,                         // Physical Minimum (0)
    0x46, 0x3B, 0x01,                   // Physical Maximum (315)
    0x65, 0x14,                         // Unit (Rotation, Degrees)
    0x75, 0x04,                         // Report Size (4)
    0x95, 0x01,                         // Report Count (1)
    0x81, 0x42,                         // Input (Data, Variable, Absolute, Null State)
    // 4 padding bits
    0x65, 0x00,                         // Unit (None)
    0x75, 0x04,                         // Report Size (4)
    0x95, 0x01,                         // Report Count (1)
    0x81, 0x03,                         // Input (Constant)
    // LX, LY, RX, RY, R2 (Z = SDL2 axis 4), L2 (Rz = SDL2 axis 5) — all uint8 0-255
    0x05, 0x01,                         // Usage Page (Generic Desktop)
    0x09, 0x30,                         // Usage (X)   — Left X
    0x09, 0x31,                         // Usage (Y)   — Left Y
    0x09, 0x33,                         // Usage (Rx)  — Right X
    0x09, 0x34,                         // Usage (Ry)  — Right Y
    0x09, 0x32,                         // Usage (Z)   — R2, SDL2 axis 4
    0x09, 0x35,                         // Usage (Rz)  — L2, SDL2 axis 5
    0x15, 0x00,                         // Logical Minimum (0)
    0x26, 0xFF, 0x00,                   // Logical Maximum (255)
    0x75, 0x08,                         // Report Size (8)
    0x95, 0x06,                         // Report Count (6)
    0x81, 0x02,                         // Input (Data, Variable, Absolute)
    0xC0,                               // End Collection
};

uint8_t const kDescConfigGamepad[] = {
    TUD_CONFIG_DESCRIPTOR(1, kItfGpTotal, 0,
                          TUD_CONFIG_DESC_LEN + 1 * TUD_HID_DESC_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(kItfGamepad, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(kDescHidGamepad),
                       kEpnGamepad, CFG_TUD_HID_EP_BUFSIZE, REPORT_INTERVAL_MS),
};

// ── Device descriptors (mode-selected at runtime) ──────────────────────────

tusb_desc_device_t const kDescDeviceKbm = {
    sizeof(tusb_desc_device_t), TUSB_DESC_DEVICE, 0x0200,
    0x00, 0x00, 0x00,
    CFG_TUD_ENDPOINT0_SIZE,
    kUsbVid, kUsbPidKbm, kUsbBcd,
    0x01, 0x02, 0x03, 0x01,
};

tusb_desc_device_t const kDescDeviceGamepad = {
    sizeof(tusb_desc_device_t), TUSB_DESC_DEVICE, 0x0200,
    0x00, 0x00, 0x00,
    CFG_TUD_ENDPOINT0_SIZE,
    kUsbVidGp, kUsbPidGp, kUsbBcd,
    0x01, 0x02, 0x03, 0x01,
};

char const* kStringDescriptorsKbm[] = {
    nullptr,
    "feather-dualsense",
    "DualSense HID Remapper",
};

char const* kStringDescriptorsGamepad[] = {
    nullptr,
    "Google LLC",
    "Stadia Controller rev. A",
};

uint16_t g_string_desc[32];

}  // namespace

extern "C" uint8_t const* tud_descriptor_device_cb() {
  if (mode::GetActive() == mode::Mode::kGamepad) {
    return reinterpret_cast<uint8_t const*>(&kDescDeviceGamepad);
  }
  return reinterpret_cast<uint8_t const*>(&kDescDeviceKbm);
}

extern "C" uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  if (mode::GetActive() == mode::Mode::kGamepad) {
    return kDescConfigGamepad;
  }
  return kDescConfigKbm;
}

extern "C" uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
  if (mode::GetActive() == mode::Mode::kGamepad) {
    return kDescHidGamepad;
  }
  return instance == 0 ? kDescHidKeyboard : kDescHidMouse;
}

extern "C" uint16_t const* tud_descriptor_string_cb(uint8_t index,
                                                      uint16_t langid) {
  (void)langid;

  const bool is_gamepad = (mode::GetActive() == mode::Mode::kGamepad);
  char const** descs = is_gamepad ? kStringDescriptorsGamepad
                                  : kStringDescriptorsKbm;
  const size_t descs_len = is_gamepad
      ? sizeof(kStringDescriptorsGamepad) / sizeof(kStringDescriptorsGamepad[0])
      : sizeof(kStringDescriptorsKbm) / sizeof(kStringDescriptorsKbm[0]);

  uint8_t chr_count = 0;

  if (index == 0) {
    g_string_desc[1] = 0x0409;
    chr_count = 1;
  } else if (index == 3) {
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    static constexpr char hex[] = "0123456789ABCDEF";
    for (uint8_t i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES && chr_count < 31;
         ++i) {
      const uint8_t byte = board_id.id[i];
      g_string_desc[1 + chr_count++] = hex[byte >> 4];
      if (chr_count < 31) {
        g_string_desc[1 + chr_count++] = hex[byte & 0x0f];
      }
    }
  } else {
    if (index >= descs_len) {
      return nullptr;
    }
    const char* str = descs[index];
    while (str[chr_count] && chr_count < 31) {
      g_string_desc[1 + chr_count] = str[chr_count];
      ++chr_count;
    }
  }

  g_string_desc[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
  return g_string_desc;
}
