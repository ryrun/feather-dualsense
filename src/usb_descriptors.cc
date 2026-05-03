#include <stdint.h>
#include <string.h>

#include "bsp/board_api.h"
#include "build_config.h"
#include "class/hid/hid.h"
#include "device_out.h"
#include "pico/unique_id.h"
#include "tusb.h"

namespace {

// ── Composite keyboard/mouse/gamepad output ────────────────────────────────

enum InterfaceNumber : uint8_t {
  kItfKeyboard = 0,
  kItfMouse,
  kItfGamepad,
  kItfTotal,
};

enum EndpointAddress : uint8_t {
  kEpnKeyboard = 0x81,
  kEpnMouse = 0x82,
  kEpnGamepad = 0x83,
};

// Stadia Controller Rev. A: Google VID/PID, macOS GCController recognizes natively.
constexpr uint16_t kUsbVid = 0x18D1;  // Google LLC
constexpr uint16_t kUsbPid = 0x9400;  // Stadia Controller
constexpr uint16_t kUsbBcd = 0x0100;

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

// ── Gamepad interface ──────────────────────────────────────────────────────

// Exact Stadia Controller Rev. A HID descriptor.
// Report ID 3, 10-byte input payload:
//   [0] hat(4)+pad(4)  [1] buttons1  [2] buttons2(6)+pad  [3-6] LX/LY/RX/RY
//   [7] brake/L2  [8] accel/R2  [9] consumer(3)+pad(5)
uint8_t const kDescHidGamepad[] = {
    0x05, 0x01,                    // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,                    // Usage (Game Pad)
    0xA1, 0x01,                    // Collection (Application)
    0x85, 0x03,                    //   Report ID (3)
    // D-Pad / Hat
    0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
    0x75, 0x04,                    //   Report Size (4)
    0x95, 0x01,                    //   Report Count (1)
    0x25, 0x07,                    //   Logical Maximum (7)
    0x46, 0x3B, 0x01,              //   Physical Maximum (315)
    0x65, 0x14,                    //   Unit (English Rotation, Centimeter)
    0x09, 0x39,                    //   Usage (Hat switch)
    0x81, 0x42,                    //   Input (Data,Var,Abs,Null State)
    0x45, 0x00,                    //   Physical Maximum (0)
    0x65, 0x00,                    //   Unit (None)
    // 4 padding bits
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x04,                    //   Report Count (4)
    0x81, 0x01,                    //   Input (Const)
    // 15 buttons (buttons1 + buttons2)
    0x05, 0x09,                    //   Usage Page (Button)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x0F,                    //   Report Count (15)
    0x09, 0x12, 0x09, 0x11, 0x09, 0x14, 0x09, 0x13,  // Y, X, B, A
    0x09, 0x0D, 0x09, 0x0C, 0x09, 0x0B, 0x09, 0x0F,  // LB, LT, RB, RT
    0x09, 0x0E, 0x09, 0x08, 0x09, 0x07, 0x09, 0x05,  // F,G,H,I
    0x09, 0x04, 0x09, 0x02, 0x09, 0x01,              // J,K,L
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    // 1 padding bit
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x01,                    //   Input (Const)
    // Left Stick X/Y (Logical Min=1, range 1-255, neutral=128)
    0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
    0x15, 0x01,                    //   Logical Minimum (1)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x09, 0x01,                    //   Usage (Pointer)
    0xA1, 0x00,                    //   Collection (Physical)
    0x09, 0x30,                    //     Usage (X)
    0x09, 0x31,                    //     Usage (Y)
    0x75, 0x08,                    //     Report Size (8)
    0x95, 0x02,                    //     Report Count (2)
    0x81, 0x02,                    //     Input (Data,Var,Abs)
    0xC0,                          //   End Collection
    // Right Stick X/Y
    0x09, 0x01,                    //   Usage (Pointer)
    0xA1, 0x00,                    //   Collection (Physical)
    0x09, 0x32,                    //     Usage (Z)
    0x09, 0x35,                    //     Usage (Rz)
    0x75, 0x08,                    //     Report Size (8)
    0x95, 0x02,                    //     Report Count (2)
    0x81, 0x02,                    //     Input (Data,Var,Abs)
    0xC0,                          //   End Collection
    // Brake (L2) and Accelerator (R2)
    0x05, 0x02,                    //   Usage Page (Sim Ctrls)
    0x75, 0x08,                    //   Report Size (8)
    0x95, 0x02,                    //   Report Count (2)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x09, 0xC5,                    //   Usage (Brake)
    0x09, 0xC4,                    //   Usage (Accelerator)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    // Consumer controls (volume up, volume down, play/pause + 5 pad bits)
    0x05, 0x0C,                    //   Usage Page (Consumer)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x09, 0xE9, 0x09, 0xEA,        //   Usage (Volume Increment, Volume Decrement)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x02,                    //   Report Count (2)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    0x09, 0xCD,                    //   Usage (Play/Pause)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    0x95, 0x05,                    //   Report Count (5)
    0x81, 0x01,                    //   Input (Const)
    // Output Report ID 5 (rumble via PID)
    0x85, 0x05,                    //   Report ID (5)
    0x06, 0x0F, 0x00,              //   Usage Page (PID Page)
    0x09, 0x97,                    //   Usage (0x97)
    0x75, 0x10,                    //   Report Size (16)
    0x95, 0x02,                    //   Report Count (2)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  //   Logical Maximum (65535)
    0x91, 0x02,                    //   Output (Data,Var,Abs)
    0xC0,                          // End Collection
};

uint8_t const kDescConfig[] = {
    TUD_CONFIG_DESCRIPTOR(1, kItfTotal, 0,
                          TUD_CONFIG_DESC_LEN + 3 * TUD_HID_DESC_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(kItfKeyboard, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(kDescHidKeyboard),
                       kEpnKeyboard, CFG_TUD_HID_EP_BUFSIZE, REPORT_INTERVAL_MS),
    TUD_HID_DESCRIPTOR(kItfMouse, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(kDescHidMouse),
                       kEpnMouse, CFG_TUD_HID_EP_BUFSIZE, REPORT_INTERVAL_MS),
    TUD_HID_DESCRIPTOR(kItfGamepad, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(kDescHidGamepad),
                       kEpnGamepad, CFG_TUD_HID_EP_BUFSIZE, REPORT_INTERVAL_MS),
};

// ── DualShock 4 mode ───────────────────────────────────────────────────────

#if FEATHER_ENABLE_DUALSHOCK4_MODE

enum InterfaceNumberDs4 : uint8_t {
  kItfDs4 = 0,
  kItfDs4Total,
};

enum EndpointAddressDs4 : uint8_t {
  kEpnDs4 = 0x81,
};

uint8_t const kDescHidDs4[] = {
    0x05, 0x01,                    // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,                    // Usage (Game Pad)
    0xA1, 0x01,                    // Collection (Application)
    0x85, 0x01,                    //   Report ID (1)
    0x09, 0x30,                    //   Usage (X)
    0x09, 0x31,                    //   Usage (Y)
    0x09, 0x32,                    //   Usage (Z)
    0x09, 0x35,                    //   Usage (Rz)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8)
    0x95, 0x04,                    //   Report Count (4)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    0x09, 0x39,                    //   Usage (Hat switch)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x07,                    //   Logical Maximum (7)
    0x35, 0x00,                    //   Physical Minimum (0)
    0x46, 0x3B, 0x01,              //   Physical Maximum (315)
    0x65, 0x14,                    //   Unit (English Rotation, Centimeter)
    0x75, 0x04,                    //   Report Size (4)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x42,                    //   Input (Data,Var,Abs,Null State)
    0x65, 0x00,                    //   Unit (None)
    0x05, 0x09,                    //   Usage Page (Button)
    0x19, 0x01,                    //   Usage Minimum (Button 1)
    0x29, 0x0E,                    //   Usage Maximum (Button 14)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x0E,                    //   Report Count (14)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    0x75, 0x06,                    //   Report Size (6)
    0x95, 0x01,                    //   Report Count (1)
    0x81, 0x01,                    //   Input (Const)
    0x05, 0x01,                    //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x33,                    //   Usage (Rx)
    0x09, 0x34,                    //   Usage (Ry)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8)
    0x95, 0x02,                    //   Report Count (2)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    0x06, 0x00, 0xFF,              //   Usage Page (Vendor Defined)
    0x09, 0x20,                    //   Usage (0x20)
    0x75, 0x08,                    //   Report Size (8)
    0x95, 0x36,                    //   Report Count (54)
    0x81, 0x02,                    //   Input (Data,Var,Abs)
    0xC0,                          // End Collection
};

uint8_t const kDescConfigDs4[] = {
    TUD_CONFIG_DESCRIPTOR(1, kItfDs4Total, 0,
                          TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 250),
    TUD_HID_DESCRIPTOR(kItfDs4, 0, HID_ITF_PROTOCOL_NONE,
                       sizeof(kDescHidDs4),
                       kEpnDs4, CFG_TUD_HID_EP_BUFSIZE, 4),
};

#endif

// ── Device descriptors (mode-selected at runtime) ──────────────────────────

tusb_desc_device_t const kDescDevice = {
    sizeof(tusb_desc_device_t), TUSB_DESC_DEVICE, 0x0200,
    0x00, 0x00, 0x00,
    CFG_TUD_ENDPOINT0_SIZE,
    kUsbVid, kUsbPid, kUsbBcd,
    0x01, 0x02, 0x03, 0x01,
};

#if FEATHER_ENABLE_DUALSHOCK4_MODE
tusb_desc_device_t const kDescDeviceDs4 = {
    sizeof(tusb_desc_device_t), TUSB_DESC_DEVICE, 0x0200,
    0x00, 0x00, 0x00,
    CFG_TUD_ENDPOINT0_SIZE,
    kUsbVidDs4, kUsbPidDs4, kUsbBcd,
    0x01, 0x02, 0x03, 0x01,
};
#endif

char const* kStringDescriptors[] = {
    nullptr,
    "Google LLC",
    "DualPakka Composite Remapper",
};

#if FEATHER_ENABLE_DUALSHOCK4_MODE
char const* kStringDescriptorsDs4[] = {
    nullptr,
    "Sony Interactive Entertainment",
    "Wireless Controller",
};
#endif

uint16_t g_string_desc[32];

}  // namespace

extern "C" uint8_t const* tud_descriptor_device_cb() {
  return reinterpret_cast<uint8_t const*>(&kDescDevice);
}

extern "C" uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return kDescConfig;
}

extern "C" uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
  switch (instance) {
    case kItfKeyboard:
      return kDescHidKeyboard;
    case kItfMouse:
      return kDescHidMouse;
    case kItfGamepad:
      return kDescHidGamepad;
    default:
      return nullptr;
  }
}

extern "C" uint16_t const* tud_descriptor_string_cb(uint8_t index,
                                                      uint16_t langid) {
  (void)langid;

  char const** descs = kStringDescriptors;
  size_t descs_len = sizeof(kStringDescriptors) / sizeof(kStringDescriptors[0]);

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
