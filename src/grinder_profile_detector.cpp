#include "grinder_profile_detector.h"

#include <cmath>

GrinderProfileDetector::GrinderProfileDetector() : GrinderProfileDetector(Config{}) {}

GrinderProfileDetector::GrinderProfileDetector(const Config& config) : config_(config) {}

bool GrinderProfileDetector::configured(int index) const {
  return index >= 0 && index < kProfileCount && profileValid_[index];
}

bool GrinderProfileDetector::setProfile(int index, const Profile& profile) {
  if (paused_ || state_ == State::AwaitTare || state_ == State::WaitPostTare ||
      state_ == State::CheckingRemoval || readyToGrind() ||
      !std::isfinite(profile.portafilterG) ||
      !std::isfinite(profile.funnelG) || !std::isfinite(profile.targetG) ||
      profile.portafilterG <= config_.placeThresholdG ||
      profile.funnelG <= 0.0f || profile.targetG < 5.0f || profile.targetG > 25.0f ||
      index < 0 || index >= kProfileCount) return false;
  profiles_[index] = profile;
  profileValid_[index] = true;
  resetDetection();
  return true;
}

void GrinderProfileDetector::resetDetection() {
  state_ = State::WaitVessel;
  selectedProfile_ = -1;
  tareKind_ = TareKind::None;
  completedTareKind_ = TareKind::None;
  tareRequestPending_ = false;
  sampleValid_ = false;
  sampleStable_ = false;
}

bool GrinderProfileDetector::setAutoDetect(bool enabled) {
  if (paused_ || state_ == State::AwaitTare || state_ == State::WaitPostTare ||
      state_ == State::CheckingRemoval || state_ == State::Fault) return false;
  autoDetect_ = enabled;
  resetDetection();
  return true;
}

bool GrinderProfileDetector::selectManualProfile(int index) {
  if (autoDetect_ || paused_ || state_ == State::AwaitTare ||
      state_ == State::WaitPostTare || state_ == State::CheckingRemoval ||
      state_ == State::Fault || !configured(index)) return false;
  selectedProfile_ = index;
  state_ = State::WaitVessel;
  return true;
}

float GrinderProfileDetector::selectedTargetG() const {
  return configured(selectedProfile_) ? profiles_[selectedProfile_].targetG : 0.0f;
}

bool GrinderProfileDetector::freshStableSample(uint32_t nowMs) const {
  return sampleValid_ && sampleStable_ &&
         nowMs - sampleMs_ <= config_.maxSampleAgeMs;
}

bool GrinderProfileDetector::requestManualTare(uint32_t nowMs) {
  if (autoDetect_ || paused_ || state_ == State::Fault ||
      state_ == State::AwaitTare || state_ == State::WaitPostTare ||
      state_ == State::CheckingRemoval ||
      !configured(selectedProfile_) || !freshStableSample(nowMs)) return false;
  if (state_ == State::ReadyManual) {
    // Only correct a small drift while prepared. Keep the selected profile.
    if (std::fabs(sampleG_) > config_.retareMaxAbsWeightG) return false;
  } else {
    // Explicit manual preparation needs evidence of an actual vessel being
    // placed on the untared scale. Taring the empty scale must not arm it.
    if (sampleG_ < config_.placeThresholdG) return false;
  }
  beginTare(TareKind::Manual, nowMs);
  return true;
}

bool GrinderProfileDetector::requestSkipFunnel(uint32_t nowMs) {
  // Only a user action while a *bare*, known portafilter is stably present.
  // In particular, an old WaitFunnel state is not enough if the load moved.
  if (!autoDetect_ || paused_ || config_.previewOnly ||
      state_ != State::WaitFunnel || !configured(selectedProfile_) ||
      !freshStableSample(nowMs) ||
      std::fabs(sampleG_ - profiles_[selectedProfile_].portafilterG) >=
        config_.matchToleranceG) return false;
  beginTare(TareKind::AutoNoFunnel, nowMs);
  return true;
}

bool GrinderProfileDetector::canUserTare(uint32_t nowMs) const {
  if (paused_) return false;
  // An in-flight automatic removal tare may also occur with autodetect OFF.
  // Never let a user tare race this operation.
  if (state_ == State::AwaitTare || state_ == State::WaitPostTare ||
      state_ == State::CheckingRemoval) return false;
  if (!autoDetect_) {
    // An already prepared manual vessel may be re-zeroed for drift only.
    // A dose must not silently disappear from the displayed weight.
    if (state_ == State::ReadyManual)
      return freshStableSample(nowMs) &&
             std::fabs(sampleG_) <= config_.retareMaxAbsWeightG;
    return true; // idle: user tare is allowed but must not imply readiness
  }
  if (state_ == State::WaitFunnel || state_ == State::CheckingFunnel) return false;
  if (state_ == State::ReadyAuto || state_ == State::ReadyNoFunnel)
    return canRetare(nowMs);
  return true; // idle/unknown: plain zero, never automatic ready
}

