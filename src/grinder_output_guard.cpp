#include "grinder_output_guard.h"

GrinderOutputGuard::GrinderOutputGuard() : GrinderOutputGuard(Config{}) {}

GrinderOutputGuard::GrinderOutputGuard(const Config& config)
    : config_(config) {}

void GrinderOutputGuard::latch(Reason reason) {
  if (latchedReason_ == Reason::None) latchedReason_ = reason;
}

void GrinderOutputGuard::observe(const Inputs& inputs, uint32_t nowMs) {
  // This only notices a scheduling gap AFTER the loop runs again. It is not
  // an independent watchdog capable of opening an SSR during a blocked loop.
  if (hadObservation_ && previousRequest_ &&
      nowMs - previousObservedMs_ > config_.maxLoopGapMs) {
    latch(Reason::LoopGap);
  }
  hadObservation_ = true;
  previousObservedMs_ = nowMs;

  // A valid OTA upload or any reboot request blocks new runs until reboot.
  if (inputs.otaOrRebootPending) latch(Reason::OtaOrReboot);

  if (inputs.logicRequest) {
    if (!previousRequest_) {
      startedMs_ = nowMs;
    } else if (previousManualTopup_ != inputs.manualTopup) {
      latch(Reason::UnexpectedModeChange);
    }

    const uint32_t limitMs = inputs.manualTopup
      ? config_.maxTopupMs : config_.maxGrindingMs;
    if (nowMs - startedMs_ >= limitMs) latch(Reason::RuntimeLimit);
    if (inputs.webAbortPending) latch(Reason::WebAbort);
    if (!inputs.contextValid) latch(Reason::InvalidContext);
    if (!inputs.sensorFresh) latch(Reason::SensorStale);
  }

  previousRequest_ = inputs.logicRequest;
  previousManualTopup_ = inputs.manualTopup;

  // Strict default deny: no request, an invalid guard, or any latched event
  // makes permitted() false. No code here ever controls an SSR pin.
  permitted_ = inputs.logicRequest && latchedReason_ == Reason::None;
  reason_ = latchedReason_ != Reason::None ? latchedReason_ :
            inputs.logicRequest ? Reason::None : Reason::NoRequest;
}

bool GrinderOutputGuard::clearAfterUnprepared(bool controllerIdle,
                                              bool vesselRemoved,
                                              bool otaOrRebootPending) {
  if (!controllerIdle || !vesselRemoved || otaOrRebootPending ||
      previousRequest_) return false;
  latchedReason_ = Reason::None;
  reason_ = Reason::NoRequest;
  permitted_ = false;
  hadObservation_ = false;
  previousManualTopup_ = false;
  startedMs_ = 0;
  return true;
}
