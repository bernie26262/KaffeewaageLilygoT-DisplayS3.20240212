#include "grinder_profile_detector.h"

#include <cassert>
#include <cstdio>

using D = GrinderProfileDetector;
using S = D::State;
using T = D::TareKind;

static void setup(D& d) {
  assert(d.setProfile(0, {449.6f, 100.1f, 8.5f}));
  assert(d.setProfile(2, {475.5f, 60.7f, 17.0f}));
}
static void settle(D& d, float grams, uint32_t ms) {
  d.onWeightSample(grams, true, ms);
  d.onWeightSample(grams, true, ms + 1600);
}

int main() {
  {
    D d;
    setup(d);
    settle(d, 449.6f, 100);
    assert(d.state() == S::WaitFunnel);
    assert(!d.canUserTare(1700)); // wait for Skip or funnel
    assert(!d.requestManualTare(1700)); // autodetect ON: never ReadyManual
    assert(!d.canRetare(1700));
    assert(!d.requestRetare(1700)); // skip or funnel required first
    assert(d.requestSkipFunnel(1700));
    assert(d.takeTareRequest() == T::AutoNoFunnel);
    d.onTareResult(true, 1800);
    d.onWeightSample(0.02f, true, 1900);
    assert(d.state() == S::ReadyNoFunnel && d.readyToGrind());
    assert(d.canUserTare(1900));

    d.onWeightSample(0.20f, true, 1950);
    assert(d.canRetare(1950));
    assert(d.requestRetare(1950));
    assert(!d.readyToGrind() && d.state() == S::AwaitTare);
    assert(!d.canUserTare(1951));
    assert(!d.requestRetare(1951)); // no double-tare
    assert(d.takeTareRequest() == T::ReTareNoFunnel);
    assert(d.takeTareRequest() == T::None);
    d.onTareResult(true, 2000);
    assert(!d.readyToGrind() && d.state() == S::WaitPostTare);
    d.onWeightSample(0.02f, false, 2020); // cannot restore on unstable zero
    assert(!d.readyToGrind());
    d.onWeightSample(2.1f, true, 2040); // too far from zero confirmation
    assert(!d.readyToGrind() && d.state() == S::WaitPostTare);
    d.onWeightSample(0.01f, true, 2060);
    assert(d.state() == S::ReadyNoFunnel && d.readyToGrind());
    assert(d.selectedProfile() == 0 && d.selectedTargetG() == 8.5f);

    d.onWeightSample(8.5f, true, 2100);
    assert(!d.canRetare(2100) && !d.requestRetare(2100));
    assert(!d.canUserTare(2100)); // dose protection
    assert(d.state() == S::ReadyNoFunnel);
    d.onWeightSample(-449.6f, false, 2200);
    assert(!d.readyToGrind()); // lift revokes ready immediately
    assert(!d.requestRetare(2200));
    d.onWeightSample(-449.6f, true, 3800);
    assert(d.takeTareRequest() == T::AfterRemoval);
    d.onTareResult(true, 3900);
    d.onWeightSample(0.00f, true, 4000);
    assert(d.state() == S::WaitVessel && !d.readyToGrind());
  }
  {
    D d;
    setup(d);
    settle(d, 549.7f, 100);
    assert(d.takeTareRequest() == T::AutoCombination);
    d.onTareResult(true, 1800);
    d.onWeightSample(0.0f, true, 1900);
    assert(d.state() == S::ReadyAuto);
    d.onWeightSample(-0.5f, true, 1920);
    assert(d.requestRetare(1920));
    assert(d.takeTareRequest() == T::ReTareAuto);
    d.onTareResult(true, 1950);
    d.onWeightSample(2.1f, true, 2000); // NOT a near-zero confirmation
    assert(d.state() == S::WaitPostTare);
    d.onWeightSample(0.02f, true, 2020);
    assert(d.state() == S::ReadyAuto && d.readyToGrind());
    d.onWeightSample(0.02f, false, 2060);
    assert(!d.requestRetare(2060)); // must be stable
    d.onWeightSample(0.02f, true, 2090);
    assert(!d.canRetare(4100)); // stale sample
    assert(d.requestRetare(2090));
    assert(d.takeTareRequest() == T::ReTareAuto);
    d.onTareResult(false, 2100);
    assert(d.state() == S::Fault && !d.readyToGrind());
  }
  {
    D d;
    setup(d);
    settle(d, 549.7f, 100);
    assert(d.takeTareRequest() == T::AutoCombination);
    d.onTareResult(true, 1800);
    d.onWeightSample(0.0f, true, 1900);
    assert(d.requestRetare(1900));
    assert(d.takeTareRequest() == T::ReTareAuto);
    d.tick(7000); // retare timeout -> no ready
    assert(d.state() == S::Fault && !d.readyToGrind());
  }
  {
    D d;
    setup(d);
    assert(d.setAutoDetect(false));
    assert(d.selectManualProfile(2));
    assert(d.canUserTare(0));
    d.onWeightSample(230.0f, true, 100);
    assert(!d.requestRetare(100)); // separate manual workflow
    assert(d.requestManualTare(100));
    assert(d.takeTareRequest() == T::Manual);
    d.onTareResult(true, 200);
    d.onWeightSample(0.0f, true, 300);
    assert(d.state() == S::ReadyManual && d.readyToGrind());
    assert(d.requestManualTare(300)); // manual re-zero also allowed
    assert(d.takeTareRequest() == T::Manual);
    d.onTareResult(true, 400);
    d.onWeightSample(0.0f, true, 500);
    assert(d.state() == S::ReadyManual);
  }
  std::puts("grinder_retare_b3f_test: PASS");
}
