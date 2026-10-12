#pragma once

#include <stdint.h>

// Phase B3a: conservative stability estimate for NEW HX711 samples only.
// Pure module: never tares the load cell, selects a profile or drives GPIO.
class GrinderWeightStability {
public:
  struct Config {
    uint32_t settleMs = 500;       // require a quiet window
    uint32_t maxSampleAgeMs = 1000; // stale data must never be stable
    float maxDriftG = 0.35f;       // maximum deviation from quiet-window anchor
    uint16_t minSamples = 4;       // avoid declaring a single reading stable
  };

  GrinderWeightStability();
  explicit GrinderWeightStability(const Config& config);

  void reset();
  void onSample(float grams, uint32_t nowMs); // call only when HX711.update() reports new data
  bool hasFreshSample(uint32_t nowMs) const;
  bool isStable(uint32_t nowMs) const;
  float lastWeightG() const { return lastWeightG_; }
  uint16_t quietSampleCount() const { return quietSampleCount_; }

private:
  Config config_;
  bool hasSample_ = false;
  uint32_t lastSampleMs_ = 0;
  uint32_t quietSinceMs_ = 0;
  float anchorG_ = 0.0f;
  float lastWeightG_ = 0.0f;
  uint16_t quietSampleCount_ = 0;
};
