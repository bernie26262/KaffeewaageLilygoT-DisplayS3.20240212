#pragma once

// B3c: staging-only measurement of a portafilter and its funnel.
// Never tares, writes NVS, changes profiles or drives the SSR.
class GrinderProfileMeasurement {
public:
  struct Result { float portafilterG; float funnelG; };

  bool begin(int index);
  void confirmTare();
  bool capturePortafilter(float grossG);
  bool captureCombination(float grossG, Result& result);
  void cancel();
  bool activeFor(int index) const { return index == index_ && index_ >= 0; }
  bool active() const { return index_ >= 0; }
  float stagedPortafilterG() const { return portafilterG_; }

private:
  int index_ = -1;
  float portafilterG_ = 0.0f;
  bool portafilterCaptured_ = false;
  bool tareConfirmed_ = false;
};
