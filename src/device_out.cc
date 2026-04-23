#include "device_out.h"

#include <string.h>

#include "build_config.h"
#include "class/hid/hid.h"
#include "tusb.h"

namespace {
uint8_t const k_hid_report_keyboard[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(),
};

// Mouse report with 16-bit relative X/Y/Wheel/Pan.
uint8_t const k_hid_report_mouse[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (1)
    0x29, 0x03,        //     Usage Maximum (3)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x03,        //     Report Count (3)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data,Var,Abs)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x05,        //     Report Size (5)
    0x81, 0x03,        //     Input (Const,Var,Abs)
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x38,        //     Usage (Wheel)
    0x05, 0x0C,        //     Usage Page (Consumer)
    0x0A, 0x38, 0x02,  //     Usage (AC Pan)
    0x16, 0x01, 0x80,  //     Logical Minimum (-32767)
    0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
    0x75, 0x10,        //     Report Size (16)
    0x95, 0x04,        //     Report Count (4)
    0x81, 0x06,        //     Input (Data,Var,Rel)
    0xC0,              //   End Collection
    0xC0               // End Collection
};

typedef struct TU_ATTR_PACKED {
  uint8_t buttons;
  int16_t x;
  int16_t y;
  int16_t wheel;
  int16_t pan;
} hid_mouse_report_16_t;

static_assert(sizeof(hid_mouse_report_16_t) == 9, "Unexpected 16-bit mouse report size");

const char* k_string_desc[] = {
    (const char[]){0x09, 0x04},
    "feather-dualsense",
    "DualSense to Keyboard+Mouse",
    "0001",
    "HID Keyboard",
    "HID Mouse",
};

uint16_t g_string_buf[32];
output_state g_last_sent{};
bool g_has_last = false;
}  // namespace

void device_out_init() {
  memset(&g_last_sent, 0, sizeof(g_last_sent));
  g_has_last = false;
}

void device_out_send_if_ready(const output_state* state) {
  if (!tud_ready() || state == nullptr) {
    return;
  }
  if (g_has_last && memcmp(state, &g_last_sent, sizeof(output_state)) == 0) {
    return;
  }

  tud_hid_n_keyboard_report(0, 0, state->modifiers, state->keycodes);

  hid_mouse_report_16_t mouse_report{};
  mouse_report.buttons = state->mouse_buttons;
  tud_hid_n_report(1, 0, &mouse_report, sizeof(mouse_report));

  g_last_sent = *state;
  g_has_last = true;
}

void device_out_clear() {
  output_state cleared{};
  device_out_send_if_ready(&cleared);
}

extern "C" uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
  return (instance == 0) ? k_hid_report_keyboard : k_hid_report_mouse;
}

extern "C" uint16_t tud_hid_get_report_cb(uint8_t, uint8_t, hid_report_type_t, uint8_t*, uint16_t) { return 0; }
extern "C" void tud_hid_set_report_cb(uint8_t, uint8_t, hid_report_type_t, uint8_t const*, uint16_t) {}

extern "C" uint8_t const* tud_descriptor_device_cb(void) {
  static tusb_desc_device_t const desc = {sizeof(tusb_desc_device_t), TUSB_DESC_DEVICE, 0x0200,
                                          TUSB_CLASS_MISC, MISC_SUBCLASS_COMMON, MISC_PROTOCOL_IAD,
                                          CFG_TUD_ENDPOINT0_SIZE, 0xCAFE, 0x4010, 0x0100,
                                          0x01, 0x02, 0x03, 0x01};
  return reinterpret_cast<uint8_t const*>(&desc);
}

extern "C" uint8_t const* tud_descriptor_configuration_cb(uint8_t) {
  constexpr uint8_t kConfigTotalLen = TUD_CONFIG_DESC_LEN + (2 * TUD_HID_DESC_LEN);
  static uint8_t const desc[] = {
      TUD_CONFIG_DESCRIPTOR(1, 2, 0, kConfigTotalLen, 0, 100),
      TUD_HID_DESCRIPTOR(0, 4, HID_ITF_PROTOCOL_KEYBOARD, sizeof(k_hid_report_keyboard), 0x81, 16,
                         BUILD_REPORT_INTERVAL_MS),
      TUD_HID_DESCRIPTOR(1, 5, HID_ITF_PROTOCOL_MOUSE, sizeof(k_hid_report_mouse), 0x82, 16,
                         BUILD_REPORT_INTERVAL_MS),
  };
  return desc;
}

extern "C" uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t) {
  uint8_t chr_count = 0;
  if (index >= (sizeof(k_string_desc) / sizeof(k_string_desc[0]))) {
    return nullptr;
  }

  const char* str = k_string_desc[index];
  if (index == 0) {
    g_string_buf[1] = static_cast<uint16_t>(str[0]) | (static_cast<uint16_t>(str[1]) << 8);
    chr_count = 1;
  } else {
    while (str[chr_count] != '\0' && chr_count < 31) {
      g_string_buf[1 + chr_count] = str[chr_count];
      ++chr_count;
    }
  }

  g_string_buf[0] = static_cast<uint16_t>((TUSB_DESC_STRING << 8) | ((2 * chr_count) + 2));
  return g_string_buf;
}
