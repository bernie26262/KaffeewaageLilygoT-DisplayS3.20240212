#include "grinder_encoder_button.h"

void GrinderEncoderButton::reset(bool pressed, uint32_t nowMs) {
  initialized_ = true;
  rawPressed_ = stablePressed_ = pressed;
  rawChangeMs_ = pressStartMs_ = nowMs;
  // A key held while booting is ignored until it has been released.
  pressStarted_ = longEligible_ = longEmitted_ = false;
}

GrinderEncoderButton::Event GrinderEncoderButton::update(bool pressed,
                                                          bool allowLongPress,
                                                          uint32_t nowMs) {
  if (!initialized_) reset(pressed, nowMs);
  if (pressed != rawPressed_) {
    rawPressed_ = pressed;
    rawChangeMs_ = nowMs;
  }
  if (rawPressed_ != stablePressed_ && nowMs - rawChangeMs_ >= kDebounceMs) {
    stablePressed_ = rawPressed_;
    if (stablePressed_) {
      pressStartMs_ = nowMs;
      pressStarted_ = true;
      longEligible_ = allowLongPress;
      longEmitted_ = false;
    } else {
      const bool emitShort = pressStarted_ && !longEmitted_;
      pressStarted_ = false;
      longEligible_ = false;
      if (emitShort) return Event::ShortClick;
    }
  }
  if (stablePressed_ && pressStarted_ && longEligible_ && !longEmitted_ &&
      nowMs - pressStartMs_ >= kLongPressMs) {
    longEmitted_ = true;
    return Event::LongPress;
  }
  return Event::None;
}
