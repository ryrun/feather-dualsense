#include "host_reader.h"

#include <stdio.h>
#include <string.h>

#include "build_config.h"
#include "mapping.h"
#include "tusb.h"


#include "device_out.h"

extern output_state g_output_state;

namespace {
struct active_controller_state {
  bool active;
  uint8_t dev_addr;
  uint8_t instance;
  const device_mapping* mapping;
};

active_controller_state g_controller{};

inline uint32_t parse_buttons(uint8_t const* report, uint16_t len) {
  if (report == nullptr || len < 11) return 0;

  const uint8_t dpad_and_shape = report[8];
  const uint8_t shoulder_misc = report[9];
  uint32_t mask = 0;

  const uint8_t dpad = dpad_and_shape & 0x0F;
  if (dpad == 0 || dpad == 1 || dpad == 7) mask |= (1u << BTN_DPAD_UP);
  if (dpad == 1 || dpad == 2 || dpad == 3) mask |= (1u << BTN_DPAD_RIGHT);
  if (dpad == 3 || dpad == 4 || dpad == 5) mask |= (1u << BTN_DPAD_DOWN);
  if (dpad == 5 || dpad == 6 || dpad == 7) mask |= (1u << BTN_DPAD_LEFT);

  if (dpad_and_shape & (1u << 4)) mask |= (1u << BTN_SQUARE);
  if (dpad_and_shape & (1u << 5)) mask |= (1u << BTN_CROSS);
  if (dpad_and_shape & (1u << 6)) mask |= (1u << BTN_CIRCLE);
  if (dpad_and_shape & (1u << 7)) mask |= (1u << BTN_TRIANGLE);

  if (shoulder_misc & (1u << 0)) mask |= (1u << BTN_L1);
  if (shoulder_misc & (1u << 1)) mask |= (1u << BTN_R1);
  if (shoulder_misc & (1u << 4)) mask |= (1u << BTN_CREATE);
  if (shoulder_misc & (1u << 5)) mask |= (1u << BTN_OPTIONS);

  return mask;
}

inline void apply_buttons(uint32_t mask, const device_mapping* mapping, output_state* out) {
  out->modifiers = 0;
  out->mouse_buttons = 0;
  memset(out->keycodes, 0, sizeof(out->keycodes));

  uint8_t key_pos = 0;
  for (size_t i = 0; i < mapping->count; ++i) {
    if ((mask & (1u << i)) == 0) continue;
    const action_entry entry = mapping->entries[i];
    switch (entry.type) {
      case ACTION_MODIFIER:
        out->modifiers |= static_cast<uint8_t>(entry.value);
        break;
      case ACTION_MOUSE_BUTTON:
        out->mouse_buttons |= static_cast<uint8_t>(entry.value);
        break;
      case ACTION_KEY:
        if (key_pos < sizeof(out->keycodes)) out->keycodes[key_pos++] = static_cast<uint8_t>(entry.value);
        break;
      case ACTION_NONE:
      default:
        break;
    }
  }
}

inline const allowed_device* find_allowed(uint16_t vid, uint16_t pid) {
  for (const auto& candidate : k_allowed_devices) {
    if (candidate.vid == vid && candidate.pid == pid) return &candidate;
  }
  return nullptr;
}
}  // namespace

void host_reader_init() { memset(&g_controller, 0, sizeof(g_controller)); }

void host_reader_task() { tuh_task(); }

extern "C" void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const*, uint16_t) {
  uint16_t vid = 0, pid = 0;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  const allowed_device* allowed = find_allowed(vid, pid);
  if (allowed == nullptr || g_controller.active) {
#if BUILD_DEBUG_UART
    printf("HID ignored vid=0x%04x pid=0x%04x\n", vid, pid);
#endif
    return;
  }

  g_controller.active = true;
  g_controller.dev_addr = dev_addr;
  g_controller.instance = instance;
  g_controller.mapping = &allowed->mapping;
#if BUILD_DEBUG_UART
  printf("HID active: %s\n", allowed->name);
#endif
  (void)tuh_hid_receive_report(dev_addr, instance);
}

extern "C" void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  if (g_controller.active && g_controller.dev_addr == dev_addr && g_controller.instance == instance) {
    memset(&g_output_state, 0, sizeof(g_output_state));
    memset(&g_controller, 0, sizeof(g_controller));
  }
}

extern "C" void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report,
                                            uint16_t len) {
  if (!g_controller.active || g_controller.dev_addr != dev_addr || g_controller.instance != instance) return;

#if BUILD_DEBUG_UART
  printf("report len=%u\n", len);
#endif
  const uint32_t current_buttons = parse_buttons(report, len);
  apply_buttons(current_buttons, g_controller.mapping, &g_output_state);
  (void)tuh_hid_receive_report(dev_addr, instance);
}
