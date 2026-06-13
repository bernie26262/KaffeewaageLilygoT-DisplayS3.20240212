#pragma once

#include <stddef.h>
#include <stdint.h>

enum class CoffeeShotSessionState : uint8_t {
    Idle,
    Armed,
    Running,
    Completed,
};

enum class CoffeeShotSessionEvent : uint8_t {
    None,
    Started,
    Stopped,
};

constexpr size_t COFFEE_SHOT_MAX_SAMPLES = 1200;  // 120 s at 10 Hz

struct CoffeeShotSample {
    uint32_t time_ms = 0;
    float weight_g = 0.0f;
    float flow_g_s = 0.0f;  // Filled by the next flow-rate step.
};

struct CoffeeShotSessionStatus {
    CoffeeShotSessionState state = CoffeeShotSessionState::Idle;
    bool armed = false;
    bool running = false;
    bool completed = false;
    uint32_t elapsed_ms = 0;
    float current_weight_g = 0.0f;
    float peak_weight_g = 0.0f;
    float final_weight_g = 0.0f;
    float current_flow_g_s = 0.0f;
    uint16_t sample_count = 0;
    bool sample_buffer_full = false;
};

// A tare on the Shot page prepares automatic first-drop detection. The
// completed shot remains available until a new shot actually starts.
void coffeeShotSessionArm(uint32_t now_ms, float current_weight_g);
void coffeeShotSessionCancelArm();

// Feed the calm Shot weight path on every new HX711 sample. The returned event
// is consumed by t4s3_main.cpp to start/stop the existing UI timer.
CoffeeShotSessionEvent coffeeShotSessionTick(uint32_t now_ms, float weight_g);

// Keep support for possible future/external START/STOP commands even though
// current Gaggiuino/WeighMyBru tests only send TARE.
CoffeeShotSessionEvent coffeeShotSessionExternalStart(uint32_t now_ms, float weight_g);
CoffeeShotSessionEvent coffeeShotSessionExternalStop(uint32_t now_ms, float weight_g);
void coffeeShotSessionReset();

CoffeeShotSessionStatus coffeeShotSessionStatus(uint32_t now_ms);
size_t coffeeShotSessionSampleCount();
bool coffeeShotSessionGetSample(size_t index, CoffeeShotSample &sample);
const char *coffeeShotSessionStateName(CoffeeShotSessionState state);
