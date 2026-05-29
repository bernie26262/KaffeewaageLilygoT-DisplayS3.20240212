#include "t4s3_settings.h"

#include <Preferences.h>
#include <math.h>
#include <ctype.h>
#include <string.h>

namespace {

Preferences prefs;
bool prefsReady = false;

constexpr const char *NAMESPACE = "t4s3ui";

constexpr uint16_t DEFAULT_TIMEOUT_MINUTES = 5;
constexpr int32_t DEFAULT_TARGET_BODENLOS_TENTHS = 180;
constexpr int32_t DEFAULT_TARGET_1ER_TENTHS = 90;
constexpr int32_t DEFAULT_TARGET_2ER_TENTHS = 180;
constexpr int32_t DEFAULT_TARGET_CUSTOM_TENTHS = 180;
constexpr const char *DEFAULT_SIEBTRAEGER_NAMES[T4S3_SIEBTRAEGER_SLOT_COUNT] = {
    "Bodenloser ST",
    "1er-Siebträger",
    "2er-Siebträger",
    "Custom ST",
};
constexpr int32_t DEFAULT_TARGET_STEP_TENTHS = 5;
constexpr int32_t GEFAESS_WEIGHT_UNSET_TENTHS = -1;

void fill_defaults(T4S3UiSettings &settings)
{
    settings.screenTimeoutMinutes = DEFAULT_TIMEOUT_MINUTES;
    settings.autodetectEnabled = true;
    settings.selectedSiebtraeger = 0;
    settings.targetTenthsBySiebtraeger[0] = DEFAULT_TARGET_BODENLOS_TENTHS;
    settings.targetTenthsBySiebtraeger[1] = DEFAULT_TARGET_1ER_TENTHS;
    settings.targetTenthsBySiebtraeger[2] = DEFAULT_TARGET_2ER_TENTHS;
    settings.targetTenthsBySiebtraeger[3] = DEFAULT_TARGET_CUSTOM_TENTHS;
    for (uint8_t i = 0; i < T4S3_SIEBTRAEGER_SLOT_COUNT; ++i) {
        strlcpy(settings.siebtraegerNames[i], DEFAULT_SIEBTRAEGER_NAMES[i], T4S3_SIEBTRAEGER_NAME_LEN);
    }
    settings.targetStepTenths = DEFAULT_TARGET_STEP_TENTHS;
    settings.totalShots = 0;
    settings.machineShots = 0;
    settings.grinderShots = 0;
    settings.filterShots = 0;
    settings.totalGramsTenths = 0;
    settings.machineGramsTenths = 0;
    settings.grinderGramsTenths = 0;
    settings.filterGramsTenths = 0;
    for (uint8_t i = 0; i < T4S3_GEFAESS_SLOT_COUNT; ++i) {
        settings.gefaessWeightTenths[i] = GEFAESS_WEIGHT_UNSET_TENTHS;
    }
}

uint16_t sanitize_timeout(uint16_t minutes)
{
    switch (minutes) {
    case 1:
    case 5:
    case 10:
    case 30:
        return minutes;
    default:
        return DEFAULT_TIMEOUT_MINUTES;
    }
}

int32_t sanitize_target(int32_t tenths, int32_t fallback)
{
    if (tenths < 50 || tenths > 600) {
        return fallback;
    }
    return tenths;
}

int32_t sanitize_step(int32_t tenths)
{
    switch (tenths) {
    case 1:
    case 5:
    case 10:
        return tenths;
    default:
        return DEFAULT_TARGET_STEP_TENTHS;
    }
}

int32_t sanitize_gefaess_weight(int32_t tenths)
{
    // -1 bedeutet: Gefäß noch nicht eingemessen.
    // Plausibler Bereich für Gefäße: 0,1 g bis 1000,0 g.
    if (tenths == GEFAESS_WEIGHT_UNSET_TENTHS) {
        return GEFAESS_WEIGHT_UNSET_TENTHS;
    }
    if (tenths < 1 || tenths > 10000) {
        return GEFAESS_WEIGHT_UNSET_TENTHS;
    }
    return tenths;
}

void sanitize_siebtraeger_name(char *dst, size_t dstLen, const String &raw, const char *fallback)
{
    if (!dst || dstLen == 0) {
        return;
    }

    String cleaned = raw;
    cleaned.trim();
    cleaned.replace("\r", " ");
    cleaned.replace("\n", " ");
    cleaned.replace("\t", " ");

    while (cleaned.indexOf("  ") >= 0) {
        cleaned.replace("  ", " ");
    }

    if (cleaned.length() == 0) {
        cleaned = fallback ? fallback : "Siebtraeger";
    }

    if (cleaned.length() >= dstLen) {
        cleaned = cleaned.substring(0, dstLen - 1);
        cleaned.trim();
    }

    strlcpy(dst, cleaned.c_str(), dstLen);
}

}  // namespace

