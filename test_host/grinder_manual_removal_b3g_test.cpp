#include "grinder_profile_detector.h"

#include <cassert>
#include <cstdio>

using Detector = GrinderProfileDetector;
using State = Detector::State;
using Tare = Detector::TareKind;

static void prepareProfile(Detector& d) {
  assert(d.setProfile(0, {449.6f, 100.1f, 8.5f}));
  assert(d.setAutoDetect(false));
  assert(d.selectManualProfile(0));
}

static void testBootLoadedRemoval(bool autodetect) {
  Detector d;
  assert(d.setProfile(0, {449.6f, 100.1f, 8.5f}));
  assert(d.setAutoDetect(autodetect));
  if (!autodetect) assert(d.selectManualProfile(0));

  // LoadCell.start(2000, true) tares the vessel already present at boot.
  // The detector has never entered a Ready state.
  d.onWeightSample(0.0f, true, 100);
  assert(d.state() == State::WaitVessel && !d.readyToGrind());
  // Small negative drift is not a vessel removal.
  d.onWeightSample(-29.0f, true, 150);
  assert(d.state() == State::WaitVessel);
  assert(d.takeTareRequest() == Tare::None);
  // A transient drop that recovers must not request any tare.
  d.onWeightSample(-549.3f, false, 200);
  assert(d.state() == State::CheckingRemoval && !d.readyToGrind());
  d.onWeightSample(0.0f, true, 400);
  assert(d.state() == State::WaitVessel);
  assert(d.takeTareRequest() == Tare::None);

  // A confirmed lift after boot must request exactly one removal tare,
  // including when Autodetect is OFF.
  d.onWeightSample(-549.3f, false, 500);
  assert(d.state() == State::CheckingRemoval);
  d.onWeightSample(-549.3f, true, 2100);
  assert(d.takeTareRequest() == Tare::AfterRemoval);
  assert(d.takeTareRequest() == Tare::None);
  d.onTareResult(true, 2150);
  assert(d.state() == State::WaitPostTare && !d.readyToGrind());
  // The fresh post-tare zero, not the previously cached sample, finishes.
  d.onWeightSample(-549.3f, false, 2175);
  assert(d.state() == State::WaitPostTare);
  d.onWeightSample(0.02f, true, 2300);
  assert(d.state() == State::WaitVessel && !d.readyToGrind());
  d.onWeightSample(0.0f, true, 2400);
  assert(d.takeTareRequest() == Tare::None);
}

int main() {
  testBootLoadedRemoval(true);
  testBootLoadedRemoval(false);
  {
    Detector d;
    prepareProfile(d);
    // A zero correction on an already empty scale is permitted, but must
    // never create ReadyManual. This is true before and after a removal.
    d.onWeightSample(0.03f, true, 100);
    assert(d.canUserTare(100));
    assert(!d.requestManualTare(100));
    assert(d.takeTareRequest() == Tare::None);
    assert(!d.readyToGrind());
    d.onWeightSample(0.0f, true, 200);
    // Deliberate manual preparation with a portafilter actually present.
    d.onWeightSample(449.6f, true, 300);
    assert(d.requestManualTare(300));
    assert(d.takeTareRequest() == Tare::Manual);
    d.onTareResult(true, 350);
    assert(!d.readyToGrind());
    d.onWeightSample(0.02f, true, 450);
    assert(d.state() == State::ReadyManual && d.readyToGrind());
    // Re-tare is still possible for drift, but not for coffee already added.
    d.onWeightSample(0.25f, true, 500);
    assert(d.canUserTare(500) && d.requestManualTare(500));
    assert(d.takeTareRequest() == Tare::Manual);
    assert(!d.readyToGrind());
    d.onTareResult(true, 550);
    d.onWeightSample(0.0f, true, 600);
    assert(d.state() == State::ReadyManual);
    d.onWeightSample(8.5f, true, 650);
    assert(!d.canUserTare(650));
    assert(!d.requestManualTare(650));
    assert(d.readyToGrind());
    // Lifting must revoke ready immediately, even with autodetect OFF.
    d.onWeightSample(-449.6f, false, 800);
    assert(d.state() == State::CheckingRemoval && !d.readyToGrind());
    assert(!d.canUserTare(800));
    d.onWeightSample(-449.6f, true, 2500);
    assert(d.takeTareRequest() == Tare::AfterRemoval);
    assert(d.takeTareRequest() == Tare::None);
    assert(!d.canUserTare(2500));
    d.onTareResult(true, 2550);
    assert(d.state() == State::WaitPostTare && !d.readyToGrind());
    d.onWeightSample(0.05f, true, 2700);
    assert(d.state() == State::WaitVessel && !d.readyToGrind());
    // The main loop re-selects the manual profile after resetDetection().
    assert(d.selectManualProfile(0));
    d.onWeightSample(0.01f, true, 2800);
    assert(d.canUserTare(2800));
    assert(!d.requestManualTare(2800));
    assert(!d.readyToGrind());
    // Another deliberate vessel + tare may arm again.
    d.onWeightSample(449.6f, true, 2900);
    assert(d.requestManualTare(2900));
    assert(d.takeTareRequest() == Tare::Manual);
    d.onTareResult(true, 3000);
    d.onWeightSample(0.0f, true, 3100);
    assert(d.state() == State::ReadyManual);
  }
  {
    Detector d;
    prepareProfile(d);
    d.onWeightSample(-450.0f, true, 10);
    assert(!d.requestManualTare(10));
    d.onWeightSample(449.6f, true, 100);
    assert(d.requestManualTare(100));
    assert(d.takeTareRequest() == Tare::Manual);
    d.onTareResult(true, 150);
    d.onWeightSample(0.0f, true, 200);
    d.onWeightSample(-449.6f, true, 250);
    d.onWeightSample(-449.6f, true, 1850);
    assert(d.takeTareRequest() == Tare::AfterRemoval);
    // An actual tare failure must NOT manufacture readiness.
    d.onTareResult(false, 1900);
    assert(d.state() == State::Fault && !d.readyToGrind());
  }
  std::puts("grinder_manual_removal_b3g_test: PASS");
}
