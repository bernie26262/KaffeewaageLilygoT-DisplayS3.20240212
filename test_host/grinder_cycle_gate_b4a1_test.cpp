// Phase B4a.1: real B1 + B3 modules, but no ESP32 hardware or relay driver.
#include "grinder_cycle_gate.h"

#include <cassert>
#include <cstdio>
#include <limits>

using D = GrinderProfileDetector;
using G = GrinderCycleGate;
using CS = GrinderController::State;
using R = GrinderController::StopReason;

static void configure(D& d) {
  assert(d.setProfile(0, {449.6f, 100.1f, 8.5f}));
  assert(d.setProfile(2, {475.5f, 60.7f, 17.0f}));
}

static void sample(D& d, G& g, float grams, bool stable, uint32_t ms) {
  d.onWeightSample(grams, stable, ms);
  g.onWeightSample(grams, stable, ms);
  d.tick(ms);
  g.tick(ms);
}

static void readyAuto(D& d, G& g) {
  sample(d, g, 549.7f, true, 100);
  sample(d, g, 549.7f, true, 1700);
  assert(d.takeTareRequest() == D::TareKind::AutoCombination);
  d.onTareResult(true, 1800);
  sample(d, g, 0.05f, true, 1900);
  assert(d.state() == D::State::ReadyAuto);
}

static void readySkip(D& d, G& g) {
  sample(d, g, 449.6f, true, 100);
  sample(d, g, 449.6f, true, 1700);
  assert(d.state() == D::State::WaitFunnel);
  assert(d.requestSkipFunnel(1750));
  assert(d.takeTareRequest() == D::TareKind::AutoNoFunnel);
  d.onTareResult(true, 1800);
  sample(d, g, 0.04f, true, 1900);
  assert(d.state() == D::State::ReadyNoFunnel);
}

static void readyManual(D& d, G& g) {
  assert(d.setAutoDetect(false));
  assert(d.selectManualProfile(2));
  sample(d, g, 475.5f, true, 100);
  assert(d.requestManualTare(100));
  assert(d.takeTareRequest() == D::TareKind::Manual);
  d.onTareResult(true, 200);
  sample(d, g, 0.0f, true, 300);
  assert(d.state() == D::State::ReadyManual);
}

static void finish(D& d, G& g, uint32_t now, float target) {
  sample(d, g, target - 0.2f, false, now);
  assert(g.state() == CS::Settling && !g.logicRunning());
  sample(d, g, target, true, now + 100);
  d.tick(now + 500);
  g.tick(now + 500);
  assert(g.state() == CS::Complete && !g.logicRunning());
  assert(g.cycleConsumed());
  assert(!g.requestStart(now + 501));
}

