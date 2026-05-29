#pragma once

#include <Arduino.h>

constexpr uint8_t T4S3_GEFAESS_SLOT_COUNT = 3;
constexpr uint8_t T4S3_SIEBTRAEGER_SLOT_COUNT = 4;
constexpr size_t T4S3_SIEBTRAEGER_NAME_LEN = 24;

struct T4S3UiSettings {
    uint16_t screenTimeoutMinutes;
    bool autodetectEnabled;
    uint8_t selectedSiebtraeger;
    int32_t targetTenthsBySiebtraeger[T4S3_SIEBTRAEGER_SLOT_COUNT];
    char siebtraegerNames[T4S3_SIEBTRAEGER_SLOT_COUNT][T4S3_SIEBTRAEGER_NAME_LEN];
    int32_t targetStepTenths;
    uint32_t totalShots;
    uint32_t machineShots;
    uint32_t grinderShots;
    uint32_t filterShots;
    int32_t totalGramsTenths;
    int32_t machineGramsTenths;
    int32_t grinderGramsTenths;
    int32_t filterGramsTenths;
    uint32_t maintenanceMachineIntervalSec;
    uint32_t maintenanceGrinderIntervalSec;
    uint32_t maintenanceFilterIntervalSec;
    int32_t gefaessWeightTenths[T4S3_GEFAESS_SLOT_COUNT];
};

void t4s3_settings_begin();
void t4s3_settings_load(T4S3UiSettings &settings);
void t4s3_settings_save(const T4S3UiSettings &settings);


void t4s3_settings_load_maintenance(uint32_t &machineEpoch,
                                    uint32_t &grinderEpoch,
                                    uint32_t &filterEpoch);
void t4s3_settings_save_maintenance(uint32_t machineEpoch,
                                    uint32_t grinderEpoch,
                                    uint32_t filterEpoch);

float t4s3_settings_load_hx711_cal_factor(float fallback);
void t4s3_settings_save_hx711_cal_factor(float factor);
