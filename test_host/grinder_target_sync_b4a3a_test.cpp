#include "grinder_target_sync.h"
#include <cassert>
#include <cstdio>
using D = GrinderProfileDetector;
int main() {
  D d;
  D::Profile p[D::kProfileCount] = {{449.6f, 100.f, 8.5f}, {475.5f, 60.f, 17.f},
                                   {480.f, 61.f, 18.f}, {490.f, 70.f, 19.f}};
  for (int i = 0; i < D::kProfileCount; ++i) assert(d.setProfile(i, p[i]));
  assert(GrinderTargetSync::targetsMatch(d, p));
  d.onWeightSample(549.6f, true, 100);
  d.onWeightSample(549.6f, true, 1700);
  assert(d.takeTareRequest() == D::TareKind::AutoCombination);
  d.onTareResult(true, 1800);
  d.onWeightSample(0.f, true, 1900);
  assert(d.state() == D::State::ReadyAuto);
  p[0].targetG = 9.f;
  assert(!GrinderTargetSync::targetsMatch(d, p));
  assert(!GrinderTargetSync::reconcileIfIdle(d, p));
  assert(d.state() == D::State::ReadyAuto); // do not disrupt prepared vessel
  assert(d.selectedTargetG() == 8.5f); // pending, start must stay forbidden
  d.invalidatePreparation();
  assert(d.state() == D::State::WaitVessel);
  assert(GrinderTargetSync::reconcileIfIdle(d, p));
  assert(GrinderTargetSync::targetsMatch(d, p));
  assert(d.profileTargetG(0) == 9.f);
  assert(d.setAutoDetect(false));
  assert(d.selectManualProfile(1));
  p[2].targetG = 20.f;
  assert(GrinderTargetSync::reconcileIfIdle(d, p));
  // setProfile resets selectedProfile: caller must restore manual selection.
  assert(d.selectManualProfile(1));
  assert(d.selectedTargetG() == 17.f);
  std::puts("grinder_target_sync_b4a3a_test: PASS");
}
