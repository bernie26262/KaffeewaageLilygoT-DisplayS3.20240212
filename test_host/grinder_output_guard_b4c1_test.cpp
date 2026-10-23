// B4c.1: Pure decision layer – no GPIO/Arduino dependency.
#include "grinder_output_guard.h"

#include <cassert>
#include <cstdio>
#include <cstdint>

using Guard = GrinderOutputGuard;
using Reason = Guard::Reason;

static Guard::Inputs readyInput(bool topup = false) {
  Guard::Inputs in;
  in.logicRequest = true;
  in.manualTopup = topup;
  in.contextValid = true;
  in.sensorFresh = true;
  return in;
}

static void stopRequest(Guard& guard, uint32_t nowMs) {
  Guard::Inputs idle;
  guard.observe(idle, nowMs);
}

int main() {
  {
    Guard guard;
    stopRequest(guard, 0);
    assert(!guard.permitted() && guard.reason() == Reason::NoRequest);

    guard.observe(readyInput(), 100);
    assert(guard.permitted() && !guard.faultLatched());
    guard.observe(readyInput(), 200);
    assert(guard.permitted());

    // A normal stop is not a fault; a later manual top-up has its own timer.
    stopRequest(guard, 250);
    assert(!guard.permitted() && !guard.faultLatched());
    guard.observe(readyInput(true), 300);
    assert(guard.permitted());
    stopRequest(guard, 350);
    assert(!guard.permitted());
  }
  {
    Guard guard;
    auto invalid = readyInput();
    invalid.contextValid = false;
    guard.observe(invalid, 100);
    assert(!guard.permitted() && guard.reason() == Reason::InvalidContext);
    guard.observe(readyInput(), 110);
    assert(!guard.permitted()); // latch: no automatic restart on recovery
    assert(!guard.clearAfterUnprepared(false, true, false));
    assert(!guard.clearAfterUnprepared(true, true, false));
    stopRequest(guard, 120);
    assert(!guard.clearAfterUnprepared(true, false, false));
    assert(guard.clearAfterUnprepared(true, true, false));
    guard.observe(readyInput(), 130);
    assert(guard.permitted());
  }
  {
    Guard guard;
    auto stale = readyInput();
    guard.observe(readyInput(), 10);
    stale.sensorFresh = false;
    guard.observe(stale, 20);
    assert(!guard.permitted() && guard.reason() == Reason::SensorStale);
  }
  {
    Guard guard;
    auto aborted = readyInput();
    aborted.webAbortPending = true;
    guard.observe(aborted, 10);
    assert(!guard.permitted() && guard.reason() == Reason::WebAbort);
  }
  {
    Guard guard;
    Guard::Inputs upload;
    upload.otaOrRebootPending = true;
    guard.observe(upload, 10);
    assert(!guard.permitted() && guard.reason() == Reason::OtaOrReboot);
    assert(!guard.clearAfterUnprepared(true, true, true));
    guard.observe(readyInput(), 20);
    assert(!guard.permitted() && guard.reason() == Reason::OtaOrReboot);
  }
  {
    Guard guard;
    guard.observe(readyInput(), 10);
    guard.observe(readyInput(), 511);
    assert(!guard.permitted() && guard.reason() == Reason::LoopGap);
    stopRequest(guard, 512);
    assert(guard.clearAfterUnprepared(true, true, false));
    stopRequest(guard, 100000); // Long idle pauses must not trigger a fault.
    guard.observe(readyInput(), 100001);
    assert(guard.permitted());
  }
  {
    Guard::Config config;
    config.maxLoopGapMs = 40000;
    Guard guard(config);
    guard.observe(readyInput(), 100);
    guard.observe(readyInput(), 30099);
    assert(guard.permitted());
    guard.observe(readyInput(), 30100);
    assert(!guard.permitted() && guard.reason() == Reason::RuntimeLimit);
  }
  {
    Guard::Config config;
    config.maxLoopGapMs = 5000;
    Guard guard(config);
    guard.observe(readyInput(true), 100);
    guard.observe(readyInput(true), 3099);
    assert(guard.permitted());
    guard.observe(readyInput(true), 3100);
    assert(!guard.permitted() && guard.reason() == Reason::RuntimeLimit);
  }
  {
    Guard guard;
    guard.observe(readyInput(), 10);
    guard.observe(readyInput(true), 20);
    assert(!guard.permitted() &&
           guard.reason() == Reason::UnexpectedModeChange);
  }
  {
    Guard guard;
    guard.observe(readyInput(), UINT32_MAX - 100);
    guard.observe(readyInput(), 50);
    assert(guard.permitted()); // millis() wraps correctly.
  }

  std::puts("grinder_output_guard_b4c1_test: PASS (NO GPIO access)");
}
