#include <Arduino.h>

#include "grinder_relay.h"
#include "grinder_output_guard.h"
#include "pin_config.h"

// B4c.2a is an intentionally OFF-only firmware. A build-flag change may NOT
// silently turn a simulated motor request into a real GPIO LOW. A separate
// later, hardware-reviewed patch is required for that transition.
#ifndef GRINDER_SSR_PHYSICAL_ENABLE
#define GRINDER_SSR_PHYSICAL_ENABLE 0
#endif
#if GRINDER_SSR_PHYSICAL_ENABLE != 0
#error "B4c.2a: physical SSR release is forbidden until independent fail-OFF hardware is reviewed"
#endif

void grinderRelayInitOff()
{
  // Old Grind-by-Weight hardware: LOW=ON, HIGH=OFF.
  // Set output latch HIGH before switching the pin to OUTPUT, avoiding
  // an avoidable LOW pulse during GPIO configuration.
  digitalWrite(PIN_GRINDER_SSR, HIGH);
  pinMode(PIN_GRINDER_SSR, OUTPUT);
  digitalWrite(PIN_GRINDER_SSR, HIGH);
}

void grinderRelayForceOff()
{
  digitalWrite(PIN_GRINDER_SSR, HIGH);
}

void grinderRelayApplyGuard(const GrinderOutputGuard& guard)
{
  // Intentionally consume the only future release signal at this API
  // boundary, without forwarding it to the physical SSR pin yet.
  (void)guard.permitted();
  grinderRelayForceOff();
}
