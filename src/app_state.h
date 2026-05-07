#pragma once
#include <Arduino.h>

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

struct SelectionState {
  int siebtraeger = 0;
  int gefaess = 0;
  bool autodetect = false;
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
  uint8_t due_count = 0;
};

struct TimeState {
  bool valid = false;
  uint32_t epoch = 0;
};

struct SystemState {
  bool wifi_connected = false;
  String ip;
  uint32_t uptime_ms = 0;
};

struct StatusState {
  AppStatusMode mode = APP_STATUS_IDLE;
  bool save_ready = false;
};

struct AppState {
  WeightState weight;
  StopwatchState stopwatch;
  SelectionState selection;
  StatsState stats;
  MaintenanceState maintenance;
  TimeState time;
  SystemState system;
  StatusState status;
};