bool GrinderProfileDetector::canRetare(uint32_t nowMs) const {
  return autoDetect_ && !paused_ && !config_.previewOnly &&
         (state_ == State::ReadyAuto || state_ == State::ReadyNoFunnel) &&
         freshStableSample(nowMs) &&
         std::fabs(sampleG_) <= config_.retareMaxAbsWeightG;
}

bool GrinderProfileDetector::requestRetare(uint32_t nowMs) {
  if (!canRetare(nowMs)) return false;
  const TareKind kind = state_ == State::ReadyAuto ?
      TareKind::ReTareAuto : TareKind::ReTareNoFunnel;
  beginTare(kind, nowMs);
  return true;
}

void GrinderProfileDetector::beginCandidate(State state, float grams, uint32_t nowMs) {
  state_ = state;
  candidateG_ = grams;
  candidateMs_ = nowMs;
}

void GrinderProfileDetector::updateCandidate(float grams, bool stable, uint32_t nowMs) {
  if (!stable || std::fabs(grams - candidateG_) > config_.steadyDeltaG) {
    candidateG_ = grams;
    candidateMs_ = nowMs;
  }
}

bool GrinderProfileDetector::candidateReady(uint32_t nowMs, bool stable) const {
  return stable && (nowMs - candidateMs_ >= config_.settleMs);
}

GrinderProfileDetector::Match GrinderProfileDetector::matchProfile(float grams) const {
  Match match = {MatchType::None, -1};
  for (int i = 0; i < kProfileCount; ++i) {
    if (!configured(i)) continue;
    const bool portafilter = std::fabs(grams - profiles_[i].portafilterG) < config_.matchToleranceG;
    const bool combination = std::fabs(grams - profiles_[i].portafilterG - profiles_[i].funnelG) < config_.matchToleranceG;
    if (!portafilter && !combination) continue;
    if (match.type != MatchType::None || (portafilter && combination))
      return {MatchType::Ambiguous, -1};
    match = {combination ? MatchType::Combination : MatchType::Portafilter, i};
  }
  return match;
}

bool GrinderProfileDetector::isCombinationForSelected(float grams) const {
  return configured(selectedProfile_) &&
         std::fabs(grams - profiles_[selectedProfile_].portafilterG -
                   profiles_[selectedProfile_].funnelG) < config_.matchToleranceG;
}

void GrinderProfileDetector::beginTare(TareKind kind, uint32_t nowMs) {
  tareKind_ = kind;
  tareRequestPending_ = true;
  tareStartMs_ = nowMs;
  state_ = State::AwaitTare;
}

GrinderProfileDetector::TareKind GrinderProfileDetector::takeTareRequest() {
  if (state_ != State::AwaitTare || !tareRequestPending_) return TareKind::None;
  tareRequestPending_ = false;
  return tareKind_;
}

void GrinderProfileDetector::onTareResult(bool successful, uint32_t nowMs) {
  if (state_ != State::AwaitTare || tareRequestPending_) return;
  if (!successful) {
    state_ = State::Fault;
    tareKind_ = TareKind::None;
    return;
  }
  // The HX711 may still expose a pre-tare sample; do not indicate readiness
  // until a NEW, stable, near-zero sample confirms the changed tare offset.
  completedTareKind_ = tareKind_;
  tareKind_ = TareKind::None;
  sampleValid_ = false;
  sampleStable_ = false;
  tareStartMs_ = nowMs;
  state_ = State::WaitPostTare;
}

