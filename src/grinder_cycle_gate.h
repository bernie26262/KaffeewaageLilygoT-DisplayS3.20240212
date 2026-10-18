#pragma once

#include <stdint.h>

#include "grinder_controller.h"
#include "grinder_profile_detector.h"

// B4a.1: host-testable gate between an already prepared vessel and the B1
// controller. No GPIO access. In particular, logicRunning() is NOT an SSR
// command and must never be wired directly to the grinder relay.
//
// Integration contract for B4a.2:
// - feed the detector first, then call onWeightSample() once per NEW HX711 sample;
// - call tick() every loop, after detector.tick();
// - requestStart() only on a deliberate, separate user action.
class GrinderCycleGate {
public:
  struct Config {
    uint32_t maxStartSampleAgeMs = 1000;
    float maxStartAbsWeightG = 1.0f;
  };

  explicit GrinderCycleGate(const GrinderProfileDetector& detector);
  GrinderCycleGate(const GrinderProfileDetector& detector, const Config& config);

  void onWeightSample(float grams, bool stable, uint32_t nowMs);
  void tick(uint32_t nowMs);
  bool requestStart(uint32_t nowMs);
  bool requestCancel(uint32_t nowMs);

  // Fault recovery is an explicit user action, only after preparation was
  // completely reset. An interrupted run must not restart automatically.
  bool acknowledgeFault();

  GrinderController::State state() const { return controller_.state(); }
  GrinderController::StopReason stopReason() const { return controller_.stopReason(); }
  float targetG() const { return controller_.targetG(); }
  float lastWeightG() const { return controller_.lastWeightG(); }
  bool logicRunning() const { return controller_.relayRequested(); }
  bool cycleConsumed() const { return cycleConsumed_; }

private:
  bool validPreparation() const;
  void observePreparation();
  bool hasFreshStableZero(uint32_t nowMs) const;

  const GrinderProfileDetector& detector_;
  Config config_;
  GrinderController controller_;
  int runningProfile_ = -1;
  float runningTargetG_ = 0.0f;
  bool cycleConsumed_ = false;
  bool sampleValid_ = false;
  bool sampleStable_ = false;
  float sampleG_ = 0.0f;
  uint32_t lastSampleMs_ = 0;
};
