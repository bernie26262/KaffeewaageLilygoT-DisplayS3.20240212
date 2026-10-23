#pragma once

class GrinderOutputGuard;

// B4c.2a: all writes to the active-LOW GPIO16 SSR pin belong here.
// Hardware release is deliberately NOT implemented in this phase.
void grinderRelayInitOff();

// Immediate, idempotent HIGH/OFF write. May also be called from an
// AsyncWebServer callback; it never changes any controller state.
void grinderRelayForceOff();

// The future actuator interface takes the separate guard, not an unchecked
// controller request. In B4c.2a it always keeps GPIO16 HIGH, even when the
// guard reports permitted() == true.
void grinderRelayApplyGuard(const GrinderOutputGuard& guard);
