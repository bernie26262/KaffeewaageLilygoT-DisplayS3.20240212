#include "grinder_controller.h"

#include <cmath>

GrinderController::GrinderController() : GrinderController(Config{}) {}

GrinderController::GrinderController(const Config& config) : config_(config) {
  reset();
}

void GrinderController::reset() {
  state_ = State::Idle;
  stopReason_ = StopReason::None;
  targetG_ = 0.0f;
  latencyG_ = 0.25f;
  weightG_ = 0.0f;
  hasWeight_ = false;
  lastSampleMs_ = 0;
  runStartMs_ = 0;
  stoppedMs_ = 0;
  learnLatency_ = false;
}

bool GrinderController::setTargetG(float targetG) {
  if (active() || state_ == State::Settling || state_ == State::Fault ||
      !std::isfinite(targetG) || targetG < 5.0f || targetG > 25.0f) {
    return false;
  }
  targetG_ = targetG;
  return true;
}

bool GrinderController::setLatencyG(float latencyG) {
  if (active() || state_ == State::Settling || state_ == State::Fault ||
      !std::isfinite(latencyG) || latencyG < 0.0f || latencyG > 2.0f) {
    return false;
  }
  latencyG_ = latencyG;
  return true;
}

bool GrinderController::freshWeight(uint32_t nowMs) const {
  return hasWeight_ && (nowMs - lastSampleMs_ <= config_.maxSampleAgeMs);
}

void GrinderController::fault(StopReason reason) {
  // Transition to Fault makes relayRequested() false immediately.
  state_ = State::Fault;
  stopReason_ = reason;
  learnLatency_ = false;
}

void GrinderController::stop(StopReason reason, uint32_t nowMs) {
  if (!active()) return;
  learnLatency_ = (state_ == State::Grinding && reason == StopReason::TargetReached);
  state_ = State::Settling;
  stopReason_ = reason;
  stoppedMs_ = nowMs;
}

void GrinderController::onWeightSample(float grams, uint32_t nowMs) {
  // Check deadlines against the previous sample before accepting the next one;
  // a late sample must not hide a sensor stall or a run-time overrun.
  if (active() && nowMs - runStartMs_ >=
      (state_ == State::ManualTopup ? config_.maxTopupMs : config_.maxRunMs)) {
    fault(StopReason::RuntimeExceeded);
    return;
  }
  // A delayed sample must not disguise a sensor outage, including while
  // waiting for the post-stop settling measurement.
  if ((active() || state_ == State::Settling) && !freshWeight(nowMs)) {
    fault(StopReason::StaleWeight);
    return;
  }
  if (!std::isfinite(grams)) {
    hasWeight_ = false;
    if (active() || state_ == State::Settling) fault(StopReason::InvalidWeight);
    return;
  }
  weightG_ = grams;
  hasWeight_ = true;
  lastSampleMs_ = nowMs;
  if (state_ == State::Grinding && weightG_ >= targetG_ - latencyG_) {
    stop(StopReason::TargetReached, nowMs);
  }
}

void GrinderController::onButtonPress(uint32_t nowMs) {
  if (state_ == State::Grinding) {
    stop(StopReason::UserCancelled, nowMs);
    return;
  }
  if (state_ != State::Idle && state_ != State::Complete) return;
  if (!(targetG_ >= 5.0f) || !freshWeight(nowMs)) return;

  // Start only from a deliberate button press, never automatically at boot.
  const bool topup = weightG_ >= targetG_ - config_.manualThresholdG;
  state_ = topup ? State::ManualTopup : State::Grinding;
  stopReason_ = StopReason::None;
  runStartMs_ = nowMs;
  learnLatency_ = false;
}

void GrinderController::onButtonRelease(uint32_t nowMs) {
  // This must always stop a manual top-up, irrespective of measured weight.
  if (state_ == State::ManualTopup) stop(StopReason::ManualReleased, nowMs);
}

void GrinderController::tick(uint32_t nowMs) {
  if (active()) {
    if (nowMs - runStartMs_ >=
        (state_ == State::ManualTopup ? config_.maxTopupMs : config_.maxRunMs)) {
      fault(StopReason::RuntimeExceeded);
      return;
    }
    if (!freshWeight(nowMs)) {
      fault(StopReason::StaleWeight);
      return;
    }
  }
  // Never mark a stopped cycle Complete after losing HX711 telemetry.
  // Run the same freshness check before a pending Settling -> Complete step.
  if (state_ == State::Settling && !freshWeight(nowMs)) {
    fault(StopReason::StaleWeight);
    return;
  }
  if (state_ != State::Settling || nowMs - stoppedMs_ < config_.settleMs) return;

  // Learn only from normal target-controlled stops, never from cancellations
  // or manual topping-up. Legacy defaults were 0.25 g and 450 ms.
  if (learnLatency_ && freshWeight(nowMs)) {
    const float learned = latencyG_ + weightG_ - targetG_;
    latencyG_ = (std::isfinite(learned) && learned >= 0.0f && learned <= 2.0f)
      ? learned : 0.25f;
  }
  learnLatency_ = false;
  state_ = State::Complete;
}

void GrinderController::emergencyStop() {
  fault(StopReason::EmergencyStop);
}

void GrinderController::clearFault() {
  if (state_ != State::Fault) return;
  state_ = State::Idle;
  stopReason_ = StopReason::None;
  // Demand a new, explicit sample and button press before restarting.
  hasWeight_ = false;
}
