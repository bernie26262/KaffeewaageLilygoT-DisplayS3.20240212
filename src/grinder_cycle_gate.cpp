#include "grinder_cycle_gate.h"

#include <cmath>

GrinderCycleGate::GrinderCycleGate(const GrinderProfileDetector& detector)
    : GrinderCycleGate(detector, Config{}) {}

GrinderCycleGate::GrinderCycleGate(const GrinderProfileDetector& detector,
                                   const Config& config)
    : detector_(detector), config_(config) {
  for (int i = 0; i < GrinderProfileDetector::kProfileCount; ++i) {
    learnedLatencyG_[i] = controller_.latencyG(); // B1 default: 0.25 g
  }
}

bool GrinderCycleGate::validPreparation() const {
  const auto state = detector_.state();
  const bool knownReady = state == GrinderProfileDetector::State::ReadyAuto ||
                          state == GrinderProfileDetector::State::ReadyNoFunnel ||
                          state == GrinderProfileDetector::State::ReadyManual;
  return knownReady && detector_.readyToGrind() && !detector_.paused() &&
         detector_.awaitingTare() == GrinderProfileDetector::TareKind::None &&
         detector_.selectedProfile() >= 0 &&
         detector_.selectedProfile() < GrinderProfileDetector::kProfileCount &&
         std::isfinite(detector_.selectedTargetG()) &&
         detector_.selectedTargetG() >= 5.0f &&
         detector_.selectedTargetG() <= 25.0f;
}

void GrinderCycleGate::observePreparation() {
  const auto state = controller_.state();
  // A new cycle is possible only after full vessel-removal / reset completion,
  // not a transient CheckingRemoval bounce back to Ready.
  if (state == GrinderController::State::Complete &&
      detector_.state() == GrinderProfileDetector::State::WaitVessel) {
    // Capture a learned value only after a normally target-stopped cycle
    // AND confirmed removal. Cancelled/faulted cycles never train.
    if (controller_.stopReason() == GrinderController::StopReason::TargetReached &&
        runningProfile_ >= 0 &&
        runningProfile_ < GrinderProfileDetector::kProfileCount) {
      const float learned = controller_.latencyG();
      if (std::isfinite(learned) && learned >= 0.0f && learned <= 2.0f) {
        learnedLatencyG_[runningProfile_] = learned;
      }
    }
    controller_.reset(); // clears the cycle, not the per-profile RAM cache
    cycleConsumed_ = false;
    runningProfile_ = -1;
    runningTargetG_ = 0.0f;
    sampleValid_ = false; // require a new post-preparation HX711 reading
    sampleStable_ = false;
    return;
  }

  // Once launched, any lost readiness, pause, profile or target change must
  // latch Fault while Grinding OR Settling. Complete remains locked out.
  if (state == GrinderController::State::Grinding ||
      state == GrinderController::State::ManualTopup ||
      state == GrinderController::State::Settling) {
    if (!validPreparation() ||
        runningProfile_ != detector_.selectedProfile() ||
        std::fabs(runningTargetG_ - detector_.selectedTargetG()) > 0.001f) {
      controller_.emergencyStop();
    }
  }
}

bool GrinderCycleGate::hasFreshStableZero(uint32_t nowMs) const {
  return sampleValid_ && sampleStable_ &&
         nowMs - lastSampleMs_ <= config_.maxStartSampleAgeMs &&
         std::fabs(sampleG_) <= config_.maxStartAbsWeightG;
}

void GrinderCycleGate::onWeightSample(float grams, bool stable, uint32_t nowMs) {
  // The detector must already have processed this sample. In particular, an
  // actual lift must revoke readiness BEFORE the B1 controller sees it.
  observePreparation();
  sampleValid_ = std::isfinite(grams);
  sampleStable_ = sampleValid_ && stable;
  sampleG_ = grams;
  lastSampleMs_ = nowMs;
  // A negative jump on removal can appear for one HX711 sample while the
  // detector still reports Ready. Never wait for that state transition.
  const auto state = controller_.state();
  if ((state == GrinderController::State::Grinding ||
       state == GrinderController::State::ManualTopup ||
       state == GrinderController::State::Settling) &&
      (!sampleValid_ || grams < config_.minActiveWeightG)) {
    controller_.emergencyStop();
    return;
  }
  controller_.onWeightSample(grams, nowMs);
  // A deliberate manual top-up may exceed the target. Release, the dedicated
  // three-second limit, or a genuine safety fault ends this request.
}

