#include "mode.h"

#include <stdint.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"

namespace {

mode::Mode g_active_mode = mode::Mode::kKeyboardMouse;

constexpr uint32_t kPersistMagic = 0x44504B41;  // "DPKA"
constexpr uint8_t kPersistVersion = 1;
constexpr uint32_t kPersistFlashOffset = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;

struct PersistedMode {
  uint32_t magic;
  uint8_t version;
  uint8_t mode;
  uint16_t checksum;
};

static_assert(kPersistFlashOffset % FLASH_SECTOR_SIZE == 0,
              "persistent mode storage must be sector-aligned");
static_assert(sizeof(PersistedMode) <= FLASH_PAGE_SIZE,
              "persistent mode record must fit in one flash page");

bool IsValidMode(uint8_t mode) {
  return mode < static_cast<uint8_t>(mode::Mode::kCount);
}

uint16_t Checksum(uint8_t mode) {
  return static_cast<uint16_t>(0xA55A ^ kPersistVersion ^ mode);
}

mode::Mode LoadPersistedMode() {
  const auto* stored = reinterpret_cast<const PersistedMode*>(
      XIP_BASE + kPersistFlashOffset);
  if (stored->magic != kPersistMagic ||
      stored->version != kPersistVersion ||
      !IsValidMode(stored->mode) ||
      stored->checksum != Checksum(stored->mode)) {
    return mode::Mode::kKeyboardMouse;
  }
  return static_cast<mode::Mode>(stored->mode);
}

void SavePersistedMode(mode::Mode mode) {
  const uint8_t mode_value = static_cast<uint8_t>(mode);
  const auto* stored = reinterpret_cast<const PersistedMode*>(
      XIP_BASE + kPersistFlashOffset);
  if (stored->magic == kPersistMagic &&
      stored->version == kPersistVersion &&
      stored->mode == mode_value &&
      stored->checksum == Checksum(mode_value)) {
    return;
  }

  uint8_t page[FLASH_PAGE_SIZE];
  memset(page, 0xff, sizeof(page));

  PersistedMode record = {
      kPersistMagic,
      kPersistVersion,
      mode_value,
      Checksum(mode_value),
  };
  memcpy(page, &record, sizeof(record));

  const uint32_t interrupts = save_and_disable_interrupts();
  flash_range_erase(kPersistFlashOffset, FLASH_SECTOR_SIZE);
  flash_range_program(kPersistFlashOffset, page, FLASH_PAGE_SIZE);
  restore_interrupts(interrupts);
}

}  // namespace

namespace mode {

void Init() {
  g_active_mode = LoadPersistedMode();
}

Mode GetActive() {
  return g_active_mode;
}

Mode Next(Mode current) {
  const uint8_t next =
      (static_cast<uint8_t>(current) + 1) % static_cast<uint8_t>(Mode::kCount);
  return static_cast<Mode>(next);
}

Mode Previous(Mode current) {
  const uint8_t current_value = static_cast<uint8_t>(current);
  const uint8_t count = static_cast<uint8_t>(Mode::kCount);
  const uint8_t previous = (current_value + count - 1) % count;
  return static_cast<Mode>(previous);
}

Mode NextRuntime() {
  g_active_mode = Next(g_active_mode);
  return g_active_mode;
}

Mode PreviousRuntime() {
  g_active_mode = Previous(g_active_mode);
  return g_active_mode;
}

void PersistAndReboot(Mode mode) {
  SavePersistedMode(mode);
  watchdog_reboot(0, 0, 0);
  while (true) {
    tight_loop_contents();
  }
}

}  // namespace mode
