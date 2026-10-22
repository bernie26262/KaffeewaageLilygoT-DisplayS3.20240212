// B4a.3b: real encoder classifier + B1/B3 gate; no ESP32 and NO relay code.
#include "grinder_cycle_gate.h"
#include "grinder_encoder_button.h"
#include <cassert>
#include <cstdio>
#include <limits>

using D = GrinderProfileDetector;
using G = GrinderCycleGate;
using S = GrinderController::State;
using E = GrinderEncoderButton::Event;

static void feed(D& d, G& g, float weight, bool stable, uint32_t time) {
  d.onWeightSample(weight, stable, time);
  d.tick(time);
  g.onWeightSample(weight, stable, time);
  g.tick(time);
}
static void prepareAuto(D& d, G& g) {
  assert(d.setProfile(0, {449.6f, 100.1f, 8.5f}));
  feed(d, g, 549.7f, true, 100);
  feed(d, g, 549.7f, true, 1700);
  assert(d.takeTareRequest() == D::TareKind::AutoCombination);
  d.onTareResult(true, 1800);
  feed(d, g, 0.02f, true, 1900);
  assert(d.state() == D::State::ReadyAuto);
}
static void prepareSkip(D& d, G& g) {
  assert(d.setProfile(0, {449.6f, 100.1f, 8.5f}));
  feed(d, g, 449.6f, true, 100);
  feed(d, g, 449.6f, true, 1700);
  assert(d.requestSkipFunnel(1750));
  assert(d.takeTareRequest() == D::TareKind::AutoNoFunnel);
  d.onTareResult(true, 1800);
  feed(d, g, 0.02f, true, 1900);
  assert(d.state() == D::State::ReadyNoFunnel);
}
static void prepareManual(D& d, G& g) {
  assert(d.setProfile(0, {449.6f, 100.1f, 8.5f}));
  assert(d.setAutoDetect(false));
  assert(d.selectManualProfile(0));
  feed(d, g, 449.6f, true, 100);
  assert(d.requestManualTare(100));
  assert(d.takeTareRequest() == D::TareKind::Manual);
  d.onTareResult(true, 200);
  feed(d, g, 0.02f, true, 300);
  assert(d.state() == D::State::ReadyManual);
}

int main() {
  // A long press is reported at the threshold and must NOT emit a menu click
  // on release, including when a start is rejected for missing readiness.
  {
    GrinderEncoderButton b;
    b.reset(false, 0);
    assert(b.update(true, true, 100) == E::None);
    assert(b.update(true, true, 126) == E::None);
    assert(b.update(true, true, 1126) == E::LongPress);
    D d; G g(d);
    assert(!g.requestStart(1126)); // valid gesture but no vessel
    assert(b.update(false, true, 1200) == E::None);
    assert(b.update(false, true, 1230) == E::None);
  }
  // All three preparations support the same manual start and automatic stop.
  for (int mode = 0; mode < 3; ++mode) {
    D d; G g(d);
    if (mode == 0) prepareAuto(d, g);
    else if (mode == 1) prepareSkip(d, g);
    else prepareManual(d, g);
    const uint32_t start = mode == 2 ? 301 : 1901;
    assert(g.requestStart(start));
    assert(g.state() == S::Grinding && g.logicRunning());
    assert(!g.requestStart(start + 1));
    feed(d, g, 4.2f, false, start + 100);
    assert(g.state() == S::Grinding);
    feed(d, g, 8.26f, false, start + 200);
    assert(g.state() == S::Settling && !g.logicRunning());
    feed(d, g, 8.5f, false, start + 300);
    g.tick(start + 700);
    assert(g.state() == S::Complete && !g.logicRunning());
    assert(!g.requestStart(start + 701));
  }
  // Short click causes logical cancel; no second launch or physical relay.
  {
    D d; G g(d); prepareAuto(d, g);
    assert(g.requestStart(1901));
    assert(g.requestCancel(2000));
    assert(g.state() == S::Settling && !g.logicRunning());
    assert(!g.requestStart(2001));
  }
  // Unexpected negative weight must fault immediately, even when the
  // detector remains ready for the current sample (one-cycle delay).
  {
    D d; G g(d); prepareAuto(d, g);
    assert(g.requestStart(1901));
    feed(d, g, -5.0f, false, 1950);
    assert(g.state() == S::Fault && !g.logicRunning());
    assert(!g.requestStart(1951));
  }
  // A minor negative drift is not a lift, and it must not fault.
  {
    D d; G g(d); prepareAuto(d, g);
    assert(g.requestStart(1901));
    feed(d, g, -1.4f, false, 1950);
    assert(g.state() == S::Grinding && g.logicRunning());
  }
  // A sensor NaN / unexpected page/target change is always an emergency stop.
  {
    D d; G g(d); prepareAuto(d, g);
    assert(g.requestStart(1901));
    feed(d, g, std::numeric_limits<float>::quiet_NaN(), false, 1950);
    assert(g.state() == S::Fault && !g.logicRunning());
    assert(!g.acknowledgeFault()); // must first completely remove vessel
  }
  {
    D d; G g(d); prepareAuto(d, g);
    assert(g.requestStart(1901));
    assert(g.requestSafetyAbort()); // context change in main.cpp
    assert(g.state() == S::Fault && !g.logicRunning());
    assert(!g.requestSafetyAbort());
  }
  // The physical relay driver is not linked into this test by design.
  std::puts("grinder_simulation_b4a3b_test: PASS");
}
