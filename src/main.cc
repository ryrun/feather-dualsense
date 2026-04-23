#include <string.h>

#include "bsp/board_api.h"
#include "device_out.h"
#include "host_reader.h"
#include "pico/stdlib.h"
#include "tusb.h"

#if defined(HAVE_PIO_USB_HEADER) && HAVE_PIO_USB_HEADER
#include "pio_usb.h"
#endif

#ifndef PICO_DEFAULT_PIO_USB_DP_PIN
#error "PICO_DEFAULT_PIO_USB_DP_PIN must be defined by the selected board configuration"
#endif

output_state g_output_state{};

namespace {
#if defined(HAVE_PIO_USB_HEADER) && HAVE_PIO_USB_HEADER && CFG_TUH_RPI_PIO_USB
repeating_timer_t g_host_sof_timer;

bool host_sof_timer_cb(repeating_timer_t*) {
  pio_usb_host_frame();
  return true;
}
#endif

void extra_init() {
#if defined(PICO_DEFAULT_PIO_USB_VBUSEN_PIN)
  gpio_init(PICO_DEFAULT_PIO_USB_VBUSEN_PIN);
  gpio_set_dir(PICO_DEFAULT_PIO_USB_VBUSEN_PIN, GPIO_OUT);
  gpio_put(PICO_DEFAULT_PIO_USB_VBUSEN_PIN, 1);
#endif

#if defined(HAVE_PIO_USB_HEADER) && HAVE_PIO_USB_HEADER && CFG_TUH_RPI_PIO_USB
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = PICO_DEFAULT_PIO_USB_DP_PIN;

  tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
  add_repeating_timer_us(-1000, host_sof_timer_cb, nullptr, &g_host_sof_timer);
#endif
}
}  // namespace

int main() {
  board_init();
  extra_init();

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
