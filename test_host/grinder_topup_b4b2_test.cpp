// B4b.2: hold-to-top-up remains a logical motor request only; NO GPIO.
#include "grinder_cycle_gate.h"
#include "grinder_topup_hold_button.h"
#include <cassert>
#include <cstdio>
#include <limits>

using D = GrinderProfileDetector;
using G = GrinderCycleGate;
using C = GrinderController;
using B = GrinderTopupHoldButton;
using E = B::Event;

static void feed(D& d, G& g, float grams, bool stable, uint32_t at) {
  d.onWeightSample(grams, stable, at);
  d.tick(at);
  g.onWeightSample(grams, stable, at);
  g.tick(at);
}

static void prepare(D& d, G& g, uint32_t t) {
  assert(d.setProfile(0, {449.6f, 100.1f, 8.5f}));
  feed(d, g, 549.7f, true, t);
  feed(d, g, 549.7f, true, t + 1600);
  assert(d.takeTareRequest() == D::TareKind::AutoCombination);
  d.onTareResult(true, t + 1700);
  feed(d, g, 0.00f, true, t + 1800);
  assert(d.state() == D::State::ReadyAuto);
  assert(g.requestStart(t + 1900));
}

static void finishUnderTarget(D& d, G& g, uint32_t t) {
  // First run has 0.25 g latency; shut off at >=8.25 g, settle below target.
  feed(d, g, 8.26f, false, t);
  assert(g.state() == C::State::Settling);
  feed(d, g, 8.20f, true, t + 350);
  g.tick(t + 500);
  assert(g.state() == C::State::Complete && !g.logicRunning());
  assert(g.stopReason() == C::StopReason::TargetReached);
  assert(g.canRequestManualTopup(t + 501));
}

