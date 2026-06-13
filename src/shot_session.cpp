#include "shot_session.h"

#include <math.h>

namespace {

// Automatic timing deliberately measures first liquid in the cup through the
// end of relevant weight growth. It is not the machine pump timer.
constexpr uint32_t kArmSettleMs = 500UL;
constexpr uint32_t kArmTimeoutMs = 45000UL;
constexpr float kStartCandidateWeightG = 0.25f;
constexpr float kStartConfirmWeightG = 0.45f;
constexpr uint32_t kStartConfirmMs = 250UL;
constexpr float kStartCancelWeightG = 0.12f;

constexpr uint32_t kMinimumShotDurationMs = 8000UL;
constexpr float kMinimumShotWeightG = 4.0f;
constexpr float kSignificantGrowthG = 0.20f;
constexpr uint32_t kStopNoGrowthMs = 3000UL;
constexpr uint32_t kSampleIntervalMs = 100UL;

CoffeeShotSessionStatus g_status;
uint32_t g_armed_at_ms = 0;
float g_arm_weight_g = 0.0f;
bool g_start_candidate = false;
uint32_t g_start_candidate_ms = 0;
uint32_t g_started_at_ms = 0;
uint32_t g_last_growth_ms = 0;
float g_last_growth_weight_g = 0.0f;
CoffeeShotSample g_samples[COFFEE_SHOT_MAX_SAMPLES];
size_t g_sample_count = 0;
uint32_t g_last_sample_ms = 0;
bool g_sample_buffer_full = false;

float finiteWeight(float value)
{
    return isfinite(value) ? value : 0.0f;
}

void clearSamples()
{
    g_sample_count = 0;
    g_last_sample_ms = 0;
    g_sample_buffer_full = false;
}

void recordSample(uint32_t now_ms, float weight_g, bool force)
{
    if (!force && g_sample_count > 0 && (now_ms - g_last_sample_ms) < kSampleIntervalMs) {
        return;
    }
    if (g_sample_count >= COFFEE_SHOT_MAX_SAMPLES) {
        g_sample_buffer_full = true;
        return;
    }

    CoffeeShotSample &sample = g_samples[g_sample_count++];
    sample.time_ms = now_ms - g_started_at_ms;
    sample.weight_g = finiteWeight(weight_g);
    sample.flow_g_s = 0.0f;
    g_last_sample_ms = now_ms;
}

void trimSamplesAfter(uint32_t stopped_at_ms)
{
    const uint32_t stopped_elapsed_ms = stopped_at_ms - g_started_at_ms;
    while (g_sample_count > 0 && g_samples[g_sample_count - 1].time_ms > stopped_elapsed_ms) {
        --g_sample_count;
    }
    g_last_sample_ms = g_sample_count > 0
                         ? g_started_at_ms + g_samples[g_sample_count - 1].time_ms
                         : 0;
}

CoffeeShotSessionEvent startSession(uint32_t now_ms, float weight_g)
{
    const float weight = finiteWeight(weight_g);
    g_status.state = CoffeeShotSessionState::Running;
    g_status.armed = false;
    g_status.running = true;
    g_status.completed = false;
    g_status.elapsed_ms = 0;
    g_status.current_weight_g = weight;
    g_status.peak_weight_g = weight;
    g_status.final_weight_g = 0.0f;

    g_started_at_ms = now_ms;
    g_last_growth_ms = now_ms;
    g_last_growth_weight_g = weight;
    g_start_candidate = false;
    clearSamples();
    recordSample(now_ms, weight, true);
    return CoffeeShotSessionEvent::Started;
}

CoffeeShotSessionEvent stopSession(uint32_t now_ms, float weight_g, bool use_last_growth_time)
{
    if (!g_status.running) {
        return CoffeeShotSessionEvent::None;
    }

    const float weight = finiteWeight(weight_g);
    if (weight > g_status.peak_weight_g) {
        g_status.peak_weight_g = weight;
    }
    const uint32_t stopped_at_ms = use_last_growth_time ? g_last_growth_ms : now_ms;
    if (use_last_growth_time) {
        trimSamplesAfter(stopped_at_ms);
    }
    const float final_sample_weight = use_last_growth_time ? g_status.peak_weight_g : weight;
    recordSample(stopped_at_ms, final_sample_weight, true);

    g_status.state = CoffeeShotSessionState::Completed;
    g_status.armed = false;
    g_status.running = false;
    g_status.completed = true;
    g_status.elapsed_ms = stopped_at_ms - g_started_at_ms;
    g_status.current_weight_g = weight;
    g_status.final_weight_g = g_status.peak_weight_g;
    return CoffeeShotSessionEvent::Stopped;
}

} // namespace