void t4s3_settings_begin()
{
    if (prefsReady) {
        return;
    }

    prefsReady = prefs.begin(NAMESPACE, false);
    if (!prefsReady) {
        Serial.println("[T4S3][Settings] Preferences begin failed");
    }
}

void t4s3_settings_load(T4S3UiSettings &settings)
{
    fill_defaults(settings);
    t4s3_settings_begin();

    if (!prefsReady) {
        Serial.println("[T4S3][Settings] using defaults (NVS unavailable)");
        return;
    }

    settings.screenTimeoutMinutes =
        sanitize_timeout(prefs.getUShort("timeoutMin", settings.screenTimeoutMinutes));

    settings.autodetectEnabled =
        prefs.getBool("autodetect", settings.autodetectEnabled);

    settings.selectedSiebtraeger =
        prefs.getUChar("selST", settings.selectedSiebtraeger);
    if (settings.selectedSiebtraeger >= 4) {
        settings.selectedSiebtraeger = 0;
    }

    settings.targetTenthsBySiebtraeger[0] =
        sanitize_target(prefs.getInt("targetST0", settings.targetTenthsBySiebtraeger[0]),
                        DEFAULT_TARGET_BODENLOS_TENTHS);
    settings.targetTenthsBySiebtraeger[1] =
        sanitize_target(prefs.getInt("targetST1", settings.targetTenthsBySiebtraeger[1]),
                        DEFAULT_TARGET_1ER_TENTHS);
    settings.targetTenthsBySiebtraeger[2] =
        sanitize_target(prefs.getInt("targetST2", settings.targetTenthsBySiebtraeger[2]),
                        DEFAULT_TARGET_2ER_TENTHS);
    settings.targetTenthsBySiebtraeger[3] =
        sanitize_target(prefs.getInt("targetST3", settings.targetTenthsBySiebtraeger[3]),
                        DEFAULT_TARGET_CUSTOM_TENTHS);

    for (uint8_t i = 0; i < T4S3_SIEBTRAEGER_SLOT_COUNT; ++i) {
        char key[12];
        snprintf(key, sizeof(key), "stName%u", i);
        sanitize_siebtraeger_name(settings.siebtraegerNames[i],
                                  T4S3_SIEBTRAEGER_NAME_LEN,
                                  prefs.getString(key, DEFAULT_SIEBTRAEGER_NAMES[i]),
                                  DEFAULT_SIEBTRAEGER_NAMES[i]);
    }

    settings.targetStepTenths =
        sanitize_step(prefs.getInt("targetStep", settings.targetStepTenths));

    
    settings.totalShots = prefs.getUInt("shotsTotal", settings.totalShots);
    settings.machineShots = prefs.getUInt("shotsMach", settings.machineShots);
    settings.grinderShots = prefs.getUInt("shotsGrind", settings.grinderShots);
    settings.filterShots = prefs.getUInt("shotsFilter", settings.filterShots);

    settings.totalGramsTenths = prefs.getInt("gramsTotal", settings.totalGramsTenths);
    settings.machineGramsTenths = prefs.getInt("gramsMach", settings.machineGramsTenths);
    settings.grinderGramsTenths = prefs.getInt("gramsGrind", settings.grinderGramsTenths);
    settings.filterGramsTenths = prefs.getInt("gramsFilter", settings.filterGramsTenths);

    for (uint8_t i = 0; i < T4S3_GEFAESS_SLOT_COUNT; ++i) {
        char key[12];
        snprintf(key, sizeof(key), "gefW%u", i);
        settings.gefaessWeightTenths[i] =
            sanitize_gefaess_weight(prefs.getInt(key, settings.gefaessWeightTenths[i]));
    }

    Serial.printf("[T4S3][Settings] loaded: timeout=%u min, auto=%u, ST=%u, targets=%ld/%ld/%ld/%ld, step=%ld\n",
                  settings.screenTimeoutMinutes,
                  settings.autodetectEnabled ? 1 : 0,
                  settings.selectedSiebtraeger,
                  static_cast<long>(settings.targetTenthsBySiebtraeger[0]),
                  static_cast<long>(settings.targetTenthsBySiebtraeger[1]),
                  static_cast<long>(settings.targetTenthsBySiebtraeger[2]),
                  static_cast<long>(settings.targetTenthsBySiebtraeger[3]),
                  static_cast<long>(settings.targetStepTenths));
}

