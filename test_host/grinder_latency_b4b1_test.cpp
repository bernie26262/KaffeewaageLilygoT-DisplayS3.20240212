// B4b.1: profile-separated, RAM-only learned latency across completed cycles.
// This deliberately links NO ESP32 hardware and NO SSR / GPIO driver.
#include "grinder_controller.h"
#include "grinder_cycle_gate.h"
#include "grinder_profile_detector.h"

#include <cassert>
#include <cmath>
#include <cstdio>

using D = GrinderProfileDetector;
using G = GrinderCycleGate;
using C = GrinderController;

static void near(float actual, float expected) {
  assert(std::fabs(actual - expected) < 0.001f);
}

static void sample(D& d, G& g, float grams, bool stable, uint32_t ms) {
  d.onWeightSample(grams, stable, ms);
  d.tick(ms);
  g.onWeightSample(grams, stable, ms);
  g.tick(ms);
}

static void configure(D& d) {
  assert(d.setProfile(0, {449.6f, 100.1f, 8.5f}));
  assert(d.setProfile(2, {475.5f, 60.7f, 17.0f}));
}

static void autoReady(D& d, G& g, uint32_t start, int profile) {
  const float combined = profile == 0 ? 549.7f : 536.2f;
  sample(d, g, combined, true, start);
  sample(d, g, combined, true, start + 1600);
  assert(d.takeTareRequest() == D::TareKind::AutoCombination);
  d.onTareResult(true, start + 1700);
  sample(d, g, 0.01f, true, start + 1800);
  assert(d.state() == D::State::ReadyAuto);
  assert(d.selectedProfile() == profile);
}

static void completedRemoval(D& d, G& g, uint32_t start, float combined) {
  assert(g.state() == C::State::Complete);
  sample(d, g, -combined, false, start);
  sample(d, g, -combined, true, start + 1600);
  assert(d.takeTareRequest() == D::TareKind::AfterRemoval);
  d.onTareResult(true, start + 1650);
  sample(d, g, 0.0f, true, start + 1700);
  assert(d.state() == D::State::WaitVessel);
  assert(g.state() == C::State::Idle);
  assert(!g.cycleConsumed() && !g.logicRunning());
}

int main() {
  D d; configure(d);
  G g(d);
  // First profile starts with the original B1 default (0.25 g).
  autoReady(d, g, 100, 0);
  assert(g.requestStart(1901));
  near(g.latencyG(), 0.25f);
  sample(d, g, 8.26f, false, 2000); // 8.50 - 0.25 = 8.25
  assert(g.state() == C::State::Settling && !g.logicRunning());
  sample(d, g, 8.80f, true, 2350);
  g.tick(2500);
  assert(g.state() == C::State::Complete);
  near(g.latencyG(), 0.55f); // 0.25 + 8.80 - 8.50
  assert(!g.requestStart(2501));
  completedRemoval(d, g, 2700, 549.7f);

  // Same profile reloads learned 0.55 g, moving the stop threshold to 7.95 g.
  autoReady(d, g, 5000, 0);
  assert(g.requestStart(6801));
  near(g.latencyG(), 0.55f);
  sample(d, g, 7.94f, false, 6900);
  assert(g.state() == C::State::Grinding);
  sample(d, g, 7.96f, false, 7000);
  assert(g.state() == C::State::Settling);
  sample(d, g, 8.40f, true, 7350);
  g.tick(7500);
  assert(g.state() == C::State::Complete);
  near(g.latencyG(), 0.45f); // second valid learning step
  completedRemoval(d, g, 7700, 549.7f);

  // Different profile must NOT inherit profile 0's learned compensation.
  autoReady(d, g, 10000, 2);
  assert(g.requestStart(11801));
  near(g.latencyG(), 0.25f);
  sample(d, g, 16.60f, false, 11900);
  assert(g.state() == C::State::Grinding);
  sample(d, g, 16.76f, false, 12000);
  assert(g.state() == C::State::Settling);
  sample(d, g, 17.30f, true, 12350);
  g.tick(12500);
  assert(g.state() == C::State::Complete);
  near(g.latencyG(), 0.55f); // learning applies to profile 2 only
  completedRemoval(d, g, 12700, 536.2f);

  // A cancelled cycle never overwrites the previous learned value.
  autoReady(d, g, 15000, 0);
  assert(g.requestStart(16801));
  near(g.latencyG(), 0.45f);
  assert(g.requestCancel(16900));
  sample(d, g, 0.30f, true, 17200);
  g.tick(17400);
  assert(g.state() == C::State::Complete);
  assert(g.stopReason() == C::StopReason::UserCancelled);
  completedRemoval(d, g, 17700, 549.7f);
  autoReady(d, g, 20000, 0);
  assert(g.requestStart(21801));
  near(g.latencyG(), 0.45f);
  assert(g.requestSafetyAbort());
  assert(g.state() == C::State::Fault && !g.logicRunning());
  sample(d, g, -549.7f, false, 21900);
  sample(d, g, -549.7f, true, 23500);
  assert(d.takeTareRequest() == D::TareKind::AfterRemoval);
  d.onTareResult(true, 23550);
  sample(d, g, 0.0f, true, 23600);
  assert(d.state() == D::State::WaitVessel);
  assert(!g.requestStart(23601));
  assert(g.acknowledgeFault());
  assert(!g.logicRunning());
  autoReady(d, g, 25000, 0);
  assert(g.requestStart(26801));
  near(g.latencyG(), 0.45f); // earlier valid learned value remains intact

  // A reboot (new instance) does not import simulated weights from NVS.
  D rebootDetector; configure(rebootDetector);
  G rebootGate(rebootDetector);
  autoReady(rebootDetector, rebootGate, 100, 0);
  assert(rebootGate.requestStart(1901));
  near(rebootGate.latencyG(), 0.25f);
  assert(rebootGate.cycleConsumed()); // started cycle remains locked

  std::puts("grinder_latency_b4b1_test: PASS");
}
