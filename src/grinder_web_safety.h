#pragma once

#include "grinder_controller.h"

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
