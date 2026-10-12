#include <Arduino.h>

#include "grinder_relay.h"
#include "pin_config.h"

void grinderRelayInitOff()
{
  // Old Grind-by-Weight hardware: LOW=ON, HIGH=OFF.
  // Set output latch HIGH before switching the pin to OUTPUT, avoiding
  // an avoidable LOW pulse during GPIO configuration.
  digitalWrite(PIN_GRINDER_SSR, HIGH);
  pinMode(PIN_GRINDER_SSR, OUTPUT);
  digitalWrite(PIN_GRINDER_SSR, HIGH);
}
