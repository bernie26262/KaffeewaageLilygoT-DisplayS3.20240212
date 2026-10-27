#include <Arduino.h>

#include "grinder_relay.h"
#include "grinder_output_guard.h"
#include "pin_config.h"

#if defined(ARDUINO_ARCH_ESP32) && GRINDER_SSR_BENCH_ENABLE == 1
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

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
bool relayOutputLow = false;
bool heartbeatFaultLatched = false;
bool supervisorReady = false;
uint32_t lastMainLoopHeartbeatMs = 0;
constexpr uint32_t kHeartbeatTimeoutMs = 750;

#if defined(ARDUINO_ARCH_ESP32) && GRINDER_SSR_BENCH_ENABLE == 1
TaskHandle_t supervisorTask = nullptr;
#endif

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

#if defined(ARDUINO_ARCH_ESP32) && GRINDER_SSR_BENCH_ENABLE == 1
void relaySupervisorTask(void*) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(25));
    grinderRelayWatchdogPoll(millis());
  }
}
#endif
} // namespace

void grinderRelayInitOff()
{
  {
    RelayLock lock;
    abortLatched = false;
    restartLatched = false;
    relayOutputLow = false;
    heartbeatFaultLatched = false;
    supervisorReady = false;
    lastMainLoopHeartbeatMs = 0;
    // Old Grind-by-Weight hardware: LOW=ON, HIGH=OFF.
    // Set output latch HIGH before switching the pin to OUTPUT, avoiding
    // an avoidable LOW pulse during GPIO configuration.
    digitalWrite(PIN_GRINDER_SSR, HIGH);
    pinMode(PIN_GRINDER_SSR, OUTPUT);
    digitalWrite(PIN_GRINDER_SSR, HIGH);
  }

#if GRINDER_SSR_BENCH_ENABLE == 1
  // The BENCH output is fail-closed unless a separate supervisor task can
  // periodically check its heartbeat. Do not create tasks inside a critical
  // section (task creation can cause an immediate context switch).
#if defined(ARDUINO_ARCH_ESP32)
  if (supervisorTask == nullptr) {
    const BaseType_t created = xTaskCreatePinnedToCore(
        relaySupervisorTask, "grinderSsrWatch", 2048, nullptr, 5,
        &supervisorTask, 0);
    RelayLock lock;
    supervisorReady = created == pdPASS;
    if (!supervisorReady) abortLatched = true;
  } else {
    RelayLock lock;
    supervisorReady = true;
  }
#else
  // Host tests invoke grinderRelayWatchdogPoll() explicitly; they do NOT
  // simulate FreeRTOS scheduling or prove bounded real-world latency.
  supervisorReady = true;
#endif
#endif
}

void grinderRelayForceOff()
{
  RelayLock lock;
  abortLatched = true;
  relayOutputLow = false;
  digitalWrite(PIN_GRINDER_SSR, HIGH);
}

void grinderRelayForceOffUntilRestart()
{
  RelayLock lock;
  restartLatched = true;
  abortLatched = true;
  relayOutputLow = false;
  digitalWrite(PIN_GRINDER_SSR, HIGH);
}

void grinderRelayRearmAfterUnprepared()
{
  RelayLock lock;
  // Never rearm while a low-going output is still possible in this call.
  digitalWrite(PIN_GRINDER_SSR, HIGH);
  relayOutputLow = false;
  if (!restartLatched) {
    abortLatched = false;
    heartbeatFaultLatched = false;
  }
}

void grinderRelayApplyGuard(const GrinderOutputGuard& guard)
{
  RelayLock lock;
#if GRINDER_SSR_BENCH_ENABLE == 1
  // Test-only output: never upload this build with the AC load connected.
  const bool allowBenchPulse = guard.permitted() && !abortLatched &&
                               supervisorReady;
  if (allowBenchPulse) lastMainLoopHeartbeatMs = millis();
  relayOutputLow = allowBenchPulse;
  digitalWrite(PIN_GRINDER_SSR, allowBenchPulse ? LOW : HIGH);
#else
  // The shipping/default build has no LOW write path at all.
  (void)guard.permitted();
  relayOutputLow = false;
  digitalWrite(PIN_GRINDER_SSR, HIGH);
#endif
}

void grinderRelayWatchdogPoll(uint32_t nowMs)
{
  RelayLock lock;
  if (relayOutputLow &&
      static_cast<uint32_t>(nowMs - lastMainLoopHeartbeatMs) >=
          kHeartbeatTimeoutMs) {
    // A stale loop is never allowed to write LOW again, even if its old
    // GrinderOutputGuard result still says permitted().
    abortLatched = true;
    heartbeatFaultLatched = true;
    relayOutputLow = false;
    digitalWrite(PIN_GRINDER_SSR, HIGH);
  }
}

bool grinderRelayWatchdogTripped()
{
  RelayLock lock;
  return heartbeatFaultLatched;
}
