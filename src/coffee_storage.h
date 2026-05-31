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

void coffeeStorageSaveMaintenanceSettings(
  Preferences& preferences,
  uint32_t machineIntervalSeconds,
  uint32_t grinderIntervalSeconds,
  uint32_t filterIntervalSeconds,
  bool machineEnabled,
  bool grinderEnabled,
  bool filterEnabled);

void coffeeStorageLoadMaintenanceSettings(
  Preferences& preferences,
  uint32_t& machineIntervalSeconds,
  uint32_t& grinderIntervalSeconds,
  uint32_t& filterIntervalSeconds,
  bool& machineEnabled,
  bool& grinderEnabled,
  bool& filterEnabled);

void coffeeStorageSaveSelectedSiebtraeger(
  Preferences& preferences,
  uint16_t selectedST);

void coffeeStorageSaveSiebtraegerSetWeights(
  Preferences& preferences,
  const float* setWeightST,
  size_t byteCount);

void coffeeStorageSaveSiebtraegerName(
  Preferences& preferences,
  uint8_t index,
  const String& name);

void coffeeStorageLoadSiebtraegerNames(
  Preferences& preferences,
  String* names,
  size_t count);

void coffeeStorageSaveDisplayTimeoutMinutes(
  Preferences& preferences,
  uint16_t minutes);

uint16_t coffeeStorageLoadDisplayTimeoutMinutes(
  Preferences& preferences,
  uint16_t defaultMinutes);

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


bool coffeeStorageLoadOrInit(
  Preferences& preferences,
  float* weightST,
  size_t weightSTByteCount,
  float* weightTrichter,
  size_t weightTrichterByteCount,
  float* weightGefaess,
  size_t weightGefaessByteCount,
  float* setWeightST,
  size_t setWeightSTByteCount,
  float& calFactor,
  uint8_t& selectedST,
  uint8_t& selectedGefaess,
  bool& autoDetect,
  float& setWeightCalibration,
  float& groundWeightForever,
  float& groundWeightSinceClean,
  float& groundWeightSinceMachineClean,
  float& groundWeightSinceFilterChange,
  uint32_t& shotCounterForever,
  uint32_t& shotCounterSinceClean,
  uint32_t& shotCounterSinceMachineClean,
  uint32_t& shotCounterSinceFilterChange,
  uint32_t& lastTimeMuehlenReinigungNTP,
  uint32_t& lastTimeKaffeemReinigungNTP,
  uint32_t& lastTimeFilterWechselNTP);
