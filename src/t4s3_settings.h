#pragma once

#include <Arduino.h>

struct T4S3UiSettings {
    uint16_t screenTimeoutMinutes;
    bool autodetectEnabled;
    uint8_t selectedSiebtraeger;
    int32_t targetTenthsBySiebtraeger[4];
    int32_t targetStepTenths;
    uint32_t totalShots;
    uint32_t machineShots;
    uint32_t grinderShots;
    uint32_t filterShots;
    int32_t totalGramsTenths;
    int32_t machineGramsTenths;
    int32_t grinderGramsTenths;
    int32_t filterGramsTenths;};

void t4s3_settings_begin();
void t4s3_settings_load(T4S3UiSettings &settings);
void t4s3_settings_save(const T4S3UiSettings &settings);

