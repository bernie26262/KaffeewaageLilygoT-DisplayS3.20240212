#pragma once

#include <stdint.h>

// Phase B2: pure, host-testable preparation logic. No HX711, GPIO, NVS, TFT,
// WiFi or grinder relay is accessed from this module.
class GrinderProfileDetector {
public:
  static constexpr int kProfileCount = 4;

  struct Profile {
    Profile(float portafilter = 0.0f, float funnel = 0.0f, float target = 0.0f)
      : portafilterG(portafilter), funnelG(funnel), targetG(target) {}
    float portafilterG;
    float funnelG;
    float targetG;
  };

  struct Config {
    uint32_t settleMs = 1500;          // legacy recognition delay
    uint32_t tareTimeoutMs = 5000;     // caller must acknowledge HX711 tare
    uint32_t maxSampleAgeMs = 1000;
    float matchToleranceG = 2.0f;      // absolute error strictly < 2 g
    float placeThresholdG = 30.0f;
    float funnelChangeThresholdG = 20.0f;
    float liftThresholdG = 30.0f;
    float emptyThresholdG = 3.0f;
    float steadyDeltaG = 0.5f;       // reject moving candidates
    float postTareZeroToleranceG = 2.0f; // require a fresh, stable zero after tare
    float retareMaxAbsWeightG = 1.0f; // avoid resetting a dose during drift correction
    bool previewOnly = false; // B3b: report matching combination WITHOUT requesting tara
  };

  enum class State : uint8_t {
    WaitVessel, CheckingVessel, WaitFunnel, CheckingFunnel,
    UnknownVessel, AmbiguousVessel, AwaitTare, WaitPostTare, ReadyAuto,
    ReadyManual, ReadyNoFunnel, CheckingRemoval, Fault, CombinationPreview
  };

  enum class TareKind : uint8_t { None, AutoCombination, AutoNoFunnel, Manual, AfterRemoval, ReTareAuto, ReTareNoFunnel };

  GrinderProfileDetector();
  explicit GrinderProfileDetector(const Config& config);

  // Profiles are supplied by the existing legacy storage arrays in phase B3.
  bool setProfile(int index, const Profile& profile);
  bool setAutoDetect(bool enabled); // only while not awaiting tare/paused
  bool selectManualProfile(int index); // requires AutoDetect OFF
  bool requestManualTare(uint32_t nowMs); // deliberate user action, stable weight required
  // B3e: the user deliberately skips the funnel after a stable portafilter match.
  bool requestSkipFunnel(uint32_t nowMs);
  // B3f: a user-initiated zero correction while already prepared.
  // Only near zero, fresh and stable; readiness is suspended until verified.
  bool canUserTare(uint32_t nowMs) const; // shared TFT/WebUI policy
  bool canRetare(uint32_t nowMs) const;
  bool requestRetare(uint32_t nowMs);
  void onWeightSample(float grams, bool stable, uint32_t nowMs);
  void tick(uint32_t nowMs);
  TareKind takeTareRequest(); // one-shot request for the future HX711 adapter
  void onTareResult(bool successful, uint32_t nowMs); // actual HX711 tare completion
  void setPaused(bool paused); // future B3: inhibit detection during grinding
  void clearFault();
  // External tare, profile edit or calibration invalidates any previous
  // readiness. Never retain a ready indication across a foreign tare.
  void invalidatePreparation();

  bool autoDetect() const { return autoDetect_; }
  bool paused() const { return paused_; }
  bool readyToGrind() const {
    return state_ == State::ReadyAuto || state_ == State::ReadyNoFunnel ||
           state_ == State::ReadyManual;
  }
  State state() const { return state_; }
  int selectedProfile() const { return selectedProfile_; }
  float selectedTargetG() const;
  float profileTargetG(int index) const; // read stored target for safe deferred synchronisation
  TareKind awaitingTare() const { return tareKind_; }

private:
  enum class MatchType : uint8_t { None, Portafilter, Combination, Ambiguous };
  struct Match { MatchType type; int index; };

  bool freshStableSample(uint32_t nowMs) const;
  void beginCandidate(State state, float grams, uint32_t nowMs);
  void updateCandidate(float grams, bool stable, uint32_t nowMs);
  void beginTare(TareKind kind, uint32_t nowMs);
  void resetDetection();
  Match matchProfile(float grams) const;
  bool isCombinationForSelected(float grams) const;
  bool candidateReady(uint32_t nowMs, bool stable) const;
  bool configured(int index) const;

  Config config_;
  Profile profiles_[kProfileCount] = {};
  bool profileValid_[kProfileCount] = {};
  State state_ = State::WaitVessel;
  State preRemovalState_ = State::WaitVessel;
  bool autoDetect_ = true;
  bool paused_ = false;
  int selectedProfile_ = -1;
  float sampleG_ = 0.0f;
  float candidateG_ = 0.0f;
  bool sampleValid_ = false;
  bool sampleStable_ = false;
  uint32_t sampleMs_ = 0;
  uint32_t candidateMs_ = 0;
  uint32_t tareStartMs_ = 0;
  TareKind tareKind_ = TareKind::None;
  TareKind completedTareKind_ = TareKind::None;
  bool tareRequestPending_ = false;
};
