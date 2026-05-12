#pragma once

#include <Arduino.h>
#include <Preferences.h>

void coffeeStorageSaveStats(
  Preferences& preferences,
  float groundWeightForever,
  float groundWeightSinceClean,
  float groundWeightSinceMachineClean,
  float groundWeightSinceFilterChange,
  uint32_t shotCounterForever,
  uint32_t shotCounterSinceClean,
  uint32_t shotCounterSinceMachineClean,
  uint32_t shotCounterSinceFilterChange);

void coffeeStorageSaveGrinderMaintenanceReset(
  Preferences& preferences,
  float groundWeightSinceClean,
  uint32_t shotCounterSinceClean,
  uint32_t lastTimeMuehlenReinigungNTP);

void coffeeStorageSaveMachineMaintenanceReset(
  Preferences& preferences,
  float groundWeightSinceMachineClean,
  uint32_t shotCounterSinceMachineClean,
  uint32_t lastTimeKaffeemReinigungNTP);

void coffeeStorageSaveFilterMaintenanceReset(
  Preferences& preferences,
  float groundWeightSinceFilterChange,
  uint32_t shotCounterSinceFilterChange,
  uint32_t lastTimeFilterWechselNTP);

void coffeeStorageSaveMaintenanceTimestamp(
  Preferences& preferences,
  const char* key,
  uint32_t timestamp);

void coffeeStorageSaveSelectedSiebtraeger(
  Preferences& preferences,
  uint16_t selectedST);

void coffeeStorageSaveSiebtraegerSetWeights(
  Preferences& preferences,
  const float* setWeightST,
  size_t byteCount);

void coffeeStorageSaveAutodetect(
  Preferences& preferences,
  bool autoDetect);

void coffeeStorageSaveCalibrationWeight(
  Preferences& preferences,
  float setWeightCalibration);

void coffeeStorageSaveCalibrationFactor(
  Preferences& preferences,
  float calFactor);

void coffeeStorageSaveSelectedGefaess(
  Preferences& preferences,
  uint16_t selectedGefaess);

void coffeeStorageSaveGefaessWeights(
  Preferences& preferences,
  const float* weightGefaess,
  size_t byteCount);
