#include "grinder_weight_stability.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <stdio.h>

static void quietReadingsReachStable() {
  GrinderWeightStability stability;
  assert(!stability.hasFreshSample(0));
  assert(!stability.isStable(0));
  stability.onSample(450.0f, 0);
  for (uint32_t t = 100; t <= 400; t += 100)
    stability.onSample(450.0f + (t % 200 ? 0.1f : -0.1f), t);
  assert(!stability.isStable(400));
  stability.onSample(450.1f, 500);
  assert(stability.isStable(500));
}

static void movementRestartsQuietWindow() {
  GrinderWeightStability stability;
  for (uint32_t t = 0; t <= 500; t += 100) stability.onSample(0.0f, t);
  assert(stability.isStable(500));
  stability.onSample(150.0f, 600);
  assert(!stability.isStable(600));
  for (uint32_t t = 700; t <= 1100; t += 100) stability.onSample(150.0f, t);
  assert(stability.isStable(1100));
  stability.onSample(150.5f, 1200); // 0.5 g exceeds the 0.35 g window
  assert(!stability.isStable(1200));
}

static void invalidAndStaleReadingsAreRejected() {
  GrinderWeightStability stability;
  for (uint32_t t = 0; t <= 500; t += 100) stability.onSample(12.0f, t);
  assert(stability.isStable(500));
  assert(!stability.hasFreshSample(1501));
  assert(!stability.isStable(1501));
  stability.onSample(12.0f, 1600); // gap forces a fresh settle window
  assert(!stability.isStable(1600));
  stability.onSample(std::numeric_limits<float>::quiet_NaN(), 1700);
  assert(!stability.hasFreshSample(1700));
  assert(!stability.isStable(1700));
}

static void needsMultipleSamplesAndHandlesTimerWrap() {
  GrinderWeightStability stability;
  stability.onSample(1.0f, 0);
  assert(!stability.isStable(800)); // a single old sample cannot become stable
  stability.reset();
  const uint32_t start = 0xFFFFFF00u;
  stability.onSample(0.0f, start);
  for (uint32_t dt = 100; dt <= 500; dt += 100)
    stability.onSample(0.0f, start + dt);
  assert(stability.isStable(start + 500));
}

int main() {
  quietReadingsReachStable();
  movementRestartsQuietWindow();
  invalidAndStaleReadingsAreRejected();
  needsMultipleSamplesAndHandlesTimerWrap();
  puts("GrinderWeightStability host tests PASS");
  return 0;
}
