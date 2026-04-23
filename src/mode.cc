#include "mode.h"

#include <string.h>

#include "hardware/flash.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

namespace {

constexpr uint32_t kMagic = 0xAA55AA55;
// Store mode in the last sector of flash, leaving all program flash untouched.
constexpr uint32_t kFlashOffset = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;

mode::Mode g_active_mode = mode::Mode::kKeyboardMouse;

// Runs from RAM: briefly tri-states the QSPI CS pin so we can read the BOOTSEL
// button state, which is wired in parallel with it on the Feather RP2040.
static bool __no_inline_not_in_flash_func(ReadBootselImpl)() {
  constexpr uint32_t kCsIndex = 1;

  uint32_t flags = save_and_disable_interrupts();

  // Override CS pin output enable to 'disabled' so we can read it as input.
  hw_write_masked(&ioqspi_hw->io[kCsIndex].ctrl,
                  GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                  IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

  // A small delay to let the pin settle.
  for (volatile int i = 0; i < 1000; ++i) {}

  // BOOTSEL is active-low: button pressed means CS is pulled low.
  bool pressed = !(sio_hw->gpio_hi_in & (1u << kCsIndex));

  // Restore CS to normal operation.
  hw_write_masked(&ioqspi_hw->io[kCsIndex].ctrl,
                  GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                  IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

  restore_interrupts(flags);
  return pressed;
}

// Writes the mode byte to flash. Runs from RAM since it erases/programs flash.
static void __no_inline_not_in_flash_func(WriteModeToFlash)(mode::Mode m) {
  uint8_t page[FLASH_PAGE_SIZE] = {};
  memcpy(page, &kMagic, sizeof(kMagic));
  page[4] = static_cast<uint8_t>(m);

  uint32_t flags = save_and_disable_interrupts();
  flash_range_erase(kFlashOffset, FLASH_SECTOR_SIZE);
  flash_range_program(kFlashOffset, page, FLASH_PAGE_SIZE);
  restore_interrupts(flags);
}

}  // namespace

namespace mode {

void Init() {
  const uint8_t* ptr =
      reinterpret_cast<const uint8_t*>(XIP_BASE + kFlashOffset);
  uint32_t magic;
  memcpy(&magic, ptr, sizeof(magic));

  if (magic == kMagic &&
      ptr[4] < static_cast<uint8_t>(Mode::kCount)) {
    g_active_mode = static_cast<Mode>(ptr[4]);
  } else {
    g_active_mode = Mode::kKeyboardMouse;
  }
}

Mode GetActive() {
  return g_active_mode;
}

bool ReadBootselButton() {
  return ReadBootselImpl();
}

[[noreturn]] void ToggleAndReboot() {
  const Mode new_mode = (g_active_mode == Mode::kKeyboardMouse)
                            ? Mode::kGamepad
                            : Mode::kKeyboardMouse;
  WriteModeToFlash(new_mode);
  watchdog_enable(10, false);
  while (true) {
    tight_loop_contents();
  }
}

}  // namespace mode
