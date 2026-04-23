#include <string.h>

#include "bsp/board_api.h"
#include "device_out.h"
#include "host_reader.h"
#include "pico/stdlib.h"
#include "tusb.h"

output_state g_output_state{};

int main() {
  board_init();

  tusb_rhport_init_t dev_init{};
  dev_init.role = TUSB_ROLE_DEVICE;
  dev_init.speed = TUSB_SPEED_AUTO;

  tusb_rhport_init_t host_init{};
  host_init.role = TUSB_ROLE_HOST;
  host_init.speed = TUSB_SPEED_FULL;

  tusb_init(0, &dev_init);
  tusb_init(1, &host_init);

  device_out_init();
  host_reader_init();
  memset(&g_output_state, 0, sizeof(g_output_state));

  while (true) {
    tud_task();
    host_reader_task();
    device_out_send_if_ready(&g_output_state);
  }

  return 0;
}
