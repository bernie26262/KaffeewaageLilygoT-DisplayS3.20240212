#include "t4s3_settings.h"

#include <Preferences.h>

namespace {

Preferences prefs;
bool prefsReady = false;

constexpr const char *NAMESPACE = "t4s3ui";

constexpr uint16_t DEFAULT_TIMEOUT_MINUTES = 5;
constexpr int32_t DEFAULT_TARGET_BODENLOS_TENTHS = 180;
constexpr int32_t DEFAULT_TARGET_1ER_TENTHS = 90;
constexpr int32_t DEFAULT_TARGET_2ER_TENTHS = 180;
constexpr int32_t DEFAULT_TARGET_CUSTOM_TENTHS = 180;
constexpr int32_t DEFAULT_TARGET_STEP_TENTHS = 5;

void fill_defaults(T4S3UiSettings &settings)
{
    settings.screenTimeoutMinutes = DEFAULT_TIMEOUT_MINUTES;
    settings.autodetectEnabled = true;
    settings.selectedSiebtraeger = 0;
    settings.targetTenthsBySiebtraeger[0] = DEFAULT_TARGET_BODENLOS_TENTHS;
    settings.targetTenthsBySiebtraeger[1] = DEFAULT_TARGET_1ER_TENTHS;
    settings.targetTenthsBySiebtraeger[2] = DEFAULT_TARGET_2ER_TENTHS;
    settings.targetTenthsBySiebtraeger[3] = DEFAULT_TARGET_CUSTOM_TENTHS;
    settings.targetStepTenths = DEFAULT_TARGET_STEP_TENTHS;
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

    settings.targetStepTenths =
        sanitize_step(prefs.getInt("targetStep", settings.targetStepTenths));

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
    prefs.putInt("targetStep", sanitize_step(settings.targetStepTenths));

    Serial.println("[T4S3][Settings] saved UI settings");
}
