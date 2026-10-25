#include <Arduino.h>

#include "grinder_relay.h"
#include "grinder_output_guard.h"
#include "pin_config.h"

// B4c.2b: the normal firmware is still strictly OFF-only. A separate,
// EXPLICIT bench flag is required to exercise GPIO LOW, and that flag is
// only permitted with the SSR's 230-V load feed physically disconnected.
#ifndef GRINDER_SSR_PHYSICAL_ENABLE
#define GRINDER_SSR_PHYSICAL_ENABLE 0
#endif
#if GRINDER_SSR_PHYSICAL_ENABLE != 0
#error "B4c.2b: production SSR release remains forbidden until independent fail-OFF hardware is reviewed"
#endif
#ifndef GRINDER_SSR_BENCH_ENABLE
#define GRINDER_SSR_BENCH_ENABLE 0
#endif
#if GRINDER_SSR_BENCH_ENABLE != 0 && GRINDER_SSR_BENCH_ENABLE != 1
#error "GRINDER_SSR_BENCH_ENABLE must be 0 (OFF-only) or 1 (230-V load disconnected)"
#endif

namespace {
// AsyncWebServer callbacks can execute on another ESP32 core while loop()
// evaluates the output guard. Protect the abort latch AND GPIO write with
// one cross-core critical section, preventing stale LOW writes after abort.
#if defined(ARDUINO_ARCH_ESP32)
portMUX_TYPE relayMux = portMUX_INITIALIZER_UNLOCKED;
#endif
bool abortLatched = false;
bool restartLatched = false;

struct RelayLock {
  RelayLock() {
#if defined(ARDUINO_ARCH_ESP32)
    portENTER_CRITICAL(&relayMux);
#endif
  }
  ~RelayLock() {
#if defined(ARDUINO_ARCH_ESP32)
    portEXIT_CRITICAL(&relayMux);
#endif
  }
};
} // namespace

void grinderRelayInitOff()
{
  RelayLock lock;
  abortLatched = false;
  restartLatched = false;
  // Old Grind-by-Weight hardware: LOW=ON, HIGH=OFF.
  // Set output latch HIGH before switching the pin to OUTPUT, avoiding
  // an avoidable LOW pulse during GPIO configuration.
  digitalWrite(PIN_GRINDER_SSR, HIGH);
  pinMode(PIN_GRINDER_SSR, OUTPUT);
  digitalWrite(PIN_GRINDER_SSR, HIGH);
}

void grinderRelayForceOff()
{
  RelayLock lock;
  abortLatched = true;
  digitalWrite(PIN_GRINDER_SSR, HIGH);
}

void grinderRelayForceOffUntilRestart()
{
  RelayLock lock;
  restartLatched = true;
  abortLatched = true;
  digitalWrite(PIN_GRINDER_SSR, HIGH);
}

void grinderRelayRearmAfterUnprepared()
{
  RelayLock lock;
  // Never rearm while a low-going output is still possible in this call.
  digitalWrite(PIN_GRINDER_SSR, HIGH);
  if (!restartLatched) abortLatched = false;
}

void grinderRelayApplyGuard(const GrinderOutputGuard& guard)
{
  RelayLock lock;
#if GRINDER_SSR_BENCH_ENABLE == 1
  // Test-only output: never upload this build with the AC load connected.
  const bool allowBenchPulse = guard.permitted() && !abortLatched;
  digitalWrite(PIN_GRINDER_SSR, allowBenchPulse ? LOW : HIGH);
#else
  // The shipping/default build has no LOW write path at all.
  (void)guard.permitted();
  digitalWrite(PIN_GRINDER_SSR, HIGH);
#endif
}
