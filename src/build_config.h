#pragma once

#ifndef FEATHER_REMAPPER_DEBUG
#define FEATHER_REMAPPER_DEBUG 0
#endif

#define REPORT_INTERVAL_MS 1
#define KEYBOARD_ROLLOVER_KEYS 14
#define HOST_REPORT_BUFFER_SIZE 64
#define STATUS_LED_SELFTEST_MS 250

#define GYRO_MOUSE_ENABLE 1
#define GYRO_MOUSE_DEADZONE 10
#define GYRO_MOUSE_SENSITIVITY_Q16 (-682)
#define GYRO_MOUSE_X_FACTOR 1.0f
#define GYRO_MOUSE_Y_FACTOR 0.7f

#define TOUCHPAD_SCROLL_ENABLE 1
#define TOUCHPAD_SCROLL_THRESHOLD 100

// Right stick numpad: only fires when ramped radius exceeds this value (0-127).
// High value = near full deflection required before a number is registered.
#define RSTICK_NUMPAD_THRESHOLD 100