void GrinderProfileDetector::onWeightSample(float grams, bool stable, uint32_t nowMs) {
  if (!std::isfinite(grams)) {
    sampleValid_ = false;
    sampleStable_ = false;
    // Never retain a grind-ready indication across an invalid scale reading.
    if (readyToGrind()) state_ = State::Fault;
    return;
  }
  sampleG_ = grams;
  sampleMs_ = nowMs;
  sampleStable_ = stable;
  sampleValid_ = true;
  if (paused_ || state_ == State::Fault || state_ == State::AwaitTare) return;
  if (state_ == State::WaitPostTare) {
    if (!stable || std::fabs(grams) > config_.postTareZeroToleranceG) return;
    const TareKind kind = completedTareKind_;
    completedTareKind_ = TareKind::None;
    if (kind == TareKind::AfterRemoval) resetDetection();
    else if (kind == TareKind::AutoCombination || kind == TareKind::ReTareAuto)
      state_ = State::ReadyAuto;
    else if (kind == TareKind::AutoNoFunnel || kind == TareKind::ReTareNoFunnel)
      state_ = State::ReadyNoFunnel;
    else if (kind == TareKind::Manual) state_ = State::ReadyManual;
    else state_ = State::Fault;
    return;
  }

  if (state_ == State::CombinationPreview) {
    // B3b is diagnostic only: do not turn a recognized combination into a
    // tare request, nor keep the previous profile if the load changes.
    if (grams <= config_.emptyThresholdG) {
      resetDetection();
    } else if (std::fabs(grams - candidateG_) >= config_.funnelChangeThresholdG) {
      beginCandidate(State::CheckingVessel, grams, nowMs);
    }
    return;
  }

  if (readyToGrind()) {
    // After tare, removing a heavy vessel produces a strongly negative weight.
    // Revoke ready status immediately; restore only if it bounces back.
    if (grams < -config_.liftThresholdG)
    {
      preRemovalState_ = state_;
      beginCandidate(State::CheckingRemoval, grams, nowMs);
    }
    return;
  }
  if (state_ == State::CheckingRemoval) {
    if (grams >= -config_.emptyThresholdG) {
      state_ = preRemovalState_; // transient disturbance, not a lift
      return;
    }
    updateCandidate(grams, stable, nowMs);
    if (candidateReady(nowMs, stable) && grams < -config_.liftThresholdG)
      beginTare(TareKind::AfterRemoval, nowMs);
    return;
  }

  if (!autoDetect_) return;

  if (state_ == State::WaitVessel || state_ == State::UnknownVessel ||
      state_ == State::AmbiguousVessel) {
    if (grams <= config_.emptyThresholdG) {
      state_ = State::WaitVessel;
      selectedProfile_ = -1;
      return;
    }
    // Unknown/ambiguous objects can be retried after a meaningful load change.
    if (state_ == State::WaitVessel ||
        std::fabs(grams - candidateG_) > config_.funnelChangeThresholdG) {
      if (grams > config_.placeThresholdG)
        beginCandidate(State::CheckingVessel, grams, nowMs);
    }
    return;
  }
  if (state_ == State::CheckingVessel) {
    if (grams < config_.placeThresholdG) {
      state_ = State::WaitVessel;
      return;
    }
    updateCandidate(grams, stable, nowMs);
    if (!candidateReady(nowMs, stable)) return;
    const Match match = matchProfile(grams);
    if (match.type == MatchType::Ambiguous) {
      selectedProfile_ = -1;
      state_ = State::AmbiguousVessel;
    } else if (match.type == MatchType::None) {
      selectedProfile_ = -1;
      state_ = State::UnknownVessel;
    } else {
      selectedProfile_ = match.index;
      if (match.type == MatchType::Portafilter) state_ = State::WaitFunnel;
      else if (config_.previewOnly) state_ = State::CombinationPreview;
      else beginTare(TareKind::AutoCombination, nowMs);
    }
    return;
  }
  if (state_ == State::WaitFunnel) {
    if (grams <= config_.emptyThresholdG) {
      selectedProfile_ = -1;
      state_ = State::WaitVessel;
    } else if (configured(selectedProfile_) &&
               std::fabs(grams - profiles_[selectedProfile_].portafilterG) >=
                 config_.funnelChangeThresholdG) {
      beginCandidate(State::CheckingFunnel, grams, nowMs);
    }
    return;
  }
  if (state_ == State::CheckingFunnel) {
    if (grams <= config_.emptyThresholdG) {
      selectedProfile_ = -1;
      state_ = State::WaitVessel;
      return;
    }
    if (configured(selectedProfile_) &&
        std::fabs(grams - profiles_[selectedProfile_].portafilterG) <
          config_.funnelChangeThresholdG) {
      state_ = State::WaitFunnel;
      return;
    }
    updateCandidate(grams, stable, nowMs);
    if (candidateReady(nowMs, stable)) {
      if (isCombinationForSelected(grams)) {
        if (config_.previewOnly) state_ = State::CombinationPreview;
        else beginTare(TareKind::AutoCombination, nowMs);
      }
      else state_ = State::WaitFunnel; // unexpected item; do not tare
    }
  }
}

void GrinderProfileDetector::tick(uint32_t nowMs) {
  if ((state_ == State::AwaitTare || state_ == State::WaitPostTare) &&
      nowMs - tareStartMs_ >= config_.tareTimeoutMs) {
    state_ = State::Fault;
    tareKind_ = TareKind::None;
    completedTareKind_ = TareKind::None;
    tareRequestPending_ = false;
  }
  // Expired measurement invalidates ready state. It must not remain usable
  // if HX711 updates stop while the device is idle.
  if (readyToGrind() && (!sampleValid_ || nowMs - sampleMs_ > config_.maxSampleAgeMs))
    state_ = State::Fault;
}

void GrinderProfileDetector::setPaused(bool paused) {
  paused_ = paused;
}

void GrinderProfileDetector::clearFault() {
  if (state_ == State::Fault) resetDetection();
}

void GrinderProfileDetector::invalidatePreparation() {
  resetDetection();
}
