#include "grinder_target_sync.h"
#include <cmath>

bool GrinderTargetSync::targetsMatch(
    const GrinderProfileDetector& detector,
    const GrinderProfileDetector::Profile (&desired)[GrinderProfileDetector::kProfileCount]) {
  for (int i = 0; i < GrinderProfileDetector::kProfileCount; ++i) {
    if (!std::isfinite(desired[i].targetG) ||
        std::fabs(detector.profileTargetG(i) - desired[i].targetG) > 0.01f)
      return false;
  }
  return true;
}

bool GrinderTargetSync::reconcileIfIdle(
    GrinderProfileDetector& detector,
    const GrinderProfileDetector::Profile (&desired)[GrinderProfileDetector::kProfileCount]) {
  if (targetsMatch(detector, desired)) return true;
  if (detector.paused() || detector.state() != GrinderProfileDetector::State::WaitVessel ||
      detector.awaitingTare() != GrinderProfileDetector::TareKind::None)
    return false;
  for (int i = 0; i < GrinderProfileDetector::kProfileCount; ++i) {
    if (std::fabs(detector.profileTargetG(i) - desired[i].targetG) > 0.01f ||
        !std::isfinite(desired[i].targetG)) {
      if (!detector.setProfile(i, desired[i])) return false;
    }
  }
  return targetsMatch(detector, desired);
}
