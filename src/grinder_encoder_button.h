#pragma once
#include <stdint.h>

// B4a.3a: pure, non-blocking button event classifier. Hardware is read by main.cpp.
// Long-press activation remains disabled in main.cpp until B4a.3b.
class GrinderEncoderButton {
public:
  enum class Event : uint8_t { None, ShortClick, LongPress };
  void reset(bool pressed, uint32_t nowMs);
  Event update(bool pressed, bool allowLongPress, uint32_t nowMs);
private:
  static constexpr uint32_t kDebounceMs = 25;
  static constexpr uint32_t kLongPressMs = 1000;
  bool initialized_ = false;
  bool rawPressed_ = false;
  bool stablePressed_ = false;
  bool longEligible_ = false;
  bool longEmitted_ = false;
  bool pressStarted_ = false;
  uint32_t rawChangeMs_ = 0;
  uint32_t pressStartMs_ = 0;
};
