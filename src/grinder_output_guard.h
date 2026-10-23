#pragma once

#include <stdint.h>

// B4c.1: Pure decision/diagnostic layer, intentionally without any GPIO API.
// A positive permitted() result is NOT permission to energize a real SSR.
// Future hardware integration requires a separate explicit release and an
// independent fail-OFF mechanism for loop stalls and MCU resets.
class GrinderOutputGuard {
public:
  enum class Reason : uint8_t {
    None,
    NoRequest,
    InvalidContext,
    SensorStale,
    WebAbort,
    OtaOrReboot,
    LoopGap,
    RuntimeLimit,
    UnexpectedModeChange
  };

  struct Config {
    uint32_t maxLoopGapMs = 500;
    uint32_t maxGrindingMs = 30000;
    uint32_t maxTopupMs = 3000;
  };

  struct Inputs {
    bool logicRequest = false;
    bool manualTopup = false;
    bool contextValid = false;
    bool sensorFresh = false;
    bool webAbortPending = false;
    bool otaOrRebootPending = false;
  };

  GrinderOutputGuard();
  explicit GrinderOutputGuard(const Config& config);

  void observe(const Inputs& inputs, uint32_t nowMs);
  // Can be called only after removal/reset has made a NEW cycle possible.
  // An OTA/reboot inhibit is never cleared during the running firmware.
  bool clearAfterUnprepared(bool controllerIdle, bool vesselRemoved,
                            bool otaOrRebootPending);

  bool permitted() const { return permitted_; }
  bool faultLatched() const { return latchedReason_ != Reason::None; }
  Reason reason() const { return reason_; }

private:
  void latch(Reason reason);

  Config config_;
  Reason reason_ = Reason::NoRequest;
  Reason latchedReason_ = Reason::None;
  bool permitted_ = false;
  bool hadObservation_ = false;
  bool previousRequest_ = false;
  bool previousManualTopup_ = false;
  uint32_t previousObservedMs_ = 0;
  uint32_t startedMs_ = 0;
};
