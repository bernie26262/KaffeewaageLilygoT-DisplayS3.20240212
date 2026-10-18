#include "grinder_profile_detector.h"

#include <cassert>
#include <cstdio>

using Detector = GrinderProfileDetector;
using State = Detector::State;
using Tare = Detector::TareKind;

static void configure(Detector& d) {
  assert(d.setProfile(0, {449.6f, 100.1f, 8.5f}));
  assert(d.setProfile(2, {475.5f, 60.7f, 17.0f}));
}

static void settle(Detector& d, float grams, uint32_t ms) {
  d.onWeightSample(grams, true, ms);
  d.onWeightSample(grams, true, ms + 1600);
}

int main() {
  {
    Detector d;
    configure(d);
    assert(!d.requestSkipFunnel(10)); // not in WaitFunnel
    settle(d, 449.6f, 100);
    assert(d.state() == State::WaitFunnel && d.selectedTargetG() == 8.5f);
    assert(!d.readyToGrind());
    assert(d.requestSkipFunnel(1800));
    assert(d.state() == State::AwaitTare);
    assert(!d.requestSkipFunnel(1801)); // double key press
    assert(d.takeTareRequest() == Tare::AutoNoFunnel);
    assert(d.takeTareRequest() == Tare::None);
    d.onTareResult(true, 1880);
    assert(d.state() == State::WaitPostTare && !d.readyToGrind());
    d.onWeightSample(449.6f, false, 1900); // pre-tare weight
    assert(!d.readyToGrind());
    d.onWeightSample(0.04f, true, 2000);
    assert(d.state() == State::ReadyNoFunnel && d.readyToGrind());
    assert(d.selectedProfile() == 0 && d.selectedTargetG() == 8.5f);
    d.onWeightSample(7.8f, true, 2100); // simulate coffee added
    d.onWeightSample(13.7f, true, 2200);
    assert(d.state() == State::ReadyNoFunnel && d.takeTareRequest() == Tare::None);
    d.onWeightSample(-449.6f, false, 2300); // remove bare portafilter
    assert(d.state() == State::CheckingRemoval && !d.readyToGrind());
    d.onWeightSample(-449.6f, true, 3900);
    assert(d.takeTareRequest() == Tare::AfterRemoval);
    d.onTareResult(true, 4000);
    d.onWeightSample(0.01f, true, 4100);
    assert(d.state() == State::WaitVessel && !d.readyToGrind());
    assert(!d.requestSkipFunnel(4200));
  }
  {
    Detector d;
    configure(d);
    settle(d, 449.6f, 100);
    d.onWeightSample(449.6f, false, 1701);
    assert(!d.requestSkipFunnel(1701)); // unstable
    d.onWeightSample(450.0f, true, 1702);
    assert(!d.requestSkipFunnel(3000)); // stale
    assert(d.state() == State::WaitFunnel);
    d.onWeightSample(480.0f, true, 3001);
    assert(!d.requestSkipFunnel(3001)); // changed mass; stale WaitFunnel state
    d.onWeightSample(449.6f, true, 3002);
    assert(d.requestSkipFunnel(3002));
    assert(d.takeTareRequest() == Tare::AutoNoFunnel);
    d.onTareResult(false, 3050);
    assert(d.state() == State::Fault && !d.readyToGrind());
    d.invalidatePreparation();
    assert(d.state() == State::WaitVessel);
  }
  {
    Detector d;
    configure(d);
    settle(d, 449.6f, 100);
    d.setPaused(true);
    assert(!d.requestSkipFunnel(1700));
    d.setPaused(false);
    assert(d.requestSkipFunnel(1700));
    assert(d.takeTareRequest() == Tare::AutoNoFunnel);
    d.tick(7001); // timeout
    assert(d.state() == State::Fault && !d.readyToGrind());
  }
  {
    Detector::Config config;
    config.previewOnly = true;
    Detector d(config);
    configure(d);
    settle(d, 449.6f, 100);
    assert(!d.requestSkipFunnel(1700));
  }
  {
    Detector d;
    configure(d);
    assert(d.setAutoDetect(false));
    assert(d.selectManualProfile(0));
    d.onWeightSample(449.6f, true, 100);
    assert(!d.requestSkipFunnel(100));
    assert(d.requestManualTare(100)); // manual operation unchanged
    assert(d.takeTareRequest() == Tare::Manual);
    d.onTareResult(true, 200);
    d.onWeightSample(0.0f, true, 300);
    assert(d.state() == State::ReadyManual && d.readyToGrind());
  }
  {
    Detector d;
    configure(d);
    settle(d, 549.7f, 100); // normal full-combination path
    assert(d.takeTareRequest() == Tare::AutoCombination);
    d.onTareResult(true, 1800);
    d.onWeightSample(0.0f, true, 1900);
    assert(d.state() == State::ReadyAuto && d.readyToGrind());
  }
  std::puts("grinder_skip_funnel_b3e_test: PASS");
}
