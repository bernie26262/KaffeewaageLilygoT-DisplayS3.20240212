#pragma once

class GrinderOutputGuard;

// B4c.2b: all writes to active-LOW GPIO16 belong to this module.
// Production builds remain OFF-only. The bench-only LOW path requires an
// explicit build flag AND a physically disconnected 230-V SSR load.
void grinderRelayInitOff();

// Immediately force HIGH/OFF and latch an emergency inhibit. Async callbacks
// may call this while the main-loop task is active.
void grinderRelayForceOff();

// OTA/reboot is irreversible until ESP setup() runs again. A concurrent
// main-loop unprepared-reset must never rearm the SSR during an upload.
void grinderRelayForceOffUntilRestart();

// Only call after the controller is idle, vessel removed, output guard reset
// and OTA/reboot inactive. This resets the asynchronous emergency latch;
// merely releasing a held button must not clear it.
void grinderRelayRearmAfterUnprepared();

// Apply the independent guard, never an unchecked motor request. Only
// dedicated bench builds can actually emit LOW.
void grinderRelayApplyGuard(const GrinderOutputGuard& guard);
