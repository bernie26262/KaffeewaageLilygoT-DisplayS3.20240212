#pragma once

#include <stdint.h>

void ui_t4s3_create(uint16_t width, uint16_t height);
void ui_t4s3_tick();

uint16_t ui_t4s3_get_screen_timeout_minutes();
uint32_t ui_t4s3_get_last_activity_ms();
void ui_t4s3_notify_activity();
void ui_t4s3_prepare_wakeup_touch();
void ui_t4s3_set_hx711_raw_value(int32_t rawValue);
void ui_t4s3_set_hx711_grams_value(float grams, bool valid);
// WebUI bridge: exposes selected T4-S3 UI actions/state without duplicating
// the HMI logic in the web server. Returns true when the command was handled.
bool ui_t4s3_handle_web_command(const char *cmd);
bool ui_t4s3_web_save_ready();
uint32_t ui_t4s3_web_stopwatch_ms();
bool ui_t4s3_web_stopwatch_running();
float ui_t4s3_web_calibration_weight_g();
const char *ui_t4s3_web_siebtraeger_name(uint8_t idx);
uint8_t ui_t4s3_web_selected_gefaess();
bool ui_t4s3_web_wizard_active();
bool ui_t4s3_is_shot_scale_mode();
bool ui_t4s3_ble_remote_control_allowed();
bool ui_t4s3_single_dose_automation_allowed();
const char *ui_t4s3_scale_mode_key();
const char *ui_t4s3_scale_mode_label();
