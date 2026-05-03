#include "mode.h"

namespace {

mode::Mode g_active_mode = mode::Mode::kKeyboardMouse;

}  // namespace

namespace mode {

void Init() {
  g_active_mode = Mode::kKeyboardMouse;
}

Mode GetActive() {
  return g_active_mode;
}

Mode NextRuntime() {
  const uint8_t next =
      (static_cast<uint8_t>(g_active_mode) + 1) % static_cast<uint8_t>(Mode::kCount);
  g_active_mode = static_cast<Mode>(next);
  return g_active_mode;
}

Mode PreviousRuntime() {
  const uint8_t current = static_cast<uint8_t>(g_active_mode);
  const uint8_t count = static_cast<uint8_t>(Mode::kCount);
  const uint8_t previous = (current + count - 1) % count;
  g_active_mode = static_cast<Mode>(previous);
  return g_active_mode;
}

}  // namespace mode