void coffeeShotSessionArm(uint32_t now_ms, float current_weight_g)
{
    // Do not erase the previous completed result here. It is replaced only
    // when the next shot is positively detected.
    g_status.state = CoffeeShotSessionState::Armed;
    g_status.armed = true;
    g_status.running = false;
    g_status.current_weight_g = finiteWeight(current_weight_g);

    g_armed_at_ms = now_ms;
    g_arm_weight_g = g_status.current_weight_g;
    g_start_candidate = false;
    g_start_candidate_ms = 0;
}

void coffeeShotSessionCancelArm()
{
    if (g_status.state != CoffeeShotSessionState::Armed) {
        return;
    }

    g_status.armed = false;
    g_status.running = false;
    g_status.state = g_status.completed
                       ? CoffeeShotSessionState::Completed
                       : CoffeeShotSessionState::Idle;
    g_start_candidate = false;
    g_start_candidate_ms = 0;
}

CoffeeShotSessionEvent coffeeShotSessionTick(uint32_t now_ms, float weight_g)
{
    const float weight = finiteWeight(weight_g);
    g_status.current_weight_g = weight;

    if (g_status.state == CoffeeShotSessionState::Armed) {
        if ((now_ms - g_armed_at_ms) >= kArmTimeoutMs) {
            coffeeShotSessionCancelArm();
            return CoffeeShotSessionEvent::None;
        }

        if ((now_ms - g_armed_at_ms) < kArmSettleMs) {
            return CoffeeShotSessionEvent::None;
        }

        const float netWeight = weight - g_arm_weight_g;
        if (netWeight < kStartCancelWeightG) {
            g_start_candidate = false;
            return CoffeeShotSessionEvent::None;
        }

        if (netWeight >= kStartCandidateWeightG) {
            if (!g_start_candidate) {
                g_start_candidate = true;
                g_start_candidate_ms = now_ms;
            }

            if ((now_ms - g_start_candidate_ms) >= kStartConfirmMs &&
                netWeight >= kStartConfirmWeightG) {
                return startSession(now_ms, weight);
            }
        }
        return CoffeeShotSessionEvent::None;
    }

    if (g_status.state != CoffeeShotSessionState::Running) {
        return CoffeeShotSessionEvent::None;
    }

    g_status.elapsed_ms = now_ms - g_started_at_ms;
    recordSample(now_ms, weight, false);
    if (weight > g_status.peak_weight_g) {
        g_status.peak_weight_g = weight;
    }

    if (weight >= (g_last_growth_weight_g + kSignificantGrowthG)) {
        g_last_growth_weight_g = weight;
        g_last_growth_ms = now_ms;
    }

    if (g_status.elapsed_ms >= kMinimumShotDurationMs &&
        g_status.peak_weight_g >= kMinimumShotWeightG &&
        (now_ms - g_last_growth_ms) >= kStopNoGrowthMs) {
        return stopSession(now_ms, weight, true);
    }

    return CoffeeShotSessionEvent::None;
}

CoffeeShotSessionEvent coffeeShotSessionExternalStart(uint32_t now_ms, float weight_g)
{
    if (g_status.running) {
        return CoffeeShotSessionEvent::None;
    }
    return startSession(now_ms, weight_g);
}

CoffeeShotSessionEvent coffeeShotSessionExternalStop(uint32_t now_ms, float weight_g)
{
    return stopSession(now_ms, weight_g, false);
}

void coffeeShotSessionReset()
{
    g_status = CoffeeShotSessionStatus{};
    g_armed_at_ms = 0;
    g_arm_weight_g = 0.0f;
    g_start_candidate = false;
    g_start_candidate_ms = 0;
    g_started_at_ms = 0;
    g_last_growth_ms = 0;
    g_last_growth_weight_g = 0.0f;
    clearSamples();
}

CoffeeShotSessionStatus coffeeShotSessionStatus(uint32_t now_ms)
{
    CoffeeShotSessionStatus result = g_status;
    if (result.running) {
        result.elapsed_ms = now_ms - g_started_at_ms;
    }
    result.sample_count = static_cast<uint16_t>(g_sample_count);
    result.sample_buffer_full = g_sample_buffer_full;
    return result;
}

size_t coffeeShotSessionSampleCount()
{
    return g_sample_count;
}

bool coffeeShotSessionGetSample(size_t index, CoffeeShotSample &sample)
{
    if (index >= g_sample_count) {
        return false;
    }
    sample = g_samples[index];
    return true;
}

const char *coffeeShotSessionStateName(CoffeeShotSessionState state)
{
    switch (state) {
        case CoffeeShotSessionState::Idle: return "idle";
        case CoffeeShotSessionState::Armed: return "armed";
        case CoffeeShotSessionState::Running: return "running";
        case CoffeeShotSessionState::Completed: return "completed";
        default: return "unknown";
    }
}
