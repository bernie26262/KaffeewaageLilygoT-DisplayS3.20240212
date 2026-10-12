#include "../src/grinder_profile_detector.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using Detector = GrinderProfileDetector;
using State = Detector::State;
using Tare = Detector::TareKind;

static void configure(Detector& d) {
  // Original project defaults; B3 will inject the already-persisted arrays.
  const Detector::Profile profiles[] = {
    {458.7f, 100.4f, 8.4f}, {546.9f, 100.4f, 8.4f},
    {606.8f, 60.9f, 16.0f}, {300.0f, 60.9f, 15.0f}
  };
  for (int i = 0; i < 4; ++i) assert(d.setProfile(i, profiles[i]));
}

static void observe(Detector& d, float grams, uint32_t startMs) {
  d.onWeightSample(grams, true, startMs);
  d.onWeightSample(grams, true, startMs + 1500);
}

int main() {
  // Profile selection based on portafilter only, without falsely requesting tare.
  {
    Detector d;
    configure(d);
    observe(d, 458.7f, 10);
    assert(d.state() == State::WaitFunnel);
    assert(d.selectedProfile() == 0);
    assert(std::fabs(d.selectedTargetG() - 8.4f) < 0.01f);
    assert(!d.readyToGrind());
    assert(d.takeTareRequest() == Tare::None);
    observe(d, 559.1f, 2000);
    assert(d.state() == State::AwaitTare);
    assert(d.takeTareRequest() == Tare::AutoCombination);
    assert(d.takeTareRequest() == Tare::None); // event delivered once
    assert(!d.readyToGrind());
    d.onTareResult(true, 3600);
    assert(d.state() == State::WaitPostTare && !d.readyToGrind());
    // Reading after tare is zero, not the old 559 g gross weight.
    d.onWeightSample(0.0f, true, 3700);
    assert(d.state() == State::ReadyAuto && d.readyToGrind());
    assert(d.readyToGrind());
    d.onWeightSample(-558.0f, true, 3750);
    assert(!d.readyToGrind()); // removed -> revoke immediately
    d.onWeightSample(-558.0f, true, 5300);
    assert(d.state() == State::AwaitTare);
    assert(d.takeTareRequest() == Tare::AfterRemoval);
    d.onTareResult(true, 5400); // HX711 tare succeeded; await fresh zero
    assert(d.state() == State::WaitPostTare);
    d.onWeightSample(0.0f, true, 5500);
    assert(d.state() == State::WaitVessel && !d.readyToGrind());
  }
  // Directly placing assembled portafilter+funnel (4th profile).
  {
    Detector d;
    configure(d);
    observe(d, 360.9f, 0);
    assert(d.selectedProfile() == 3);
    assert(d.takeTareRequest() == Tare::AutoCombination);
    d.onTareResult(true, 1600);
    assert(!d.readyToGrind());
    d.onWeightSample(360.9f, true, 1700); // old buffered data must not count
    assert(!d.readyToGrind());
    d.onWeightSample(0.0f, true, 1800);
    assert(std::fabs(d.selectedTargetG() - 15.0f) < 0.01f);
    assert(d.readyToGrind());
  }
  // Unknown load may still be ground into, but only via manual selection and tare.
  {
    Detector d;
    configure(d);
    observe(d, 350.0f, 0);
    assert(d.state() == State::UnknownVessel && !d.readyToGrind());
    assert(d.setAutoDetect(false));
    assert(d.selectManualProfile(2));
    d.onWeightSample(350.0f, true, 1600);
    assert(d.requestManualTare(1601));
    assert(d.takeTareRequest() == Tare::Manual);
    assert(!d.readyToGrind());
    d.onTareResult(true, 1650);
    assert(!d.readyToGrind());
    d.onWeightSample(0.0f, true, 1700);
    assert(d.readyToGrind());
    assert(d.selectedProfile() == 2 && d.selectedTargetG() == 16.0f);
  }
  // A hanging tare fails closed; completion cannot be presumed.
  {
    Detector d;
    configure(d);
    observe(d, 559.1f, 0);
    assert(d.takeTareRequest() == Tare::AutoCombination);
    d.tick(6500);
    assert(d.state() == State::Fault && !d.readyToGrind());
  }
  // Even a completed tare is not sufficient without a fresh, stable zero.
  {
    Detector d;
    configure(d);
    observe(d, 559.1f, 0);
    assert(d.takeTareRequest() == Tare::AutoCombination);
    d.onTareResult(true, 1600);
    d.onWeightSample(3.0f, true, 1800);
    assert(!d.readyToGrind());
    d.tick(6600);
    assert(d.state() == State::Fault && !d.readyToGrind());
  }
  // No recognition from an unstable / moving sample.
  {
    Detector d;
    configure(d);
    d.onWeightSample(559.1f, false, 0);
    d.onWeightSample(559.1f, false, 1600);
    assert(d.state() == State::CheckingVessel);
    d.onWeightSample(559.1f, true, 2000);
    assert(!d.readyToGrind() && d.takeTareRequest() == Tare::None);
    d.onWeightSample(559.1f, true, 3500);
    assert(d.takeTareRequest() == Tare::AutoCombination);
  }
  // Competing profiles must not lead to an arbitrary automatic selection.
  {
    Detector d;
    assert(d.setProfile(0, {500.0f, 100.0f, 15.0f}));
    assert(d.setProfile(1, {500.0f, 80.0f, 16.0f}));
    observe(d, 500.0f, 0);
    assert(d.state() == State::AmbiguousVessel);
    assert(d.selectedProfile() == -1 && !d.readyToGrind());
  }
  // Invalid sample or lost sample freshness invalidates grind preparation.
  {
    Detector d;
    configure(d);
    observe(d, 559.1f, 0);
    d.takeTareRequest();
    d.onTareResult(true, 1550);
    d.onWeightSample(0.0f, true, 1600);
    d.tick(2601);
    assert(d.state() == State::Fault);
    d.clearFault();
    observe(d, 559.1f, 3000);
    d.takeTareRequest();
    d.onTareResult(true, 4550);
    d.onWeightSample(0.0f, true, 4555);
    d.onWeightSample(std::numeric_limits<float>::quiet_NaN(), false, 4600);
    assert(d.state() == State::Fault && !d.readyToGrind());
  }
  // Paused recognition does not absorb samples into a new profile selection.
  {
    Detector d;
    configure(d);
    d.setPaused(true);
    observe(d, 559.1f, 0);
    assert(d.selectedProfile() == -1 && !d.readyToGrind());
    d.setPaused(false);
    observe(d, 559.1f, 1600);
    assert(d.takeTareRequest() == Tare::AutoCombination);
  }
  std::puts("grinder_profile_detector_test: PASS");
  return 0;
}