void GrinderCycleGate::tick(uint32_t nowMs) {
  observePreparation();
  controller_.tick(nowMs); // existing B1 stale-sample / run-time protection
  observePreparation();
}

bool GrinderCycleGate::requestStart(uint32_t nowMs) {
  observePreparation();
  if (cycleConsumed_ || controller_.state() != GrinderController::State::Idle ||
      !validPreparation() || !hasFreshStableZero(nowMs)) return false;

  const float target = detector_.selectedTargetG();
  const int profile = detector_.selectedProfile(); // validated above
  if (!controller_.setTargetG(target) ||
      !controller_.setLatencyG(learnedLatencyG_[profile])) return false;
  // requestStart() is the ONLY caller of onButtonPress() for starting.
  controller_.onButtonPress(nowMs);
  if (controller_.state() != GrinderController::State::Grinding) return false;
  runningProfile_ = detector_.selectedProfile();
  runningTargetG_ = target;
  cycleConsumed_ = true;
  return true;
}

bool GrinderCycleGate::requestCancel(uint32_t nowMs) {
  observePreparation();
  if (controller_.state() != GrinderController::State::Grinding) return false;
  controller_.onButtonPress(nowMs); // B1 user cancel, stops logical demand
  return controller_.state() == GrinderController::State::Settling;
}

bool GrinderCycleGate::canRequestManualTopup(uint32_t nowMs) const {
  if (!cycleConsumed_ || controller_.state() != GrinderController::State::Complete ||
      !validPreparation() || runningProfile_ != detector_.selectedProfile() ||
      std::fabs(runningTargetG_ - detector_.selectedTargetG()) > 0.001f ||
      !sampleValid_ || !sampleStable_ ||
      nowMs - lastSampleMs_ > config_.maxStartSampleAgeMs) return false;
  // After a normally completed dose (or previous top-up) the user may
  // deliberately top up irrespective of the final dose weight. Never restart
  // from cancellation, fault, lost preparation or confirmed vessel removal.
  const auto reason = controller_.stopReason();
  if (reason != GrinderController::StopReason::TargetReached &&
      reason != GrinderController::StopReason::ManualReleased &&
      reason != GrinderController::StopReason::ManualTimeLimit) return false;
  return sampleG_ >= config_.minActiveWeightG;
}

bool GrinderCycleGate::requestManualTopupPress(uint32_t nowMs) {
  observePreparation();
  if (!canRequestManualTopup(nowMs)) return false;
  // An explicit top-up MUST NOT fall back to normal Grinding when weight is
  // well below target. The B1 generic button would do exactly that.
  return controller_.onManualTopupPress(nowMs) &&
         controller_.state() == GrinderController::State::ManualTopup;
}

bool GrinderCycleGate::requestManualTopupRelease(uint32_t nowMs) {
  if (controller_.state() != GrinderController::State::ManualTopup) return false;
  controller_.onButtonRelease(nowMs);
  return controller_.state() == GrinderController::State::Settling;
}

bool GrinderCycleGate::requestSafetyAbort() {
  const auto state = controller_.state();
  if (state != GrinderController::State::Grinding &&
      state != GrinderController::State::ManualTopup &&
      state != GrinderController::State::Settling) return false;
  controller_.emergencyStop();
  return true;
}

bool GrinderCycleGate::acknowledgeFault() {
  if (controller_.state() != GrinderController::State::Fault ||
      detector_.state() != GrinderProfileDetector::State::WaitVessel ||
      detector_.readyToGrind()) return false;
  controller_.reset();
  cycleConsumed_ = false;
  runningProfile_ = -1;
  runningTargetG_ = 0.0f;
  sampleValid_ = false;
  sampleStable_ = false;
  return true;
}
