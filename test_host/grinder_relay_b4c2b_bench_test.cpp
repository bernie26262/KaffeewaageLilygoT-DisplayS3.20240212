// B4c.2b: bench-only LOW output test; no real SSR, ESP or AC load attached.
// Compile with -DGRINDER_SSR_BENCH_ENABLE=1 and test_host/fakes/Arduino.h.
#include "grinder_relay.h"
#include "grinder_output_guard.h"
#include "pin_config.h"
#include <Arduino.h>

#include <cassert>
#include <cstdint>
#include <cstdio>

#if GRINDER_SSR_BENCH_ENABLE != 1
#error "Bench test requires GRINDER_SSR_BENCH_ENABLE=1"
#endif

namespace {
uint8_t lastValue = LOW;
unsigned lowWrites = 0;
unsigned highWrites = 0;
unsigned outputModes = 0;

void expectOff() { assert(lastValue == HIGH); }
void expectOn() { assert(lastValue == LOW); }

GrinderOutputGuard::Inputs ready() {
  GrinderOutputGuard::Inputs input;
  input.logicRequest = true;
  input.contextValid = true;
  input.sensorFresh = true;
  return input;
}
}  // namespace

void digitalWrite(uint8_t pin, uint8_t value) {
  assert(pin == PIN_GRINDER_SSR);
  assert(value == HIGH || value == LOW);
  lastValue = value;
  if (value == LOW) ++lowWrites;
  else ++highWrites;
}

void pinMode(uint8_t pin, uint8_t mode) {
  assert(pin == PIN_GRINDER_SSR && mode == OUTPUT);
  ++outputModes;
  expectOff(); // The output latch must have been set HIGH first.
}

int main() {
  grinderRelayInitOff();
  expectOff();
  assert(outputModes == 1);

  GrinderOutputGuard guard;
  grinderRelayApplyGuard(guard); // Default deny.
  expectOff();
  guard.observe(ready(), 10);
  assert(guard.permitted());
  grinderRelayApplyGuard(guard);
  expectOn();

  // An asynchronous WebUI abort must have priority over a stale, already
  // permitted guard result until full cycle removal and explicit rearm.
  grinderRelayForceOff();
  expectOff();
  grinderRelayApplyGuard(guard);
  expectOff();

  GrinderOutputGuard::Inputs idle;
  guard.observe(idle, 20);
  assert(guard.clearAfterUnprepared(true, true, false));
  grinderRelayRearmAfterUnprepared();
  grinderRelayApplyGuard(guard);
  expectOff();

  guard.observe(ready(), 30); // New deliberate cycle (logic layer simulated).
  grinderRelayApplyGuard(guard);
  expectOn();

  // OTA/reboot cannot be cleared by an ordinary unprepared rearm.
  grinderRelayForceOffUntilRestart();
  expectOff();
  grinderRelayRearmAfterUnprepared();
  grinderRelayApplyGuard(guard);
  expectOff();

  grinderRelayInitOff(); // Simulate a new boot: HIGH before OUTPUT.
  expectOff();
  grinderRelayApplyGuard(guard);
  expectOn();

  assert(lowWrites == 3);
  assert(highWrites > lowWrites);
  std::puts("grinder_relay_b4c2b_bench_test: PASS (bench LOW only with permit)");
}