int main() {
  // No preparation: even valid zero and explicit start must do nothing.
  {
    D d; configure(d); G g(d);
    sample(d, g, 0.0f, true, 10);
    assert(!g.requestStart(11) && !g.logicRunning());
  }
  // Automatic combination: no old/stale/nonzero/unstable start.
  {
    D d; configure(d); G g(d); readyAuto(d, g);
    assert(!g.requestStart(3001)); // old measurement
    sample(d, g, 0.0f, false, 3010);
    assert(!g.requestStart(3010)); // unstable
    sample(d, g, 2.0f, true, 3020);
    assert(!g.requestStart(3020)); // already dosing
    sample(d, g, 0.0f, true, 3030);
    assert(g.requestStart(3040) && g.state() == CS::Grinding);
    assert(!g.requestStart(3041)); // no double start
    assert(g.targetG() == 8.5f);
    finish(d, g, 3100, 8.5f);
    // CheckingRemoval is NOT a new cycle; even if it bounces back.
    sample(d, g, -449.6f, false, 4000);
    assert(d.state() == D::State::CheckingRemoval);
    assert(!g.requestStart(4001));
    sample(d, g, 0.0f, true, 4100);
    assert(d.state() == D::State::ReadyAuto);
    assert(!g.requestStart(4101));
    // Only a confirmed removal + re-zero resets the consumed cycle.
    sample(d, g, -449.6f, false, 4200);
    sample(d, g, -449.6f, true, 5800);
    assert(d.takeTareRequest() == D::TareKind::AfterRemoval);
    d.onTareResult(true, 5850);
    sample(d, g, 0.0f, true, 5900);
    assert(d.state() == D::State::WaitVessel);
    assert(!g.cycleConsumed());
    assert(!g.requestStart(5901));
    // A genuinely NEW combination must be prepared before another start.
    sample(d, g, 549.7f, true, 6000);
    sample(d, g, 549.7f, true, 7600);
    assert(d.takeTareRequest() == D::TareKind::AutoCombination);
    d.onTareResult(true, 7700);
    sample(d, g, 0.0f, true, 7800);
    assert(d.state() == D::State::ReadyAuto);
    assert(g.requestStart(7801) && g.logicRunning());
  }
  // Before the confirmed post-tare zero, start remains forbidden.
  {
    D d; configure(d); G g(d);
    sample(d, g, 449.6f, true, 100);
    sample(d, g, 449.6f, true, 1700);
    assert(d.state() == D::State::WaitFunnel);
    assert(!g.requestStart(1701));
    assert(d.requestSkipFunnel(1750));
    assert(d.takeTareRequest() == D::TareKind::AutoNoFunnel);
    d.onTareResult(true, 1800);
    assert(d.state() == D::State::WaitPostTare);
    assert(!g.requestStart(1801));
  }
  // Trichter skip: same gate as automatic combination.
  {
    D d; configure(d); G g(d); readySkip(d, g);
    assert(g.requestStart(1901));
    finish(d, g, 2000, 8.5f);
  }
  // AutoDetect OFF: manual readiness works, wrong target/cancel locked.
  {
    D d; configure(d); G g(d); readyManual(d, g);
    assert(g.requestStart(301) && g.targetG() == 17.0f);
    assert(g.requestCancel(400));
    assert(g.stopReason() == R::UserCancelled && !g.logicRunning());
    g.tick(900);
    assert(g.state() == CS::Complete && !g.requestStart(901));
  }
  // Immediate fault if a vessel is lifted during a run; no silent recovery.
  {
    D d; configure(d); G g(d); readyManual(d, g);
    assert(g.requestStart(301));
    sample(d, g, -475.5f, false, 400);
    assert(d.state() == D::State::CheckingRemoval);
    assert(g.state() == CS::Fault && !g.logicRunning());
    assert(g.stopReason() == R::EmergencyStop);
    assert(!g.acknowledgeFault() && !g.requestStart(401));
    sample(d, g, -475.5f, true, 2000);
    assert(d.takeTareRequest() == D::TareKind::AfterRemoval);
    d.onTareResult(true, 2010);
    sample(d, g, 0.0f, true, 2100);
    assert(d.state() == D::State::WaitVessel);
    assert(!g.requestStart(2101)); // explicit acknowledgement needed
    assert(g.acknowledgeFault() && !g.cycleConsumed());
    assert(!g.requestStart(2102)); // new readiness still needed
  }
  // A profile-mode change during a run is a latched error, not a new start.
  {
    D d; configure(d); G g(d); readyAuto(d, g);
    assert(g.requestStart(1901));
    assert(d.setAutoDetect(false)); // detector explicitly clears readiness
    g.tick(1902);
    assert(g.state() == CS::Fault && g.stopReason() == R::EmergencyStop);
    assert(!g.logicRunning() && !g.requestStart(1903));
  }
  // Lost readiness from pause/invalid sample, and B1 stale sample timeout.
  {
    D d; configure(d); G g(d); readyAuto(d, g);
    d.setPaused(true);
    assert(!g.requestStart(1901));
    d.setPaused(false);
    assert(g.requestStart(1902));
    d.setPaused(true);
    g.tick(1903);
    assert(g.state() == CS::Fault && !g.logicRunning());
  }
  {
    D d; configure(d); G g(d); readyAuto(d, g);
    assert(g.requestStart(1901));
    sample(d, g, std::numeric_limits<float>::quiet_NaN(), false, 1950);
    assert(g.state() == CS::Fault && !g.logicRunning());
  }
  {
    D d; configure(d); G g(d); readyAuto(d, g);
    assert(g.requestStart(1901));
    d.tick(3001);
    g.tick(3001);
    assert(g.state() == CS::Fault && !g.logicRunning());
  }
  // B1 runtime limit remains effective even with regular fresh readings.
  {
    D d; configure(d); G g(d); readyAuto(d, g);
    assert(g.requestStart(1901));
    for (uint32_t t = 2000; t < 32500; t += 500) {
      sample(d, g, 1.0f, false, t);
      if (g.state() == CS::Fault) break;
    }
    assert(g.state() == CS::Fault);
    assert(g.stopReason() == R::RuntimeExceeded);
    assert(!g.logicRunning());
  }
  std::puts("grinder_cycle_gate_b4a1_test: PASS");
}
