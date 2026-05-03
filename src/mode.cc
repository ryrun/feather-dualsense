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

Mode ToggleRuntime() {
  const uint8_t next =
      (static_cast<uint8_t>(g_active_mode) + 1) % static_cast<uint8_t>(Mode::kCount);
  g_active_mode = static_cast<Mode>(next);
  return g_active_mode;
}

}  // namespace mode
