#pragma once

#include <stdint.h>

// Phase B1: pure grinder decision logic. No GPIO, WiFi, UI or NVS access.
// The caller MUST NOT connect relayRequested() to an output in this phase.
class GrinderController {
public:
  enum class State : uint8_t { Idle, Grinding, ManualTopup, Settling, Complete, Fault };
  enum class StopReason : uint8_t {
    None, TargetReached, UserCancelled, ManualReleased,
    EmergencyStop, InvalidWeight, StaleWeight, RuntimeExceeded
  };

  struct Config {
    uint32_t maxRunMs = 30000;       // provisional, tune before motor tests
    uint32_t maxTopupMs = 3000;     // B4b.2: hold-to-run has a separate hard cap
    uint32_t maxSampleAgeMs = 1000;  // no new HX711 sample -> stop
    uint32_t settleMs = 450;         // original grinder's measurement delay
    float manualThresholdG = 0.5f;  // near target -> hold-to-top-up
  };

  GrinderController();
  explicit GrinderController(const Config& config);

  // All state starts with relayRequested() == false, including after reset.
  void reset();
  bool setTargetG(float targetG);  // valid range: 5..25 g (legacy); not while active
  bool setLatencyG(float latencyG); // valid range: 0..2 g; not while active
  void onWeightSample(float grams, uint32_t nowMs);
  void onButtonPress(uint32_t nowMs);
  void onButtonRelease(uint32_t nowMs);
  void tick(uint32_t nowMs);
  void emergencyStop();             // latches Fault, never starts the relay
  void clearFault();                // explicit reset, does not start grinding

  State state() const { return state_; }
  StopReason stopReason() const { return stopReason_; }
  bool relayRequested() const {
    return state_ == State::Grinding || state_ == State::ManualTopup;
  }
  float targetG() const { return targetG_; }
  float latencyG() const { return latencyG_; }
  float lastWeightG() const { return weightG_; }
  bool hasValidWeight() const { return hasWeight_; }

private:
  bool active() const { return relayRequested(); }
  bool freshWeight(uint32_t nowMs) const;
  void stop(StopReason reason, uint32_t nowMs);
  void fault(StopReason reason);

  Config config_;
  State state_ = State::Idle;
  StopReason stopReason_ = StopReason::None;
  float targetG_ = 0.0f;
  float latencyG_ = 0.25f;
  float weightG_ = 0.0f;
  bool hasWeight_ = false;
  uint32_t lastSampleMs_ = 0;
  uint32_t runStartMs_ = 0;
  uint32_t stoppedMs_ = 0;
  bool learnLatency_ = false;
};
