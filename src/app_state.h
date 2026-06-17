#pragma once
#include <Arduino.h>

enum ScaleUiMode {
  SCALE_UI_MODE_SINGLE_DOSE,
  SCALE_UI_MODE_SHOT
};

enum AppStatusMode {
  APP_STATUS_IDLE,
  APP_STATUS_MEASURING,
  APP_STATUS_STABLE,
  APP_STATUS_SAVE_READY
};

struct WeightState {
  float actual_g = 0.0f;
  float set_g = 0.0f;
  bool stable = false;
};

struct StopwatchState {
  uint32_t ms = 0;
  bool running = false;
};

struct ShotSessionState {
  uint32_t session_id = 0;
  String state = "idle";
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

struct SelectionState {
  int siebtraeger = 0;
  String siebtraeger_names[4];
  int gefaess = 0;
  bool autodetect = false;
};

struct CalibrationState {
  float set_weight_g = 0.0f;
  float factor = 0.0f;
};

struct GefaessState {
  float weights_g[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct StatsGround {
  float total_g = 0.0f;
  float since_grinder_clean_g = 0.0f;
  float since_machine_clean_g = 0.0f;
  float since_filter_change_g = 0.0f;
};

struct StatsShots {
  uint32_t total = 0;
  uint32_t since_grinder_clean = 0;
  uint32_t since_machine_clean = 0;
  uint32_t since_filter_change = 0;
};

struct StatsState {
  StatsGround ground;
  StatsShots shots;
};

struct MaintenanceState {
  bool grinder_clean_due = false;
  bool machine_clean_due = false;
  bool filter_change_due = false;
  int32_t grinder_seconds_to_due = 0;
  int32_t machine_seconds_to_due = 0;
  int32_t filter_seconds_to_due = 0;
  uint32_t grinder_interval_seconds = 0;
  uint32_t machine_interval_seconds = 0;
  uint32_t filter_interval_seconds = 0;
  bool grinder_enabled = true;
  bool machine_enabled = true;
  bool filter_enabled = true;
  uint8_t due_count = 0;
};

struct TimeState {
  bool valid = false;
  uint32_t epoch = 0;
};

struct SystemState {
  bool wifi_connected = false;
  String ip;
  int16_t wifi_rssi_dbm = 0;
  uint8_t wifi_signal_level = 0;
  String wifi_signal_label;
  uint32_t uptime_ms = 0;
  bool web_wizard_active = false;
  bool autodetect_paused = false;
  uint16_t display_timeout_minutes = 10;
  ScaleUiMode scale_mode = SCALE_UI_MODE_SINGLE_DOSE;
  bool shot_mode = false;
  bool single_dose_automation_allowed = true;
};


struct BleScaleState {
  bool enabled = false;
  bool started = false;
  bool connected = false;
  bool advertising = false;
  String mode = "aus";
  String last_command = "---";
  float notify_hz = 0.0f;
  float last_weight_g = 0.0f;
  uint32_t packets_sent = 0;
  uint32_t commands_received = 0;
  uint32_t last_notify_age_ms = 0;
  uint32_t log_sequence = 0;
  String log_text;
};

struct StatusState {
  AppStatusMode mode = APP_STATUS_IDLE;
  bool save_ready = false;
};

struct AppState {
  WeightState weight;
  StopwatchState stopwatch;
  ShotSessionState shot;
  SelectionState selection;
  CalibrationState calibration;
  GefaessState gefaesse;
  StatsState stats;
  MaintenanceState maintenance;
  TimeState time;
  SystemState system;
  StatusState status;
  BleScaleState ble;
};