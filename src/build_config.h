#pragma once

#ifndef FEATHER_REMAPPER_DEBUG
#define FEATHER_REMAPPER_DEBUG 0
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

#define TOUCHPAD_SCROLL_ENABLE 1
#define TOUCHPAD_SCROLL_THRESHOLD 100

// Minimum X distance in touchpad pixels (0-1919) for each leg of the
// R→L→R swipe gesture that switches mode. Prevents accidental triggers.
#define SWIPE_GESTURE_MIN_X 300

// Right stick numpad: only fires when ramped radius exceeds this value (0-127).
// High value = near full deflection required before a number is registered.
#define RSTICK_NUMPAD_THRESHOLD 100
