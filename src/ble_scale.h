#pragma once

#include <Arduino.h>

#ifndef ENABLE_BLE_SCALE
#define ENABLE_BLE_SCALE 0
#endif

struct CoffeeBleScaleStatus {
  bool enabled = false;
  bool started = false;
  bool advertising = false;
  bool connected = false;
  const char* mode = "aus";
  const char* last_command = "---";
  float last_weight_g = 0.0f;
  float notify_hz = 0.0f;
  uint32_t packets_sent = 0;
  uint32_t commands_received = 0;
  uint32_t last_notify_age_ms = 0;
  uint32_t log_sequence = 0;
};

enum class CoffeeBleScaleCommand : uint8_t {
  None = 0,
  Tare,
  TimerStart,
  TimerStop,
  TimerReset
};

void coffeeBleScaleBegin(const char* deviceName);
void coffeeBleScaleTick(uint32_t nowMs, float weightG, bool stable);
bool coffeeBleScalePopCommand(CoffeeBleScaleCommand& command);
CoffeeBleScaleStatus coffeeBleScaleStatus();
const char* coffeeBleScaleCommandName(CoffeeBleScaleCommand command);
void coffeeBleScaleAppendLog(const char* message);
void coffeeBleScaleCopyLog(char* destination, size_t destinationSize);
