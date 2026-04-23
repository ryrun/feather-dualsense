#pragma once

#include <stddef.h>
#include <stdint.h>
#include "usb_helpers.h"

namespace mapping {

constexpr uint16_t kSonyVid = 0x054c;
constexpr uint16_t kDualSensePid = 0x0ce6;
constexpr uint16_t kDualSenseEdgePid = 0x0ce7;
constexpr uint16_t kDualSenseEdgeAltPid = 0x0df2;

enum class Button : uint8_t {
  kSquare = 0,
  kCross,
  kCircle,
  kTriangle,
  kL1,
  kR1,
  kL2,
  kR2,
  kCreate,
  kOptions,
  kL3,
  kR3,
  kTouchpad,
  kPs,
  kMute,
  kLeftPaddle,
  kRightPaddle,
  kDpadUp,
  kDpadRight,
  kDpadDown,
  kDpadLeft,
  kInnerRing,
  kOuterRing,
  kLStickN,
  kLStickE,
  kLStickS,
  kLStickW,
  kRStick1,   // Right stick West
  kRStick2,   // Right stick South
  kRStick3,   // Right stick East
  kRStick4,   // Right stick North
  kRStick5,   // Right stick Southwest
  kRStick6,   // Right stick Southeast
  kRStick7,   // Right stick Northeast
  kRStick8,   // Right stick Northwest
  kFn1,       // DS Edge left Fn button
  kFn2,       // DS Edge right Fn button
  kCount,
};

constexpr size_t kButtonCount = static_cast<size_t>(Button::kCount);

enum class ActionType : uint8_t {
  kNone = 0,
  kKey,
  kModifier,
  kMouseButton,
  kGamepadButton,
};

struct Action {
  ActionType type;
  uint16_t value;
};

// Gamepad button values for ActionType::kGamepadButton.
// 0-7:  bit position in GamepadReport::buttons1
// 8-14: bit position in GamepadReport::buttons2 (subtract 8 for actual bit)
namespace gp {
  // Face buttons (confirmed / derived from confirmed X=bit12)
  constexpr uint16_t kA       = 14;  // buttons2 bit6  Cross     (confirmed)
  constexpr uint16_t kB       = 13;  // buttons2 bit5  Circle    (confirmed)
  constexpr uint16_t kX       = 12;  // buttons2 bit4  Square    (confirmed)
  constexpr uint16_t kY       = 11;  // buttons2 bit3  Triangle  (confirmed)
  // Stick clicks (confirmed)
  constexpr uint16_t kL3      = 8;   // buttons2 bit0  L3  (confirmed)
  constexpr uint16_t kR3      = 7;   // buttons1 bit7  R3  (confirmed)
  // Bumpers
  constexpr uint16_t kLB      = 10;  // buttons2 bit2  L1  (confirmed)
  constexpr uint16_t kRB      = 6;   // buttons1 bit6  R1  (hypothesis)
  // Triggers digital
  constexpr uint16_t kLT      = 5;   // buttons1 bit5  L2 digital  (hypothesis)
  constexpr uint16_t kRT      = 3;   // buttons1 bit3  R2 digital  (confirmed)
  // System buttons
  constexpr uint16_t kMenu    = 1;   // buttons1 bit1  3-dots/Menu  (confirmed)
  constexpr uint16_t kGuide   = 4;   // buttons1 bit4  Stadia/Guide  (confirmed)
  constexpr uint16_t kBack    = 2;   // buttons1 bit2  Create/Back  (unverified)
  constexpr uint16_t kCapture = 0;   // buttons1 bit0  Capture/Screenshot  (confirmed)
}  // namespace gp

struct AllowedDevice {
  uint16_t vid;
  uint16_t pid;
};

struct MappingSet {
  uint16_t pid;
  const Action* actions;
};

constexpr Action kDualSenseActions[kButtonCount] = {
    {ActionType::kKey, usb::kKeyR},                     // Square
    {ActionType::kKey, usb::kKeyF},                     // Cross
    {ActionType::kKey, usb::kKeyV},                     // Circle
    {ActionType::kKey, usb::kKeyT},                     // Triangle
    {ActionType::kKey, usb::kKeyQ},                     // L1
    {ActionType::kKey, usb::kKeyE},                     // R1
    {ActionType::kMouseButton, usb::kMouseButtonRight}, // L2
    {ActionType::kMouseButton, usb::kMouseButtonLeft},  // R2
    {ActionType::kKey, usb::kKeyTab},                   // Create/Share
    {ActionType::kKey, usb::kKeyEscape},                // Options
    {ActionType::kKey, usb::kKeyJ},                     // L3
    {ActionType::kKey, usb::kKey0},                     // R3
    {ActionType::kNone, 0},                             // Touchpad click (handled by zone logic)
    {ActionType::kKey, usb::kKeyEnter},                 // PS button
    {ActionType::kKey, usb::kKeyF12},                   // Mute button
    {ActionType::kNone, 0},                             // Left paddle (Edge only)
    {ActionType::kNone, 0},                             // Right paddle (Edge only)
    {ActionType::kKey, usb::kKeyArrowUp},               // D-Pad Up
    {ActionType::kKey, usb::kKeyArrowRight},            // D-Pad Right
    {ActionType::kKey, usb::kKeyArrowDown},             // D-Pad Down
    {ActionType::kKey, usb::kKeyArrowLeft},             // D-Pad Left
    {ActionType::kKey, usb::kKeyL},                     // Inner ring (walk)
    {ActionType::kNone, 0},                             // Outer ring
    {ActionType::kKey, usb::kKeyW},                     // LStick North
    {ActionType::kKey, usb::kKeyD},                     // LStick East
    {ActionType::kKey, usb::kKeyS},                     // LStick South
    {ActionType::kKey, usb::kKeyA},                     // LStick West
    {ActionType::kKey, usb::kKey1},                     // RStick W
    {ActionType::kKey, usb::kKey2},                     // RStick S
    {ActionType::kKey, usb::kKey3},                     // RStick E
    {ActionType::kKey, usb::kKey4},                     // RStick N
    {ActionType::kKey, usb::kKey5},                     // RStick SW
    {ActionType::kKey, usb::kKey6},                     // RStick SE
    {ActionType::kKey, usb::kKey7},                     // RStick NE
    {ActionType::kKey, usb::kKey8},                     // RStick NW
    {ActionType::kNone, 0},                             // Fn1 (Edge only)
    {ActionType::kNone, 0},                             // Fn2 (Edge only)
};

constexpr Action kDualSenseEdgeActions[kButtonCount] = {
    {ActionType::kKey, usb::kKeyR},                     // Square
    {ActionType::kKey, usb::kKeyF},                     // Cross
    {ActionType::kKey, usb::kKeyV},                     // Circle
    {ActionType::kKey, usb::kKeyT},                     // Triangle
    {ActionType::kKey, usb::kKeyQ},                     // L1
    {ActionType::kKey, usb::kKeyE},                     // R1
    {ActionType::kMouseButton, usb::kMouseButtonRight}, // L2
    {ActionType::kMouseButton, usb::kMouseButtonLeft},  // R2
    {ActionType::kKey, usb::kKeyTab},                   // Create/Share
    {ActionType::kKey, usb::kKeyEscape},                // Options
    {ActionType::kKey, usb::kKeyJ},                     // L3
    {ActionType::kKey, usb::kKey0},                     // R3
    {ActionType::kNone, 0},                             // Touchpad click (handled by zone logic)
    {ActionType::kKey, usb::kKeyEnter},                 // PS button
    {ActionType::kKey, usb::kKeyF12},                   // Mute button
    {ActionType::kModifier, usb::kModLeftShift},        // Left rear paddle
    {ActionType::kKey, usb::kKeySpace},                 // Right rear paddle
    {ActionType::kKey, usb::kKeyArrowUp},               // D-Pad Up
    {ActionType::kKey, usb::kKeyArrowRight},            // D-Pad Right
    {ActionType::kKey, usb::kKeyArrowDown},             // D-Pad Down
    {ActionType::kKey, usb::kKeyArrowLeft},             // D-Pad Left
    {ActionType::kKey, usb::kKeyL},                     // Inner ring (walk)
    {ActionType::kNone, 0},                             // Outer ring
    {ActionType::kKey, usb::kKeyW},                     // LStick North
    {ActionType::kKey, usb::kKeyD},                     // LStick East
    {ActionType::kKey, usb::kKeyS},                     // LStick South
    {ActionType::kKey, usb::kKeyA},                     // LStick West
    {ActionType::kKey, usb::kKey1},                     // RStick W
    {ActionType::kKey, usb::kKey2},                     // RStick S
    {ActionType::kKey, usb::kKey3},                     // RStick E
    {ActionType::kKey, usb::kKey4},                     // RStick N
    {ActionType::kKey, usb::kKey5},                     // RStick SW
    {ActionType::kKey, usb::kKey6},                     // RStick SE
    {ActionType::kKey, usb::kKey7},                     // RStick NE
    {ActionType::kKey, usb::kKey8},                     // RStick NW
    {ActionType::kKey, usb::kKeyX},                     // Fn1
    {ActionType::kKey, usb::kKeyY},                     // Fn2
};

constexpr Action kDualSenseGamepadActions[kButtonCount] = {
    {ActionType::kGamepadButton, gp::kX},       // Square       → X
    {ActionType::kGamepadButton, gp::kA},       // Cross        → A
    {ActionType::kGamepadButton, gp::kB},       // Circle       → B
    {ActionType::kGamepadButton, gp::kY},       // Triangle     → Y
    {ActionType::kGamepadButton, gp::kLB},      // L1           → LB
    {ActionType::kGamepadButton, gp::kRB},      // R1           → RB
    {ActionType::kGamepadButton, gp::kLT},      // L2 digital   → LT
    {ActionType::kGamepadButton, gp::kRT},      // R2 digital   → RT
    {ActionType::kGamepadButton, gp::kBack},    // Create       → Back
    {ActionType::kGamepadButton, gp::kMenu},    // Options      → Menu
    {ActionType::kGamepadButton, gp::kL3},      // L3           → L3
    {ActionType::kGamepadButton, gp::kR3},      // R3           → R3
    {ActionType::kGamepadButton, gp::kCapture}, // Touchpad     → Capture
    {ActionType::kGamepadButton, gp::kGuide},   // PS           → Guide
    {ActionType::kNone, 0},                     // Mute         → none
    {ActionType::kNone, 0},                     // LeftPaddle   → none (no paddles on standard DS)
    {ActionType::kNone, 0},                     // RightPaddle  → none
    {ActionType::kNone, 0},                     // DpadUp       → hat (direct)
    {ActionType::kNone, 0},                     // DpadRight    → hat (direct)
    {ActionType::kNone, 0},                     // DpadDown     → hat (direct)
    {ActionType::kNone, 0},                     // DpadLeft     → hat (direct)
    {ActionType::kNone, 0},                     // InnerRing    → analog
    {ActionType::kNone, 0},                     // OuterRing    → analog
    {ActionType::kNone, 0},                     // LStickN      → analog
    {ActionType::kNone, 0},                     // LStickE      → analog
    {ActionType::kNone, 0},                     // LStickS      → analog
    {ActionType::kNone, 0},                     // LStickW      → analog
    {ActionType::kNone, 0},                     // RStick1      → analog
    {ActionType::kNone, 0},                     // RStick2      → analog
    {ActionType::kNone, 0},                     // RStick3      → analog
    {ActionType::kNone, 0},                     // RStick4      → analog
    {ActionType::kNone, 0},                     // RStick5      → analog
    {ActionType::kNone, 0},                     // RStick6      → analog
    {ActionType::kNone, 0},                     // RStick7      → analog
    {ActionType::kNone, 0},                     // RStick8      → analog
    {ActionType::kNone, 0},                     // Fn1          → none
    {ActionType::kNone, 0},                     // Fn2          → none
};

constexpr Action kDualSenseEdgeGamepadActions[kButtonCount] = {
    {ActionType::kGamepadButton, gp::kX},       // Square       → X
    {ActionType::kGamepadButton, gp::kA},       // Cross        → A
    {ActionType::kGamepadButton, gp::kB},       // Circle       → B
    {ActionType::kGamepadButton, gp::kY},       // Triangle     → Y
    {ActionType::kGamepadButton, gp::kLB},      // L1           → LB
    {ActionType::kGamepadButton, gp::kRB},      // R1           → RB
    {ActionType::kGamepadButton, gp::kLT},      // L2 digital   → LT
    {ActionType::kGamepadButton, gp::kRT},      // R2 digital   → RT
    {ActionType::kGamepadButton, gp::kBack},    // Create       → Back
    {ActionType::kGamepadButton, gp::kMenu},    // Options      → Menu
    {ActionType::kGamepadButton, gp::kL3},      // L3           → L3
    {ActionType::kGamepadButton, gp::kR3},      // R3           → R3
    {ActionType::kGamepadButton, gp::kCapture}, // Touchpad     → Capture
    {ActionType::kGamepadButton, gp::kGuide},   // PS           → Guide
    {ActionType::kNone, 0},                     // Mute         → none
    {ActionType::kGamepadButton, gp::kL3},      // LeftPaddle   → L3
    {ActionType::kGamepadButton, gp::kA},       // RightPaddle  → A
    {ActionType::kNone, 0},                     // DpadUp       → hat (direct)
    {ActionType::kNone, 0},                     // DpadRight    → hat (direct)
    {ActionType::kNone, 0},                     // DpadDown     → hat (direct)
    {ActionType::kNone, 0},                     // DpadLeft     → hat (direct)
    {ActionType::kNone, 0},                     // InnerRing    → analog
    {ActionType::kNone, 0},                     // OuterRing    → analog
    {ActionType::kNone, 0},                     // LStickN      → analog
    {ActionType::kNone, 0},                     // LStickE      → analog
    {ActionType::kNone, 0},                     // LStickS      → analog
    {ActionType::kNone, 0},                     // LStickW      → analog
    {ActionType::kNone, 0},                     // RStick1      → analog
    {ActionType::kNone, 0},                     // RStick2      → analog
    {ActionType::kNone, 0},                     // RStick3      → analog
    {ActionType::kNone, 0},                     // RStick4      → analog
    {ActionType::kNone, 0},                     // RStick5      → analog
    {ActionType::kNone, 0},                     // RStick6      → analog
    {ActionType::kNone, 0},                     // RStick7      → analog
    {ActionType::kNone, 0},                     // RStick8      → analog
    {ActionType::kNone, 0},                     // Fn1          → none
    {ActionType::kNone, 0},                     // Fn2          → none
};

struct GamepadMappingSet {
  uint16_t pid;
  const Action* actions;
};

constexpr GamepadMappingSet kGamepadMappingSets[] = {
    {kDualSensePid,        kDualSenseGamepadActions},
    {kDualSenseEdgePid,    kDualSenseEdgeGamepadActions},
    {kDualSenseEdgeAltPid, kDualSenseEdgeGamepadActions},
};

inline const Action* FindGamepadMapping(uint16_t vid, uint16_t pid) {
  if (vid != kSonyVid) return nullptr;
  for (const auto& set : kGamepadMappingSets) {
    if (set.pid == pid) return set.actions;
  }
  return nullptr;
}

constexpr AllowedDevice kAllowedDevices[] = {
    {kSonyVid, kDualSensePid},
    {kSonyVid, kDualSenseEdgePid},
    {kSonyVid, kDualSenseEdgeAltPid},
};

constexpr MappingSet kMappingSets[] = {
    {kDualSensePid, kDualSenseActions},
    {kDualSenseEdgePid, kDualSenseEdgeActions},
    {kDualSenseEdgeAltPid, kDualSenseEdgeActions},
};

inline constexpr uint64_t ButtonMask(Button button) {
  return 1ull << static_cast<uint8_t>(button);
}

inline const Action* FindMapping(uint16_t vid, uint16_t pid) {
  if (vid != kSonyVid) {
    return nullptr;
  }

  for (const auto& device : kAllowedDevices) {
    if (device.vid == vid && device.pid == pid) {
      for (const auto& set : kMappingSets) {
        if (set.pid == pid) {
          return set.actions;
        }
      }
    }
  }

  return nullptr;
}

}  // namespace mapping
