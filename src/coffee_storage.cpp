#include "coffee_storage.h"

namespace {
constexpr const char* STORAGE_NAMESPACE = "savedValues";
constexpr bool RW_MODE = false;
}

void coffeeStorageSaveStats(
  Preferences& preferences,
  float groundWeightForever,
  float groundWeightSinceClean,
  float groundWeightSinceMachineClean,
  float groundWeightSinceFilterChange,
  uint32_t shotCounterForever,
  uint32_t shotCounterSinceClean,
  uint32_t shotCounterSinceMachineClean,
  uint32_t shotCounterSinceFilterChange)
{
  preferences.begin(STORAGE_NAMESPACE, RW_MODE);
  preferences.putFloat("grndWghtFrvr", groundWeightForever);
  preferences.putFloat("grndWghtCln", groundWeightSinceClean);
  preferences.putFloat("grndWghtKffm", groundWeightSinceMachineClean);
  preferences.putFloat("grndWghtFlt", groundWeightSinceFilterChange);

  preferences.putULong("shotsFrvr", shotCounterForever);
  preferences.putULong("shotsCln", shotCounterSinceClean);
  preferences.putULong("shotsKffm", shotCounterSinceMachineClean);
  preferences.putULong("shotsFlt", shotCounterSinceFilterChange);
  preferences.end();
}

void coffeeStorageSaveGrinderMaintenanceReset(
  Preferences& preferences,
  float groundWeightSinceClean,
  uint32_t shotCounterSinceClean,
  uint32_t lastTimeMuehlenReinigungNTP)
{
  preferences.begin(STORAGE_NAMESPACE, RW_MODE);
  preferences.putFloat("grndWghtCln", groundWeightSinceClean);
  preferences.putULong("shotsCln", shotCounterSinceClean);
  preferences.putULong("lstMhlRngng", lastTimeMuehlenReinigungNTP);
  preferences.end();
}

void coffeeStorageSaveMachineMaintenanceReset(
  Preferences& preferences,
  float groundWeightSinceMachineClean,
  uint32_t shotCounterSinceMachineClean,
  uint32_t lastTimeKaffeemReinigungNTP)
{
  preferences.begin(STORAGE_NAMESPACE, RW_MODE);
  preferences.putULong("lstKffmRngng", lastTimeKaffeemReinigungNTP);
  preferences.putFloat("grndWghtKffm", groundWeightSinceMachineClean);
  preferences.putULong("shotsKffm", shotCounterSinceMachineClean);
  preferences.end();
}

void coffeeStorageSaveFilterMaintenanceReset(
  Preferences& preferences,
  float groundWeightSinceFilterChange,
  uint32_t shotCounterSinceFilterChange,
  uint32_t lastTimeFilterWechselNTP)
{
  preferences.begin(STORAGE_NAMESPACE, RW_MODE);
  preferences.putULong("lstFltwchsl", lastTimeFilterWechselNTP);
  preferences.putFloat("grndWghtFlt", groundWeightSinceFilterChange);
  preferences.putULong("shotsFlt", shotCounterSinceFilterChange);
  preferences.end();
}

void coffeeStorageSaveMaintenanceTimestamp(
  Preferences& preferences,
  const char* key,
  uint32_t timestamp)
{
  preferences.begin(STORAGE_NAMESPACE, RW_MODE);
  preferences.putULong(key, timestamp);
  preferences.end();
}

void coffeeStorageSaveSelectedSiebtraeger(
  Preferences& preferences,
  uint16_t selectedST)
{
  preferences.begin(STORAGE_NAMESPACE, RW_MODE);
  preferences.putShort("savedSelST", selectedST);
  preferences.end();
}

void coffeeStorageSaveSiebtraegerSetWeights(
  Preferences& preferences,
  const float* setWeightST,
  size_t byteCount)
{
  preferences.begin(STORAGE_NAMESPACE, RW_MODE);
  preferences.putBytes("svdSetWeightST", setWeightST, byteCount);
  preferences.end();
}

void coffeeStorageSaveAutodetect(
  Preferences& preferences,
  bool autoDetect)
{
  preferences.begin(STORAGE_NAMESPACE, RW_MODE);
  preferences.putBool("savedAutoDetect", autoDetect);
  preferences.end();
}

void coffeeStorageSaveCalibrationWeight(
  Preferences& preferences,
  float setWeightCalibration)
{
  preferences.begin(STORAGE_NAMESPACE, RW_MODE);
  preferences.putFloat("savedCalWeight", setWeightCalibration);
  preferences.end();
}

void coffeeStorageSaveCalibrationFactor(
  Preferences& preferences,
  float calFactor)
{
  preferences.begin(STORAGE_NAMESPACE, RW_MODE);
  preferences.putFloat("savedCalFact", calFactor);
  preferences.end();
}

void coffeeStorageSaveSelectedGefaess(
  Preferences& preferences,
  uint16_t selectedGefaess)
{
  preferences.begin(STORAGE_NAMESPACE, RW_MODE);
  preferences.putShort("savedSelGef", selectedGefaess);
  preferences.end();
}

void coffeeStorageSaveGefaessWeights(
  Preferences& preferences,
  const float* weightGefaess,
  size_t byteCount)
{
  preferences.begin(STORAGE_NAMESPACE, RW_MODE);
  preferences.putBytes("savedWeightGef", weightGefaess, byteCount);
  preferences.end();
}
