#include "grinder_weight_stability.h"

#include <cmath>

GrinderWeightStability::GrinderWeightStability()
  : GrinderWeightStability(Config{}) {}

GrinderWeightStability::GrinderWeightStability(const Config& config)
  : config_(config) {}

void GrinderWeightStability::reset() {
  hasSample_ = false;
  lastSampleMs_ = 0;
  quietSinceMs_ = 0;
  anchorG_ = 0.0f;
  lastWeightG_ = 0.0f;
  quietSampleCount_ = 0;
}

void GrinderWeightStability::onSample(float grams, uint32_t nowMs) {
  if (!std::isfinite(grams)) {
    reset();
    return;
  }

  const bool restartWindow =
    !hasSample_ ||
    (nowMs - lastSampleMs_ > config_.maxSampleAgeMs) ||
    (std::fabs(grams - anchorG_) > config_.maxDriftG);
  if (restartWindow) {
    quietSinceMs_ = nowMs;
    anchorG_ = grams;
    quietSampleCount_ = 1;
  } else if (quietSampleCount_ < UINT16_MAX) {
    ++quietSampleCount_;
  }

  hasSample_ = true;
  lastSampleMs_ = nowMs;
  lastWeightG_ = grams;
}

bool GrinderWeightStability::hasFreshSample(uint32_t nowMs) const {
  return hasSample_ && nowMs - lastSampleMs_ <= config_.maxSampleAgeMs;
}

bool GrinderWeightStability::isStable(uint32_t nowMs) const {
  return hasFreshSample(nowMs) &&
         quietSampleCount_ >= config_.minSamples &&
         nowMs - quietSinceMs_ >= config_.settleMs;
}
