#include "grinder_profile_measurement.h"

#include <cmath>

bool GrinderProfileMeasurement::begin(int index) {
  cancel();
  if (index < 0 || index >= 4) return false;
  index_ = index;
  return true;
}

void GrinderProfileMeasurement::confirmTare() {
  if (index_ >= 0) tareConfirmed_ = true;
}

bool GrinderProfileMeasurement::capturePortafilter(float grossG) {
  if (index_ < 0 || !tareConfirmed_ || !std::isfinite(grossG) ||
      grossG < 30.0f || grossG > 1200.0f) return false;
  portafilterG_ = grossG;
  portafilterCaptured_ = true;
  return true;
}

bool GrinderProfileMeasurement::captureCombination(float grossG, Result& result) {
  if (index_ < 0 || !portafilterCaptured_ || !std::isfinite(grossG) ||
      grossG > 1500.0f) return false;
  const float funnelG = grossG - portafilterG_;
  if (funnelG < 5.0f || funnelG > 300.0f) return false;
  result = {portafilterG_, funnelG};
  return true;
}

void GrinderProfileMeasurement::cancel() {
  index_ = -1;
  portafilterG_ = 0.0f;
  portafilterCaptured_ = false;
  tareConfirmed_ = false;
}
