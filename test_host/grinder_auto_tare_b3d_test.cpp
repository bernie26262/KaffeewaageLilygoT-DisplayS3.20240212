#include "grinder_profile_detector.h"
#include <cassert>
#include <cstdio>
using D = GrinderProfileDetector;
using S = D::State;
using T = D::TareKind;

static void setup(D& detector) {
  assert(detector.setProfile(0, {449.6f, 100.1f, 8.5f}));
  assert(detector.setProfile(2, {475.5f, 60.7f, 17.0f}));
}
static void settle(D& d, float value, uint32_t start) {
  d.onWeightSample(value, true, start);
  d.onWeightSample(value, true, start + 1600);
}
int main() {
  {
    D d;
    setup(d);
    settle(d, 449.6f, 10);
    assert(d.state() == S::WaitFunnel && d.selectedProfile() == 0);
    assert(d.selectedTargetG() == 8.5f && !d.readyToGrind());
    settle(d, 549.7f, 2000);
    assert(d.state() == S::AwaitTare);
    assert(d.takeTareRequest() == T::AutoCombination);
    assert(d.takeTareRequest() == T::None);  // once per combination
    d.onTareResult(true, 3650);
    assert(d.state() == S::WaitPostTare);
    d.onWeightSample(549.7f, false, 3675);
    assert(!d.readyToGrind());  // stale reading after tare
    d.onWeightSample(0.05f, true, 3700);
    assert(d.state() == S::ReadyAuto && d.readyToGrind());
    d.onWeightSample(-549.6f, false, 4000);
    assert(!d.readyToGrind());  // immediate revocation on lift
    d.onWeightSample(-549.6f, true, 5600);
    assert(d.takeTareRequest() == T::AfterRemoval);
    d.onTareResult(true, 5650);
    d.onWeightSample(0.02f, true, 5800);
    assert(d.state() == S::WaitVessel && !d.readyToGrind());
    settle(d, 549.7f, 6000); // direct combination after removal
    assert(d.takeTareRequest() == T::AutoCombination);
    d.onTareResult(false, 7650);
    assert(d.state() == S::Fault && !d.readyToGrind());
    d.invalidatePreparation();
    assert(d.state() == S::WaitVessel);
  }
  {
    D d;
    setup(d);
    assert(d.setAutoDetect(false));
    assert(d.selectManualProfile(2));
    d.onWeightSample(600.0f, true, 10);
    assert(d.requestManualTare(10));
    assert(d.takeTareRequest() == T::Manual);
    d.onTareResult(true, 100);
    d.onWeightSample(0.0f, true, 200);
    assert(d.state() == S::ReadyManual);
    d.invalidatePreparation();  // external (wizard) tare revokes readiness
    assert(!d.readyToGrind());
  }
  std::puts("grinder_auto_tare_b3d_test: PASS");
}