int main() {
  // Physical hold classifier: 25ms press debounce, *immediate* raw release,
  // one press per gesture, no late re-arm or boot-held top-up.
  {
    B b; b.reset(true, 0);
    assert(b.update(true, true, 1000) == E::None);
    assert(b.update(false, false, 1001) == E::None);
    assert(b.update(true, true, 1100) == E::None);
    assert(b.update(true, true, 1124) == E::None);
    assert(b.update(true, true, 1125) == E::Press);
    assert(b.update(true, true, 1200) == E::None);
    assert(b.update(false, false, 1201) == E::Release);
    assert(b.update(false, false, 1202) == E::None);
    assert(b.update(true, false, 1300) == E::None);
    assert(b.update(true, true, 1600) == E::None); // no late arm
    assert(b.update(false, false, 1601) == E::None);
    assert(b.update(true, true, 1700) == E::None);
    assert(b.update(true, false, 1701) == E::None);
    assert(b.update(true, true, 1900) == E::None);
    assert(b.update(false, true, 1901) == E::None);
    assert(b.update(true, true, 2000) == E::None);
    assert(b.update(true, true, 2025) == E::Press);
    assert(b.update(true, false, 2026) == E::Interrupted);
    assert(b.update(true, true, 2500) == E::None);
  }
  // A normal completed dose with a 0.30g deficit allows hold-to-run only.
  {
    D d; G g(d); prepare(d,g,100);
    finishUnderTarget(d,g,2100);
    assert(!g.requestStart(2601)); // original consumed-cycle lockout
    assert(g.requestManualTopupPress(2601));
    assert(g.state() == C::State::ManualTopup && g.logicRunning());
    assert(!g.requestManualTopupPress(2602));
    assert(g.requestManualTopupRelease(2603));
    assert(g.state() == C::State::Settling && !g.logicRunning());
    assert(g.stopReason() == C::StopReason::ManualReleased);
    feed(d, g, 8.24f, true, 2900);
    g.tick(3100);
    assert(g.state() == C::State::Complete);
    assert(g.canRequestManualTopup(3101)); // allow multiple deliberate holds
    assert(g.requestManualTopupPress(3102));
    assert(g.state() == C::State::ManualTopup);
    feed(d,g,8.46f,true,3103); // manual hold may cross target
    assert(g.state() == C::State::ManualTopup && g.logicRunning());
    assert(g.requestManualTopupRelease(3104));
    assert(g.state() == C::State::Settling && !g.logicRunning());
    feed(d,g,8.46f,true,3470);
    g.tick(3600);
    assert(g.state() == C::State::Complete);
    assert(g.canRequestManualTopup(3601)); // explicit hold even above target
    assert(g.requestManualTopupPress(3601));
    assert(g.state() == C::State::ManualTopup);
    assert(g.requestManualTopupRelease(3602));
  }
  // Separate 3s bound is a NORMAL stop, with re-press only after raw release.
  // It does not affect the 30s normal-grinding safety limit.
  {
    D d; G g(d); prepare(d,g,100);
    finishUnderTarget(d,g,2100);
    B b; b.reset(false, 2500);
    assert(b.update(true, true, 2601) == E::None);
    assert(b.update(true, true, 2626) == E::Press);
    assert(g.requestManualTopupPress(2626));
    // Fresh samples but still held with no weight change.
    for (uint32_t ms=2800; ms<5601; ms+=200) feed(d,g,8.2f,true,ms);
    assert(g.state() == C::State::ManualTopup);
    g.tick(5626); // exactly 3s from activation
    assert(g.state() == C::State::Settling && !g.logicRunning());
    assert(g.stopReason() == C::StopReason::ManualTimeLimit);
    assert(!g.requestManualTopupPress(5627));
    assert(b.update(true, false, 5627) == E::Interrupted);
    feed(d,g,8.60f,true,5800);
    feed(d,g,8.60f,true,6080);
    assert(g.state() == C::State::Complete);
    assert(g.canRequestManualTopup(6081)); // also beyond target
    assert(b.update(true, true, 6081) == E::None); // still held: no restart
    assert(b.update(false, true, 6082) == E::None);
    assert(b.update(true, true, 6100) == E::None);
    assert(b.update(true, true, 6125) == E::Press);
    assert(g.requestManualTopupPress(6125));
    assert(g.state() == C::State::ManualTopup && g.logicRunning());
    assert(b.update(false, true, 6200) == E::Release);
    assert(g.requestManualTopupRelease(6200));
    assert(g.stopReason() == C::StopReason::ManualReleased);
    assert(!g.logicRunning());
  }
  // Even a large stable deficit after a normally completed dose allows an
  // EXPLICIT top-up, but must never restart the automatic Grinding state.
  {
    D d; G g(d); prepare(d,g,100);
    finishUnderTarget(d,g,2100);
    feed(d,g,3.5f,true,2601); // 5.0g below target: former 0.5g gate denied
    assert(g.canRequestManualTopup(2602));
    assert(g.requestManualTopupPress(2602));
    assert(g.state() == C::State::ManualTopup && g.logicRunning());
    assert(!g.requestStart(2603));
    assert(g.requestManualTopupRelease(2604));
    assert(!g.logicRunning());
  }
  // Not available from ordinary Idle, incomplete, manually canceled cycle.
  {
    D d; G g(d); prepare(d,g,100);
    assert(!g.canRequestManualTopup(2001));
    assert(!g.requestManualTopupPress(2001));
    assert(g.requestCancel(2002));
    feed(d,g,8.2f,true,2350); g.tick(2505);
    assert(g.state()==C::State::Complete);
    assert(!g.canRequestManualTopup(2506));
    assert(!g.requestManualTopupPress(2506));
  }
  // A stale sample, unstable weight or detector state change disallows top-up.
  {
    D d; G g(d); prepare(d,g,100);
    finishUnderTarget(d,g,2100);
    feed(d,g,8.19f,false,2601);
    assert(!g.canRequestManualTopup(2602));
    feed(d,g,8.19f,true,2700);
    assert(!g.canRequestManualTopup(3701));
    assert(g.requestManualTopupPress(2701));
    assert(g.requestSafetyAbort());
    assert(g.state()==C::State::Fault && !g.logicRunning());
  }
  // In-progress manual top-up must not teach the adaptive latency estimator.
  {
    D d; G g(d); prepare(d,g,100);
    finishUnderTarget(d,g,2100);
    const float before = g.latencyG();
    assert(g.requestManualTopupPress(2601));
    assert(g.requestManualTopupRelease(2602));
    feed(d,g,8.25f,true,3000); g.tick(3100);
    assert(g.state()==C::State::Complete);
    assert(g.latencyG()==before);
  }
  std::puts("grinder_topup_b4b2_test: PASS");
}
