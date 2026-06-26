#pragma once

// Small bridge API between the T4-S3 UI and the active scale backend.
// The HX711 implementation currently lives in t4s3_main.cpp so the first
// hardware bring-up can stay small and local.

bool t4s3_scale_is_ready();
bool t4s3_scale_is_stable();

bool t4s3_scale_tare();
bool t4s3_scale_calibrate(float knownGrams);

float t4s3_scale_current_grams();
float t4s3_scale_shot_grams();

// Handle leaving the Shot page on the Arduino/LVGL loop task. Armed sessions
// are cancelled; running sessions deliberately continue in the background.
void t4s3_scale_leave_shot_mode();

float t4s3_scale_calibration_factor();