void t4s3_settings_save(const T4S3UiSettings &settings)
{
    t4s3_settings_begin();

    if (!prefsReady) {
        Serial.println("[T4S3][Settings] save skipped (NVS unavailable)");
        return;
    }

    prefs.putUShort("timeoutMin", sanitize_timeout(settings.screenTimeoutMinutes));
    prefs.putBool("autodetect", settings.autodetectEnabled);
    prefs.putUChar("selST", settings.selectedSiebtraeger < 4 ? settings.selectedSiebtraeger : 0);
    prefs.putInt("targetST0", sanitize_target(settings.targetTenthsBySiebtraeger[0], DEFAULT_TARGET_BODENLOS_TENTHS));
    prefs.putInt("targetST1", sanitize_target(settings.targetTenthsBySiebtraeger[1], DEFAULT_TARGET_1ER_TENTHS));
    prefs.putInt("targetST2", sanitize_target(settings.targetTenthsBySiebtraeger[2], DEFAULT_TARGET_2ER_TENTHS));
    prefs.putInt("targetST3", sanitize_target(settings.targetTenthsBySiebtraeger[3], DEFAULT_TARGET_CUSTOM_TENTHS));
    for (uint8_t i = 0; i < T4S3_SIEBTRAEGER_SLOT_COUNT; ++i) {
        char key[12];
        snprintf(key, sizeof(key), "stName%u", i);
        char cleanName[T4S3_SIEBTRAEGER_NAME_LEN];
        sanitize_siebtraeger_name(cleanName,
                                  sizeof(cleanName),
                                  String(settings.siebtraegerNames[i]),
                                  DEFAULT_SIEBTRAEGER_NAMES[i]);
        prefs.putString(key, cleanName);
    }
    prefs.putInt("targetStep", sanitize_step(settings.targetStepTenths));

    
    prefs.putUInt("shotsTotal", settings.totalShots);
    prefs.putUInt("shotsMach", settings.machineShots);
    prefs.putUInt("shotsGrind", settings.grinderShots);
    prefs.putUInt("shotsFilter", settings.filterShots);

    prefs.putInt("gramsTotal", settings.totalGramsTenths);
    prefs.putInt("gramsMach", settings.machineGramsTenths);
    prefs.putInt("gramsGrind", settings.grinderGramsTenths);
    prefs.putInt("gramsFilter", settings.filterGramsTenths);

    for (uint8_t i = 0; i < T4S3_GEFAESS_SLOT_COUNT; ++i) {
        char key[12];
        snprintf(key, sizeof(key), "gefW%u", i);
        prefs.putInt(key, sanitize_gefaess_weight(settings.gefaessWeightTenths[i]));
    }

    Serial.println("[T4S3][Settings] saved UI settings");
}


void t4s3_settings_load_maintenance(uint32_t &machineEpoch,
                                    uint32_t &grinderEpoch,
                                    uint32_t &filterEpoch)
{
    t4s3_settings_begin();

    machineEpoch = 0;
    grinderEpoch = 0;
    filterEpoch = 0;

    if (!prefsReady) {
        Serial.println("[T4S3][Settings] maintenance load skipped (NVS unavailable)");
        return;
    }

    machineEpoch = prefs.getULong("lastKaffee", 0);
    grinderEpoch = prefs.getULong("lastMuehle", 0);
    filterEpoch = prefs.getULong("lastFilter", 0);

    Serial.printf("[T4S3][Settings] maintenance loaded: machine=%lu, grinder=%lu, filter=%lu\n",
                  static_cast<unsigned long>(machineEpoch),
                  static_cast<unsigned long>(grinderEpoch),
                  static_cast<unsigned long>(filterEpoch));
}

void t4s3_settings_save_maintenance(uint32_t machineEpoch,
                                    uint32_t grinderEpoch,
                                    uint32_t filterEpoch)
{
    t4s3_settings_begin();

    if (!prefsReady) {
        Serial.println("[T4S3][Settings] maintenance save skipped (NVS unavailable)");
        return;
    }

    prefs.putULong("lastKaffee", machineEpoch);
    prefs.putULong("lastMuehle", grinderEpoch);
    prefs.putULong("lastFilter", filterEpoch);

    Serial.printf("[T4S3][Settings] maintenance saved: machine=%lu, grinder=%lu, filter=%lu\n",
                  static_cast<unsigned long>(machineEpoch),
                  static_cast<unsigned long>(grinderEpoch),
                  static_cast<unsigned long>(filterEpoch));
}


float t4s3_settings_load_hx711_cal_factor(float fallback)
{
    t4s3_settings_begin();

    if (!prefsReady) {
        Serial.println("[T4S3][Settings] HX711 calibration load skipped (NVS unavailable)");
        return fallback;
    }

    const float factor = prefs.getFloat("hxCal", fallback);
    if (!isfinite(factor) || factor < 1.0f || factor > 1000000.0f) {
        return fallback;
    }

    Serial.printf("[T4S3][Settings] HX711 calibration factor loaded: %.4f raw/g\n", factor);
    return factor;
}

void t4s3_settings_save_hx711_cal_factor(float factor)
{
    t4s3_settings_begin();

    if (!prefsReady) {
        Serial.println("[T4S3][Settings] HX711 calibration save skipped (NVS unavailable)");
        return;
    }

    if (!isfinite(factor) || factor < 1.0f || factor > 1000000.0f) {
        Serial.printf("[T4S3][Settings] HX711 calibration factor rejected: %.4f\n", factor);
        return;
    }

    prefs.putFloat("hxCal", factor);
    Serial.printf("[T4S3][Settings] HX711 calibration factor saved: %.4f raw/g\n", factor);
}
