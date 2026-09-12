#pragma once

#include <stddef.h>
#include <stdint.h>

// Lightweight diagnostic recorder for the T4-S3 HX711 pipeline. Samples are
// stored in PSRAM so recording does not depend on an attached USB/serial port.

enum T4S3WeightDiagEvent : uint32_t {
    T4S3_DIAG_EVENT_NONE           = 0,
    T4S3_DIAG_EVENT_RECORD_START   = 1u << 0,
    T4S3_DIAG_EVENT_AUTO_TARE      = 1u << 1,
    T4S3_DIAG_EVENT_TARE_BEGIN     = 1u << 2,
    T4S3_DIAG_EVENT_TARE_DONE      = 1u << 3,
    T4S3_DIAG_EVENT_BLE_TARE       = 1u << 4,
    T4S3_DIAG_EVENT_STABLE_REACHED = 1u << 5,
    T4S3_DIAG_EVENT_MOVEMENT       = 1u << 6,
    T4S3_DIAG_EVENT_SHOT_ARMED     = 1u << 7,
    T4S3_DIAG_EVENT_SHOT_START     = 1u << 8,
    T4S3_DIAG_EVENT_SHOT_STOP      = 1u << 9,
    T4S3_DIAG_EVENT_SHOT_ABORT     = 1u << 10,
    T4S3_DIAG_EVENT_SHOT_DISARM    = 1u << 11,
};

struct T4S3WeightDiagSample {
    uint32_t timestamp_ms;
    float library_raw;
    float median_raw;
    float library_g;
    float median_g;
    float fast_g;
    float stable_g;
    float display_g;
    float shot_g;
    float flow_g_s;
    uint32_t events;
    uint8_t moving;
    uint8_t stable;
    uint8_t shot_state;
    uint8_t reserved;
};

struct T4S3WeightDiagStatus {
    bool allocated = false;
    bool running = false;
    uint32_t started_ms = 0;
    uint32_t duration_ms = 0;
    size_t sample_count = 0;
    size_t capacity = 0;
    uint32_t dropped_samples = 0;
    uint32_t sample_interval_ms = 100;
};

bool t4s3WeightDiagStart(uint32_t now_ms);
void t4s3WeightDiagStop(uint32_t now_ms);
void t4s3WeightDiagClear(uint32_t now_ms);
void t4s3WeightDiagMarkEvent(uint32_t event_flags);

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
                          uint8_t shot_state);

T4S3WeightDiagStatus t4s3WeightDiagGetStatus(uint32_t now_ms);
bool t4s3WeightDiagGetSample(size_t logical_index, T4S3WeightDiagSample &sample);
