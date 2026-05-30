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

#ifndef GYRO_STICK_PROFILE_ENABLE
#define GYRO_STICK_PROFILE_ENABLE 0
#endif

#ifndef FEATHER_STATUS_HID_ENABLE
#define FEATHER_STATUS_HID_ENABLE 1
#endif

#ifndef FEATHER_STATUS_HID_RATE_HZ
#define FEATHER_STATUS_HID_RATE_HZ 60
#endif

// Gyro-to-right-stick sensitivity in Q8. Negative matches mouse X direction.
#define GYRO_STICK_SENSITIVITY_Q8 (-64)
// Assumed game right-stick deadzone, as percent of full deflection.
#define GYRO_STICK_DEADZONE_SKIP_PERCENT 50
// Pulse gyro movement below the assumed deadzone instead of dropping it.
#define GYRO_STICK_DEADZONE_PULSE_ENABLE 1

#ifndef LEAN_ESTIMATE_ENABLE
#define LEAN_ESTIMATE_ENABLE FEATHER_STATUS_HID_ENABLE
#endif
// DualSense accelerometer raw magnitude is roughly 8192 at 1g.
#define LEAN_ACCEL_1G_RAW 8192
// Ignore accelerometer correction when movement pushes magnitude far away from 1g.
#define LEAN_ACCEL_1G_TOLERANCE_PERCENT 35
// IIR gravity alignment update: 1 / 2^shift per accepted input sample.
#define LEAN_GRAVITY_FILTER_SHIFT 5
// Empirical DualSense roll calibration. 52% maps a measured ~55 deg to ~45 deg.
#define LEAN_ROLL_SCALE_PERCENT 52
// Gyro-Z integration for lean fusion, centidegrees Q16 per raw sample at 1000 Hz.
#define LEAN_GYRO_ROLL_CENTIDEG_Q16_PER_REPORT 330
// Slow accelerometer correction of the gyro-integrated lean angle: 1 / 2^shift.
#define LEAN_ACCEL_CORRECTION_SHIFT 7
// Update lean gyro bias only when the corrected gyro is below this raw threshold.
#define LEAN_GYRO_STILL_THRESHOLD_RAW 12
// Number of valid gravity-like input reports before the current lean becomes neutral.
#define LEAN_NEUTRAL_SETTLE_REPORTS 250

#define TOUCHPAD_SCROLL_ENABLE 1
#define TOUCHPAD_SCROLL_THRESHOLD 100

// Full-width swipe mode-switch gesture: finger must start within this many
// pixels of either edge and end past the opposite threshold.
// Touchpad width = 1920 px; margin 200 = ~10% from each edge.
#define SWIPE_GESTURE_EDGE_MARGIN 200

// Right stick numpad: only fires when ramped radius exceeds this value (0-127).
// High value = near full deflection required before a number is registered.
#define RSTICK_NUMPAD_THRESHOLD 100
