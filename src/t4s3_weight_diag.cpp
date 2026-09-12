#include "t4s3_weight_diag.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

namespace {
constexpr size_t kPreferredCapacity = 9000;  // 15 min at 10 Hz
constexpr size_t kFallbackCapacity = 1200;   // 2 min if PSRAM allocation fails
constexpr uint32_t kSampleIntervalMs = 100UL;

T4S3WeightDiagSample *g_samples = nullptr;
size_t g_capacity = 0;
size_t g_head = 0;
size_t g_count = 0;
uint32_t g_dropped = 0;
volatile bool g_running = false;
uint32_t g_startedMs = 0;
uint32_t g_stoppedMs = 0;
uint32_t g_lastSampleMs = 0;
uint32_t g_pendingEvents = 0;
portMUX_TYPE g_diagMux = portMUX_INITIALIZER_UNLOCKED;

bool ensureBuffer()
{
    if (g_samples) {
        return true;
    }

    auto *buffer = static_cast<T4S3WeightDiagSample *>(
        heap_caps_malloc(sizeof(T4S3WeightDiagSample) * kPreferredCapacity,
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    size_t capacity = kPreferredCapacity;

    if (!buffer) {
        buffer = static_cast<T4S3WeightDiagSample *>(
            heap_caps_malloc(sizeof(T4S3WeightDiagSample) * kFallbackCapacity,
                             MALLOC_CAP_8BIT));
        capacity = buffer ? kFallbackCapacity : 0;
    }

    portENTER_CRITICAL(&g_diagMux);
    if (!g_samples && buffer) {
        g_samples = buffer;
        g_capacity = capacity;
        buffer = nullptr;
    }
    const bool ok = g_samples != nullptr;
    portEXIT_CRITICAL(&g_diagMux);

    if (buffer) {
        heap_caps_free(buffer);
    }
    return ok;
}

void clearUnlocked(uint32_t now_ms)
{
    g_head = 0;
    g_count = 0;
    g_dropped = 0;
    g_startedMs = now_ms;
    g_stoppedMs = now_ms;
    g_lastSampleMs = 0;
    g_pendingEvents = 0;
}
}  // namespace

bool t4s3WeightDiagStart(uint32_t now_ms)
{
    if (!ensureBuffer()) {
        return false;
    }

    portENTER_CRITICAL(&g_diagMux);
    clearUnlocked(now_ms);
    g_running = true;
    g_pendingEvents |= T4S3_DIAG_EVENT_RECORD_START;
    portEXIT_CRITICAL(&g_diagMux);
    return true;
}

void t4s3WeightDiagStop(uint32_t now_ms)
{
    portENTER_CRITICAL(&g_diagMux);
    g_running = false;
    g_stoppedMs = now_ms;
    portEXIT_CRITICAL(&g_diagMux);
}

void t4s3WeightDiagClear(uint32_t now_ms)
{
    portENTER_CRITICAL(&g_diagMux);
    const bool wasRunning = g_running;
    clearUnlocked(now_ms);
    g_running = wasRunning;
    if (wasRunning) {
        g_pendingEvents |= T4S3_DIAG_EVENT_RECORD_START;
    }
    portEXIT_CRITICAL(&g_diagMux);
}

void t4s3WeightDiagMarkEvent(uint32_t event_flags)
{
    if (event_flags == 0 || !g_running) {
        return;
    }

    portENTER_CRITICAL(&g_diagMux);
    if (g_running) {
        g_pendingEvents |= event_flags;
    }
    portEXIT_CRITICAL(&g_diagMux);
}

void t4s3WeightDiagRecord(uint32_t now_ms,
                          float library_raw,
                          float median_raw,
                          float library_g,
                          float median_g,
                          float fast_g,
                          float stable_g,
                          float display_g,
                          float shot_g,
                          float flow_g_s,
                          bool moving,
                          bool stable,
                          uint8_t shot_state)
{
    if (!g_running) {
        return;
    }

    portENTER_CRITICAL(&g_diagMux);
    if (!g_running || !g_samples || g_capacity == 0 ||
        (g_lastSampleMs != 0 && (now_ms - g_lastSampleMs) < kSampleIntervalMs)) {
        portEXIT_CRITICAL(&g_diagMux);
        return;
    }

    g_lastSampleMs = now_ms;

    T4S3WeightDiagSample sample{};
    sample.timestamp_ms = now_ms;
    sample.library_raw = library_raw;
    sample.median_raw = median_raw;
    sample.library_g = library_g;
    sample.median_g = median_g;
    sample.fast_g = fast_g;
    sample.stable_g = stable_g;
    sample.display_g = display_g;
    sample.shot_g = shot_g;
    sample.flow_g_s = flow_g_s;
    sample.events = g_pendingEvents;
    sample.moving = moving ? 1 : 0;
    sample.stable = stable ? 1 : 0;
    sample.shot_state = shot_state;
    g_pendingEvents = 0;

    if (g_count < g_capacity) {
        const size_t index = (g_head + g_count) % g_capacity;
        g_samples[index] = sample;
        ++g_count;
    } else {
        g_samples[g_head] = sample;
        g_head = (g_head + 1) % g_capacity;
        ++g_dropped;
    }
    portEXIT_CRITICAL(&g_diagMux);
}

T4S3WeightDiagStatus t4s3WeightDiagGetStatus(uint32_t now_ms)
{
    T4S3WeightDiagStatus status;
    portENTER_CRITICAL(&g_diagMux);
    status.allocated = g_samples != nullptr;
    status.running = g_running;
    status.started_ms = g_startedMs;
    status.duration_ms = g_startedMs == 0
                           ? 0
                           : ((g_running ? now_ms : g_stoppedMs) - g_startedMs);
    status.sample_count = g_count;
    status.capacity = g_capacity;
    status.dropped_samples = g_dropped;
    status.sample_interval_ms = kSampleIntervalMs;
    portEXIT_CRITICAL(&g_diagMux);
    return status;
}

bool t4s3WeightDiagGetSample(size_t logical_index, T4S3WeightDiagSample &sample)
{
    portENTER_CRITICAL(&g_diagMux);
    if (!g_samples || logical_index >= g_count || g_capacity == 0) {
        portEXIT_CRITICAL(&g_diagMux);
        return false;
    }

    const size_t index = (g_head + logical_index) % g_capacity;
    sample = g_samples[index];
    portEXIT_CRITICAL(&g_diagMux);
    return true;
}
