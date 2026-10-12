#include "../src/grinder_profile_detector.h"

#include <cassert>
#include <cstdio>

using Detector = GrinderProfileDetector;
using State = Detector::State;
using Tare = Detector::TareKind;

static void registerProfiles(Detector& detector)
{
  assert(detector.setProfile(0, {458.7f, 100.4f, 8.4f}));
  assert(detector.setProfile(1, {546.9f, 100.4f, 8.4f}));
  assert(detector.setProfile(2, {606.8f, 60.9f, 16.0f}));
  assert(detector.setProfile(3, {300.0f, 60.9f, 15.0f}));
}

static void sample(Detector& d, float grams, uint32_t ms)
{
  d.onWeightSample(grams, true, ms);
  d.onWeightSample(grams, true, ms + 1500);
}

int main()
{
  Detector::Config config;
  config.previewOnly = true;

  // Full combination recognized without any tare request or grind readiness.
  {
    Detector detector(config);
    registerProfiles(detector);
    sample(detector, 559.1f, 0);
    assert(detector.state() == State::CombinationPreview);
    assert(detector.selectedProfile() == 0);
    assert(detector.selectedTargetG() == 8.4f);
    assert(detector.takeTareRequest() == Tare::None);
    assert(!detector.readyToGrind());
    detector.onWeightSample(0, true, 1600);
    assert(detector.state() == State::WaitVessel);
    assert(detector.selectedProfile() == -1);
    sample(detector, 667.7f, 1700);
    assert(detector.state() == State::CombinationPreview);
    assert(detector.selectedProfile() == 2);
  }

  // Staged placement: recognizable filter followed by its funnel.
  {
    Detector detector(config);
    registerProfiles(detector);
    sample(detector, 458.7f, 0);
    assert(detector.state() == State::WaitFunnel);
    assert(detector.selectedProfile() == 0);
    sample(detector, 559.1f, 1800);
    assert(detector.state() == State::CombinationPreview);
    assert(detector.takeTareRequest() == Tare::None);
    assert(!detector.readyToGrind());
  }

  // Unknown vessel and ambiguous profile do not generate tare requests.
  {
    Detector detector(config);
    registerProfiles(detector);
    sample(detector, 432.0f, 0);
    assert(detector.state() == State::UnknownVessel);
    assert(detector.takeTareRequest() == Tare::None);
  }
  {
    Detector detector(config);
    assert(detector.setProfile(0, {460.0f, 100.0f, 9.0f}));
    assert(detector.setProfile(1, {460.0f, 110.0f, 10.0f}));
    sample(detector, 460.0f, 0);
    assert(detector.state() == State::AmbiguousVessel);
    assert(detector.takeTareRequest() == Tare::None);
  }
  std::puts("grinder_profile_preview_b3b_test: PASS");
}
