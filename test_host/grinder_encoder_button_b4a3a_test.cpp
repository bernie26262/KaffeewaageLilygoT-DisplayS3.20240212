#include "grinder_encoder_button.h"
#include <cassert>
#include <cstdio>
using E = GrinderEncoderButton::Event;
int main() {
  GrinderEncoderButton b;
  b.reset(false, 0);
  assert(b.update(true, true, 10) == E::None);
  assert(b.update(false, true, 20) == E::None); // bounce
  assert(b.update(true, true, 30) == E::None);
  assert(b.update(true, true, 56) == E::None); // debounced press
  assert(b.update(false, true, 140) == E::None);
  assert(b.update(false, true, 166) == E::ShortClick);
  assert(b.update(false, true, 200) == E::None);
  assert(b.update(true, true, 300) == E::None);
  assert(b.update(true, true, 326) == E::None);
  assert(b.update(true, true, 1326) == E::LongPress);
  assert(b.update(true, true, 1700) == E::None);
  assert(b.update(false, true, 1710) == E::None);
  assert(b.update(false, true, 1736) == E::None); // no click on long-release
  assert(b.update(true, false, 1800) == E::None);
  assert(b.update(true, false, 1826) == E::None);
  assert(b.update(true, false, 3000) == E::None); // long disabled
  assert(b.update(false, false, 3010) == E::None);
  assert(b.update(false, false, 3036) == E::ShortClick); // legacy click
  // A held button on boot must not generate an action.
  b.reset(true, 4000);
  assert(b.update(true, true, 5300) == E::None);
  assert(b.update(false, true, 5310) == E::None);
  assert(b.update(false, true, 5340) == E::None);
  // millis() wrap, including long press timing.
  b.reset(false, 0xffffff00u);
  assert(b.update(true, true, 0xffffff10u) == E::None);
  assert(b.update(true, true, 0xffffff30u) == E::None);
  assert(b.update(true, true, 0x00000318u) == E::LongPress);
  std::puts("grinder_encoder_button_b4a3a_test: PASS");
}
