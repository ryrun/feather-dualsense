#pragma once

#ifndef FEATHER_REMAPPER_DEBUG
#define FEATHER_REMAPPER_DEBUG 0
#endif

#ifndef FEATHER_GAMEPAD_BACKEND_DUALSHOCK4
#define FEATHER_GAMEPAD_BACKEND_DUALSHOCK4 0
#endif

#define REPORT_INTERVAL_MS 1
#define KEYBOARD_ROLLOVER_KEYS 14
#define STATUS_LED_SELFTEST_MS 250

#define GYRO_MOUSE_ENABLE 1
// Sensitivity in Q16 fixed-point. Negative = invert direction.
// Tuned for 1000 Hz USB polling (endpoint interval forced to 1 ms in tuh_hid_mount_cb).
#define GYRO_MOUSE_SENSITIVITY_Q16 (-171)
// Y axis scale factor relative to X (applied as integer Q16 at compile time).
#define GYRO_MOUSE_Y_FACTOR 0.7f
// Gyro-to-right-stick sensitivity in Q8. Negative matches mouse X direction.
#define GYRO_STICK_SENSITIVITY_Q8 (-64)

#define TOUCHPAD_SCROLL_ENABLE 1
#define TOUCHPAD_SCROLL_THRESHOLD 100

// Full-width swipe mode-switch gesture: finger must start within this many
// pixels of either edge and end past the opposite threshold.
// Touchpad width = 1920 px; margin 200 = ~10% from each edge.
#define SWIPE_GESTURE_EDGE_MARGIN 200

// Right stick numpad: only fires when ramped radius exceeds this value (0-127).
// High value = near full deflection required before a number is registered.
#define RSTICK_NUMPAD_THRESHOLD 100
