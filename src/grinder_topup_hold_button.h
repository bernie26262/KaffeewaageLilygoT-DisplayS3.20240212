#pragma once
#include <stdint.h>

// B4b.2: physical right-key hold gesture, independent of legacy Save/Skip.
// Debounced activation, immediate RAW release, no retrigger while held.
// No Arduino/relay dependencies. Return an event only on the initial edge.
class GrinderTopupHoldButton {
 public:
  enum class Event : uint8_t { None, Press, Release, Interrupted };
  void reset(bool rawPressed, uint32_t nowMs) {
    initialized_ = true;
    pressed_ = rawPressed;
    eligible_ = false; // A key held on boot cannot arm a top-up.
    engaged_ = false;
    pressedAtMs_ = nowMs;
  }
  Event update(bool rawPressed, bool contextAllowed, uint32_t nowMs) {
    if (!initialized_) reset(rawPressed, nowMs);
    if (!rawPressed) {
      const bool release = engaged_;
      pressed_ = false;
      eligible_ = false;
      engaged_ = false;
      return release ? Event::Release : Event::None;
    }
    if (!pressed_) {
      pressed_ = true;
      pressedAtMs_ = nowMs;
      eligible_ = contextAllowed;
    }
    if (engaged_) {
      if (contextAllowed) return Event::None;
      engaged_ = false;
      eligible_ = false;
      return Event::Interrupted;
    }
    // A disallowed press never becomes eligible later during the same hold.
    if (!contextAllowed) eligible_ = false;
    if (eligible_ && nowMs - pressedAtMs_ >= 25U) {
      engaged_ = true;
      return Event::Press;
    }
    return Event::None;
  }
 private:
  bool initialized_ = false;
  bool pressed_ = false;
  bool eligible_ = false;
  bool engaged_ = false;
  uint32_t pressedAtMs_ = 0;
};
