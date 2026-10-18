#pragma once
#include "grinder_profile_detector.h"

// B4a.3a: only update target profiles when the detector is safely empty.
// A pending target change during Ready MUST NOT silently reset preparation.
class GrinderTargetSync {
public:
  static bool targetsMatch(const GrinderProfileDetector& detector,
                           const GrinderProfileDetector::Profile (&desired)[GrinderProfileDetector::kProfileCount]);
  static bool reconcileIfIdle(GrinderProfileDetector& detector,
                              const GrinderProfileDetector::Profile (&desired)[GrinderProfileDetector::kProfileCount]);
};
