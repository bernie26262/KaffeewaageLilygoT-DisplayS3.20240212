// B4c.2a: HOST TEST of the physical GPIO boundary. No actual SSR attached.
#include "grinder_relay.h"
#include "grinder_output_guard.h"
#include "pin_config.h"
#include <Arduino.h>

#include <cassert>
#include <cstdint>
#include <cstdio>

namespace {
enum class EventKind : uint8_t { Write, Mode };
struct Event {
  EventKind kind;
  uint8_t pin;
  uint8_t value;
};
Event events[20];
unsigned eventCount = 0;

void record(EventKind kind, uint8_t pin, uint8_t value) {
  assert(eventCount < sizeof(events) / sizeof(events[0]));
  assert(pin == PIN_GRINDER_SSR);
  // Even the future ON candidate must NEVER cause a GPIO LOW in B4c.2a.
  if (kind == EventKind::Write) assert(value == HIGH);
  events[eventCount++] = {kind, pin, value};
}

GrinderOutputGuard::Inputs readyRequest() {
  GrinderOutputGuard::Inputs result;
  result.logicRequest = true;
  result.contextValid = true;
  result.sensorFresh = true;
  return result;
}
} // namespace

void digitalWrite(uint8_t pin, uint8_t level) {
  record(EventKind::Write, pin, level);
}
void pinMode(uint8_t pin, uint8_t mode) {
  assert(mode == OUTPUT);
  record(EventKind::Mode, pin, mode);
}

int main() {
  grinderRelayInitOff();
  assert(eventCount == 3);
  // Latch HIGH first, OUTPUT second, verify HIGH once more.
  assert(events[0].kind == EventKind::Write && events[0].value == HIGH);
  assert(events[1].kind == EventKind::Mode);
  assert(events[2].kind == EventKind::Write && events[2].value == HIGH);

  GrinderOutputGuard guard;
  grinderRelayApplyGuard(guard); // No logical request.
  guard.observe(readyRequest(), 10);
  assert(guard.permitted());
  grinderRelayApplyGuard(guard); // Even a permitted request MUST stay OFF.

  auto otaAbort = readyRequest();
  otaAbort.otaOrRebootPending = true;
  guard.observe(otaAbort, 20);
  assert(!guard.permitted());
  grinderRelayForceOff(); // Async abort path.
  grinderRelayApplyGuard(guard);

  // Reset and later manual top-up also do not remove the physical lock.
  GrinderOutputGuard topupGuard;
  auto topup = readyRequest();
  topup.manualTopup = true;
  topupGuard.observe(topup, 100);
  assert(topupGuard.permitted());
  grinderRelayApplyGuard(topupGuard);

  assert(eventCount == 8);
  std::puts("grinder_relay_b4c2a_test: PASS (GPIO16 never LOW)");
}
