#pragma once

#include "grinder_controller.h"
#include <cstring>

// B4d.1: The WebUI is not a motor-start interface. All received commands
// are dispatched through an explicit command router, which already rejects
// unknown names. Reject common motor-start names here as a second, visible
// boundary; new start actions must never be added to the WebUI router.
inline bool grinderWebIsMotorStartCommand(const char* cmd) {
  if (!cmd) return false;
  const char* const forbiddenPrefixes[] = {
    "grind_start", "grinder_start", "gbw_start",
    "start_grind", "start_grinder", "start_gbw",
    "grind_topup", "grinder_topup", "gbw_topup",
    "motor_on", "grinder_on", "gbw_on"
  };
  for (const char* prefix : forbiddenPrefixes) {
    const size_t length = std::strlen(prefix);
    if (std::strncmp(cmd, prefix, length) == 0 &&
        (cmd[length] == '\0' || cmd[length] == '_')) {
      return true;
    }
  }
  return false;
}

// B4a.4: deliberately conservative WebUI policy for the USB-only simulation.
// This is only a command gate; passive WebSocket telemetry is not affected.
// No GPIO or relay interfaces are exposed here.
enum class GrinderWebCommandPolicy : uint8_t {
  Allow,
  Reject,
  AbortAndReject
};

inline GrinderWebCommandPolicy grinderWebCommandPolicy(
    GrinderController::State state, bool cycleConsumed, bool isSaveDose) {
  if (state == GrinderController::State::Grinding ||
      state == GrinderController::State::ManualTopup ||
      state == GrinderController::State::Settling) {
    // Even calibration/mode actions must be rejected BEFORE their handlers
    // can change the HX711 tare or page. Require a fresh preparation afterward.
    return GrinderWebCommandPolicy::AbortAndReject;
  }
  if (isSaveDose && cycleConsumed) {
    // Never record simulation weights as a real coffee dose, including
    // after a normal stop, user cancel or latched Fault.
    return GrinderWebCommandPolicy::Reject;
  }
  return GrinderWebCommandPolicy::Allow;
}
