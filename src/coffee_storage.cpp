#include "coffee_storage.h"

namespace {
constexpr const char* STORAGE_NAMESPACE = "savedValues";
constexpr bool RW_MODE = false;
constexpr bool RO_MODE = true;

String siebtraegerNameKey(uint8_t index)
{
  return String("stName") + String(index);
}
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

void coffeeStorageSaveSiebtraegerName(
  Preferences& preferences,
  uint8_t index,
  const String& name)
{
  preferences.begin(STORAGE_NAMESPACE, RW_MODE);
  preferences.putString(siebtraegerNameKey(index).c_str(), name);
  preferences.end();
}

void coffeeStorageLoadSiebtraegerNames(
  Preferences& preferences,
  String* names,
  size_t count)
{
  preferences.begin(STORAGE_NAMESPACE, RO_MODE);
  for (size_t i = 0; i < count; ++i) {
    const String key = siebtraegerNameKey(static_cast<uint8_t>(i));
    if (preferences.isKey(key.c_str())) {
      names[i] = preferences.getString(key.c_str(), names[i]);
    }
  }
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


namespace {

void loadBytesIfPresent(Preferences& preferences, const char* key, void* data, size_t expectedByteCount)
{
  const size_t storedByteCount = preferences.getBytesLength(key);
  if (storedByteCount == 0) {
    return;
  }
  preferences.getBytes(key, data, min(storedByteCount, expectedByteCount));
}
}

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
  uint32_t& lastTimeFilterWechselNTP)
{
  preferences.begin(STORAGE_NAMESPACE, RO_MODE);
  const bool nvsInitialised = preferences.isKey("nvsInitialised");

  if (!nvsInitialised) {
    preferences.end();
    preferences.begin(STORAGE_NAMESPACE, RW_MODE);

    preferences.putBytes("savedWeightST", weightST, weightSTByteCount);
    preferences.putBytes("savedWeightTri", weightTrichter, weightTrichterByteCount);
    preferences.putBytes("savedWeightGef", weightGefaess, weightGefaessByteCount);
    preferences.putBytes("svdSetWeightST", setWeightST, setWeightSTByteCount);
    preferences.putFloat("savedCalFact", calFactor);
    preferences.putShort("savedSelST", selectedST);
    preferences.putShort("savedSelGef", selectedGefaess);
    preferences.putBool("savedAutoDetect", autoDetect);
    preferences.putFloat("savedCalWeight", setWeightCalibration);
    preferences.putFloat("grndWghtFrvr", groundWeightForever);
    preferences.putFloat("grndWghtCln", groundWeightSinceClean);
    preferences.putFloat("grndWghtKffm", groundWeightSinceMachineClean);
    preferences.putFloat("grndWghtFlt", groundWeightSinceFilterChange);
    preferences.putULong("shotsFrvr", shotCounterForever);
    preferences.putULong("shotsCln", shotCounterSinceClean);
    preferences.putULong("shotsKffm", shotCounterSinceMachineClean);
    preferences.putULong("shotsFlt", shotCounterSinceFilterChange);
    preferences.putULong("lstMhlRngng", lastTimeMuehlenReinigungNTP);
    preferences.putULong("lstKffmRngng", lastTimeKaffeemReinigungNTP);
    preferences.putULong("lstFltwchsl", lastTimeFilterWechselNTP);
    preferences.putBool("nvsInitialised", true);

    preferences.end();
    preferences.begin(STORAGE_NAMESPACE, RO_MODE);
  }

  loadBytesIfPresent(preferences, "savedWeightST", weightST, weightSTByteCount);
  loadBytesIfPresent(preferences, "savedWeightTri", weightTrichter, weightTrichterByteCount);
  loadBytesIfPresent(preferences, "svdSetWeightST", setWeightST, setWeightSTByteCount);
  loadBytesIfPresent(preferences, "savedWeightGef", weightGefaess, weightGefaessByteCount);

  calFactor = preferences.getFloat("savedCalFact", calFactor);
  selectedST = static_cast<uint8_t>(preferences.getShort("savedSelST", selectedST));
  selectedGefaess = static_cast<uint8_t>(preferences.getShort("savedSelGef", selectedGefaess));
  autoDetect = preferences.getBool("savedAutoDetect", autoDetect);
  setWeightCalibration = preferences.getFloat("savedCalWeight", setWeightCalibration);
  groundWeightForever = preferences.getFloat("grndWghtFrvr", preferences.getULong("grndWghtFrvr", 0));
  groundWeightSinceClean = preferences.getFloat("grndWghtCln", preferences.getULong("grndWghtCln", 0));
  groundWeightSinceMachineClean = preferences.getFloat("grndWghtKffm", 0.0f);
  groundWeightSinceFilterChange = preferences.getFloat("grndWghtFlt", 0.0f);
  shotCounterForever = preferences.getULong("shotsFrvr", 0);
  shotCounterSinceClean = preferences.getULong("shotsCln", 0);
  shotCounterSinceMachineClean = preferences.getULong("shotsKffm", 0);
  shotCounterSinceFilterChange = preferences.getULong("shotsFlt", 0);
  const bool statsV2Initialised = preferences.getBool("statsV2Init", false);
  lastTimeMuehlenReinigungNTP = preferences.getULong("lstMhlRngng", 0);
  lastTimeKaffeemReinigungNTP = preferences.getULong("lstKffmRngng", 0);
  lastTimeFilterWechselNTP = preferences.getULong("lstFltwchsl", 0);

  preferences.end();

  if (!statsV2Initialised) {
    groundWeightSinceMachineClean = 0.0f;
    groundWeightSinceFilterChange = 0.0f;
    shotCounterSinceMachineClean = 0;
    shotCounterSinceFilterChange = 0;

    preferences.begin(STORAGE_NAMESPACE, RW_MODE);
    preferences.putFloat("grndWghtKffm", groundWeightSinceMachineClean);
    preferences.putFloat("grndWghtFlt", groundWeightSinceFilterChange);
    preferences.putULong("shotsKffm", shotCounterSinceMachineClean);
    preferences.putULong("shotsFlt", shotCounterSinceFilterChange);
    preferences.putBool("statsV2Init", true);
    preferences.end();
  }

  return nvsInitialised;
}
