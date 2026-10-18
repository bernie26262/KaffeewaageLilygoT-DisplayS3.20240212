// B4a.2: Mirror the passive detector->gate order from main.cpp.
// The test intentionally never calls requestStart() and has no relay API.
#include "grinder_cycle_gate.h"

#include <cassert>
#include <cstdio>

using Detector = GrinderProfileDetector;
using Gate = GrinderCycleGate;
using State = GrinderController::State;

static void sample(Detector& d, Gate& gate, float weight, bool stable,
                   uint32_t ms) {
  d.onWeightSample(weight, stable, ms);
  d.tick(ms);
  gate.onWeightSample(weight, stable, ms);
  gate.tick(ms);
  // Passive wiring must never produce a logical motor request by itself.
  assert(gate.state() == State::Idle);
  assert(!gate.logicRunning());
  assert(!gate.cycleConsumed());
}

int main() {
  {
    Detector d;
    assert(d.setProfile(0, {449.6f, 100.1f, 8.5f}));
    Gate gate(d);
    sample(d, gate, 549.7f, false, 100);
    sample(d, gate, 549.7f, true, 1700);
    assert(d.takeTareRequest() == Detector::TareKind::AutoCombination);
    d.onTareResult(true, 1800);
    sample(d, gate, 0.0f, true, 1900);
    assert(d.state() == Detector::State::ReadyAuto);
    // Even a simulated dose crossing the target cannot start the controller.
    sample(d, gate, 9.0f, false, 2000);
    gate.tick(3500);  // no active process: timeout must not cause a start
    assert(gate.state() == State::Idle && !gate.logicRunning());
    d.setPaused(true);
    gate.tick(3600);  // pausing the detector must not cause a start
    assert(gate.state() == State::Idle && !gate.logicRunning());
  }
  {
    Detector d;
    assert(d.setProfile(2, {475.5f, 60.7f, 17.0f}));
    assert(d.setAutoDetect(false));
    assert(d.selectManualProfile(2));
    Gate gate(d);
    sample(d, gate, 475.5f, true, 100);
    assert(d.requestManualTare(101));
    assert(d.takeTareRequest() == Detector::TareKind::Manual);
    d.onTareResult(true, 200);
    sample(d, gate, 0.0f, true, 300);
    assert(d.state() == Detector::State::ReadyManual);
    sample(d, gate, 17.0f, false, 400);
    assert(gate.state() == State::Idle);
  }
  {
    Detector d;
    assert(d.setProfile(0, {449.6f, 100.1f, 8.5f}));
    Gate gate(d);
    sample(d, gate, 449.6f, true, 100);
    sample(d, gate, 449.6f, true, 1700);
    assert(d.state() == Detector::State::WaitFunnel);
    assert(d.requestSkipFunnel(1800));
    assert(d.takeTareRequest() == Detector::TareKind::AutoNoFunnel);
    d.onTareResult(true, 1900);
    sample(d, gate, 0.0f, true, 2000);
    assert(d.state() == Detector::State::ReadyNoFunnel);
    sample(d, gate, 8.5f, false, 2100);
    assert(gate.state() == State::Idle);
  }
  std::puts("grinder_passive_wiring_b4a2_test: PASS");
}
