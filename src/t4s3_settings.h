#pragma once

#include <Arduino.h>

struct T4S3UiSettings {
    uint16_t screenTimeoutMinutes;
    bool autodetectEnabled;
    uint8_t selectedSiebtraeger;
    int32_t targetTenthsBySiebtraeger[4];
    int32_t targetStepTenths;
};

void t4s3_settings_begin();
void t4s3_settings_load(T4S3UiSettings &settings);
void t4s3_settings_save(const T4S3UiSettings &settings);
