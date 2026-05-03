#include "mode.h"

#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

namespace {

constexpr uint32_t kMagic = 0xAA55AA55;
// Store mode in the last sector of flash, leaving all program flash untouched.
constexpr uint32_t kFlashOffset = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;

mode::Mode g_active_mode = mode::Mode::kKeyboardMouse;

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

Mode ToggleRuntime() {
  const uint8_t next =
      (static_cast<uint8_t>(g_active_mode) + 1) % static_cast<uint8_t>(Mode::kCount);
  g_active_mode = static_cast<Mode>(next);
  return g_active_mode;
}

Mode ToggleAndSave() {
  const Mode new_mode = ToggleRuntime();
  WriteModeToFlash(new_mode);
  return g_active_mode;
}

}  // namespace mode
