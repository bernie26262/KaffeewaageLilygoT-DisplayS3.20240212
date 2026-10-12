// Host-only behavioral checks; not compiled into ESP32 firmware.
#include "../src/grinder_controller.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using State = GrinderController::State;
using Reason = GrinderController::StopReason;

int main() {
  {
    GrinderController g;
    assert(!g.relayRequested());
    g.onButtonPress(0); // no valid target or measurement
    assert(!g.relayRequested());
    assert(g.setTargetG(16.0f));
    g.onWeightSample(0.0f, 10);
    g.onButtonPress(20);
    assert(g.state() == State::Grinding && g.relayRequested());
    g.onWeightSample(15.7f, 100);
    assert(g.state() == State::Grinding);
    g.onWeightSample(15.76f, 120); // 16 - 0.25
    assert(!g.relayRequested() && g.state() == State::Settling);
    g.onWeightSample(16.15f, 500);
    g.tick(570); // 450 ms after shutoff
    assert(g.state() == State::Complete);
    assert(std::fabs(g.latencyG() - 0.40f) < 0.001f);
    assert(!g.relayRequested());
  }
  {
    GrinderController g;
    assert(g.setTargetG(16.0f));
    g.onWeightSample(15.8f, 1);
    g.onButtonPress(2);
    assert(g.state() == State::ManualTopup);
    g.onWeightSample(15.9f, 10); // still below target
    g.onButtonRelease(11);
    assert(g.state() == State::Settling && !g.relayRequested());
    assert(g.stopReason() == Reason::ManualReleased);
  }
  {
    GrinderController g;
    assert(g.setTargetG(16.0f));
    g.onWeightSample(0.0f, 0);
    g.onButtonPress(1);
    g.onButtonPress(2); // second press cancels grinding
    assert(!g.relayRequested() && g.stopReason() == Reason::UserCancelled);
  }
  {
    GrinderController g;
    assert(g.setTargetG(16.0f));
    g.onWeightSample(0.0f, 1);
    g.onButtonPress(2);
    g.tick(1002); // last scale reading expired
    assert(g.state() == State::Fault && !g.relayRequested());
    assert(g.stopReason() == Reason::StaleWeight);
    g.onButtonPress(1003); // fault never auto-clears
    assert(!g.relayRequested());
    g.clearFault();
    g.onButtonPress(1004); // needs a new measurement
    assert(!g.relayRequested());
  }
  {
    GrinderController g;
    assert(g.setTargetG(16.0f));
    g.onWeightSample(0.0f, 1);
    g.onButtonPress(2);
    g.onWeightSample(std::numeric_limits<float>::quiet_NaN(), 3);
    assert(!g.relayRequested() && g.stopReason() == Reason::InvalidWeight);
  }
  {
    GrinderController::Config cfg;
    cfg.maxSampleAgeMs = 40000; // isolate runtime limit from sensor timeout
    GrinderController g(cfg);
    assert(g.setTargetG(16.0f));
    g.onWeightSample(0.0f, 1);
    g.onButtonPress(2);
    g.onWeightSample(1.0f, 30001);
    g.tick(30002);
    assert(!g.relayRequested() && g.stopReason() == Reason::RuntimeExceeded);
  }
  {
    GrinderController g;
    assert(g.setTargetG(16.0f));
    g.onWeightSample(0.0f, 1);
    g.onButtonPress(2);
    g.emergencyStop();
    assert(!g.relayRequested() && g.state() == State::Fault);
    g.reset();
    assert(!g.relayRequested());
  }
  std::puts("grinder_controller_test: PASS");
  return 0;
}
