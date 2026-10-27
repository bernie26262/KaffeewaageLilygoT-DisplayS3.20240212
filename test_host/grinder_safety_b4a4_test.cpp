// B4a.4: safety regressions for stale/invalid HX711 data and WebUI commands.
// Pure PC tests: the ESP32, USB connection, and relay driver are not linked.
#include "grinder_controller.h"
#include "grinder_cycle_gate.h"
#include "grinder_web_safety.h"

#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <limits>

using C = GrinderController;
using S = C::State;
using R = C::StopReason;
using W = GrinderWebCommandPolicy;

static C stoppedController() {
  C c;
  assert(c.setTargetG(8.4f));
  c.onWeightSample(0.0f, 100);
  c.onButtonPress(101);
  assert(c.state() == S::Grinding && c.relayRequested());
  c.onWeightSample(8.16f, 200); // 8.40 - 0.25
  assert(c.state() == S::Settling && !c.relayRequested());
  return c;
}

int main() {
  // B4d.1: No WebSocket motor-start spelling may be accepted by the
  // remote-command gate, including suffixed/replayed variants.
  for (const char* command : {"grind_start", "grinder_start", "gbw_start",
                              "start_grind", "start_grinder", "start_gbw",
                              "grind_topup", "grinder_topup", "gbw_topup",
                              "motor_on", "grinder_on", "gbw_on",
                              "gbw_start_1", "grinder_topup_start"}) {
    assert(grinderWebIsMotorStartCommand(command));
  }
  for (const char* command : {"tare", "save_dose", "mode_settings",
                              "gbw_measure_begin_0", "set_selected_siebtraeger_weight_8.4",
                              "grind_status", "grinder_startled"}) {
    assert(!grinderWebIsMotorStartCommand(command));
  }
  assert(!grinderWebIsMotorStartCommand(nullptr));

  // Previously tick() could mark a stopped run Complete despite >1s of
  // missing HX711 samples. It must now latch a stale-sample fault.
  {
    C c = stoppedController();
    c.tick(1201);
    assert(c.state() == S::Fault);
    assert(c.stopReason() == R::StaleWeight && !c.relayRequested());
    c.onWeightSample(8.4f, 1202);
    assert(c.state() == S::Fault); // a late sample cannot clear the fault
  }
  // A delayed sample delivered BEFORE tick must not reset the stale clock.
  {
    C c = stoppedController();
    c.onWeightSample(8.4f, 1201);
    assert(c.state() == S::Fault && c.stopReason() == R::StaleWeight);
  }
  // Non-finite weight is unsafe even after the logical motor stop.
  {
    C c = stoppedController();
    c.onWeightSample(std::numeric_limits<float>::quiet_NaN(), 250);
    assert(c.state() == S::Fault && c.stopReason() == R::InvalidWeight);
    assert(!c.relayRequested());
  }
  // A normal settling cycle with fresh samples is unaffected.
  {
    C c = stoppedController();
    c.onWeightSample(8.4f, 400);
    c.tick(650);
    assert(c.state() == S::Complete && !c.relayRequested());
    assert(c.stopReason() == R::TargetReached);
  }
  // Integration gate must propagate stale Settling telemetry as a latched
  // fault, not silently reset to Idle or allow another start.
  {
    GrinderProfileDetector d;
    assert(d.setProfile(0, {449.6f, 100.1f, 8.5f}));
    GrinderCycleGate g(d);
    d.onWeightSample(549.7f, true, 100);
    d.onWeightSample(549.7f, true, 1700);
    assert(d.takeTareRequest() == GrinderProfileDetector::TareKind::AutoCombination);
    d.onTareResult(true, 1800);
    d.onWeightSample(0.0f, true, 1900);
    d.tick(1900);
    g.onWeightSample(0.0f, true, 1900);
    assert(g.requestStart(1901));
    d.onWeightSample(8.30f, false, 2000);
    d.tick(2000);
    g.onWeightSample(8.30f, false, 2000);
    assert(g.state() == S::Settling);
    g.tick(3001); // no fresh HX711 data, even though settling time elapsed
    assert(g.state() == S::Fault && !g.logicRunning());
    assert(g.stopReason() == R::StaleWeight);
    assert(!g.requestStart(3002));
  }
  // WebUI commands cannot act during active grinding, manual topup or
  // settling, including a possible tare, mode switch or dose save.
  for (S activeState : {S::Grinding, S::ManualTopup, S::Settling}) {
    assert(grinderWebCommandPolicy(activeState, true, false) == W::AbortAndReject);
    assert(grinderWebCommandPolicy(activeState, true, true) == W::AbortAndReject);
  }
  // After a cycle, only simulated-dose saves remain forbidden. Telemetry
  // is not processed by this policy (it is not a WebUI command).
  for (S inactiveState : {S::Complete, S::Fault}) {
    assert(grinderWebCommandPolicy(inactiveState, true, true) == W::Reject);
    assert(grinderWebCommandPolicy(inactiveState, true, false) == W::Allow);
  }
  assert(grinderWebCommandPolicy(S::Idle, false, true) == W::Allow);
  assert(grinderWebCommandPolicy(S::Idle, false, false) == W::Allow);
  std::puts("grinder_safety_b4a4_test: PASS");
}
