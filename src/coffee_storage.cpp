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
