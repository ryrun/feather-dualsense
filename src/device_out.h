#pragma once

#include <stdbool.h>
#include <stdint.h>

struct output_state {
  uint8_t modifiers;
  uint8_t keycodes[6];
  uint8_t mouse_buttons;
};

void device_out_init();
void device_out_send_if_ready(const output_state* state);
void device_out_clear();
