#include "../src/grinder_profile_measurement.h"

#include <cassert>
#include <cmath>
#include <limits>

int main() {
  GrinderProfileMeasurement m;
  GrinderProfileMeasurement::Result r{};
  assert(!m.begin(4));
  assert(m.begin(1));
  assert(!m.captureCombination(350.0f, r));
  assert(!m.capturePortafilter(246.8f)); // explicit zeroing required
  m.confirmTare();
  assert(!m.capturePortafilter(2.0f));
  assert(!m.capturePortafilter(std::numeric_limits<float>::quiet_NaN()));
  assert(m.capturePortafilter(246.8f));
  assert(!m.captureCombination(245.0f, r));
  assert(!m.captureCombination(600.0f, r));
  assert(m.captureCombination(305.9f, r));
  assert(std::fabs(r.portafilterG - 246.8f) < 0.01f);
  assert(std::fabs(r.funnelG - 59.1f) < 0.01f);
  assert(m.activeFor(1)); // storage success decides when to cancel
  m.cancel();
  assert(!m.activeFor(1));
  assert(!m.captureCombination(306.0f, r));
  assert(m.begin(0));
  m.confirmTare();
  assert(m.capturePortafilter(450.0f));
  m.cancel();
  assert(!m.captureCombination(500.0f, r));
  return 0;
}
