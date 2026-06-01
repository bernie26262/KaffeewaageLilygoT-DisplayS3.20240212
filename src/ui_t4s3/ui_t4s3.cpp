#include "ui_t4s3.h"
#include "coffee_ui_umlaut_font.h"

#include "../t4s3_wifi.h"
#include "../t4s3_time.h"
#include "../t4s3_settings.h"
#include "../t4s3_scale.h"
#include "../coffee_wifi.h"

#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr uint8_t kMaxGefaessSlots = 3;
constexpr uint8_t kMaxSiebtraegerSlots = 4;
constexpr uint8_t kScaleCalIncrementCount = 4;
constexpr int32_t kScaleCalIncrementsTenths[kScaleCalIncrementCount] = {10, 50, 100, 1000};
constexpr uint8_t kTotalsEditModeCount = 4;
constexpr uint8_t kTotalsEditStepCount = 4;
constexpr uint16_t kTotalsEditSteps[kTotalsEditStepCount] = {1, 5, 10, 100};
constexpr uint8_t kNoDetectedGefaess = 0xFF;
constexpr float kGefaessAutodetectToleranceGrams = 0.7f;
constexpr uint32_t kGefaessAutoTareDelayMs = 800;
constexpr float kGefaessRemovedThresholdGrams = 30.0f;

constexpr uint32_t COLOR_BG = 0x000000;
constexpr uint32_t COLOR_PANEL = 0x07120a;
constexpr uint32_t COLOR_GREEN = 0x2E8B57;
constexpr uint32_t COLOR_GREEN_DARK = 0x1F5F3D;
constexpr uint32_t COLOR_WHITE = 0xFFFFFF;
constexpr uint32_t COLOR_MUTED = 0xA8B8A8;
constexpr uint32_t COLOR_DIM = 0x5F705F;
constexpr uint32_t COLOR_AUTODETECT_LED_ON = 0x00FF4A;
constexpr uint32_t COLOR_AUTODETECT_LED_OFF = 0x232A26;
constexpr uint32_t COLOR_SAVE_DISABLED_BG = 0x263241;
constexpr uint32_t COLOR_SAVE_DISABLED_BORDER = 0x475569;
constexpr uint32_t COLOR_SAVE_DISABLED_TEXT = 0xB4C0D0;

constexpr uint16_t SCREEN_W = 600;
constexpr uint16_t SCREEN_H = 450;

enum class Page : uint8_t {
    Waage,
    Stoppuhr,
    Daten,
    DatenMahldaten,
    DatenSystem,
    Settings,
    SettingsWartung,
    SettingsWaage,
    SettingsTotals,
    SettingsWlan,
    SettingsSystem,
};

Page activePage = Page::Waage;
uint16_t screenWidth = SCREEN_W;
uint16_t screenHeight = SCREEN_H;

lv_obj_t *weightLabel = nullptr;
lv_obj_t *statusLabel = nullptr;
lv_obj_t *touchLabel = nullptr;
lv_obj_t *simLabel = nullptr;
lv_obj_t *timerLabel = nullptr;
lv_obj_t *timerButtonLabel = nullptr;
lv_obj_t *targetLabel = nullptr;
lv_obj_t *clockLabel = nullptr;
lv_obj_t *autodetectButtonLabel = nullptr;
lv_obj_t *autodetectStateLabel = nullptr;
lv_obj_t *autodetectLed = nullptr;
lv_obj_t *vesselLabel = nullptr;
lv_obj_t *gefaessStoredLabel = nullptr;
lv_obj_t *totalShotsLabel = nullptr;
lv_obj_t *shotsMachineLabel = nullptr;
lv_obj_t *shotsGrinderLabel = nullptr;
lv_obj_t *shotsFilterLabel = nullptr;
lv_obj_t *totalGramsLabel = nullptr;
lv_obj_t *gramsMachineLabel = nullptr;
lv_obj_t *gramsGrinderLabel = nullptr;
lv_obj_t *gramsFilterLabel = nullptr;
lv_obj_t *targetOverlay = nullptr;
lv_obj_t *targetOverlayValueLabel = nullptr;
lv_obj_t *targetOverlayStepLabel = nullptr;
lv_obj_t *vesselOverlay = nullptr;
lv_obj_t *gefaessManageOverlay = nullptr;
lv_obj_t *gefaessDeleteOverlay = nullptr;
lv_obj_t *gefaessManageLabels[T4S3_GEFAESS_SLOT_COUNT] = {nullptr, nullptr, nullptr};
uint8_t gefaessDeleteSlot = 0;
lv_obj_t *gefaessMeasureOverlay = nullptr;
lv_obj_t *gefaessMeasureWeightLabel = nullptr;
lv_obj_t *restartOverlay = nullptr;
lv_obj_t *wlanSetupOverlay = nullptr;
lv_obj_t *wlanSetupOverlayTitleLabel = nullptr;
lv_obj_t *wlanSetupOverlayMessageLabel = nullptr;
lv_obj_t *wlanSetupOverlayButtonLabel = nullptr;
bool wlanSetupOverlaySavedMode = false;
lv_obj_t *scaleCalibrationOverlay = nullptr;
lv_obj_t *scaleCalibrationWeightLabel = nullptr;
lv_obj_t *scaleCalibrationFactorLabel = nullptr;
lv_obj_t *scaleCalibrationTargetWeightLabel = nullptr;
lv_obj_t *scaleCalibrationIncrementLabel = nullptr;
uint8_t scaleCalibrationStep = 1;
int32_t scaleCalibrationTargetTenths = 2000;
uint8_t scaleCalibrationIncrementIndex = 2;
lv_obj_t *vesselOptionLabels[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t *screenTimeoutValueLabel = nullptr;
lv_obj_t *systemTimeStatusLabel = nullptr;
lv_obj_t *systemTimeLocalLabel = nullptr;
lv_obj_t *systemTimeZoneLabel = nullptr;
lv_obj_t *systemTimeSourceLabel = nullptr;
lv_obj_t *systemUptimeLabel = nullptr;
lv_obj_t *systemDataSsidLabel = nullptr;
lv_obj_t *systemDataIpLabel = nullptr;
lv_obj_t *systemDataSignalLabel = nullptr;
lv_obj_t *systemHx711RawLabel = nullptr;
lv_obj_t *systemHx711GramsLabel = nullptr;
lv_obj_t *headerWifiBars[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t *wlanStatusLabel = nullptr;
lv_obj_t *wlanSsidLabel = nullptr;
lv_obj_t *wlanIpLabel = nullptr;
lv_obj_t *wlanSignalLabel = nullptr;
lv_obj_t *wlanSetupApLabel = nullptr;
lv_obj_t *wlanWebUiLabel = nullptr;
lv_obj_t *wlanQualityBars[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t *maintenanceWarningButton = nullptr;
lv_obj_t *saveButton = nullptr;
lv_obj_t *totalsEditOverlay = nullptr;
lv_obj_t *totalsEditModeLabel = nullptr;
lv_obj_t *totalsEditShotsLabel = nullptr;
lv_obj_t *totalsEditGramsLabel = nullptr;
lv_obj_t *totalsEditStepLabel = nullptr;


uint32_t lastSimMs = 0;
uint32_t lastTimerMs = 0;
uint32_t timerBaseMs = 0;
uint32_t timerStartedMs = 0;
bool timerRunning = false;
int32_t simTenths = 0;
int8_t simDir = 1;
float hx711DisplayGrams = 0.0f;
bool hx711DisplayValid = false;
bool autodetectEnabled = true;
uint8_t autodetectDetectedGefaess = kNoDetectedGefaess;
uint8_t autodetectPendingGefaess = kNoDetectedGefaess;
uint8_t autodetectAutoTaredGefaess = kNoDetectedGefaess;
uint32_t autodetectPendingSinceMs = 0;
bool saveReady = false;
bool manualTareSaveArmed = false;
uint16_t demoTotalShots = 0;
uint16_t demoMachineShots = 0;
uint16_t demoGrinderShots = 0;
uint16_t demoFilterShots = 0;
int32_t demoTotalGramsTenths = 0;
int32_t demoMachineGramsTenths = 0;
int32_t demoGrinderGramsTenths = 0;
int32_t demoFilterGramsTenths = 0;
uint8_t totalsEditMode = 0;
uint8_t totalsEditStepIndex = 0;
bool totalsEditGramsMode = false;
uint16_t totalsEditDraftShots[kTotalsEditModeCount] = {0, 0, 0, 0};
int32_t totalsEditDraftGramsTenths[kTotalsEditModeCount] = {0, 0, 0, 0};
int32_t targetTenthsBySiebtraeger[4] = {180, 90, 180, 180};
char siebtraegerNames[T4S3_SIEBTRAEGER_SLOT_COUNT][T4S3_SIEBTRAEGER_NAME_LEN] = {
    "Bodenloser ST",
    "1er-Siebträger",
    "2er-Siebträger",
    "Custom ST",
};
int32_t gefaessWeightTenths[T4S3_GEFAESS_SLOT_COUNT] = {-1, -1, -1};
uint8_t gefaessMeasureStep = 0;
uint8_t gefaessMeasureSlot = 0;
uint8_t webSelectedGefaessSlot = 0;
bool webWizardActive = false;
int32_t gefaessMeasureTareTenths = 0;
int32_t gefaessMeasureValueTenths = 0;
int32_t demoTargetTenths = 180;
int32_t draftTargetTenths = 180;
int32_t targetStepTenths = 5;
int32_t draftTargetStepTenths = 5;
uint8_t currentVesselIndex = 0;
uint8_t draftVesselIndex = 0;
constexpr uint16_t SCREEN_TIMEOUT_MINUTES[] = {1, 5, 10, 30};
uint8_t screenTimeoutIndex = 1;
uint32_t lastUserActivityMs = 0;
bool suppressNextClick = false;
bool maintenanceDue = false;
uint32_t maintenanceMachineEpoch = 0;
uint32_t maintenanceGrinderEpoch = 0;
uint32_t maintenanceFilterEpoch = 0;
uint32_t maintenanceMachineIntervalSec = 864000UL;
uint32_t maintenanceGrinderIntervalSec = 2419200UL;
uint32_t maintenanceFilterIntervalSec = 7257600UL;
bool maintenanceMachineEnabled = true;
bool maintenanceGrinderEnabled = true;
bool maintenanceFilterEnabled = true;
lv_obj_t *maintenanceResetOverlay = nullptr;
const char *maintenancePendingAction = nullptr;
const char *maintenanceRequestedAction = nullptr;
uint32_t maintenanceResetRequestedAtMs = 0;
uint32_t maintenanceResetCooldownUntilMs = 0;
bool maintenanceRefreshPending = false;
lv_obj_t *maintenanceMachineLeftLabel = nullptr;
lv_obj_t *maintenanceMachineTimeLabel = nullptr;
lv_obj_t *maintenanceGrinderLeftLabel = nullptr;
lv_obj_t *maintenanceGrinderTimeLabel = nullptr;
lv_obj_t *maintenanceFilterLeftLabel = nullptr;
lv_obj_t *maintenanceFilterTimeLabel = nullptr;

void build_current_page();
void open_target_overlay();
void close_target_overlay(bool save);
void open_vessel_overlay();
void close_vessel_overlay(bool save);
void open_gefaess_manage_overlay();
void close_gefaess_manage_overlay();
void open_gefaess_delete_overlay(uint8_t slot);
void close_gefaess_delete_overlay();
void open_scale_calibration_overlay();
void close_scale_calibration_overlay();
void update_scale_calibration_display();
void update_gefaess_manage_display();
void open_gefaess_measure_overlay();
void close_gefaess_measure_overlay();
void render_gefaess_measure_overlay();
void update_gefaess_measure_weight_display();
void open_restart_overlay();
void close_restart_overlay();
void open_wlan_setup_overlay(bool savedMode);
void close_wlan_setup_overlay();
void update_wlan_setup_overlay_mode(bool savedMode);
void process_wlan_setup_overlay_state();
void open_totals_edit_overlay(bool editGrams);
void close_totals_edit_overlay(bool save);
void update_totals_edit_display();
void change_totals_edit_mode(int dir);
void adjust_totals_edit_shots(int dir);
void adjust_totals_edit_grams(int dir);
void change_totals_edit_step(int dir);


void open_maintenance_reset_overlay(const char *action);
void close_maintenance_reset_overlay();
void process_pending_maintenance_reset();
void perform_maintenance_reset(const char *action);
lv_obj_t *create_button(lv_obj_t *parent, const char *text, const char *action, int width, int height);
void style_panel(lv_obj_t *obj);
void style_label(lv_obj_t *obj, uint32_t color);
void update_status(const char *msg);
void update_autodetect_gefaess_preview();
void update_save_button_display();
void set_save_ready(bool ready);
void update_save_ready_from_weight();
bool recover_negative_startup_tare_if_needed();
int32_t current_save_weight_tenths();
void update_demo_stats_display();

void add_saved_dose(int32_t doseTenths);
void format_grams(char *buf, size_t len, int32_t tenths);
void format_grams_float(char *buf, size_t len, float grams);
void load_saved_ui_settings();
void save_current_ui_settings();

void reset_dynamic_labels()
{
    weightLabel = nullptr;
    statusLabel = nullptr;
    touchLabel = nullptr;
    simLabel = nullptr;
    timerLabel = nullptr;
    timerButtonLabel = nullptr;
    targetLabel = nullptr;
    clockLabel = nullptr;
    autodetectButtonLabel = nullptr;
    autodetectStateLabel = nullptr;
    autodetectLed = nullptr;
    vesselLabel = nullptr;
    gefaessStoredLabel = nullptr;
    totalShotsLabel = nullptr;
    shotsMachineLabel = nullptr;
    shotsGrinderLabel = nullptr;
    shotsFilterLabel = nullptr;
    totalGramsLabel = nullptr;
    gramsMachineLabel = nullptr;
    gramsGrinderLabel = nullptr;
    gramsFilterLabel = nullptr;
    targetOverlayValueLabel = nullptr;
    targetOverlayStepLabel = nullptr;
    gefaessMeasureWeightLabel = nullptr;
    scaleCalibrationWeightLabel = nullptr;
    scaleCalibrationFactorLabel = nullptr;
    scaleCalibrationTargetWeightLabel = nullptr;
    scaleCalibrationIncrementLabel = nullptr;
    screenTimeoutValueLabel = nullptr;
    systemTimeStatusLabel = nullptr;
    systemTimeLocalLabel = nullptr;
    systemTimeZoneLabel = nullptr;
    systemTimeSourceLabel = nullptr;
    systemUptimeLabel = nullptr;
    systemDataSsidLabel = nullptr;
    systemDataIpLabel = nullptr;
    systemDataSignalLabel = nullptr;
    systemHx711RawLabel = nullptr;
    systemHx711GramsLabel = nullptr;
    saveButton = nullptr;
    totalsEditOverlay = nullptr;
    wlanSetupOverlay = nullptr;
    wlanSetupOverlayTitleLabel = nullptr;
    wlanSetupOverlayMessageLabel = nullptr;
    wlanSetupOverlayButtonLabel = nullptr;
    totalsEditModeLabel = nullptr;
    totalsEditShotsLabel = nullptr;
    totalsEditGramsLabel = nullptr;
    totalsEditStepLabel = nullptr;
    wlanStatusLabel = nullptr;
    wlanSsidLabel = nullptr;
    wlanIpLabel = nullptr;
    wlanSignalLabel = nullptr;
    wlanSetupApLabel = nullptr;
    wlanWebUiLabel = nullptr;
    for (uint8_t i = 0; i < 4; ++i) {
        headerWifiBars[i] = nullptr;
        wlanQualityBars[i] = nullptr;
    }
    for (uint8_t i = 0; i < kMaxSiebtraegerSlots; ++i) {
        vesselOptionLabels[i] = nullptr;
    }
    for (uint8_t i = 0; i < kMaxGefaessSlots; ++i) {
        gefaessManageLabels[i] = nullptr;
    }
}

void set_text(lv_obj_t *obj, const char *text)
{
    if (!obj || !text) {
        return;
    }

    // LVGL-Objektzeiger koennen nach Overlays/Seitenwechseln veralten.
    // Ohne diese Pruefung kann lv_label_set_text() mit LoadProhibited crashen.
    if (!lv_obj_is_valid(obj)) {
        return;
    }

    lv_label_set_text(obj, text);
}

void set_text_if_changed(lv_obj_t *obj, const char *text)
{
    if (!obj || !text) {
        return;
    }

    if (!lv_obj_is_valid(obj)) {
        return;
    }

    const char *current = lv_label_get_text(obj);
    if (!current || strcmp(current, text) != 0) {
        lv_label_set_text(obj, text);
    }
}

uint16_t current_screen_timeout_minutes()
{
    return SCREEN_TIMEOUT_MINUTES[screenTimeoutIndex];
}

void update_screen_timeout_display()
{
    if (!screenTimeoutValueLabel) {
        return;
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%u min", current_screen_timeout_minutes());
    lv_label_set_text(screenTimeoutValueLabel, buf);
}
void update_system_time_display()
{
    if (!systemTimeStatusLabel && !systemTimeLocalLabel && !systemTimeZoneLabel && !systemTimeSourceLabel) {
        return;
    }

    const bool valid = t4s3_time_is_valid();
    set_text(systemTimeStatusLabel, valid ? "synchronisiert" : "warte auf NTP");
    if (systemTimeStatusLabel) {
        lv_obj_set_style_text_color(systemTimeStatusLabel, lv_color_hex(valid ? COLOR_GREEN : COLOR_MUTED), 0);
    }

    char buf[32];
    t4s3_time_format_local(buf, sizeof(buf));
    set_text(systemTimeLocalLabel, buf);
    set_text(systemTimeZoneLabel, t4s3_time_zone_label());
    set_text(systemTimeSourceLabel, t4s3_time_source_label());
}

void format_uptime(char *buf, size_t len, uint32_t uptimeMs)
{
    uint32_t totalSeconds = uptimeMs / 1000UL;
    const uint32_t days = totalSeconds / 86400UL;
    totalSeconds %= 86400UL;
    const uint32_t hours = totalSeconds / 3600UL;
    totalSeconds %= 3600UL;
    const uint32_t minutes = totalSeconds / 60UL;
    const uint32_t seconds = totalSeconds % 60UL;

    if (days == 0) {
        snprintf(buf, len, "%02lu:%02lu:%02lu",
                 static_cast<unsigned long>(hours),
                 static_cast<unsigned long>(minutes),
                 static_cast<unsigned long>(seconds));
    } else {
        snprintf(buf, len, "%lu %s %02lu:%02lu:%02lu",
                 static_cast<unsigned long>(days),
                 days == 1 ? "Tag" : "Tage",
                 static_cast<unsigned long>(hours),
                 static_cast<unsigned long>(minutes),
                 static_cast<unsigned long>(seconds));
    }
}

void update_system_uptime_display()
{
    char buf[48];
    format_uptime(buf, sizeof(buf), millis());
    set_text(systemUptimeLabel, buf);
}

void update_data_system_wifi_display()
{
    if (!systemDataSsidLabel && !systemDataIpLabel && !systemDataSignalLabel) {
        return;
    }

    T4S3WifiStatus wifi;
    t4s3_wifi_get_status(wifi);

    set_text_if_changed(systemDataSsidLabel, wifi.ssid);
    set_text_if_changed(systemDataIpLabel, wifi.ip);
    set_text_if_changed(systemDataSignalLabel, wifi.signal);
}

void update_hx711_raw_display(int32_t rawValue)
{
    // RAW bleibt intern/seriell nutzbar, wird aber nicht mehr dauerhaft im HMI angezeigt.
    (void)rawValue;
}

void update_hx711_grams_display(float grams, bool valid)
{
    hx711DisplayGrams = grams;
    hx711DisplayValid = valid;

    char buf[32];
    if (!valid) {
        snprintf(buf, sizeof(buf), "-");
    } else {
        format_grams_float(buf, sizeof(buf), grams);
    }

    set_text_if_changed(systemHx711GramsLabel, buf);
    set_text_if_changed(scaleCalibrationWeightLabel, buf);
    update_gefaess_measure_weight_display();

    // First integration step: show the real HX711 DISPLAY value on the
    // main scale page, but leave save/autodetect logic untouched for now.
    if (valid) {
        set_text_if_changed(weightLabel, buf);
        if (autodetectDetectedGefaess == kNoDetectedGefaess) {
            set_text_if_changed(simLabel, "HX711-Gewicht aktiv");
        }
    }

    if (recover_negative_startup_tare_if_needed()) {
        return;
    }

    update_autodetect_gefaess_preview();
    update_save_ready_from_weight();
}


void change_screen_timeout(int8_t delta)
{
    const int8_t next = static_cast<int8_t>(screenTimeoutIndex) + delta;
    if (next < 0 || next >= static_cast<int8_t>(sizeof(SCREEN_TIMEOUT_MINUTES) / sizeof(SCREEN_TIMEOUT_MINUTES[0]))) {
        return;
    }

    screenTimeoutIndex = static_cast<uint8_t>(next);
    update_screen_timeout_display();

    char msg[64];
    snprintf(msg, sizeof(msg), "Bildschirmtimeout auf %u min gesetzt", current_screen_timeout_minutes());
    update_status(msg);

    save_current_ui_settings();
}



void timeout_index_from_minutes(uint16_t minutes)
{
    for (uint8_t i = 0; i < sizeof(SCREEN_TIMEOUT_MINUTES) / sizeof(SCREEN_TIMEOUT_MINUTES[0]); ++i) {
        if (SCREEN_TIMEOUT_MINUTES[i] == minutes) {
            screenTimeoutIndex = i;
            return;
        }
    }

    screenTimeoutIndex = 1;  // default: 5 min
}

void save_current_ui_settings()
{
    T4S3UiSettings settings;
    settings.screenTimeoutMinutes = current_screen_timeout_minutes();
    settings.autodetectEnabled = autodetectEnabled;
    settings.selectedSiebtraeger = currentVesselIndex;
    settings.targetTenthsBySiebtraeger[0] = targetTenthsBySiebtraeger[0];
    settings.targetTenthsBySiebtraeger[1] = targetTenthsBySiebtraeger[1];
    settings.targetTenthsBySiebtraeger[2] = targetTenthsBySiebtraeger[2];
    settings.targetTenthsBySiebtraeger[3] = targetTenthsBySiebtraeger[3];
    for (uint8_t i = 0; i < T4S3_SIEBTRAEGER_SLOT_COUNT; ++i) {
        strlcpy(settings.siebtraegerNames[i], siebtraegerNames[i], T4S3_SIEBTRAEGER_NAME_LEN);
    }
    settings.targetStepTenths = targetStepTenths;

    for (uint8_t i = 0; i < T4S3_GEFAESS_SLOT_COUNT; ++i) {
        settings.gefaessWeightTenths[i] = gefaessWeightTenths[i];
    }

    settings.totalShots = demoTotalShots;
    settings.machineShots = demoMachineShots;
    settings.grinderShots = demoGrinderShots;
    settings.filterShots = demoFilterShots;
    settings.totalGramsTenths = demoTotalGramsTenths;
    settings.machineGramsTenths = demoMachineGramsTenths;
    settings.grinderGramsTenths = demoGrinderGramsTenths;
    settings.filterGramsTenths = demoFilterGramsTenths;
    settings.maintenanceMachineIntervalSec = maintenanceMachineIntervalSec;
    settings.maintenanceGrinderIntervalSec = maintenanceGrinderIntervalSec;
    settings.maintenanceFilterIntervalSec = maintenanceFilterIntervalSec;
    settings.maintenanceMachineEnabled = maintenanceMachineEnabled;
    settings.maintenanceGrinderEnabled = maintenanceGrinderEnabled;
    settings.maintenanceFilterEnabled = maintenanceFilterEnabled;

    t4s3_settings_save(settings);
}

void load_saved_ui_settings()
{
    T4S3UiSettings settings;
    t4s3_settings_load(settings);

    timeout_index_from_minutes(settings.screenTimeoutMinutes);
    autodetectEnabled = settings.autodetectEnabled;

    currentVesselIndex = settings.selectedSiebtraeger < 4 ? settings.selectedSiebtraeger : 0;
    draftVesselIndex = currentVesselIndex;

    for (uint8_t i = 0; i < 4; ++i) {
        targetTenthsBySiebtraeger[i] = settings.targetTenthsBySiebtraeger[i];
    }
    for (uint8_t i = 0; i < T4S3_SIEBTRAEGER_SLOT_COUNT; ++i) {
        strlcpy(siebtraegerNames[i], settings.siebtraegerNames[i], T4S3_SIEBTRAEGER_NAME_LEN);
    }

    targetStepTenths = settings.targetStepTenths;
    draftTargetStepTenths = targetStepTenths;

    demoTargetTenths = targetTenthsBySiebtraeger[currentVesselIndex];
    draftTargetTenths = demoTargetTenths;

    for (uint8_t i = 0; i < T4S3_GEFAESS_SLOT_COUNT; ++i) {
        gefaessWeightTenths[i] = settings.gefaessWeightTenths[i];
    }

    demoTotalShots = settings.totalShots;
    demoMachineShots = settings.machineShots;
    demoGrinderShots = settings.grinderShots;
    demoFilterShots = settings.filterShots;
    demoTotalGramsTenths = settings.totalGramsTenths;
    demoMachineGramsTenths = settings.machineGramsTenths;
    demoGrinderGramsTenths = settings.grinderGramsTenths;
    demoFilterGramsTenths = settings.filterGramsTenths;
    maintenanceMachineIntervalSec = settings.maintenanceMachineIntervalSec;
    maintenanceGrinderIntervalSec = settings.maintenanceGrinderIntervalSec;
    maintenanceFilterIntervalSec = settings.maintenanceFilterIntervalSec;
    maintenanceMachineEnabled = settings.maintenanceMachineEnabled;
    maintenanceGrinderEnabled = settings.maintenanceGrinderEnabled;
    maintenanceFilterEnabled = settings.maintenanceFilterEnabled;
}
bool consume_suppressed_click()
{
    if (!suppressNextClick) {
        return false;
    }

    suppressNextClick = false;
    lastUserActivityMs = millis();
    update_status("Display aufgeweckt - erste Berührung ignoriert");
    return true;
}

const char *vessel_name(uint8_t index)
{
    if (index < T4S3_SIEBTRAEGER_SLOT_COUNT && siebtraegerNames[index][0] != '\0') {
        return siebtraegerNames[index];
    }
    return "Siebtraeger";
}

void update_autodetect_display()
{
    const bool autodetectEffective = autodetectEnabled && !webWizardActive;
    set_text(autodetectStateLabel, webWizardActive ? "Auto Pause" : "Auto");
    if (autodetectLed) {
        lv_obj_set_style_bg_color(autodetectLed, lv_color_hex(autodetectEffective ? COLOR_AUTODETECT_LED_ON : COLOR_AUTODETECT_LED_OFF), 0);
    }
    if (autodetectButtonLabel && lv_obj_is_valid(autodetectButtonLabel)) {
        lv_label_set_text(autodetectButtonLabel, webWizardActive ? "Autodetect: PAUSE" : (autodetectEnabled ? "Autodetect: AN" : "Autodetect: AUS"));
    }
}

void update_save_button_display()
{
    if (!saveButton || !lv_obj_is_valid(saveButton)) {
        saveButton = nullptr;
        return;
    }

    const uint32_t bg = saveReady ? COLOR_GREEN : COLOR_SAVE_DISABLED_BG;
    const uint32_t border = saveReady ? COLOR_GREEN : COLOR_SAVE_DISABLED_BORDER;
    const uint32_t text = saveReady ? COLOR_WHITE : COLOR_SAVE_DISABLED_TEXT;

    lv_obj_clear_state(saveButton, LV_STATE_DISABLED);

    lv_obj_set_style_bg_color(saveButton, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(saveButton, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(saveButton, lv_color_hex(border), 0);
    lv_obj_set_style_border_opa(saveButton, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(saveButton, LV_OPA_COVER, 0);

    if (saveReady) {
        lv_obj_add_flag(saveButton, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_clear_flag(saveButton, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t *label = lv_obj_get_child(saveButton, 0);
    if (label && lv_obj_is_valid(label)) {
        lv_obj_set_style_text_color(label, lv_color_hex(text), 0);
        lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    }
}

void set_save_ready(bool ready)
{
    if (saveReady == ready) {
        update_save_button_display();
        return;
    }

    saveReady = ready;
    update_save_button_display();
}

bool recover_negative_startup_tare_if_needed()
{
    const bool blocked =
        activePage != Page::Waage ||
        !hx711DisplayValid ||
        !t4s3_scale_is_ready() ||
        !t4s3_scale_is_stable() ||
        vesselOverlay ||
        gefaessManageOverlay ||
        gefaessMeasureOverlay ||
        scaleCalibrationOverlay ||
        maintenanceResetOverlay ||
        restartOverlay ||
        wlanSetupOverlay ||
        webWizardActive;

    if (blocked) {
        return false;
    }

    // Normales Abheben nach eigener Auto-/Manuell-Tara wird bereits in den
    // Modus-State-Machines behandelt. Diese Recovery ist fuer den Startfall:
    // ESP startet mit Gefaess auf der Waage und steht deshalb bei 0,0 g.
    if (autodetectAutoTaredGefaess != kNoDetectedGefaess || manualTareSaveArmed || saveReady) {
        return false;
    }

    if (hx711DisplayGrams > -kGefaessRemovedThresholdGrams) {
        return false;
    }

    set_save_ready(false);
    autodetectAutoTaredGefaess = kNoDetectedGefaess;
    autodetectDetectedGefaess = kNoDetectedGefaess;
    autodetectPendingGefaess = kNoDetectedGefaess;
    autodetectPendingSinceMs = 0;

    if (t4s3_scale_tare()) {
        hx711DisplayGrams = 0.0f;
        hx711DisplayValid = true;
        set_text(weightLabel, "0,0 g");
        set_text_if_changed(systemHx711GramsLabel, "0,0 g");
        set_text_if_changed(scaleCalibrationWeightLabel, "0,0 g");
        set_text_if_changed(simLabel, "HX711-Gewicht aktiv");
        update_status("Negatives Gewicht erkannt - Tara gesetzt");
    } else {
        update_status("Tara nach negativem Gewicht fehlgeschlagen");
    }

    return true;
}

void update_save_ready_from_weight()
{
    if (!hx711DisplayValid ||
        !t4s3_scale_is_ready() ||
        !t4s3_scale_is_stable()) {
        return;
    }

    if (autodetectEnabled) {
        if (autodetectAutoTaredGefaess == kNoDetectedGefaess) {
            return;
        }

        if (!saveReady && fabsf(hx711DisplayGrams) <= 0.2f) {
            set_save_ready(true);
            update_status("Save bereit");
        }
        return;
    }

    if (!manualTareSaveArmed) {
        return;
    }

    if (saveReady && hx711DisplayGrams <= -kGefaessRemovedThresholdGrams) {
        if (t4s3_scale_tare()) {
            hx711DisplayGrams = 0.0f;
            hx711DisplayValid = true;
            set_text(weightLabel, "0,0 g");
            set_text_if_changed(systemHx711GramsLabel, "0,0 g");
            set_text_if_changed(scaleCalibrationWeightLabel, "0,0 g");
            set_text_if_changed(simLabel, "HX711-Gewicht aktiv");
            update_status("Gewicht entfernt - Tara gesetzt");
        } else {
            update_status("Tara nach Entfernen fehlgeschlagen");
        }

        manualTareSaveArmed = false;
        set_save_ready(false);
        return;
    }

    if (!saveReady && fabsf(hx711DisplayGrams) <= 0.2f) {
        set_save_ready(true);
        update_status("Save bereit");
    }
}

void update_autodetect_gefaess_preview()
{
    const bool blocked =
        activePage != Page::Waage ||
        !autodetectEnabled ||
        !hx711DisplayValid ||
        !t4s3_scale_is_ready() ||
        !t4s3_scale_is_stable() ||
        vesselOverlay ||
        gefaessManageOverlay ||
        gefaessMeasureOverlay ||
        scaleCalibrationOverlay ||
        webWizardActive;

    const uint32_t now = millis();

    if (blocked) {
        autodetectPendingGefaess = kNoDetectedGefaess;
        autodetectPendingSinceMs = 0;
        if (autodetectAutoTaredGefaess == kNoDetectedGefaess &&
            autodetectDetectedGefaess != kNoDetectedGefaess) {
            autodetectDetectedGefaess = kNoDetectedGefaess;
            update_status("Autodetect aktiv - kein Gefäß erkannt");
        }
        return;
    }

    const float currentGrams = hx711DisplayGrams;

    if (autodetectAutoTaredGefaess != kNoDetectedGefaess) {
        // Nach einer Auto-Tara liegt das aufliegende Gefäß bei netto 0,0 g.
        // Beim Abheben erscheint deshalb ungefähr das negative Gefäßgewicht.
        // Erst bei stabilem Gewicht <= -30 g tarieren wir erneut auf leer.
        if (currentGrams <= -kGefaessRemovedThresholdGrams) {
            set_save_ready(false);
            if (t4s3_scale_tare()) {
                hx711DisplayGrams = 0.0f;
                hx711DisplayValid = true;
                set_text(weightLabel, "0,0 g");
                set_text_if_changed(systemHx711GramsLabel, "0,0 g");
                set_text_if_changed(scaleCalibrationWeightLabel, "0,0 g");
                set_text_if_changed(simLabel, "HX711-Gewicht aktiv");
                update_status("Gefäß entfernt - Tara gesetzt");
            } else {
                update_status("Tara nach Entfernen fehlgeschlagen");
            }

            autodetectAutoTaredGefaess = kNoDetectedGefaess;
            autodetectDetectedGefaess = kNoDetectedGefaess;
            autodetectPendingGefaess = kNoDetectedGefaess;
            autodetectPendingSinceMs = 0;
        }
        return;
    }

    uint8_t bestSlot = kNoDetectedGefaess;
    float bestDiff = kGefaessAutodetectToleranceGrams;

    for (uint8_t i = 0; i < T4S3_GEFAESS_SLOT_COUNT; ++i) {
        if (gefaessWeightTenths[i] < 0) {
            continue;
        }

        const float expectedGrams = static_cast<float>(gefaessWeightTenths[i]) / 10.0f;
        const float diff = fabsf(currentGrams - expectedGrams);
        if (diff <= bestDiff) {
            bestDiff = diff;
            bestSlot = i;
        }
    }

    if (bestSlot == kNoDetectedGefaess) {
        autodetectPendingGefaess = kNoDetectedGefaess;
        autodetectPendingSinceMs = 0;

        if (autodetectDetectedGefaess != kNoDetectedGefaess) {
            autodetectDetectedGefaess = kNoDetectedGefaess;
            update_status("Autodetect aktiv - kein Gefäß erkannt");
        }
        return;
    }

    if (bestSlot != autodetectDetectedGefaess) {
        autodetectDetectedGefaess = bestSlot;
        autodetectPendingGefaess = bestSlot;
        autodetectPendingSinceMs = now;

        char grams[24];
        format_grams(grams, sizeof(grams), gefaessWeightTenths[bestSlot]);

        char msg[96];
        snprintf(msg, sizeof(msg), "Autodetect: Gefäß %u erkannt (%s)",
                 static_cast<unsigned>(bestSlot + 1),
                 grams);
        update_status(msg);
        return;
    }

    if (autodetectPendingGefaess != bestSlot) {
        autodetectPendingGefaess = bestSlot;
        autodetectPendingSinceMs = now;
        return;
    }

    if (autodetectPendingSinceMs == 0) {
        autodetectPendingSinceMs = now;
        return;
    }

    if (now - autodetectPendingSinceMs < kGefaessAutoTareDelayMs) {
        return;
    }

    if (!t4s3_scale_tare()) {
        update_status("Auto-Tara fehlgeschlagen - HX711 nicht bereit");
        autodetectPendingGefaess = kNoDetectedGefaess;
        autodetectPendingSinceMs = 0;
        return;
    }

    set_save_ready(false);

    autodetectAutoTaredGefaess = bestSlot;
    autodetectPendingGefaess = kNoDetectedGefaess;
    autodetectPendingSinceMs = 0;

    hx711DisplayGrams = 0.0f;
    hx711DisplayValid = true;
    set_text(weightLabel, "0,0 g");
    set_text_if_changed(systemHx711GramsLabel, "0,0 g");
    set_text_if_changed(scaleCalibrationWeightLabel, "0,0 g");
    set_text_if_changed(simLabel, "HX711-Gewicht aktiv");

    char msg[96];
    snprintf(msg, sizeof(msg), "Gefäß %u erkannt - Auto-Tara",
             static_cast<unsigned>(bestSlot + 1));
    update_status(msg);
}


void update_vessel_display()
{
    set_text(vesselLabel, vessel_name(currentVesselIndex));
}

void update_vessel_overlay_display()
{
    for (uint8_t i = 0; i < kMaxSiebtraegerSlots; ++i) {
        if (!vesselOptionLabels[i]) {
            continue;
        }
        char buf[48];
        snprintf(buf, sizeof(buf), "%s%s", i == draftVesselIndex ? "> " : "  ", vessel_name(i));
        lv_label_set_text(vesselOptionLabels[i], buf);
        lv_obj_set_style_text_color(vesselOptionLabels[i],
                                    lv_color_hex(i == draftVesselIndex ? COLOR_GREEN : COLOR_WHITE),
                                    0);
    }
}

void format_grams(char *buf, size_t len, int32_t tenths)
{
    const char *sign = tenths < 0 ? "-" : "";
    int32_t absTenths = abs(tenths);
    snprintf(buf, len, "%s%ld,%ld g",
             sign,
             static_cast<long>(absTenths / 10),
             static_cast<long>(absTenths % 10));
}

void format_grams_float(char *buf, size_t len, float grams)
{
    const char *sign = grams < 0.0f ? "-" : "";
    float absGrams = fabsf(grams);
    const int32_t tenths = static_cast<int32_t>(absGrams * 10.0f + 0.5f);
    snprintf(buf, len, "%s%ld,%ld g",
             sign,
             static_cast<long>(tenths / 10),
             static_cast<long>(tenths % 10));
}


uint8_t stored_gefaess_count()
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < T4S3_GEFAESS_SLOT_COUNT; ++i) {
        if (gefaessWeightTenths[i] >= 0) {
            ++count;
        }
    }
    return count;
}

void update_gefaess_storage_display()
{
    char buf[40];
    snprintf(buf, sizeof(buf), "%u/%u eingemessen", stored_gefaess_count(), static_cast<unsigned>(T4S3_GEFAESS_SLOT_COUNT));
    set_text(gefaessStoredLabel, buf);
}

void update_gefaess_manage_display()
{
    char buf[48];
    for (uint8_t i = 0; i < T4S3_GEFAESS_SLOT_COUNT; ++i) {
        if (!gefaessManageLabels[i]) {
            continue;
        }

        if (gefaessWeightTenths[i] >= 0) {
            char grams[18];
            format_grams(grams, sizeof(grams), gefaessWeightTenths[i]);
            snprintf(buf, sizeof(buf), "Gefäß %u: %s", static_cast<unsigned>(i + 1), grams);
        } else {
            snprintf(buf, sizeof(buf), "Gefäß %u: nicht eingemessen", static_cast<unsigned>(i + 1));
        }
        set_text_if_changed(gefaessManageLabels[i], buf);
    }
}


void close_gefaess_delete_overlay()
{
    if (gefaessDeleteOverlay && lv_obj_is_valid(gefaessDeleteOverlay)) {
        lv_obj_del(gefaessDeleteOverlay);
    }
    gefaessDeleteOverlay = nullptr;
}

void open_gefaess_delete_overlay(uint8_t slot)
{
    if (slot >= T4S3_GEFAESS_SLOT_COUNT) {
        return;
    }
    if (gefaessWeightTenths[slot] < 0) {
        update_status("Gefäß ist nicht eingemessen");
        return;
    }

    close_gefaess_delete_overlay();
    gefaessDeleteSlot = slot;

    lv_obj_t *screen = lv_scr_act();
    if (!screen) {
        return;
    }

    gefaessDeleteOverlay = lv_obj_create(screen);
    lv_obj_set_size(gefaessDeleteOverlay, screenWidth, screenHeight);
    lv_obj_align(gefaessDeleteOverlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(gefaessDeleteOverlay, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(gefaessDeleteOverlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(gefaessDeleteOverlay, 0, 0);
    lv_obj_set_style_pad_all(gefaessDeleteOverlay, 0, 0);
    lv_obj_clear_flag(gefaessDeleteOverlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(gefaessDeleteOverlay);
    style_panel(panel);
    lv_obj_set_size(panel, 390, 230);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    char grams[18];
    format_grams(grams, sizeof(grams), gefaessWeightTenths[slot]);

    char buf[96];
    snprintf(buf, sizeof(buf), "Gefäß %u löschen?", static_cast<unsigned>(slot + 1));
    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, buf);
    style_label(title, COLOR_WHITE);
    lv_obj_set_width(title, 330);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    snprintf(buf, sizeof(buf), "Gespeichertes Gewicht: %s\nDieser Vorgang kann nicht rückgängig gemacht werden.", grams);
    lv_obj_t *hint = lv_label_create(panel);
    lv_label_set_text(hint, buf);
    style_label(hint, COLOR_MUTED);
    lv_obj_set_width(hint, 330);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 0, 48);

    lv_obj_t *cancel = create_button(panel, "Abbrechen", "gefaess_delete_cancel", 150, 48);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *confirm = create_button(panel, "Löschen", "gefaess_delete_confirm", 150, 48);
    lv_obj_align(confirm, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}



float preview_scale_calibration_factor()
{
    const float knownGrams = static_cast<float>(scaleCalibrationTargetTenths) / 10.0f;
    float currentGrams = t4s3_scale_current_grams();
    if (currentGrams < 0.0f) {
        currentGrams = -currentGrams;
    }

    const float currentFactor = t4s3_scale_calibration_factor();
    if (knownGrams <= 0.0f || currentFactor <= 0.0f) {
        return 0.0f;
    }

    const float currentRaw = currentGrams * currentFactor;
    if (currentRaw < 100.0f) {
        return 0.0f;
    }

    return currentRaw / knownGrams;
}

void update_scale_calibration_display()
{
    char buf[40];
    snprintf(buf, sizeof(buf), "%.1f g", static_cast<double>(t4s3_scale_current_grams()));
    set_text_if_changed(scaleCalibrationWeightLabel, buf);

    if (scaleCalibrationStep == 4) {
        const float factor = preview_scale_calibration_factor();
        if (factor > 0.0f) {
            snprintf(buf, sizeof(buf), "Kalibrierfaktor: %.1f raw/g", static_cast<double>(factor));
        } else {
            snprintf(buf, sizeof(buf), "Kalibrierfaktor: -");
        }
    } else {
        snprintf(buf, sizeof(buf), "Faktor: %.1f raw/g", static_cast<double>(t4s3_scale_calibration_factor()));
    }
    set_text_if_changed(scaleCalibrationFactorLabel, buf);

    format_grams(buf, sizeof(buf), scaleCalibrationTargetTenths);
    set_text_if_changed(scaleCalibrationTargetWeightLabel, buf);

    const int32_t incTenths = kScaleCalIncrementsTenths[scaleCalibrationIncrementIndex];
    if ((incTenths % 10) == 0) {
        snprintf(buf, sizeof(buf), "%ld g", static_cast<long>(incTenths / 10));
    } else {
        format_grams(buf, sizeof(buf), incTenths);
    }
    set_text_if_changed(scaleCalibrationIncrementLabel, buf);
}

void close_scale_calibration_overlay()
{
    if (scaleCalibrationOverlay && lv_obj_is_valid(scaleCalibrationOverlay)) {
        lv_obj_del(scaleCalibrationOverlay);
    }
    scaleCalibrationOverlay = nullptr;
    scaleCalibrationWeightLabel = nullptr;
    scaleCalibrationFactorLabel = nullptr;
    scaleCalibrationTargetWeightLabel = nullptr;
    scaleCalibrationIncrementLabel = nullptr;
}

void open_scale_calibration_overlay()
{
    close_scale_calibration_overlay();

    lv_obj_t *screen = lv_scr_act();
    if (!screen) {
        return;
    }

    scaleCalibrationOverlay = lv_obj_create(screen);
    lv_obj_set_size(scaleCalibrationOverlay, screenWidth, screenHeight);
    lv_obj_align(scaleCalibrationOverlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(scaleCalibrationOverlay, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(scaleCalibrationOverlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(scaleCalibrationOverlay, 0, 0);
    lv_obj_set_style_pad_all(scaleCalibrationOverlay, 0, 0);
    lv_obj_clear_flag(scaleCalibrationOverlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(scaleCalibrationOverlay);
    style_panel(panel);
    lv_obj_set_size(panel, 430, 310);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, scaleCalibrationStep == 4 ? "Kalibrierung abgeschlossen" : "Waage kalibrieren");
    style_label(title, COLOR_WHITE);
    lv_obj_set_width(title, 270);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *stepLabel = lv_label_create(panel);
    char stepText[24];
    snprintf(stepText, sizeof(stepText), "Schritt %u/4", static_cast<unsigned>(scaleCalibrationStep));
    lv_label_set_text(stepLabel, stepText);
    style_label(stepLabel, COLOR_MUTED);
    lv_obj_set_width(stepLabel, 120);
    lv_label_set_long_mode(stepLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(stepLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(stepLabel, LV_ALIGN_TOP_RIGHT, 0, 0);

    if (scaleCalibrationStep == 1 || scaleCalibrationStep == 2) {
        lv_obj_t *hint = lv_label_create(panel);
        style_label(hint, COLOR_MUTED);
        lv_obj_set_width(hint, 380);
        lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
        lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 0, 40);

        if (scaleCalibrationStep == 1) {
            lv_label_set_text(hint, "Waage entlasten. Danach Tara drücken und mit Weiter fortfahren.");
        } else {
            lv_label_set_text(hint, "Kalibriergewicht auflegen und warten, bis die Anzeige ruhig ist.");
        }
    }

    const int weightRowY = (scaleCalibrationStep >= 3) ? 54 : 122;

    if (scaleCalibrationStep <= 3) {
        lv_obj_t *weightTitle = lv_label_create(panel);
        lv_label_set_text(weightTitle, scaleCalibrationStep == 3 ? "Aktuell:" : "Aktuelles Gewicht:");
        style_label(weightTitle, COLOR_MUTED);
        lv_obj_align(weightTitle, LV_ALIGN_TOP_LEFT, 0, weightRowY);

        scaleCalibrationWeightLabel = lv_label_create(panel);
        lv_obj_set_width(scaleCalibrationWeightLabel, 150);
        lv_label_set_long_mode(scaleCalibrationWeightLabel, LV_LABEL_LONG_DOT);
        style_label(scaleCalibrationWeightLabel, COLOR_WHITE);
        lv_obj_align(scaleCalibrationWeightLabel, LV_ALIGN_TOP_LEFT, 190, weightRowY);
    }

    if (scaleCalibrationStep == 4) {
        scaleCalibrationFactorLabel = lv_label_create(panel);
        lv_obj_set_width(scaleCalibrationFactorLabel, 360);
        lv_label_set_long_mode(scaleCalibrationFactorLabel, LV_LABEL_LONG_DOT);
        style_label(scaleCalibrationFactorLabel, COLOR_WHITE);
        lv_obj_align(scaleCalibrationFactorLabel, LV_ALIGN_TOP_LEFT, 0, 78);
    }

    if (scaleCalibrationStep == 1) {
        lv_obj_t *tara = create_button(panel, "Tara", "scale_cal_tare", 130, 46);
        lv_obj_align(tara, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_t *next = create_button(panel, "Weiter", "scale_cal_to_load", 130, 46);
        lv_obj_align(next, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_t *close = create_button(panel, "Abbr.", "scale_cal_close", 110, 46);
        lv_obj_align(close, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    } else if (scaleCalibrationStep == 2) {
        lv_obj_t *back = create_button(panel, "Zurück", "scale_cal_back_tare", 130, 46);
        lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_t *next = create_button(panel, "Weiter", "scale_cal_to_weight", 130, 46);
        lv_obj_align(next, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_t *close = create_button(panel, "Abbr.", "scale_cal_close", 110, 46);
        lv_obj_align(close, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    } else if (scaleCalibrationStep == 3) {
        lv_obj_t *targetTitle = lv_label_create(panel);
        lv_label_set_text(targetTitle, "Kalibriergewicht");
        style_label(targetTitle, COLOR_MUTED);
        lv_obj_align(targetTitle, LV_ALIGN_TOP_LEFT, 0, 78);

        lv_obj_t *targetMinus = create_button(panel, "-", "scale_cal_weight_dec", 56, 38);
        lv_obj_align(targetMinus, LV_ALIGN_TOP_LEFT, 0, 102);

        scaleCalibrationTargetWeightLabel = lv_label_create(panel);
        lv_obj_set_width(scaleCalibrationTargetWeightLabel, 160);
        lv_label_set_long_mode(scaleCalibrationTargetWeightLabel, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(scaleCalibrationTargetWeightLabel, LV_TEXT_ALIGN_CENTER, 0);
        style_label(scaleCalibrationTargetWeightLabel, COLOR_WHITE);
        lv_obj_align(scaleCalibrationTargetWeightLabel, LV_ALIGN_TOP_LEFT, 72, 109);

        lv_obj_t *targetPlus = create_button(panel, "+", "scale_cal_weight_inc", 56, 38);
        lv_obj_align(targetPlus, LV_ALIGN_TOP_LEFT, 248, 102);

        lv_obj_t *incrementTitle = lv_label_create(panel);
        lv_label_set_text(incrementTitle, "Schrittweite");
        style_label(incrementTitle, COLOR_MUTED);
        lv_obj_align(incrementTitle, LV_ALIGN_TOP_LEFT, 0, 142);

        lv_obj_t *incrementMinus = create_button(panel, "-", "scale_cal_step_dec", 56, 38);
        lv_obj_align(incrementMinus, LV_ALIGN_TOP_LEFT, 0, 166);

        scaleCalibrationIncrementLabel = lv_label_create(panel);
        lv_obj_set_width(scaleCalibrationIncrementLabel, 160);
        lv_label_set_long_mode(scaleCalibrationIncrementLabel, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(scaleCalibrationIncrementLabel, LV_TEXT_ALIGN_CENTER, 0);
        style_label(scaleCalibrationIncrementLabel, COLOR_WHITE);
        lv_obj_align(scaleCalibrationIncrementLabel, LV_ALIGN_TOP_LEFT, 72, 173);

        lv_obj_t *incrementPlus = create_button(panel, "+", "scale_cal_step_inc", 56, 38);
        lv_obj_align(incrementPlus, LV_ALIGN_TOP_LEFT, 248, 166);

        lv_obj_t *back = create_button(panel, "Zurück", "scale_cal_back_load", 105, 46);
        lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_t *next = create_button(panel, "Weiter", "scale_cal_to_confirm", 135, 46);
        lv_obj_align(next, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_t *close = create_button(panel, "Abbr.", "scale_cal_close", 105, 46);
        lv_obj_align(close, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    } else {
        lv_obj_t *hint = lv_label_create(panel);
        lv_label_set_text(hint, "Kalibrierfaktor prüfen und bestätigen.");
        style_label(hint, COLOR_MUTED);
        lv_obj_set_width(hint, 360);
        lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
        lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 0, 40);

        lv_obj_t *back = create_button(panel, "Zurück", "scale_cal_back_weight", 130, 46);
        lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_t *save = create_button(panel, "Bestätigen", "scale_cal_save", 160, 46);
        lv_obj_align(save, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    }

    update_scale_calibration_display();
    update_status("Kalibrierung geöffnet");
}


int32_t current_gefaess_measure_tenths()
{
    float grams = 0.0f;
    if (t4s3_scale_is_ready()) {
        grams = t4s3_scale_current_grams();
    } else {
        grams = (simTenths - gefaessMeasureTareTenths) / 10.0f;
    }

    if (grams < 0.0f) {
        grams = 0.0f;
    }
    return static_cast<int32_t>(grams * 10.0f + 0.5f);
}

void update_gefaess_measure_weight_display()
{
    if (!gefaessMeasureWeightLabel) {
        return;
    }

    char grams[18];
    if (t4s3_scale_is_ready()) {
        format_grams_float(grams, sizeof(grams), t4s3_scale_current_grams());
    } else if (gefaessMeasureStep == 1) {
        format_grams(grams, sizeof(grams), simTenths);
    } else {
        format_grams(grams, sizeof(grams), current_gefaess_measure_tenths());
    }
    set_text_if_changed(gefaessMeasureWeightLabel, grams);
}

void close_gefaess_measure_overlay()
{
    if (gefaessMeasureOverlay && lv_obj_is_valid(gefaessMeasureOverlay)) {
        lv_obj_del(gefaessMeasureOverlay);
    }
    gefaessMeasureOverlay = nullptr;
    gefaessMeasureWeightLabel = nullptr;
}

void render_gefaess_measure_overlay()
{
    close_gefaess_measure_overlay();

    lv_obj_t *screen = lv_scr_act();
    if (!screen) {
        return;
    }

    gefaessMeasureOverlay = lv_obj_create(screen);
    lv_obj_set_size(gefaessMeasureOverlay, screenWidth, screenHeight);
    lv_obj_align(gefaessMeasureOverlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(gefaessMeasureOverlay, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(gefaessMeasureOverlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(gefaessMeasureOverlay, 0, 0);
    lv_obj_set_style_pad_all(gefaessMeasureOverlay, 0, 0);
    lv_obj_clear_flag(gefaessMeasureOverlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(gefaessMeasureOverlay);
    style_panel(panel);
    lv_obj_set_size(panel, 430, 350);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title = lv_label_create(panel);
    style_label(title, COLOR_WHITE);
    lv_obj_set_width(title, 370);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *hint = lv_label_create(panel);
    style_label(hint, COLOR_MUTED);
    lv_obj_set_width(hint, 370);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 0, 36);

    lv_obj_t *stepLabel = lv_label_create(panel);
    style_label(stepLabel, COLOR_MUTED);
    lv_obj_set_width(stepLabel, 370);
    lv_label_set_long_mode(stepLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(stepLabel, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(stepLabel, LV_ALIGN_TOP_RIGHT, 0, 0);

    char buf[64];

    if (gefaessMeasureStep == 0) {
        lv_label_set_text(title, "Gefäß einmessen");
        lv_label_set_text(stepLabel, "Auswahl");
        lv_label_set_text(hint, "Welches Gefäß soll eingemessen werden?");

        for (uint8_t i = 0; i < T4S3_GEFAESS_SLOT_COUNT; ++i) {
            char label[48];
            if (gefaessWeightTenths[i] >= 0) {
                char grams[18];
                format_grams(grams, sizeof(grams), gefaessWeightTenths[i]);
                snprintf(label, sizeof(label), "Gefäß %u (%s)", static_cast<unsigned>(i + 1), grams);
            } else {
                snprintf(label, sizeof(label), "Gefäß %u", static_cast<unsigned>(i + 1));
            }
            char *action = nullptr;
            switch (i) {
            case 0: action = const_cast<char *>("gefaess_measure_slot_0"); break;
            case 1: action = const_cast<char *>("gefaess_measure_slot_1"); break;
            default: action = const_cast<char *>("gefaess_measure_slot_2"); break;
            }
            lv_obj_t *slot = create_button(panel, label, action, 350, 48);
            lv_obj_align(slot, LV_ALIGN_TOP_MID, 0, 88 + i * 58);
        }

        lv_obj_t *cancel = create_button(panel, "Abbrechen", "gefaess_measure_cancel", 150, 44);
        lv_obj_align(cancel, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        return;
    }

    snprintf(buf, sizeof(buf), "Gefäß %u einmessen", static_cast<unsigned>(gefaessMeasureSlot + 1));
    lv_label_set_text(title, buf);
    snprintf(buf, sizeof(buf), "Schritt %u/3", static_cast<unsigned>(gefaessMeasureStep));
    lv_label_set_text(stepLabel, buf);

    gefaessMeasureWeightLabel = lv_label_create(panel);
    style_label(gefaessMeasureWeightLabel, COLOR_GREEN);
    lv_obj_set_width(gefaessMeasureWeightLabel, 370);
    lv_obj_set_style_text_align(gefaessMeasureWeightLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(gefaessMeasureWeightLabel, LV_ALIGN_TOP_MID, 0, 125);

    if (gefaessMeasureStep == 1) {
        lv_label_set_text(hint, "Bitte Waage entlasten. Danach Tara drücken und weiter.");
        lv_obj_t *tara = create_button(panel, "Tara", "gefaess_measure_tara", 130, 48);
        lv_obj_align(tara, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_t *next = create_button(panel, "Weiter", "gefaess_measure_to_load", 130, 48);
        lv_obj_align(next, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_t *cancel = create_button(panel, "Abbr.", "gefaess_measure_cancel", 110, 48);
        lv_obj_align(cancel, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    } else if (gefaessMeasureStep == 2) {
        lv_label_set_text(hint, "Bitte Gefäß auflegen. Das aktuelle Gewicht wird live angezeigt.");
        lv_obj_t *back = create_button(panel, "Zurück", "gefaess_measure_back_tare", 130, 48);
        lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_t *next = create_button(panel, "Weiter", "gefaess_measure_to_confirm", 130, 48);
        lv_obj_align(next, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_t *cancel = create_button(panel, "Abbr.", "gefaess_measure_cancel", 110, 48);
        lv_obj_align(cancel, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    } else {
        char grams[18];
        format_grams(grams, sizeof(grams), gefaessMeasureValueTenths);
        snprintf(buf, sizeof(buf), "Gefäß %u: %s", static_cast<unsigned>(gefaessMeasureSlot + 1), grams);
        lv_label_set_text(hint, "Final bestätigen und speichern:");
        set_text(gefaessMeasureWeightLabel, buf);

        lv_obj_t *cancel = create_button(panel, "Abbrechen", "gefaess_measure_cancel", 150, 48);
        lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_t *save = create_button(panel, "Speichern", "gefaess_measure_save", 170, 48);
        lv_obj_align(save, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        return;
    }

    update_gefaess_measure_weight_display();
}

void open_gefaess_measure_overlay()
{
    gefaessMeasureStep = 0;
    gefaessMeasureSlot = 0;
    gefaessMeasureTareTenths = 0;
    gefaessMeasureValueTenths = 0;
    render_gefaess_measure_overlay();
    update_status("Gefäß einmessen gestartet");
}

int32_t current_save_weight_tenths()
{
    if (hx711DisplayValid && t4s3_scale_is_ready()) {
        return static_cast<int32_t>(hx711DisplayGrams * 10.0f +
                                    (hx711DisplayGrams >= 0.0f ? 0.5f : -0.5f));
    }

    return simTenths;
}

void update_demo_stats_display();

void add_saved_dose(int32_t doseTenths)
{
    demoTotalShots++;
    demoMachineShots++;
    demoGrinderShots++;
    demoFilterShots++;

    demoTotalGramsTenths += doseTenths;
    demoMachineGramsTenths += doseTenths;
    demoGrinderGramsTenths += doseTenths;
    demoFilterGramsTenths += doseTenths;

    update_demo_stats_display();
}

void update_demo_stats_display()
{
    char buf[32];

    snprintf(buf, sizeof(buf), "%u", demoTotalShots);
    set_text(totalShotsLabel, buf);

    snprintf(buf, sizeof(buf), "%u", demoMachineShots);
    set_text(shotsMachineLabel, buf);

    snprintf(buf, sizeof(buf), "%u", demoGrinderShots);
    set_text(shotsGrinderLabel, buf);

    snprintf(buf, sizeof(buf), "%u", demoFilterShots);
    set_text(shotsFilterLabel, buf);

    format_grams(buf, sizeof(buf), demoTotalGramsTenths);
    set_text(totalGramsLabel, buf);

    format_grams(buf, sizeof(buf), demoMachineGramsTenths);
    set_text(gramsMachineLabel, buf);

    format_grams(buf, sizeof(buf), demoGrinderGramsTenths);
    set_text(gramsGrinderLabel, buf);

    format_grams(buf, sizeof(buf), demoFilterGramsTenths);
    set_text(gramsFilterLabel, buf);
}

void update_target_display()
{
    char buf[24];
    format_grams(buf, sizeof(buf), demoTargetTenths);
    set_text(targetLabel, buf);
}

void update_target_overlay_display()
{
    char buf[24];
    format_grams(buf, sizeof(buf), draftTargetTenths);
    set_text(targetOverlayValueLabel, buf);

    format_grams(buf, sizeof(buf), draftTargetStepTenths);
    set_text(targetOverlayStepLabel, buf);
}

void format_header_datetime(char *buf, size_t len)
{
    t4s3_time_format_header(buf, len);
}

void update_clock_display()
{
    char buf[32];
    format_header_datetime(buf, sizeof(buf));
    set_text(clockLabel, buf);
    update_system_time_display();
}

void style_screen(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
}

void style_panel(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(COLOR_GREEN_DARK), 0);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_radius(obj, 14, 0);
    lv_obj_set_style_pad_all(obj, 14, 0);
}

void style_label(lv_obj_t *obj, uint32_t color)
{
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(obj, coffee_ui_font(), 0);
}

void style_button(lv_obj_t *btn)
{
    lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(COLOR_GREEN), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_pad_all(btn, 12, 0);

    lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_GREEN_DARK), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, lv_color_hex(COLOR_GREEN), LV_STATE_PRESSED);
}

void style_plain_block(lv_obj_t *obj, uint32_t color)
{
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 2, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

void style_nav_button(lv_obj_t *btn, bool active)
{
    style_button(btn);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_pad_all(btn, 8, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(active ? COLOR_GREEN : COLOR_DIM), 0);
    if (active) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_GREEN_DARK), 0);
    }
}

void update_status(const char *msg)
{
    Serial.printf("[T4S3] %s\n", msg);
    set_text(statusLabel, msg);
}

void format_timer(char *buf, size_t len)
{
    uint32_t elapsed = timerBaseMs;
    if (timerRunning) {
        elapsed += millis() - timerStartedMs;
    }

    const uint32_t tenths = (elapsed / 100) % 10;
    const uint32_t seconds = (elapsed / 1000) % 60;
    const uint32_t minutes = (elapsed / 60000) % 60;

    snprintf(buf, len, "%02lu:%02lu:%lu",
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds),
             static_cast<unsigned long>(tenths));
}

void update_timer_display()
{
    char buf[24];
    format_timer(buf, sizeof(buf));
    set_text(timerLabel, buf);
}

void set_timer_running(bool running)
{
    if (running == timerRunning) {
        return;
    }

    if (running) {
        timerStartedMs = millis();
        timerRunning = true;
        set_text(timerButtonLabel, "Stop");
        update_status("Stoppuhr gestartet");
    } else {
        timerBaseMs += millis() - timerStartedMs;
        timerRunning = false;
        set_text(timerButtonLabel, "Start");
        update_status("Stoppuhr gestoppt");
    }
    update_timer_display();
}

void reset_timer()
{
    timerBaseMs = 0;
    timerStartedMs = millis();
    update_timer_display();
    update_status("Stoppuhr auf 00:00:0 gesetzt");
}

void navigate_to(Page page)
{
    activePage = page;
    build_current_page();
}

void update_wlan_page_display();
void update_maintenance_display();
void update_maintenance_due_state();
void update_maintenance_warning_display();

#if COFFEE_T4S3_MAINTENANCE_TEST_30S
constexpr uint32_t DEFAULT_MAINTENANCE_MACHINE_INTERVAL_SEC = 30UL;  // Test: 30 Sekunden
constexpr uint32_t DEFAULT_MAINTENANCE_GRINDER_INTERVAL_SEC = 30UL;  // Test: 30 Sekunden
constexpr uint32_t DEFAULT_MAINTENANCE_FILTER_INTERVAL_SEC  = 30UL;  // Test: 30 Sekunden
#else
constexpr uint32_t DEFAULT_MAINTENANCE_MACHINE_INTERVAL_SEC = 864000UL;     // 10 Tage
constexpr uint32_t DEFAULT_MAINTENANCE_GRINDER_INTERVAL_SEC = 2419200UL;    // 28 Tage
constexpr uint32_t DEFAULT_MAINTENANCE_FILTER_INTERVAL_SEC  = 7257600UL;    // 12 Wochen / 84 Tage
#endif
constexpr uint32_t MIN_MAINTENANCE_INTERVAL_SEC = 60UL;
constexpr uint32_t MAX_MAINTENANCE_INTERVAL_SEC = 31536000UL;

const char *maintenance_action_title(const char *action)
{
    if (!action) {
        return "Wartung";
    }
    if (strcmp(action, "maintenance_reset_machine") == 0) {
        return "Kaffeemaschine";
    }
    if (strcmp(action, "maintenance_reset_grinder") == 0) {
        return "Kaffeemühle";
    }
    if (strcmp(action, "maintenance_reset_filter") == 0) {
        return "Filter";
    }
    return "Wartung";
}

void save_maintenance_epochs()
{
    t4s3_settings_save_maintenance(maintenanceMachineEpoch,
                                   maintenanceGrinderEpoch,
                                   maintenanceFilterEpoch);
}

void load_maintenance_epochs()
{
    t4s3_settings_load_maintenance(maintenanceMachineEpoch,
                                   maintenanceGrinderEpoch,
                                   maintenanceFilterEpoch);
}

void ensure_maintenance_epochs_initialized()
{
    const uint32_t now = t4s3_time_now_epoch();
    if (now == 0) {
        return;
    }

    bool changed = false;
    if (maintenanceMachineEpoch == 0) {
        maintenanceMachineEpoch = now;
        changed = true;
    }
    if (maintenanceGrinderEpoch == 0) {
        maintenanceGrinderEpoch = now;
        changed = true;
    }
    if (maintenanceFilterEpoch == 0) {
        maintenanceFilterEpoch = now;
        changed = true;
    }

    if (changed) {
        save_maintenance_epochs();
    }
}

void format_maintenance_duration(char *buf, size_t len, uint32_t seconds)
{
    const uint32_t days = seconds / 86400UL;
    seconds %= 86400UL;
    const uint32_t hours = seconds / 3600UL;
    seconds %= 3600UL;
    const uint32_t minutes = seconds / 60UL;
    const uint32_t secs = seconds % 60UL;

    snprintf(buf, len, "%lu %s, %02lu:%02lu:%02lu",
             static_cast<unsigned long>(days),
             days == 1 ? "Tag" : "Tagen",
             static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(secs));
}

void maintenance_status(uint32_t lastEpoch,
                        uint32_t intervalSec,
                        char *direction,
                        size_t directionLen,
                        char *timeText,
                        size_t timeLen)
{
    const uint32_t now = t4s3_time_now_epoch();
    if (now == 0 || lastEpoch == 0) {
        strlcpy(direction, "-", directionLen);
        strlcpy(timeText, "warte auf Zeit", timeLen);
        return;
    }

    const uint32_t dueEpoch = lastEpoch + intervalSec;
    if (now < dueEpoch) {
        strlcpy(direction, "in", directionLen);
        format_maintenance_duration(timeText, timeLen, dueEpoch - now);
    } else {
        strlcpy(direction, "seit", directionLen);
        format_maintenance_duration(timeText, timeLen, now - dueEpoch);
    }
}


void update_maintenance_row_display(lv_obj_t *leftLabel,
                                    lv_obj_t *timeLabel,
                                    const char *title,
                                    uint32_t lastEpoch,
                                    uint32_t intervalSec,
                                    bool enabled)
{
    if (!leftLabel || !timeLabel) {
        return;
    }

    if (!enabled) {
        char leftText[80];
        snprintf(leftText, sizeof(leftText), "%s: inaktiv", title);
        set_text(leftLabel, leftText);
        set_text(timeLabel, "keine Warnung");

        if (leftLabel && lv_obj_is_valid(leftLabel)) {
            lv_obj_set_style_text_color(leftLabel, lv_color_hex(COLOR_MUTED), 0);
        }
        if (timeLabel && lv_obj_is_valid(timeLabel)) {
            lv_obj_set_style_text_color(timeLabel, lv_color_hex(COLOR_DIM), 0);
        }
        return;
    }

    char direction[8];
    char timeText[32];
    maintenance_status(lastEpoch, intervalSec,
                       direction, sizeof(direction),
                       timeText, sizeof(timeText));

    char leftText[80];
    snprintf(leftText, sizeof(leftText), "%s: %s", title, direction);

    set_text(leftLabel, leftText);
    set_text(timeLabel, timeText);

    const uint32_t nowEpoch = t4s3_time_now_epoch();
    const bool due = (nowEpoch != 0 && lastEpoch != 0 && nowEpoch >= (lastEpoch + intervalSec));
    const uint32_t leftColor = due ? 0xD65A5A : COLOR_WHITE;
    const uint32_t timeColor = due ? 0xD65A5A : COLOR_MUTED;

    if (leftLabel && lv_obj_is_valid(leftLabel)) {
        lv_obj_set_style_text_color(leftLabel, lv_color_hex(leftColor), 0);
    }
    if (timeLabel && lv_obj_is_valid(timeLabel)) {
        lv_obj_set_style_text_color(timeLabel, lv_color_hex(timeColor), 0);
    }
}

void update_maintenance_display()
{
    update_maintenance_row_display(maintenanceMachineLeftLabel,
                                   maintenanceMachineTimeLabel,
                                   "Kaffeemaschine",
                                   maintenanceMachineEpoch,
                                   maintenanceMachineIntervalSec,
                                   maintenanceMachineEnabled);

    update_maintenance_row_display(maintenanceGrinderLeftLabel,
                                   maintenanceGrinderTimeLabel,
                                   "Kaffeemühle",
                                   maintenanceGrinderEpoch,
                                   maintenanceGrinderIntervalSec,
                                   maintenanceGrinderEnabled);

    update_maintenance_row_display(maintenanceFilterLeftLabel,
                                   maintenanceFilterTimeLabel,
                                   "Filter",
                                   maintenanceFilterEpoch,
                                   maintenanceFilterIntervalSec,
                                   maintenanceFilterEnabled);
}

bool maintenance_item_due(uint32_t lastEpoch, uint32_t intervalSec, uint32_t now)
{
    if (now == 0 || lastEpoch == 0) {
        return false;
    }

    return now >= (lastEpoch + intervalSec);
}

void update_maintenance_warning_display()
{
    if (!maintenanceWarningButton) {
        return;
    }

    if (!lv_obj_is_valid(maintenanceWarningButton)) {
        maintenanceWarningButton = nullptr;
        return;
    }

    if (maintenanceDue) {
        lv_obj_clear_flag(maintenanceWarningButton, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(maintenanceWarningButton, LV_OBJ_FLAG_HIDDEN);
    }
}

void update_maintenance_due_state()
{
    const uint32_t now = t4s3_time_now_epoch();

    const bool due =
        (maintenanceMachineEnabled && maintenance_item_due(maintenanceMachineEpoch, maintenanceMachineIntervalSec, now)) ||
        (maintenanceGrinderEnabled && maintenance_item_due(maintenanceGrinderEpoch, maintenanceGrinderIntervalSec, now)) ||
        (maintenanceFilterEnabled && maintenance_item_due(maintenanceFilterEpoch, maintenanceFilterIntervalSec, now));

    if (maintenanceDue != due) {
        maintenanceDue = due;
        Serial.printf("[T4S3][Maintenance] due changed: %u\n", maintenanceDue ? 1 : 0);
        update_maintenance_warning_display();
    }
}
void perform_maintenance_reset(const char *action)
{
    const uint32_t nowEpoch = t4s3_time_now_epoch();
    if (nowEpoch == 0) {
        update_status("Wartung kann erst nach NTP-Sync zurückgesetzt werden");
        return;
    }

    if (strcmp(action, "maintenance_reset_machine") == 0) {
        maintenanceMachineEpoch = nowEpoch;
        demoMachineShots = 0;
        demoMachineGramsTenths = 0;
    } else if (strcmp(action, "maintenance_reset_grinder") == 0) {
        maintenanceGrinderEpoch = nowEpoch;
        demoGrinderShots = 0;
        demoGrinderGramsTenths = 0;
    } else if (strcmp(action, "maintenance_reset_filter") == 0) {
        maintenanceFilterEpoch = nowEpoch;
        demoFilterShots = 0;
        demoFilterGramsTenths = 0;
    } else {
        return;
    }

    save_maintenance_epochs();
    save_current_ui_settings();

    // Nicht direkt im Reset-Pfad LVGL-Objekte aktualisieren.
    // Das hatte LoadProhibited-Crashes verursacht. Stattdessen kurz warten
    // und dann im normalen ui_t4s3_tick() aktualisieren.
    maintenanceResetCooldownUntilMs = millis() + 500UL;
    maintenanceRefreshPending = true;

    char msg[96];
    snprintf(msg, sizeof(msg), "%s zurückgesetzt", maintenance_action_title(action));
    update_status(msg);
}

void close_maintenance_reset_overlay()
{
    if (maintenanceResetOverlay && lv_obj_is_valid(maintenanceResetOverlay)) {
        lv_obj_add_flag(maintenanceResetOverlay, LV_OBJ_FLAG_HIDDEN);
    } else {
        maintenanceResetOverlay = nullptr;
    }
    maintenancePendingAction = nullptr;
    update_status("Wartungs-Reset abgebrochen");
}

void process_pending_maintenance_reset()
{
    if (!maintenanceRequestedAction) {
        return;
    }

    const uint32_t nowMs = millis();
    if (nowMs - maintenanceResetRequestedAtMs < 900UL) {
        return;
    }

    const char *action = maintenanceRequestedAction;
    maintenanceRequestedAction = nullptr;
    maintenanceResetRequestedAtMs = 0;

    if (maintenanceResetOverlay && lv_obj_is_valid(maintenanceResetOverlay)) {
        lv_obj_add_flag(maintenanceResetOverlay, LV_OBJ_FLAG_HIDDEN);
    } else {
        maintenanceResetOverlay = nullptr;
    }
    maintenancePendingAction = nullptr;

    perform_maintenance_reset(action);
}

void open_maintenance_reset_overlay(const char *action)
{
    if (maintenanceResetOverlay && !lv_obj_is_valid(maintenanceResetOverlay)) {
        maintenanceResetOverlay = nullptr;
    }

    if (maintenanceResetOverlay) {
        maintenancePendingAction = action;
        lv_obj_clear_flag(maintenanceResetOverlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(maintenanceResetOverlay);
        update_status("Wartungs-Reset bestätigen oder abbrechen");
        return;
    }

    maintenancePendingAction = action;
    const char *titleText = maintenance_action_title(action);

    lv_obj_t *screen = lv_scr_act();

    maintenanceResetOverlay = lv_obj_create(screen);
    lv_obj_set_size(maintenanceResetOverlay, screenWidth, screenHeight);
    lv_obj_align(maintenanceResetOverlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(maintenanceResetOverlay, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(maintenanceResetOverlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(maintenanceResetOverlay, 0, 0);
    lv_obj_set_style_pad_all(maintenanceResetOverlay, 0, 0);
    lv_obj_clear_flag(maintenanceResetOverlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(maintenanceResetOverlay);
    style_panel(panel);
    lv_obj_set_size(panel, 430, 230);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "Wartung zurücksetzen");
    style_label(title, COLOR_GREEN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    char msgText[120];
    snprintf(msgText, sizeof(msgText), "%s jetzt als erledigt markieren?", titleText);

    lv_obj_t *msg = lv_label_create(panel);
    lv_label_set_text(msg, msgText);
    lv_obj_set_width(msg, 360);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    style_label(msg, COLOR_WHITE);
    lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 62);

    lv_obj_t *hint = lv_label_create(panel);
    lv_label_set_text(hint, "Der aktuelle NTP-Zeitpunkt wird gespeichert.");
    lv_obj_set_width(hint, 360);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    style_label(hint, COLOR_MUTED);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 100);

    lv_obj_t *cancel = create_button(panel, "Abbr.", "maintenance_reset_cancel", 150, 56);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 18, 0);

    lv_obj_t *confirm = create_button(panel, "Reset", "maintenance_reset_confirm", 170, 56);
    lv_obj_align(confirm, LV_ALIGN_BOTTOM_RIGHT, -18, 0);

    lv_obj_move_foreground(maintenanceResetOverlay);
    update_status("Wartungs-Reset bestätigen oder abbrechen");
}

const char *totals_edit_mode_name(uint8_t mode)
{
    switch (mode) {
    case 0: return "Gesamt";
    case 1: return "Kaffeemaschine";
    case 2: return "Kaffeemühle";
    case 3: return "Filter";
    default: return "Gesamt";
    }
}

void load_totals_edit_drafts()
{
    totalsEditDraftShots[0] = demoTotalShots;
    totalsEditDraftShots[1] = demoMachineShots;
    totalsEditDraftShots[2] = demoGrinderShots;
    totalsEditDraftShots[3] = demoFilterShots;

    totalsEditDraftGramsTenths[0] = demoTotalGramsTenths;
    totalsEditDraftGramsTenths[1] = demoMachineGramsTenths;
    totalsEditDraftGramsTenths[2] = demoGrinderGramsTenths;
    totalsEditDraftGramsTenths[3] = demoFilterGramsTenths;
}

void apply_totals_edit_drafts()
{
    demoTotalShots = totalsEditDraftShots[0];
    demoMachineShots = totalsEditDraftShots[1];
    demoGrinderShots = totalsEditDraftShots[2];
    demoFilterShots = totalsEditDraftShots[3];

    demoTotalGramsTenths = totalsEditDraftGramsTenths[0];
    demoMachineGramsTenths = totalsEditDraftGramsTenths[1];
    demoGrinderGramsTenths = totalsEditDraftGramsTenths[2];
    demoFilterGramsTenths = totalsEditDraftGramsTenths[3];
}

void update_totals_edit_display()
{
    char buf[40];

    // Es werden bewusst nur die Gesamtwerte korrigiert.
    // Die Wartungszaehler werden ueber die Wartungs-Reset-Funktionen gepflegt.
    if (totalsEditGramsMode) {
        format_grams(buf, sizeof(buf), totalsEditDraftGramsTenths[0]);
    } else {
        snprintf(buf, sizeof(buf), "%u", totalsEditDraftShots[0]);
    }
    set_text_if_changed(totalsEditShotsLabel, buf);

    snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(kTotalsEditSteps[totalsEditStepIndex]));
    set_text_if_changed(totalsEditStepLabel, buf);
}

void change_totals_edit_mode(int dir)
{
    int next = static_cast<int>(totalsEditMode) + dir;
    if (next < 0) {
        next = kTotalsEditModeCount - 1;
    } else if (next >= kTotalsEditModeCount) {
        next = 0;
    }
    totalsEditMode = static_cast<uint8_t>(next);
    update_totals_edit_display();
}

void adjust_totals_edit_shots(int dir)
{
    const int delta = static_cast<int>(kTotalsEditSteps[totalsEditStepIndex]) * dir;
    int value = static_cast<int>(totalsEditDraftShots[0]) + delta;
    if (value < 0) {
        value = 0;
    } else if (value > 9999) {
        value = 9999;
    }
    totalsEditDraftShots[0] = static_cast<uint16_t>(value);
    update_totals_edit_display();
}

void adjust_totals_edit_grams(int dir)
{
    const int32_t delta = static_cast<int32_t>(kTotalsEditSteps[totalsEditStepIndex]) * 10L * dir;
    int32_t value = totalsEditDraftGramsTenths[0] + delta;
    if (value < 0) {
        value = 0;
    } else if (value > 999990) {
        value = 999990;
    }
    totalsEditDraftGramsTenths[0] = value;
    update_totals_edit_display();
}

void change_totals_edit_step(int dir)
{
    int next = static_cast<int>(totalsEditStepIndex) + dir;
    if (next < 0) {
        next = kTotalsEditStepCount - 1;
    } else if (next >= kTotalsEditStepCount) {
        next = 0;
    }
    totalsEditStepIndex = static_cast<uint8_t>(next);
    update_totals_edit_display();
}

void close_totals_edit_overlay(bool save)
{
    if (save) {
        apply_totals_edit_drafts();
        update_demo_stats_display();
        save_current_ui_settings();
        update_status("Gesamtwerte gespeichert");
    } else {
        update_status("Gesamtwerte nicht geändert");
    }

    if (totalsEditOverlay && lv_obj_is_valid(totalsEditOverlay)) {
        lv_obj_del(totalsEditOverlay);
    }
    totalsEditOverlay = nullptr;
    totalsEditModeLabel = nullptr;
    totalsEditShotsLabel = nullptr;
    totalsEditGramsLabel = nullptr;
    totalsEditStepLabel = nullptr;
    totalsEditGramsMode = false;
}

void open_totals_edit_overlay(bool editGrams)
{
    if (totalsEditOverlay) {
        return;
    }

    load_totals_edit_drafts();
    totalsEditMode = 0;
    totalsEditStepIndex = 0;
    totalsEditGramsMode = editGrams;

    lv_obj_t *screen = lv_scr_act();
    totalsEditOverlay = lv_obj_create(screen);
    lv_obj_set_size(totalsEditOverlay, screenWidth, screenHeight);
    lv_obj_align(totalsEditOverlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(totalsEditOverlay, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(totalsEditOverlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(totalsEditOverlay, 0, 0);
    lv_obj_set_style_pad_all(totalsEditOverlay, 0, 0);
    lv_obj_clear_flag(totalsEditOverlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(totalsEditOverlay);
    style_panel(panel);
    lv_obj_set_size(panel, 430, 258);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, editGrams ? "Gesamt-Mahlgut" : "Gesamt-Shots");
    style_label(title, COLOR_GREEN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);
lv_obj_t *valueMinus = create_button(panel, "-", editGrams ? "totals_grams_dec" : "totals_shots_dec", 56, 42);
    lv_obj_align(valueMinus, LV_ALIGN_TOP_LEFT, 0, 56);

    totalsEditShotsLabel = lv_label_create(panel);
    lv_obj_set_width(totalsEditShotsLabel, 150);
    lv_obj_set_style_text_align(totalsEditShotsLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(totalsEditShotsLabel, LV_LABEL_LONG_DOT);
    style_label(totalsEditShotsLabel, COLOR_WHITE);
    lv_obj_align(totalsEditShotsLabel, LV_ALIGN_TOP_MID, 0, 66);

    lv_obj_t *valuePlus = create_button(panel, "+", editGrams ? "totals_grams_inc" : "totals_shots_inc", 56, 42);
    lv_obj_align(valuePlus, LV_ALIGN_TOP_RIGHT, 0, 56);

    lv_obj_t *stepTitle = lv_label_create(panel);
    lv_label_set_text(stepTitle, "Schrittweite");
    style_label(stepTitle, COLOR_MUTED);
    lv_obj_align(stepTitle, LV_ALIGN_TOP_LEFT, 0, 110);

    lv_obj_t *stepMinus = create_button(panel, "-", "totals_step_dec", 56, 40);
    lv_obj_align(stepMinus, LV_ALIGN_TOP_LEFT, 0, 134);

    totalsEditStepLabel = lv_label_create(panel);
    lv_obj_set_width(totalsEditStepLabel, 120);
    lv_obj_set_style_text_align(totalsEditStepLabel, LV_TEXT_ALIGN_CENTER, 0);
    style_label(totalsEditStepLabel, COLOR_WHITE);
    lv_obj_align(totalsEditStepLabel, LV_ALIGN_TOP_MID, 0, 143);

    lv_obj_t *stepPlus = create_button(panel, "+", "totals_step_inc", 56, 40);
    lv_obj_align(stepPlus, LV_ALIGN_TOP_RIGHT, 0, 134);

    lv_obj_t *cancel = create_button(panel, "Abbr.", "totals_edit_cancel", 150, 46);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *save = create_button(panel, "Speichern", "totals_edit_save", 170, 46);
    lv_obj_align(save, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    update_totals_edit_display();
    update_status(editGrams ? "Gesamt-Mahlgut korrigieren" : "Gesamt-Shots korrigieren");
}

static void button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    if (consume_suppressed_click()) {
        return;
    }
    ui_t4s3_notify_activity();

    const char *action = static_cast<const char *>(lv_event_get_user_data(event));
    if (!action) {
        return;
    }

    if (strcmp(action, "tara") == 0) {
        if (t4s3_scale_is_ready()) {
            if (t4s3_scale_tare()) {
                hx711DisplayGrams = 0.0f;
                hx711DisplayValid = true;
                set_text(weightLabel, "0,0 g");
                set_text_if_changed(systemHx711GramsLabel, "0,0 g");
                set_text_if_changed(scaleCalibrationWeightLabel, "0,0 g");
                set_text_if_changed(simLabel, "HX711-Gewicht aktiv");
                manualTareSaveArmed = !autodetectEnabled;
                update_status(autodetectEnabled ? "Tara gesetzt" : "Tara gesetzt - Save wird vorbereitet");
                set_save_ready(false);
            } else {
                update_status("Tara fehlgeschlagen - HX711 nicht bereit");
            }
        } else {
            simTenths = 0;
            manualTareSaveArmed = false;
            set_text(weightLabel, "0,0 g");
            update_status("Tara gedrückt - Demo-Gewicht auf 0,0 g gesetzt");
        }
    } else if (strcmp(action, "save") == 0) {
        if (!saveReady) {
            update_status("Save noch nicht bereit");
            return;
        }

        const int32_t saveTenths = current_save_weight_tenths();
        if (saveTenths <= 0) {
            update_status("Save ignoriert - Gewicht ist 0,0 g");
            return;
        }

        add_saved_dose(saveTenths);

        char msg[96];
        char grams[24];
        format_grams(grams, sizeof(grams), saveTenths);
        snprintf(msg, sizeof(msg), "Bezug %s gespeichert", grams);
        update_status(msg);
        manualTareSaveArmed = false;
        set_save_ready(false);
        save_current_ui_settings();
    } else if (strcmp(action, "autodetect") == 0) {
        autodetectEnabled = !autodetectEnabled;
        autodetectDetectedGefaess = kNoDetectedGefaess;
        autodetectPendingGefaess = kNoDetectedGefaess;
        autodetectAutoTaredGefaess = kNoDetectedGefaess;
        autodetectPendingSinceMs = 0;
        manualTareSaveArmed = false;
        set_save_ready(false);
        update_autodetect_display();
        update_status(autodetectEnabled ? "Autodetect eingeschaltet" : "Autodetect ausgeschaltet");
        save_current_ui_settings();
    } else if (strcmp(action, "maintenance_warning") == 0) {
        navigate_to(Page::SettingsWartung);
    } else if (strcmp(action, "target_open") == 0) {
        open_target_overlay();
    } else if (strcmp(action, "target_overlay_minus") == 0) {
        if (draftTargetTenths > 50) {
            draftTargetTenths -= draftTargetStepTenths;
            if (draftTargetTenths < 50) {
                draftTargetTenths = 50;
            }
        }
        update_target_overlay_display();
    } else if (strcmp(action, "target_overlay_plus") == 0) {
        if (draftTargetTenths < 600) {
            draftTargetTenths += draftTargetStepTenths;
            if (draftTargetTenths > 600) {
                draftTargetTenths = 600;
            }
        }
        update_target_overlay_display();
    } else if (strcmp(action, "target_step_minus") == 0) {
        if (draftTargetStepTenths == 10) {
            draftTargetStepTenths = 5;
        } else if (draftTargetStepTenths == 5) {
            draftTargetStepTenths = 1;
        }
        update_target_overlay_display();
    } else if (strcmp(action, "target_step_plus") == 0) {
        if (draftTargetStepTenths == 1) {
            draftTargetStepTenths = 5;
        } else if (draftTargetStepTenths == 5) {
            draftTargetStepTenths = 10;
        }
        update_target_overlay_display();
    } else if (strcmp(action, "target_overlay_cancel") == 0) {
        close_target_overlay(false);
    } else if (strcmp(action, "target_overlay_save") == 0) {
        close_target_overlay(true);
    } else if (strcmp(action, "vessel_select") == 0) {
        open_vessel_overlay();
    } else if (strncmp(action, "vessel_option_", 14) == 0) {
        const uint8_t idx = static_cast<uint8_t>(action[14] - '0');
        if (idx < 4) {
            draftVesselIndex = idx;
            close_vessel_overlay(true);
        }
    } else if (strcmp(action, "vessel_cancel") == 0) {
        close_vessel_overlay(false);
    } else if (strcmp(action, "vessel_save") == 0) {
        close_vessel_overlay(true);
    } else if (strcmp(action, "timer") == 0 || strcmp(action, "timer_start_stop") == 0) {
        set_timer_running(!timerRunning);
    } else if (strcmp(action, "timer_reset") == 0) {
        reset_timer();
    } else if (strcmp(action, "nav_waage") == 0) {
        navigate_to(Page::Waage);
    } else if (strcmp(action, "nav_stoppuhr") == 0) {
        navigate_to(Page::Stoppuhr);
    } else if (strcmp(action, "nav_daten") == 0) {
        navigate_to(Page::Daten);
    } else if (strcmp(action, "data_stats") == 0) {
        navigate_to(Page::DatenMahldaten);
    } else if (strcmp(action, "data_system") == 0) {
        navigate_to(Page::DatenSystem);
    } else if (strcmp(action, "data_back") == 0) {
        navigate_to(Page::Daten);
    } else if (strcmp(action, "nav_settings") == 0) {
        navigate_to(Page::Settings);
    } else if (strcmp(action, "settings_wartung") == 0) {
        navigate_to(Page::SettingsWartung);
    } else if (strcmp(action, "settings_back") == 0) {
        navigate_to(Page::Settings);
    } else if (strcmp(action, "maintenance_reset_machine") == 0) {
        open_maintenance_reset_overlay(action);
    } else if (strcmp(action, "maintenance_reset_grinder") == 0) {
        open_maintenance_reset_overlay(action);
    } else if (strcmp(action, "maintenance_reset_filter") == 0) {
        open_maintenance_reset_overlay(action);
    } else if (strcmp(action, "maintenance_reset_cancel") == 0) {
        close_maintenance_reset_overlay();
    } else if (strcmp(action, "maintenance_reset_confirm") == 0) {
        if (maintenancePendingAction) {
            maintenanceRequestedAction = maintenancePendingAction;
            maintenanceResetRequestedAtMs = millis();
            update_status("Wartungs-Reset wird vorbereitet ...");
        }
    } else if (strcmp(action, "settings_waage") == 0) {
        navigate_to(Page::SettingsWaage);
    } else if (strcmp(action, "scale_calibration") == 0) {
        scaleCalibrationStep = 1;
        scaleCalibrationTargetTenths = 2000;
        scaleCalibrationIncrementIndex = 2;
        open_scale_calibration_overlay();
    } else if (strcmp(action, "scale_cal_close") == 0) {
        close_scale_calibration_overlay();
    } else if (strcmp(action, "scale_cal_to_load") == 0) {
        scaleCalibrationStep = 2;
        open_scale_calibration_overlay();
    } else if (strcmp(action, "scale_cal_back_tare") == 0) {
        scaleCalibrationStep = 1;
        open_scale_calibration_overlay();
    } else if (strcmp(action, "scale_cal_to_weight") == 0) {
        scaleCalibrationStep = 3;
        open_scale_calibration_overlay();
    } else if (strcmp(action, "scale_cal_to_confirm") == 0) {
        scaleCalibrationStep = 4;
        open_scale_calibration_overlay();
    } else if (strcmp(action, "scale_cal_back_weight") == 0) {
        scaleCalibrationStep = 3;
        open_scale_calibration_overlay();
    } else if (strcmp(action, "scale_cal_back_load") == 0) {
        scaleCalibrationStep = 2;
        open_scale_calibration_overlay();
    } else if (strcmp(action, "scale_cal_tare") == 0) {
        if (t4s3_scale_tare()) {
            update_hx711_grams_display(0.0f, true);
            update_scale_calibration_display();
            update_status("Tara gesetzt");
        } else {
            update_status("Tara nicht möglich - HX711 nicht bereit");
        }
    } else if (strcmp(action, "scale_cal_weight_dec") == 0) {
        const int32_t inc = kScaleCalIncrementsTenths[scaleCalibrationIncrementIndex];
        scaleCalibrationTargetTenths = scaleCalibrationTargetTenths > inc ? scaleCalibrationTargetTenths - inc : 10;
        update_scale_calibration_display();
    } else if (strcmp(action, "scale_cal_weight_inc") == 0) {
        const int32_t inc = kScaleCalIncrementsTenths[scaleCalibrationIncrementIndex];
        scaleCalibrationTargetTenths = scaleCalibrationTargetTenths + inc > 50000 ? 50000 : scaleCalibrationTargetTenths + inc;
        update_scale_calibration_display();
    } else if (strcmp(action, "scale_cal_step_dec") == 0) {
        if (scaleCalibrationIncrementIndex > 0) {
            --scaleCalibrationIncrementIndex;
        } else {
            scaleCalibrationIncrementIndex = kScaleCalIncrementCount - 1;
        }
        update_scale_calibration_display();
    } else if (strcmp(action, "scale_cal_step_inc") == 0) {
        scaleCalibrationIncrementIndex = (scaleCalibrationIncrementIndex + 1) % kScaleCalIncrementCount;
        update_scale_calibration_display();
    } else if (strcmp(action, "scale_cal_save") == 0) {
        const float calibrationWeight = static_cast<float>(scaleCalibrationTargetTenths) / 10.0f;
        if (t4s3_scale_calibrate(calibrationWeight)) {
            char msg[64];
            char grams[24];
            format_grams(grams, sizeof(grams), scaleCalibrationTargetTenths);
            snprintf(msg, sizeof(msg), "Kalibrierung mit %s gespeichert", grams);
            close_scale_calibration_overlay();
            update_status(msg);
        } else {
            update_status("Kalibrierung fehlgeschlagen");
        }
    } else if (strcmp(action, "vessels_measure") == 0) {
        open_gefaess_measure_overlay();
    } else if (strcmp(action, "vessels_manage") == 0) {
        open_gefaess_manage_overlay();
    } else if (strcmp(action, "gefaess_manage_close") == 0) {
        close_gefaess_manage_overlay();
    } else if (strcmp(action, "gefaess_delete_cancel") == 0) {
        close_gefaess_delete_overlay();
    } else if (strcmp(action, "gefaess_delete_confirm") == 0) {
        if (gefaessDeleteSlot < T4S3_GEFAESS_SLOT_COUNT) {
            gefaessWeightTenths[gefaessDeleteSlot] = -1;
            autodetectDetectedGefaess = kNoDetectedGefaess;
            save_current_ui_settings();
            update_gefaess_storage_display();
            close_gefaess_delete_overlay();
            if (gefaessManageOverlay && lv_obj_is_valid(gefaessManageOverlay)) {
                close_gefaess_manage_overlay();
                open_gefaess_manage_overlay();
            } else {
                update_gefaess_manage_display();
            }
            update_status("Gefäß gelöscht");
        } else {
            close_gefaess_delete_overlay();
        }
    } else if (strncmp(action, "gefaess_delete_", 15) == 0) {
        const uint8_t idx = static_cast<uint8_t>(action[15] - '0');
        open_gefaess_delete_overlay(idx);
    } else if (strcmp(action, "gefaess_measure_cancel") == 0) {
        close_gefaess_measure_overlay();
        update_status("Gefäß einmessen abgebrochen");
    } else if (strncmp(action, "gefaess_measure_slot_", 21) == 0) {
        const uint8_t idx = static_cast<uint8_t>(action[21] - '0');
        if (idx < T4S3_GEFAESS_SLOT_COUNT) {
            gefaessMeasureSlot = idx;
            gefaessMeasureStep = 1;
            render_gefaess_measure_overlay();
        }
    } else if (strcmp(action, "gefaess_measure_tara") == 0) {
        if (t4s3_scale_is_ready()) {
            if (t4s3_scale_tare()) {
                update_hx711_grams_display(0.0f, true);
                update_gefaess_measure_weight_display();
                update_status("Tara für Gefäßmessung gesetzt");
            } else {
                update_status("Tara nicht möglich - HX711 nicht bereit");
            }
        } else {
            gefaessMeasureTareTenths = simTenths;
            simTenths = 0;
            set_text(weightLabel, "0,0 g");
            update_gefaess_measure_weight_display();
            update_status("Demo-Tara für Gefäßmessung gesetzt");
        }
    } else if (strcmp(action, "gefaess_measure_to_load") == 0) {
        gefaessMeasureStep = 2;
        render_gefaess_measure_overlay();
    } else if (strcmp(action, "gefaess_measure_back_tare") == 0) {
        gefaessMeasureStep = 1;
        render_gefaess_measure_overlay();
    } else if (strcmp(action, "gefaess_measure_to_confirm") == 0) {
        gefaessMeasureValueTenths = current_gefaess_measure_tenths();
        gefaessMeasureStep = 3;
        render_gefaess_measure_overlay();
    } else if (strcmp(action, "gefaess_measure_save") == 0) {
        if (gefaessMeasureValueTenths < 1) {
            update_status("Gefäß nicht gespeichert - Gewicht ist 0,0 g");
            return;
        }
        gefaessWeightTenths[gefaessMeasureSlot] = gefaessMeasureValueTenths;
        autodetectDetectedGefaess = kNoDetectedGefaess;
        save_current_ui_settings();
        update_gefaess_storage_display();
        close_gefaess_measure_overlay();
        if (gefaessManageOverlay && lv_obj_is_valid(gefaessManageOverlay)) {
            close_gefaess_manage_overlay();
            open_gefaess_manage_overlay();
        } else {
            update_gefaess_manage_display();
        }
        update_status("Gefäßgewicht gespeichert");
    } else if (strcmp(action, "totals_edit") == 0) {
        navigate_to(Page::SettingsTotals);
    } else if (strcmp(action, "totals_shots_open") == 0) {
        open_totals_edit_overlay(false);
    } else if (strcmp(action, "totals_grams_open") == 0) {
        open_totals_edit_overlay(true);
    } else if (strcmp(action, "totals_mode_prev") == 0) {
        change_totals_edit_mode(-1);
    } else if (strcmp(action, "totals_mode_next") == 0) {
        change_totals_edit_mode(1);
    } else if (strcmp(action, "totals_shots_dec") == 0) {
        adjust_totals_edit_shots(-1);
    } else if (strcmp(action, "totals_shots_inc") == 0) {
        adjust_totals_edit_shots(1);
    } else if (strcmp(action, "totals_grams_dec") == 0) {
        adjust_totals_edit_grams(-1);
    } else if (strcmp(action, "totals_grams_inc") == 0) {
        adjust_totals_edit_grams(1);
    } else if (strcmp(action, "totals_step_dec") == 0) {
        change_totals_edit_step(-1);
    } else if (strcmp(action, "totals_step_inc") == 0) {
        change_totals_edit_step(1);
    } else if (strcmp(action, "totals_edit_cancel") == 0) {
        close_totals_edit_overlay(false);
    } else if (strcmp(action, "totals_edit_save") == 0) {
        close_totals_edit_overlay(true);
    } else if (strcmp(action, "settings_wlan") == 0) {
        navigate_to(Page::SettingsWlan);
    } else if (strcmp(action, "wlan_start_setup") == 0) {
        if (t4s3_wifi_start_setup_ap()) {
            update_wlan_page_display();
            open_wlan_setup_overlay(coffeeWifiSetupCredentialsSavedPendingRestart());
        } else {
            update_status("Setup-AP konnte nicht gestartet werden");
        }
    } else if (strcmp(action, "wlan_setup_overlay_action") == 0) {
        if (coffeeWifiSetupCredentialsSavedPendingRestart() || wlanSetupOverlaySavedMode) {
            update_status("Neustart wird ausgeführt ...");
            delay(120);
            ESP.restart();
        } else {
            coffeeWifiStopSetupAp();
            close_wlan_setup_overlay();
            update_wlan_page_display();
            navigate_to(Page::SettingsWlan);
            update_status("Setup-WLAN abgebrochen");
        }
    } else if (strcmp(action, "settings_system") == 0) {
        navigate_to(Page::SettingsSystem);
    } else if (strcmp(action, "timeout_minus") == 0) {
        change_screen_timeout(-1);
    } else if (strcmp(action, "timeout_plus") == 0) {
        change_screen_timeout(1);
    } else if (strcmp(action, "system_logs") == 0) {
        update_status("Logs / Diagnose: Anzeige folgt später");
    } else if (strcmp(action, "system_restart") == 0) {
        open_restart_overlay();
    } else if (strcmp(action, "restart_cancel") == 0) {
        close_restart_overlay();
    } else if (strcmp(action, "restart_confirm") == 0) {
        update_status("Neustart wird ausgeführt ...");
        delay(120);
        ESP.restart();
    }
}


void close_target_overlay(bool save)
{
    if (save) {
        demoTargetTenths = draftTargetTenths;
        targetStepTenths = draftTargetStepTenths;
        targetTenthsBySiebtraeger[currentVesselIndex] = demoTargetTenths;
        save_current_ui_settings();
        update_target_display();
    lv_obj_move_foreground(weightLabel);

        char grams[24];
        char msg[72];
        format_grams(grams, sizeof(grams), demoTargetTenths);
        snprintf(msg, sizeof(msg), "Sollgewicht gespeichert: %s", grams);
        update_status(msg);
    } else {
        update_status("Sollgewicht nicht geändert");
    }

    if (targetOverlay) {
        lv_obj_del(targetOverlay);
        targetOverlay = nullptr;
        targetOverlayValueLabel = nullptr;
        targetOverlayStepLabel = nullptr;
    }
}


void close_restart_overlay()
{
    if (restartOverlay) {
        lv_obj_del(restartOverlay);
        restartOverlay = nullptr;
    }
    update_status("Neustart abgebrochen");
}

void open_restart_overlay()
{
    if (restartOverlay) {
        return;
    }

    lv_obj_t *screen = lv_scr_act();

    restartOverlay = lv_obj_create(screen);
    lv_obj_set_size(restartOverlay, screenWidth, screenHeight);
    lv_obj_align(restartOverlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(restartOverlay, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(restartOverlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(restartOverlay, 0, 0);
    lv_obj_set_style_pad_all(restartOverlay, 0, 0);
    lv_obj_clear_flag(restartOverlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(restartOverlay);
    style_panel(panel);
    lv_obj_set_size(panel, 430, 230);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "Neustart");
    style_label(title, COLOR_GREEN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *msg = lv_label_create(panel);
    lv_label_set_text(msg, "Waage jetzt neu starten?");
    lv_obj_set_width(msg, 360);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    style_label(msg, COLOR_WHITE);
    lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 58);

    lv_obj_t *hint = lv_label_create(panel);
    lv_label_set_text(hint, "Nicht gespeicherte Änderungen gehen verloren.");
    lv_obj_set_width(hint, 360);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    style_label(hint, COLOR_MUTED);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 92);

    lv_obj_t *cancel = create_button(panel, "Abbr.", "restart_cancel", 150, 56);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 18, 0);

    lv_obj_t *confirm = create_button(panel, "Neustart", "restart_confirm", 170, 56);
    lv_obj_align(confirm, LV_ALIGN_BOTTOM_RIGHT, -18, 0);

    lv_obj_move_foreground(restartOverlay);
    update_status("Neustart bestätigen oder abbrechen");
}

void close_wlan_setup_overlay()
{
    if (wlanSetupOverlay && lv_obj_is_valid(wlanSetupOverlay)) {
        lv_obj_del(wlanSetupOverlay);
    }
    wlanSetupOverlay = nullptr;
    wlanSetupOverlayTitleLabel = nullptr;
    wlanSetupOverlayMessageLabel = nullptr;
    wlanSetupOverlayButtonLabel = nullptr;
    wlanSetupOverlaySavedMode = false;
}

void update_wlan_setup_overlay_mode(bool savedMode)
{
    wlanSetupOverlaySavedMode = savedMode;

    if (!wlanSetupOverlay || !lv_obj_is_valid(wlanSetupOverlay)) {
        return;
    }

    if (savedMode) {
        set_text(wlanSetupOverlayTitleLabel, "WLAN-Daten gespeichert");
        set_text(wlanSetupOverlayMessageLabel,
                 "Die WLAN-Daten wurden gespeichert und aktiviert.\n"
                 "Bitte starte die Waage neu.");
        set_text(wlanSetupOverlayButtonLabel, "Neustart");
        update_status("WLAN-Daten gespeichert - Neustart erforderlich");
    } else {
        set_text(wlanSetupOverlayTitleLabel, "Setup-WLAN aktiv");
        set_text(wlanSetupOverlayMessageLabel,
                 "Bitte mit WLAN Waagen-Setup verbinden.\n"
                 "Dann im Browser 192.168.4.1 aufrufen.");
        set_text(wlanSetupOverlayButtonLabel, "Abbrechen");
        update_status("Setup-WLAN aktiv: Waagen-Setup / 192.168.4.1");
    }
}

void open_wlan_setup_overlay(bool savedMode)
{
    if (wlanSetupOverlay && !lv_obj_is_valid(wlanSetupOverlay)) {
        wlanSetupOverlay = nullptr;
        wlanSetupOverlayTitleLabel = nullptr;
        wlanSetupOverlayMessageLabel = nullptr;
        wlanSetupOverlayButtonLabel = nullptr;
    }

    if (wlanSetupOverlay) {
        lv_obj_clear_flag(wlanSetupOverlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(wlanSetupOverlay);
        update_wlan_setup_overlay_mode(savedMode);
        return;
    }

    lv_obj_t *screen = lv_scr_act();

    wlanSetupOverlay = lv_obj_create(screen);
    lv_obj_set_size(wlanSetupOverlay, screenWidth, screenHeight);
    lv_obj_align(wlanSetupOverlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(wlanSetupOverlay, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(wlanSetupOverlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(wlanSetupOverlay, 0, 0);
    lv_obj_set_style_pad_all(wlanSetupOverlay, 0, 0);
    lv_obj_clear_flag(wlanSetupOverlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(wlanSetupOverlay);
    style_panel(panel);
    lv_obj_set_size(panel, 460, 250);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    wlanSetupOverlayTitleLabel = lv_label_create(panel);
    lv_obj_set_width(wlanSetupOverlayTitleLabel, 390);
    lv_obj_set_style_text_align(wlanSetupOverlayTitleLabel, LV_TEXT_ALIGN_CENTER, 0);
    style_label(wlanSetupOverlayTitleLabel, COLOR_GREEN);
    lv_obj_align(wlanSetupOverlayTitleLabel, LV_ALIGN_TOP_MID, 0, 8);

    wlanSetupOverlayMessageLabel = lv_label_create(panel);
    lv_obj_set_width(wlanSetupOverlayMessageLabel, 390);
    lv_obj_set_style_text_align(wlanSetupOverlayMessageLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(wlanSetupOverlayMessageLabel, LV_LABEL_LONG_WRAP);
    style_label(wlanSetupOverlayMessageLabel, COLOR_WHITE);
    lv_obj_align(wlanSetupOverlayMessageLabel, LV_ALIGN_TOP_MID, 0, 62);

    lv_obj_t *action = create_button(panel, "Abbrechen", "wlan_setup_overlay_action", 240, 62);
    lv_obj_align(action, LV_ALIGN_BOTTOM_MID, 0, -4);
    wlanSetupOverlayButtonLabel = lv_obj_get_child(action, 0);

    lv_obj_move_foreground(wlanSetupOverlay);
    update_wlan_setup_overlay_mode(savedMode);
}

void process_wlan_setup_overlay_state()
{
    if (coffeeWifiSetupCredentialsSavedPendingRestart()) {
        if (!wlanSetupOverlay || !lv_obj_is_valid(wlanSetupOverlay) || !wlanSetupOverlaySavedMode) {
            open_wlan_setup_overlay(true);
        }
        return;
    }

    if (wlanSetupOverlay && !lv_obj_is_valid(wlanSetupOverlay)) {
        wlanSetupOverlay = nullptr;
        wlanSetupOverlayTitleLabel = nullptr;
        wlanSetupOverlayMessageLabel = nullptr;
        wlanSetupOverlayButtonLabel = nullptr;
    }
}
void open_target_overlay()
{
    if (targetOverlay) {
        return;
    }

    draftTargetTenths = demoTargetTenths;
draftTargetStepTenths = targetStepTenths;

    lv_obj_t *screen = lv_scr_act();

    targetOverlay = lv_obj_create(screen);
    lv_obj_set_size(targetOverlay, screenWidth, screenHeight);
    lv_obj_align(targetOverlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(targetOverlay, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(targetOverlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(targetOverlay, 0, 0);
    lv_obj_set_style_pad_all(targetOverlay, 0, 0);
    lv_obj_clear_flag(targetOverlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(targetOverlay);
    style_panel(panel);
    lv_obj_set_size(panel, 430, 350);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "Sollgewicht");
    style_label(title, COLOR_GREEN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t *valueTitle = lv_label_create(panel);
    lv_label_set_text(valueTitle, "Wert");
    style_label(valueTitle, COLOR_MUTED);
    lv_obj_align(valueTitle, LV_ALIGN_TOP_MID, 0, 42);

    targetOverlayValueLabel = lv_label_create(panel);
    lv_obj_set_width(targetOverlayValueLabel, 150);
    lv_obj_set_style_text_align(targetOverlayValueLabel, LV_TEXT_ALIGN_CENTER, 0);
    style_label(targetOverlayValueLabel, COLOR_WHITE);
    lv_obj_align(targetOverlayValueLabel, LV_ALIGN_TOP_MID, 0, 72);

    lv_obj_t *minus = create_button(panel, "-", "target_overlay_minus", 86, 58);
    lv_obj_align(minus, LV_ALIGN_TOP_MID, -130, 62);

    lv_obj_t *plus = create_button(panel, "+", "target_overlay_plus", 86, 58);
    lv_obj_align(plus, LV_ALIGN_TOP_MID, 130, 62);

    lv_obj_t *stepTitle = lv_label_create(panel);
    lv_label_set_text(stepTitle, "Schrittweite");
    style_label(stepTitle, COLOR_MUTED);
    lv_obj_align(stepTitle, LV_ALIGN_TOP_MID, 0, 138);

    targetOverlayStepLabel = lv_label_create(panel);
    lv_obj_set_width(targetOverlayStepLabel, 150);
    lv_obj_set_style_text_align(targetOverlayStepLabel, LV_TEXT_ALIGN_CENTER, 0);
    style_label(targetOverlayStepLabel, COLOR_WHITE);
    lv_obj_align(targetOverlayStepLabel, LV_ALIGN_TOP_MID, 0, 168);

    lv_obj_t *stepMinus = create_button(panel, "-", "target_step_minus", 86, 52);
    lv_obj_align(stepMinus, LV_ALIGN_TOP_MID, -130, 158);

    lv_obj_t *stepPlus = create_button(panel, "+", "target_step_plus", 86, 52);
    lv_obj_align(stepPlus, LV_ALIGN_TOP_MID, 130, 158);

    update_target_overlay_display();

    lv_obj_t *cancel = create_button(panel, "Abbr.", "target_overlay_cancel", 150, 52);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *save = create_button(panel, "Speichern", "target_overlay_save", 175, 52);
    lv_obj_align(save, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    update_status("Sollgewicht bearbeiten");
}



void close_vessel_overlay(bool save)
{
    if (save && draftVesselIndex != currentVesselIndex) {
        currentVesselIndex = draftVesselIndex;
        demoTargetTenths = targetTenthsBySiebtraeger[currentVesselIndex];
        draftTargetTenths = demoTargetTenths;
        update_vessel_display();
        update_target_display();
        save_current_ui_settings();

        char msg[72];
        snprintf(msg, sizeof(msg), "Siebträger gespeichert: %s", vessel_name(currentVesselIndex));
        update_status(msg);
    } else {
        update_status("Siebträger-Auswahl nicht geändert");
    }

    if (vesselOverlay) {
        lv_obj_del(vesselOverlay);
    }
    vesselOverlay = nullptr;
    for (uint8_t i = 0; i < kMaxSiebtraegerSlots; ++i) {
        vesselOptionLabels[i] = nullptr;
    }
}

void close_gefaess_manage_overlay()
{
    close_gefaess_delete_overlay();
    if (gefaessManageOverlay && lv_obj_is_valid(gefaessManageOverlay)) {
        lv_obj_del(gefaessManageOverlay);
    }
    gefaessManageOverlay = nullptr;
    for (uint8_t i = 0; i < T4S3_GEFAESS_SLOT_COUNT; ++i) {
        gefaessManageLabels[i] = nullptr;
    }
}

void open_gefaess_manage_overlay()
{
    if (gefaessManageOverlay) {
        close_gefaess_manage_overlay();
    }

    lv_obj_t *screen = lv_scr_act();
    if (!screen) {
        return;
    }

    gefaessManageOverlay = lv_obj_create(screen);
    lv_obj_set_size(gefaessManageOverlay, screenWidth, screenHeight);
    lv_obj_align(gefaessManageOverlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(gefaessManageOverlay, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(gefaessManageOverlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(gefaessManageOverlay, 0, 0);
    lv_obj_set_style_pad_all(gefaessManageOverlay, 0, 0);
    lv_obj_clear_flag(gefaessManageOverlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(gefaessManageOverlay);
    style_panel(panel);
    lv_obj_set_size(panel, 430, 350);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "Gefäße verwalten");
    style_label(title, COLOR_WHITE);
    lv_obj_set_width(title, 370);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *hint = lv_label_create(panel);
    lv_label_set_text(hint, "Gespeicherte Gefäßgewichte für Autodetect");
    style_label(hint, COLOR_MUTED);
    lv_obj_set_width(hint, 370);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_DOT);
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 0, 36);

    for (uint8_t i = 0; i < T4S3_GEFAESS_SLOT_COUNT; ++i) {
        lv_obj_t *row = lv_obj_create(panel);
        lv_obj_set_size(row, 370, 50);
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, 76 + i * 58);
        lv_obj_set_style_bg_color(row, lv_color_hex(COLOR_GREEN_DARK), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_30, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(COLOR_GREEN), 0);
        lv_obj_set_style_border_opa(row, LV_OPA_40, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 10, 0);
        lv_obj_set_style_pad_left(row, 12, 0);
        lv_obj_set_style_pad_right(row, 12, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        gefaessManageLabels[i] = lv_label_create(row);
        style_label(gefaessManageLabels[i], COLOR_WHITE);
        lv_obj_set_width(gefaessManageLabels[i], 245);
        lv_label_set_long_mode(gefaessManageLabels[i], LV_LABEL_LONG_DOT);
        lv_obj_align(gefaessManageLabels[i], LV_ALIGN_LEFT_MID, 0, 0);

        if (gefaessWeightTenths[i] >= 0) {
            const char *deleteAction = (i == 0) ? "gefaess_delete_0" : ((i == 1) ? "gefaess_delete_1" : "gefaess_delete_2");
            lv_obj_t *deleteButton = create_button(row, "Löschen", deleteAction, 90, 36);
            lv_obj_align(deleteButton, LV_ALIGN_RIGHT_MID, 0, 0);
        }
    }

    lv_obj_t *measure = create_button(panel, "Gefäß einmessen", "vessels_measure", 210, 44);
    lv_obj_align(measure, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *close = create_button(panel, "Schließen", "gefaess_manage_close", 145, 44);
    lv_obj_align(close, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    update_gefaess_manage_display();
    update_status("Gefäße verwalten geöffnet");
}

void open_vessel_overlay()
{
    if (vesselOverlay) {
        return;
    }

    draftVesselIndex = currentVesselIndex;

    lv_obj_t *screen = lv_scr_act();

    vesselOverlay = lv_obj_create(screen);
    lv_obj_set_size(vesselOverlay, screenWidth, screenHeight);
    lv_obj_align(vesselOverlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(vesselOverlay, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(vesselOverlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(vesselOverlay, 0, 0);
    lv_obj_set_style_pad_all(vesselOverlay, 0, 0);
    lv_obj_clear_flag(vesselOverlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(vesselOverlay);
    style_panel(panel);
    lv_obj_set_size(panel, 430, 340);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "Siebträger wählen");
    style_label(title, COLOR_GREEN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    const char *actions[] = {
        "vessel_option_0",
        "vessel_option_1",
        "vessel_option_2",
        "vessel_option_3",
    };

    for (uint8_t i = 0; i < kMaxSiebtraegerSlots; ++i) {
        lv_obj_t *option = create_button(panel, "", actions[i], 350, 44);
        lv_obj_align(option, LV_ALIGN_TOP_MID, 0, 44 + i * 50);

        vesselOptionLabels[i] = lv_label_create(option);
        style_label(vesselOptionLabels[i], COLOR_WHITE);
        lv_obj_set_width(vesselOptionLabels[i], 310);
        lv_obj_set_style_text_align(vesselOptionLabels[i], LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(vesselOptionLabels[i], LV_ALIGN_LEFT_MID, 10, 0);
    }
    update_vessel_overlay_display();

    lv_obj_t *close = create_button(panel, "Schließen", "vessel_cancel", 145, 44);
    lv_obj_align(close, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    update_status("Siebträger direkt per Touch wählen");
}


lv_obj_t *create_button(lv_obj_t *parent, const char *text, const char *action, int width = 170, int height = 58)
{
    lv_obj_t *btn = lv_btn_create(parent);
    style_button(btn);
    lv_obj_set_width(btn, width);
    lv_obj_set_height(btn, height);
    lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, const_cast<char *>(action));

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    style_label(label, COLOR_WHITE);
    lv_obj_center(label);
    if (strcmp(action, "timer") == 0 || strcmp(action, "timer_start_stop") == 0) {
        timerButtonLabel = label;
        lv_label_set_text(timerButtonLabel, timerRunning ? "Stop" : "Start");
    } else if (strcmp(action, "autodetect") == 0) {
        autodetectButtonLabel = label;
        lv_label_set_text(autodetectButtonLabel, autodetectEnabled ? "Autodetect: AN" : "Autodetect: AUS");
    } else if (strcmp(action, "vessel_select") == 0) {
        vesselLabel = label;
        lv_label_set_text(vesselLabel, vessel_name(currentVesselIndex));
    }
    return btn;
}

lv_obj_t *create_nav_button(lv_obj_t *parent, const char *text, const char *action, bool active)
{
    lv_obj_t *btn = lv_btn_create(parent);
    style_nav_button(btn, active);
    lv_obj_set_size(btn, 128, 60);
    lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, const_cast<char *>(action));

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    style_label(label, active ? COLOR_WHITE : COLOR_MUTED);
    lv_obj_center(label);
    return btn;
}

lv_obj_t *create_panel_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    style_label(label, COLOR_MUTED);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 0);
    return label;
}

lv_obj_t *create_info_card(lv_obj_t *parent, const char *title, const char *value, int x, int y, int w, int h, lv_obj_t **valueOut = nullptr)
{
    lv_obj_t *card = lv_obj_create(parent);
    style_panel(card);
    lv_obj_set_size(card, w, h);
    lv_obj_align(card, LV_ALIGN_TOP_LEFT, x, y);

    lv_obj_t *titleLabel = lv_label_create(card);
    lv_label_set_text(titleLabel, title);
    style_label(titleLabel, COLOR_MUTED);
    lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *valueLabel = lv_label_create(card);
    lv_label_set_text(valueLabel, value);
    style_label(valueLabel, COLOR_WHITE);
    lv_obj_align(valueLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    if (valueOut) {
        *valueOut = valueLabel;
    }
    return card;
}

void update_sim_weight()
{
    if (hx711DisplayValid) {
        return;
    }

    char buf[24];
    format_grams(buf, sizeof(buf), simTenths);
    set_text(weightLabel, buf);

    char sim[64];
    snprintf(sim, sizeof(sim), "Fallback-Gewicht: %s", buf);
    set_text(simLabel, sim);
}

void create_wifi_quality_icon(lv_obj_t *screen,
                              int x,
                              int y,
                              uint8_t quality,
                              lv_obj_t **barsOut = nullptr)
{
    for (uint8_t i = 0; i < 4; ++i) {
        const int h = 6 + i * 4;
        lv_obj_t *bar = lv_obj_create(screen);
        style_plain_block(bar, i < quality ? COLOR_GREEN : COLOR_DIM);
        lv_obj_set_size(bar, 5, h);
        lv_obj_align(bar, LV_ALIGN_TOP_LEFT, x + i * 8, y + (18 - h));
        if (barsOut) {
            barsOut[i] = bar;
        }
    }
}

void update_wifi_bars(lv_obj_t **bars, uint8_t quality)
{
    if (!bars) {
        return;
    }

    for (uint8_t i = 0; i < 4; ++i) {
        if (bars[i]) {
            style_plain_block(bars[i], i < quality ? COLOR_GREEN : COLOR_DIM);
        }
    }
}

void update_header_wifi_display()
{
    T4S3WifiStatus wifi;
    t4s3_wifi_get_status(wifi);
    update_wifi_bars(headerWifiBars, wifi.qualityBars);
}

void update_wlan_page_display()
{
    T4S3WifiStatus wifi;
    t4s3_wifi_get_status(wifi);

    set_text(wlanStatusLabel, wifi.status);
    if (wlanStatusLabel) {
        style_label(wlanStatusLabel, wifi.connected || wifi.setupApActive ? COLOR_GREEN : COLOR_MUTED);
    }

    set_text(wlanSsidLabel, wifi.ssid);
    set_text(wlanIpLabel, wifi.ip);
    set_text(wlanSignalLabel, wifi.signal);
    set_text(wlanSetupApLabel, wifi.setupApSsid);
    set_text(wlanWebUiLabel, wifi.webUiAddress);
    update_wifi_bars(wlanQualityBars, wifi.qualityBars);
}

void create_header(lv_obj_t *screen)
{
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Single-Dose-Waage");
    style_label(title, COLOR_GREEN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 18, 12);

    clockLabel = lv_label_create(screen);
    lv_obj_set_width(clockLabel, 190);
    lv_obj_set_style_text_align(clockLabel, LV_TEXT_ALIGN_RIGHT, 0);
    style_label(clockLabel, COLOR_WHITE);
    lv_obj_align(clockLabel, LV_ALIGN_TOP_RIGHT, -74, 12);
    update_clock_display();

    create_wifi_quality_icon(screen, 540, 10, 0, headerWifiBars);
    update_header_wifi_display();
}

void create_footer(lv_obj_t *screen, const char *statusText, const char *hintText)
{
    // Auf dem kleinen AMOLED ist eine einzeilige Statusleiste deutlich lesbarer.
    // Detail-/Hilfetexte führten zu Überlagerungen mit der Navigation.
    (void)hintText;

    statusLabel = lv_label_create(screen);
    lv_label_set_text(statusLabel, statusText);
    lv_obj_set_width(statusLabel, 560);
    lv_label_set_long_mode(statusLabel, LV_LABEL_LONG_DOT);
    style_label(statusLabel, COLOR_WHITE);
    lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_LEFT, 18, -98);

    touchLabel = nullptr;
}

void create_nav(lv_obj_t *screen)
{
    lv_obj_t *navWaage = create_nav_button(screen, "Waage", "nav_waage", activePage == Page::Waage);
    lv_obj_align(navWaage, LV_ALIGN_BOTTOM_LEFT, 18, -16);

    lv_obj_t *navStoppuhr = create_nav_button(screen, "Stoppuhr", "nav_stoppuhr", activePage == Page::Stoppuhr);
    lv_obj_align(navStoppuhr, LV_ALIGN_BOTTOM_LEFT, 158, -16);

    const bool dataActive = activePage == Page::Daten ||
                            activePage == Page::DatenMahldaten ||
                            activePage == Page::DatenSystem;
    lv_obj_t *navDaten = create_nav_button(screen, "Daten", "nav_daten", dataActive);
    lv_obj_align(navDaten, LV_ALIGN_BOTTOM_LEFT, 298, -16);

    const bool settingsActive = activePage == Page::Settings ||
                                activePage == Page::SettingsWartung ||
                                activePage == Page::SettingsWaage ||
                                activePage == Page::SettingsTotals ||
                                activePage == Page::SettingsWlan ||
                                activePage == Page::SettingsSystem;
    lv_obj_t *navSettings = create_nav_button(screen, "Settings", "nav_settings", settingsActive);
    lv_obj_align(navSettings, LV_ALIGN_BOTTOM_LEFT, 438, -16);
}

void create_waage_page(lv_obj_t *screen)
{
    lv_obj_t *dataPanel = lv_obj_create(screen);
    style_panel(dataPanel);
    lv_obj_set_size(dataPanel, 365, 258);
    lv_obj_align(dataPanel, LV_ALIGN_TOP_LEFT, 18, 54);
    lv_obj_clear_flag(dataPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(dataPanel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(dataPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(dataPanel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(dataPanel, "Gewicht");

    lv_obj_t *autoBox = lv_btn_create(dataPanel);
    lv_obj_set_size(autoBox, 82, 66);
    lv_obj_align(autoBox, LV_ALIGN_TOP_RIGHT, 0, -8);
    lv_obj_set_style_bg_color(autoBox, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(autoBox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(autoBox, 0, 0);
    lv_obj_set_style_radius(autoBox, 16, 0);
    lv_obj_set_style_pad_all(autoBox, 6, 0);
    lv_obj_add_event_cb(autoBox, button_event_cb, LV_EVENT_CLICKED, const_cast<char *>("autodetect"));

    autodetectStateLabel = lv_label_create(autoBox);
    lv_obj_set_width(autodetectStateLabel, 48);
    lv_obj_set_style_text_align(autodetectStateLabel, LV_TEXT_ALIGN_RIGHT, 0);
    style_label(autodetectStateLabel, COLOR_MUTED);
    lv_obj_align(autodetectStateLabel, LV_ALIGN_LEFT_MID, 0, 0);

    autodetectLed = lv_obj_create(autoBox);
    style_plain_block(autodetectLed, autodetectEnabled ? COLOR_AUTODETECT_LED_ON : COLOR_AUTODETECT_LED_OFF);
    lv_obj_set_size(autodetectLed, 14, 14);
    lv_obj_set_style_radius(autodetectLed, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(autodetectLed, LV_ALIGN_RIGHT_MID, -4, 0);
    update_autodetect_display();

    weightLabel = lv_label_create(dataPanel);
    lv_label_set_text(weightLabel, "0,0 g");
    lv_obj_set_width(weightLabel, 330);
    lv_label_set_long_mode(weightLabel, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(weightLabel, LV_TEXT_ALIGN_CENTER, 0);
    style_label(weightLabel, COLOR_WHITE);
    lv_obj_set_style_text_font(weightLabel, &lv_font_montserrat_48, 0);
    lv_obj_align(weightLabel, LV_ALIGN_CENTER, 0, -28);
    update_maintenance_due_state();
    maintenanceWarningButton = create_button(dataPanel, "Wartung erforderlich", "maintenance_warning", 337, 34);
    lv_obj_align(maintenanceWarningButton, LV_ALIGN_BOTTOM_LEFT, 0, -64);
    lv_obj_set_style_bg_color(maintenanceWarningButton, lv_color_hex(0x8B3A3A), 0);
    lv_obj_set_style_border_color(maintenanceWarningButton, lv_color_hex(0xC85A5A), 0);
    update_maintenance_warning_display();

    lv_obj_t *targetBox = lv_btn_create(dataPanel);
    lv_obj_set_size(targetBox, 337, 58);
    lv_obj_align(targetBox, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(targetBox, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(targetBox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(targetBox, lv_color_hex(COLOR_GREEN_DARK), 0);
    lv_obj_set_style_border_width(targetBox, 1, 0);
    lv_obj_set_style_radius(targetBox, 10, 0);
    lv_obj_set_style_pad_all(targetBox, 10, 0);
    lv_obj_add_event_cb(targetBox, button_event_cb, LV_EVENT_CLICKED, const_cast<char *>("target_open"));

    lv_obj_t *targetTitle = lv_label_create(targetBox);
    lv_label_set_text(targetTitle, "Sollgewicht");
    style_label(targetTitle, COLOR_MUTED);
    lv_obj_align(targetTitle, LV_ALIGN_LEFT_MID, 0, 0);

    targetLabel = lv_label_create(targetBox);
    lv_obj_set_width(targetLabel, 110);
    lv_obj_set_style_text_align(targetLabel, LV_TEXT_ALIGN_RIGHT, 0);
    style_label(targetLabel, COLOR_GREEN);
    lv_obj_align(targetLabel, LV_ALIGN_RIGHT_MID, 0, 0);
    update_target_display();
    lv_obj_move_foreground(weightLabel);


    lv_obj_t *inputPanel = lv_obj_create(screen);
    style_plain_block(inputPanel, COLOR_BG);
    lv_obj_set_size(inputPanel, 195, 258);
    lv_obj_align(inputPanel, LV_ALIGN_TOP_RIGHT, -18, 54);

    lv_obj_t *tara = create_button(inputPanel, "Tara", "tara", 175, 78);
    lv_obj_align(tara, LV_ALIGN_TOP_MID, 0, 0);

    saveButton = create_button(inputPanel, "Save", "save", 175, 78);
    lv_obj_align(saveButton, LV_ALIGN_TOP_MID, 0, 90);
    update_save_button_display();

    lv_obj_t *vesselBtn = create_button(inputPanel, vessel_name(currentVesselIndex), "vessel_select", 175, 78);
    lv_obj_align(vesselBtn, LV_ALIGN_TOP_MID, 0, 180);

    create_footer(screen,
                  "Waage bereit",
                  "");
}
void create_stoppuhr_page(lv_obj_t *screen)
{
    lv_obj_t *timerPanel = lv_obj_create(screen);
    style_panel(timerPanel);
    lv_obj_set_size(timerPanel, 365, 258);
    lv_obj_align(timerPanel, LV_ALIGN_TOP_LEFT, 18, 54);
    lv_obj_clear_flag(timerPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(timerPanel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(timerPanel, "Stoppuhr");

    timerLabel = lv_label_create(timerPanel);
    lv_label_set_text(timerLabel, "00:00:0");
    lv_obj_set_width(timerLabel, 330);
    lv_label_set_long_mode(timerLabel, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(timerLabel, LV_TEXT_ALIGN_CENTER, 0);
    style_label(timerLabel, COLOR_WHITE);
    lv_obj_set_style_text_font(timerLabel, &lv_font_montserrat_48, 0);
    lv_obj_align(timerLabel, LV_ALIGN_CENTER, 0, -24);
    update_timer_display();

    lv_obj_t *timerInfo = lv_label_create(timerPanel);
    lv_label_set_text(timerInfo, timerRunning ? "läuft" : "bereit");
    lv_obj_set_width(timerInfo, 330);
    lv_obj_set_style_text_align(timerInfo, LV_TEXT_ALIGN_CENTER, 0);
    style_label(timerInfo, timerRunning ? COLOR_GREEN : COLOR_MUTED);
    lv_obj_align(timerInfo, LV_ALIGN_BOTTOM_MID, 0, -22);

    lv_obj_t *inputPanel = lv_obj_create(screen);
    style_plain_block(inputPanel, COLOR_BG);
    lv_obj_set_size(inputPanel, 195, 258);
    lv_obj_align(inputPanel, LV_ALIGN_TOP_RIGHT, -18, 54);

    lv_obj_t *start = create_button(inputPanel, timerRunning ? "Stop" : "Start", "timer_start_stop", 175, 78);
    lv_obj_align(start, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *reset = create_button(inputPanel, "Reset", "timer_reset", 175, 78);
    lv_obj_align(reset, LV_ALIGN_TOP_MID, 0, 90);

    create_footer(screen,
                  "Stoppuhr bereit",
                  "");
}

void create_daten_page(lv_obj_t *screen)
{
    lv_obj_t *panel = lv_obj_create(screen);
    style_panel(panel);
    lv_obj_set_size(panel, 564, 258);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(panel, "Daten");

    struct DataCard {
        const char *title;
        const char *line1;
        const char *action;
        int y;
    };

    const DataCard cards[] = {
        {"Mahldaten", "Shots und Mahlgut", "data_stats", 42},
        {"System", "Uptime, WLAN und Waage", "data_system", 132},
    };

    for (const auto &card : cards) {
        lv_obj_t *btn = lv_btn_create(panel);
        style_button(btn);
        lv_obj_set_size(btn, 526, 78);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, card.y);
        lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, const_cast<char *>(card.action));

        lv_obj_t *title = lv_label_create(btn);
        lv_label_set_text(title, card.title);
        style_label(title, COLOR_GREEN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, -2);

        lv_obj_t *line1 = lv_label_create(btn);
        lv_label_set_text(line1, card.line1);
        lv_obj_set_width(line1, 470);
        lv_label_set_long_mode(line1, LV_LABEL_LONG_DOT);
        style_label(line1, COLOR_WHITE);
        lv_obj_align(line1, LV_ALIGN_TOP_LEFT, 0, 28);

    }

    create_footer(screen,
                  "Daten bereit",
                  "");
}

void create_daten_mahldaten_page(lv_obj_t *screen)
{
    char totalShots[16];
    char machineShots[16];
    char grinderShots[16];
    char filterShots[16];
    char totalGrams[24];
    char machineGrams[24];
    char grinderGrams[24];
    char filterGrams[24];

    snprintf(totalShots, sizeof(totalShots), "%u", demoTotalShots);
    snprintf(machineShots, sizeof(machineShots), "%u", demoMachineShots);
    snprintf(grinderShots, sizeof(grinderShots), "%u", demoGrinderShots);
    snprintf(filterShots, sizeof(filterShots), "%u", demoFilterShots);
    format_grams(totalGrams, sizeof(totalGrams), demoTotalGramsTenths);
    format_grams(machineGrams, sizeof(machineGrams), demoMachineGramsTenths);
    format_grams(grinderGrams, sizeof(grinderGrams), demoGrinderGramsTenths);
    format_grams(filterGrams, sizeof(filterGrams), demoFilterGramsTenths);

    auto addRow = [](lv_obj_t *parent, const char *name, const char *value, int y, lv_obj_t **valueOut) {
        lv_obj_t *nameLabel = lv_label_create(parent);
        lv_label_set_text(nameLabel, name);
        lv_obj_set_width(nameLabel, 168);
        lv_label_set_long_mode(nameLabel, LV_LABEL_LONG_DOT);
        style_label(nameLabel, COLOR_MUTED);
        lv_obj_align(nameLabel, LV_ALIGN_TOP_LEFT, 0, y);

        lv_obj_t *valueLabel = lv_label_create(parent);
        lv_label_set_text(valueLabel, value);
        lv_obj_set_width(valueLabel, 74);
        lv_obj_set_style_text_align(valueLabel, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_long_mode(valueLabel, LV_LABEL_LONG_DOT);
        style_label(valueLabel, COLOR_WHITE);
        lv_obj_align(valueLabel, LV_ALIGN_TOP_RIGHT, 0, y);
        if (valueOut) {
            *valueOut = valueLabel;
        }
    };

    auto addSection = [](lv_obj_t *parent, const char *text, int y) {
        lv_obj_t *label = lv_label_create(parent);
        lv_label_set_text(label, text);
        lv_obj_set_width(label, 236);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        style_label(label, COLOR_MUTED);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, y);
    };

    lv_obj_t *panel = lv_obj_create(screen);
    style_panel(panel);
    lv_obj_set_size(panel, 564, 258);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(panel, "Mahldaten");
    lv_obj_t *back = create_button(panel, "Zurück", "data_back", 112, 40);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, 0, -4);

    lv_obj_t *shotsPanel = lv_obj_create(panel);
    style_panel(shotsPanel);
    lv_obj_set_size(shotsPanel, 260, 174);
    lv_obj_align(shotsPanel, LV_ALIGN_TOP_LEFT, 0, 58);
    lv_obj_clear_flag(shotsPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(shotsPanel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(shotsPanel, "Shots");
    addRow(shotsPanel, "Gesamt", totalShots, 30, &totalShotsLabel);
    addSection(shotsPanel, "seit Reinigung / Wechsel", 58);
    addRow(shotsPanel, "Kaffeemaschine", machineShots, 84, &shotsMachineLabel);
    addRow(shotsPanel, "Kaffeemühle", grinderShots, 110, &shotsGrinderLabel);
    addRow(shotsPanel, "Filter", filterShots, 136, &shotsFilterLabel);

    lv_obj_t *gramsPanel = lv_obj_create(panel);
    style_panel(gramsPanel);
    lv_obj_set_size(gramsPanel, 260, 174);
    lv_obj_align(gramsPanel, LV_ALIGN_TOP_RIGHT, 0, 58);
    lv_obj_clear_flag(gramsPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(gramsPanel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(gramsPanel, "Mahlgut");
    addRow(gramsPanel, "Gesamt", totalGrams, 30, &totalGramsLabel);
    addSection(gramsPanel, "seit Reinigung / Wechsel", 58);
    addRow(gramsPanel, "Kaffeemaschine", machineGrams, 84, &gramsMachineLabel);
    addRow(gramsPanel, "Kaffeemühle", grinderGrams, 110, &gramsGrinderLabel);
    addRow(gramsPanel, "Filter", filterGrams, 136, &gramsFilterLabel);

    create_footer(screen,
                  "Mahldaten bereit",
                  "");
}

void create_daten_system_page(lv_obj_t *screen)
{
    lv_obj_t *panel = lv_obj_create(screen);
    style_panel(panel);
    lv_obj_set_size(panel, 564, 258);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(panel, "Systemdaten");
    lv_obj_t *back = create_button(panel, "Zurück", "data_back", 112, 40);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, 0, -4);

    T4S3WifiStatus wifi;
    t4s3_wifi_get_status(wifi);

    auto addLabelPair = [](lv_obj_t *parent, const char *name, int x, int y, lv_obj_t **valueOut) {
        lv_obj_t *nameLabel = lv_label_create(parent);
        lv_label_set_text(nameLabel, name);
        lv_obj_set_width(nameLabel, 110);
        lv_label_set_long_mode(nameLabel, LV_LABEL_LONG_DOT);
        style_label(nameLabel, COLOR_MUTED);
        lv_obj_align(nameLabel, LV_ALIGN_TOP_LEFT, x, y);

        lv_obj_t *valueLabel = lv_label_create(parent);
        lv_obj_set_width(valueLabel, 330);
        lv_label_set_long_mode(valueLabel, LV_LABEL_LONG_DOT);
        style_label(valueLabel, COLOR_WHITE);
        lv_obj_align(valueLabel, LV_ALIGN_TOP_LEFT, x + 120, y);
        if (valueOut) {
            *valueOut = valueLabel;
        }
    };

    addLabelPair(panel, "Uptime:", 0, 58, &systemUptimeLabel);
    addLabelPair(panel, "WLAN:", 0, 88, &systemDataSsidLabel);
    addLabelPair(panel, "IP:", 0, 118, &systemDataIpLabel);
    addLabelPair(panel, "Signal:", 0, 148, &systemDataSignalLabel);
    addLabelPair(panel, "HX711:", 0, 178, &systemHx711GramsLabel);

    systemHx711RawLabel = nullptr;
    set_text(systemHx711GramsLabel, "-");

    update_system_uptime_display();
    update_data_system_wifi_display();

    create_footer(screen,
                  "Systemdaten bereit",
                  "");
}

void create_maintenance_row(lv_obj_t *parent,
                            const char *title,
                            const char *direction,
                            const char *timeText,
                            const char *action,
                            int y)
{
    char leftText[80];
    snprintf(leftText, sizeof(leftText), "%s: %s", title, direction);

    lv_obj_t *leftLabel = lv_label_create(parent);
    lv_label_set_text(leftLabel, leftText);
    lv_obj_set_width(leftLabel, 238);
    lv_label_set_long_mode(leftLabel, LV_LABEL_LONG_DOT);
    style_label(leftLabel, COLOR_WHITE);
    lv_obj_align(leftLabel, LV_ALIGN_TOP_LEFT, 0, y + 10);

    lv_obj_t *timeLabel = lv_label_create(parent);
    lv_label_set_text(timeLabel, timeText);
    lv_obj_set_width(timeLabel, 206);
    lv_label_set_long_mode(timeLabel, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(timeLabel, LV_TEXT_ALIGN_RIGHT, 0);
    style_label(timeLabel, COLOR_MUTED);
    lv_obj_align(timeLabel, LV_ALIGN_TOP_RIGHT, -120, y + 10);

    
    if (strcmp(action, "maintenance_reset_machine") == 0) {
        maintenanceMachineLeftLabel = leftLabel;
        maintenanceMachineTimeLabel = timeLabel;
    } else if (strcmp(action, "maintenance_reset_grinder") == 0) {
        maintenanceGrinderLeftLabel = leftLabel;
        maintenanceGrinderTimeLabel = timeLabel;
    } else if (strcmp(action, "maintenance_reset_filter") == 0) {
        maintenanceFilterLeftLabel = leftLabel;
        maintenanceFilterTimeLabel = timeLabel;
    }lv_obj_t *button = create_button(parent, "Reset", action, 112, 42);
    lv_obj_align(button, LV_ALIGN_TOP_RIGHT, 0, y);

    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, 444, 1);
    lv_obj_set_style_bg_color(line, lv_color_hex(COLOR_DIM), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_50, 0);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, y + 49);
}



void create_wlan_info_row(lv_obj_t *parent,
                          const char *label,
                          const char *value,
                          int y,
                          lv_obj_t **valueOut = nullptr)
{
    lv_obj_t *labelObj = lv_label_create(parent);
    lv_label_set_text(labelObj, label);
    lv_obj_set_width(labelObj, 112);
    lv_label_set_long_mode(labelObj, LV_LABEL_LONG_DOT);
    style_label(labelObj, COLOR_MUTED);
    lv_obj_align(labelObj, LV_ALIGN_TOP_LEFT, 0, y);

    lv_obj_t *valueObj = lv_label_create(parent);
    lv_label_set_text(valueObj, value);
    lv_obj_set_width(valueObj, 330);
    lv_label_set_long_mode(valueObj, LV_LABEL_LONG_DOT);
    style_label(valueObj, COLOR_WHITE);
    lv_obj_align(valueObj, LV_ALIGN_TOP_LEFT, 122, y);
    if (valueOut) {
        *valueOut = valueObj;
    }
}

void create_settings_wlan_page(lv_obj_t *screen)
{
    lv_obj_t *panel = lv_obj_create(screen);
    style_panel(panel);
    lv_obj_set_size(panel, 564, 258);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(panel, "WLAN / Netzwerk");

    lv_obj_t *back = create_button(panel, "Zurück", "settings_back", 112, 40);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, 0, -4);

    wlanStatusLabel = lv_label_create(panel);
    lv_label_set_text(wlanStatusLabel, "verbinden ...");
    style_label(wlanStatusLabel, COLOR_MUTED);
    lv_obj_align(wlanStatusLabel, LV_ALIGN_TOP_LEFT, 0, 34);

    create_wifi_quality_icon(panel, 156, 29, 0, wlanQualityBars);

    create_wlan_info_row(panel, "SSID:", "-", 72, &wlanSsidLabel);
    create_wlan_info_row(panel, "IP:", "-", 104, &wlanIpLabel);
    create_wlan_info_row(panel, "Signal:", "-", 136, &wlanSignalLabel);
    create_wlan_info_row(panel, "Setup-AP:", "Waagen-Setup", 168, &wlanSetupApLabel);
    create_wlan_info_row(panel, "WebUI:", "192.168.4.1", 200, &wlanWebUiLabel);

    lv_obj_t *setupBtn = create_button(panel, "Setup-AP starten", "wlan_start_setup", 150, 52);
    lv_obj_align(setupBtn, LV_ALIGN_TOP_RIGHT, 0, 86);

    update_wlan_page_display();

    create_footer(screen,
                  "WLAN bereit",
                  "");
}

void create_settings_totals_page(lv_obj_t *screen)
{
    lv_obj_t *panel = lv_obj_create(screen);
    style_panel(panel);
    lv_obj_set_size(panel, 564, 258);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(panel, "Gesamtwerte");

    lv_obj_t *back = create_button(panel, "Zurück", "settings_waage", 112, 40);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, 0, -4);

    struct TotalsCard {
        const char *title;
        const char *line1;
        const char *action;
        int y;
    };

    const TotalsCard cards[] = {
        {"Shots korrigieren", "Gesamt / Maschine / Mühle / Filter", "totals_shots_open", 48},
        {"Mahlgut korrigieren", "Gesamt / Maschine / Mühle / Filter", "totals_grams_open", 142},
    };

    for (const auto &card : cards) {
        lv_obj_t *btn = lv_btn_create(panel);
        style_button(btn);
        lv_obj_set_size(btn, 526, 78);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, card.y);
        lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, const_cast<char *>(card.action));

        lv_obj_t *title = lv_label_create(btn);
        lv_label_set_text(title, card.title);
        style_label(title, COLOR_GREEN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t *line1 = lv_label_create(btn);
        lv_label_set_text(line1, card.line1);
        style_label(line1, COLOR_WHITE);
        lv_obj_align(line1, LV_ALIGN_TOP_LEFT, 0, 36);
    }

    create_footer(screen,
                  "Gesamtwerte bereit",
                  "");
}

void create_settings_waage_page(lv_obj_t *screen)
{
    lv_obj_t *panel = lv_obj_create(screen);
    style_panel(panel);
    lv_obj_set_size(panel, 564, 258);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(panel, "Waage / Gefäße");

    lv_obj_t *back = create_button(panel, "Zurück", "settings_back", 112, 40);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, 0, -4);

    struct ScaleCard {
        const char *title;
        const char *line1;
        const char *line2;
        const char *action;
        int x;
        int y;
    };

    const ScaleCard cards[] = {
        {"Kalibrierung", "bekanntes Gewicht", "Waage abgleichen", "scale_calibration", 0, 42},
        {"Gefäße verwalten", "anzeigen / löschen", "0/3 eingemessen", "vessels_manage", 274, 42},
        {"Gesamtwerte", "Shots und Mahlgut", "korrigieren", "totals_edit", 0, 142},
    };

    for (const auto &card : cards) {
        lv_obj_t *btn = lv_btn_create(panel);
        style_button(btn);
        lv_obj_set_size(btn, 260, 88);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, card.x, card.y);
        lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, const_cast<char *>(card.action));

        lv_obj_t *title = lv_label_create(btn);
        lv_label_set_text(title, card.title);
        style_label(title, COLOR_GREEN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, -2);

        lv_obj_t *line1 = lv_label_create(btn);
        lv_label_set_text(line1, card.line1);
        lv_obj_set_width(line1, 220);
        lv_label_set_long_mode(line1, LV_LABEL_LONG_DOT);
        style_label(line1, COLOR_WHITE);
        lv_obj_align(line1, LV_ALIGN_TOP_LEFT, 0, 30);

        lv_obj_t *line2 = lv_label_create(btn);
        lv_label_set_text(line2, card.line2);
        lv_obj_set_width(line2, 220);
        lv_label_set_long_mode(line2, LV_LABEL_LONG_DOT);
        style_label(line2, COLOR_MUTED);
        lv_obj_align(line2, LV_ALIGN_TOP_LEFT, 0, 54);

        if (strcmp(card.action, "vessels_manage") == 0) {
            gefaessStoredLabel = line2;
        }
    }

    update_gefaess_storage_display();

    create_footer(screen,
                  "Waage / Gefäße bereit",
                  "");
}

void create_settings_wartung_page(lv_obj_t *screen)
{
    lv_obj_t *panel = lv_obj_create(screen);
    style_panel(panel);
    lv_obj_set_size(panel, 564, 258);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(panel, "Wartung");

    lv_obj_t *back = create_button(panel, "Zurück", "settings_back", 112, 40);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, 0, -4);

    ensure_maintenance_epochs_initialized();

    lv_obj_t *hint = lv_label_create(panel);
    lv_label_set_text_fmt(hint, "Intervalle: Kaffeemaschine %lu Tage, Mühle %lu Tage, Filter %lu Tage",
                      static_cast<unsigned long>(maintenanceMachineIntervalSec / 86400UL),
                      static_cast<unsigned long>(maintenanceGrinderIntervalSec / 86400UL),
                      static_cast<unsigned long>(maintenanceFilterIntervalSec / 86400UL));
    lv_obj_set_width(hint, 410);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_DOT);
    style_label(hint, COLOR_MUTED);
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 0, 32);

    char dirMachine[8];
    char dirGrinder[8];
    char dirFilter[8];
    char timeMachine[32];
    char timeGrinder[32];
    char timeFilter[32];

    if (maintenanceMachineEnabled) {
        maintenance_status(maintenanceMachineEpoch, maintenanceMachineIntervalSec,
                           dirMachine, sizeof(dirMachine), timeMachine, sizeof(timeMachine));
    } else {
        strlcpy(dirMachine, "inaktiv", sizeof(dirMachine));
        strlcpy(timeMachine, "keine Warnung", sizeof(timeMachine));
    }
    if (maintenanceGrinderEnabled) {
        maintenance_status(maintenanceGrinderEpoch, maintenanceGrinderIntervalSec,
                           dirGrinder, sizeof(dirGrinder), timeGrinder, sizeof(timeGrinder));
    } else {
        strlcpy(dirGrinder, "inaktiv", sizeof(dirGrinder));
        strlcpy(timeGrinder, "keine Warnung", sizeof(timeGrinder));
    }
    if (maintenanceFilterEnabled) {
        maintenance_status(maintenanceFilterEpoch, maintenanceFilterIntervalSec,
                           dirFilter, sizeof(dirFilter), timeFilter, sizeof(timeFilter));
    } else {
        strlcpy(dirFilter, "inaktiv", sizeof(dirFilter));
        strlcpy(timeFilter, "keine Warnung", sizeof(timeFilter));
    }

    create_maintenance_row(panel, "Kaffeemaschine", dirMachine, timeMachine, "maintenance_reset_machine", 72);
    create_maintenance_row(panel, "Kaffeemühle", dirGrinder, timeGrinder, "maintenance_reset_grinder", 128);
    create_maintenance_row(panel, "Filter", dirFilter, timeFilter, "maintenance_reset_filter", 184);

    
    update_maintenance_display();
    create_footer(screen,
                  "Wartung bereit",
                  "");
}
void create_settings_system_page(lv_obj_t *screen)
{
    lv_obj_t *panel = lv_obj_create(screen);
    style_panel(panel);
    lv_obj_set_size(panel, 564, 258);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(panel, "System");

    lv_obj_t *back = create_button(panel, "Zurück", "settings_back", 112, 40);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, 0, -4);

    lv_obj_t *timeTitle = lv_label_create(panel);
    lv_label_set_text(timeTitle, "Zeit / NTP:");
    lv_obj_set_width(timeTitle, 130);
    lv_label_set_long_mode(timeTitle, LV_LABEL_LONG_DOT);
    style_label(timeTitle, COLOR_MUTED);
    lv_obj_align(timeTitle, LV_ALIGN_TOP_LEFT, 0, 52);

    systemTimeStatusLabel = lv_label_create(panel);
    lv_label_set_text(systemTimeStatusLabel, "warte auf NTP");
    lv_obj_set_width(systemTimeStatusLabel, 190);
    lv_label_set_long_mode(systemTimeStatusLabel, LV_LABEL_LONG_DOT);
    style_label(systemTimeStatusLabel, COLOR_WHITE);
    lv_obj_align(systemTimeStatusLabel, LV_ALIGN_TOP_LEFT, 138, 52);

    lv_obj_t *timeoutTitle = lv_label_create(panel);
    lv_label_set_text(timeoutTitle, "Bildschirmtimeout");
    style_label(timeoutTitle, COLOR_MUTED);
    lv_obj_align(timeoutTitle, LV_ALIGN_TOP_LEFT, 0, 112);

    lv_obj_t *minus = create_button(panel, "-", "timeout_minus", 54, 50);
    lv_obj_align(minus, LV_ALIGN_TOP_LEFT, 0, 146);

    lv_obj_t *timeoutBox = lv_obj_create(panel);
    style_panel(timeoutBox);
    lv_obj_set_size(timeoutBox, 128, 50);
    lv_obj_align(timeoutBox, LV_ALIGN_TOP_LEFT, 66, 146);
    lv_obj_clear_flag(timeoutBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(timeoutBox, LV_SCROLLBAR_MODE_OFF);

    screenTimeoutValueLabel = lv_label_create(timeoutBox);
    lv_obj_set_width(screenTimeoutValueLabel, 108);
    lv_obj_set_style_text_align(screenTimeoutValueLabel, LV_TEXT_ALIGN_CENTER, 0);
    style_label(screenTimeoutValueLabel, COLOR_WHITE);
    lv_obj_center(screenTimeoutValueLabel);
    update_screen_timeout_display();

    lv_obj_t *plus = create_button(panel, "+", "timeout_plus", 54, 50);
    lv_obj_align(plus, LV_ALIGN_TOP_LEFT, 206, 146);

    lv_obj_t *restart = create_button(panel, "Neustart", "system_restart", 176, 50);
    lv_obj_align(restart, LV_ALIGN_TOP_RIGHT, 0, 146);

    update_system_time_display();

    create_footer(screen,
                  "System bereit",
                  "");
}
void create_settings_page(lv_obj_t *screen)
{
    lv_obj_t *panel = lv_obj_create(screen);
    style_panel(panel);
    lv_obj_set_size(panel, 564, 258);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(panel, "Settings");

    struct SettingsCard {
        const char *title;
        const char *line1;
        const char *line2;
        const char *action;
        int x;
        int y;
    };

    const SettingsCard cards[] = {
        {"Wartung", "Reinigung / Filterwechsel", "Zeit bis oder seit Wartung", "settings_wartung", 0, 36},
        {"Waage / Gefäße", "Kalibrieren, einmessen", "Gesamtwerte korrigieren", "settings_waage", 274, 36},
        {"WLAN", "SSID, IP, Signal", "später Setup-WLAN", "settings_wlan", 0, 138},
        {"System", "Timeout, Neustart", "Logs / Diagnose", "settings_system", 274, 138},
    };

    for (const auto &card : cards) {
        lv_obj_t *btn = lv_btn_create(panel);
        style_button(btn);
        lv_obj_set_size(btn, 260, 88);
        lv_obj_align(btn, LV_ALIGN_TOP_LEFT, card.x, card.y);
        lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, const_cast<char *>(card.action));

        lv_obj_t *title = lv_label_create(btn);
        lv_label_set_text(title, card.title);
        style_label(title, COLOR_GREEN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, -2);

        lv_obj_t *line1 = lv_label_create(btn);
        lv_label_set_text(line1, card.line1);
        lv_obj_set_width(line1, 220);
        lv_label_set_long_mode(line1, LV_LABEL_LONG_DOT);
        style_label(line1, COLOR_WHITE);
        lv_obj_align(line1, LV_ALIGN_TOP_LEFT, 0, 30);

        lv_obj_t *line2 = lv_label_create(btn);
        lv_label_set_text(line2, card.line2);
        lv_obj_set_width(line2, 220);
        lv_label_set_long_mode(line2, LV_LABEL_LONG_DOT);
        style_label(line2, COLOR_MUTED);
        lv_obj_align(line2, LV_ALIGN_TOP_LEFT, 0, 54);
    }

    create_footer(screen,
                  "Settings bereit",
                  "");
}

void build_current_page()
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    reset_dynamic_labels();
    style_screen(screen);
    create_header(screen);

    switch (activePage) {
    case Page::Waage:
        create_waage_page(screen);
        break;
    case Page::Stoppuhr:
        create_stoppuhr_page(screen);
        break;
    case Page::Daten:
        create_daten_page(screen);
        break;
    case Page::DatenMahldaten:
        create_daten_mahldaten_page(screen);
        break;
    case Page::DatenSystem:
        create_daten_system_page(screen);
        break;
    case Page::Settings:
        create_settings_page(screen);
        break;
    case Page::SettingsWartung:
        create_settings_wartung_page(screen);
        break;
    case Page::SettingsWaage:
        create_settings_waage_page(screen);
        break;
    case Page::SettingsTotals:
        create_settings_totals_page(screen);
        break;
    case Page::SettingsWlan:
        create_settings_wlan_page(screen);
        break;
    case Page::SettingsSystem:
        create_settings_system_page(screen);
        break;
    }

    create_nav(screen);
}

}  // namespace

uint16_t ui_t4s3_get_screen_timeout_minutes()
{
    return current_screen_timeout_minutes();
}

uint32_t ui_t4s3_get_last_activity_ms()
{
    return lastUserActivityMs;
}

void ui_t4s3_notify_activity()
{
    lastUserActivityMs = millis();
}

void ui_t4s3_prepare_wakeup_touch()
{
    // Wake-touch suppression is handled by the fullscreen sleep overlay
    // in t4s3_main.cpp. Keep this hook harmless.
    suppressNextClick = false;
    lastUserActivityMs = millis();
}

void ui_t4s3_set_hx711_raw_value(int32_t rawValue)
{
    update_hx711_raw_display(rawValue);
}

void ui_t4s3_set_hx711_grams_value(float grams, bool valid)
{
    update_hx711_grams_display(grams, valid);
}

bool ui_t4s3_web_save_ready()
{
    return saveReady;
}

uint32_t ui_t4s3_web_stopwatch_ms()
{
    uint32_t elapsed = timerBaseMs;
    if (timerRunning) {
        elapsed += millis() - timerStartedMs;
    }
    return elapsed;
}

bool ui_t4s3_web_stopwatch_running()
{
    return timerRunning;
}


float ui_t4s3_web_calibration_weight_g()
{
    return static_cast<float>(scaleCalibrationTargetTenths) / 10.0f;
}

const char *ui_t4s3_web_siebtraeger_name(uint8_t idx)
{
    return vessel_name(idx);
}

uint8_t ui_t4s3_web_selected_gefaess()
{
    return webSelectedGefaessSlot;
}

bool ui_t4s3_web_wizard_active()
{
    return webWizardActive;
}

static bool ui_t4s3_web_tare()
{
    if (!t4s3_scale_is_ready()) {
        simTenths = 0;
        manualTareSaveArmed = false;
        set_text(weightLabel, "0,0 g");
        update_status("Tara gedrückt - Demo-Gewicht auf 0,0 g gesetzt");
        return true;
    }

    if (!t4s3_scale_tare()) {
        update_status("Tara fehlgeschlagen - HX711 nicht bereit");
        return false;
    }

    hx711DisplayGrams = 0.0f;
    hx711DisplayValid = true;
    set_text(weightLabel, "0,0 g");
    set_text_if_changed(systemHx711GramsLabel, "0,0 g");
    set_text_if_changed(scaleCalibrationWeightLabel, "0,0 g");
    set_text_if_changed(simLabel, "HX711-Gewicht aktiv");
    manualTareSaveArmed = !autodetectEnabled;
    update_status(autodetectEnabled ? "Tara gesetzt" : "Tara gesetzt - Save wird vorbereitet");
    set_save_ready(false);
    return true;
}

static bool ui_t4s3_web_save_dose()
{
    if (!saveReady) {
        update_status("Save noch nicht bereit");
        return false;
    }

    const int32_t saveTenths = current_save_weight_tenths();
    if (saveTenths <= 0) {
        update_status("Save ignoriert - Gewicht ist 0,0 g");
        return false;
    }

    add_saved_dose(saveTenths);

    char msg[96];
    char gramsText[24];
    format_grams(gramsText, sizeof(gramsText), saveTenths);
    snprintf(msg, sizeof(msg), "Bezug %s gespeichert", gramsText);
    update_status(msg);

    manualTareSaveArmed = false;
    set_save_ready(false);
    save_current_ui_settings();
    return true;
}

static bool ui_t4s3_web_set_autodetect(bool enabled)
{
    autodetectEnabled = enabled;
    autodetectDetectedGefaess = kNoDetectedGefaess;
    autodetectPendingGefaess = kNoDetectedGefaess;
    autodetectAutoTaredGefaess = kNoDetectedGefaess;
    autodetectPendingSinceMs = 0;
    manualTareSaveArmed = false;
    set_save_ready(false);
    update_autodetect_display();
    update_status(autodetectEnabled ? "Autodetect eingeschaltet" : "Autodetect ausgeschaltet");
    save_current_ui_settings();
    return true;
}

static bool ui_t4s3_web_select_siebtraeger(uint8_t idx)
{
    if (idx >= kMaxSiebtraegerSlots) {
        return false;
    }

    currentVesselIndex = idx;
    draftVesselIndex = idx;
    demoTargetTenths = targetTenthsBySiebtraeger[currentVesselIndex];
    draftTargetTenths = demoTargetTenths;
    update_vessel_display();
    update_target_display();
    save_current_ui_settings();

    char msg[64];
    snprintf(msg, sizeof(msg), "Siebträger %u ausgewählt", static_cast<unsigned>(idx + 1));
    update_status(msg);
    return true;
}

static bool ui_t4s3_web_set_selected_weight(float grams)
{
    if (!isfinite(grams) || grams <= 0.0f || grams > 60.0f) {
        return false;
    }

    const int32_t tenths = static_cast<int32_t>(grams * 10.0f + 0.5f);
    demoTargetTenths = tenths;
    draftTargetTenths = tenths;
    targetTenthsBySiebtraeger[currentVesselIndex] = tenths;
    save_current_ui_settings();
    update_target_display();

    char gramsText[24];
    char msg[80];
    format_grams(gramsText, sizeof(gramsText), tenths);
    snprintf(msg, sizeof(msg), "Sollgewicht gespeichert: %s", gramsText);
    update_status(msg);
    return true;
}


static bool hex_to_nibble(char c, uint8_t &value)
{
    if (c >= '0' && c <= '9') {
        value = static_cast<uint8_t>(c - '0');
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        value = static_cast<uint8_t>(c - 'a' + 10);
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        value = static_cast<uint8_t>(c - 'A' + 10);
        return true;
    }
    return false;
}

static void decode_web_component(char *dst, size_t dstLen, const char *src)
{
    if (!dst || dstLen == 0) {
        return;
    }
    dst[0] = '\0';
    if (!src) {
        return;
    }

    size_t out = 0;
    for (size_t i = 0; src[i] != '\0' && out + 1 < dstLen; ++i) {
        if (src[i] == '%' && src[i + 1] != '\0' && src[i + 2] != '\0') {
            uint8_t hi = 0;
            uint8_t lo = 0;
            if (hex_to_nibble(src[i + 1], hi) && hex_to_nibble(src[i + 2], lo)) {
                dst[out++] = static_cast<char>((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        dst[out++] = src[i] == '+' ? ' ' : src[i];
    }
    dst[out] = '\0';
}

static void sanitize_web_name(char *name, size_t len, const char *fallback)
{
    if (!name || len == 0) {
        return;
    }

    size_t read = 0;
    size_t write = 0;
    bool lastSpace = true;
    while (name[read] != '\0' && write + 1 < len) {
        char c = name[read++];
        if (c == '\r' || c == '\n' || c == '\t') {
            c = ' ';
        }
        if (c == ' ') {
            if (lastSpace) {
                continue;
            }
            lastSpace = true;
        } else {
            lastSpace = false;
        }
        name[write++] = c;
    }
    while (write > 0 && name[write - 1] == ' ') {
        --write;
    }
    name[write] = '\0';

    if (write == 0) {
        strlcpy(name, fallback ? fallback : "Siebtraeger", len);
    }
}

static bool ui_t4s3_web_set_siebtraeger_name(uint8_t idx, const char *encodedName)
{
    if (idx >= T4S3_SIEBTRAEGER_SLOT_COUNT || !encodedName) {
        return false;
    }

    char decoded[T4S3_SIEBTRAEGER_NAME_LEN];
    decode_web_component(decoded, sizeof(decoded), encodedName);
    sanitize_web_name(decoded, sizeof(decoded), vessel_name(idx));

    strlcpy(siebtraegerNames[idx], decoded, sizeof(siebtraegerNames[idx]));
    save_current_ui_settings();
    update_vessel_display();
    update_vessel_overlay_display();

    char msg[96];
    snprintf(msg, sizeof(msg), "Siebträger %u benannt: %s", static_cast<unsigned>(idx + 1), siebtraegerNames[idx]);
    update_status(msg);
    return true;
}

static bool ui_t4s3_web_set_totals(uint32_t shots, int32_t gramsTenths)
{
    if (gramsTenths < 0 || shots > 65535UL || gramsTenths > 20000000L) {
        update_status("Gesamtwerte ungültig");
        return false;
    }

    demoTotalShots = static_cast<uint16_t>(shots);
    demoTotalGramsTenths = gramsTenths;
    save_current_ui_settings();
    update_demo_stats_display();

    char gramsText[24];
    char msg[96];
    format_grams(gramsText, sizeof(gramsText), demoTotalGramsTenths);
    snprintf(msg, sizeof(msg), "Gesamtwerte gesetzt: %u Shots, %s", demoTotalShots, gramsText);
    update_status(msg);
    return true;
}

static bool ui_t4s3_web_set_maintenance_enabled(const char *item, bool enabled)
{
    if (!item) {
        return false;
    }

    const char *label = nullptr;
    if (strcmp(item, "machine") == 0) {
        maintenanceMachineEnabled = enabled;
        label = "Kaffeemaschine";
    } else if (strcmp(item, "grinder") == 0) {
        maintenanceGrinderEnabled = enabled;
        label = "Mühle";
    } else if (strcmp(item, "filter") == 0) {
        maintenanceFilterEnabled = enabled;
        label = "Filter";
    } else {
        return false;
    }

    save_current_ui_settings();
    update_maintenance_due_state();
    update_maintenance_display();

    char msg[96];
    snprintf(msg, sizeof(msg), "Wartung %s: %s", label, enabled ? "aktiv" : "inaktiv");
    update_status(msg);
    return true;
}

static bool ui_t4s3_web_set_maintenance_interval(const char *item, uint32_t seconds)
{
    if (!item || seconds > MAX_MAINTENANCE_INTERVAL_SEC) {
        update_status("Wartungsintervall ungültig");
        return false;
    }

    const bool isMachine = strcmp(item, "machine") == 0;
    const bool isGrinder = strcmp(item, "grinder") == 0;
    const bool isFilter = strcmp(item, "filter") == 0;
    if (!isMachine && !isGrinder && !isFilter) {
        return false;
    }

    const uint32_t minSeconds = isMachine ? 60UL : 86400UL;
    if (seconds < minSeconds) {
        update_status("Wartungsintervall ungültig");
        return false;
    }

    if (isMachine) {
        maintenanceMachineIntervalSec = seconds;
    } else if (isGrinder) {
        maintenanceGrinderIntervalSec = seconds;
    } else if (isFilter) {
        maintenanceFilterIntervalSec = seconds;
    }
    save_current_ui_settings();
    update_maintenance_due_state();
    update_maintenance_display();

    char msg[96];
    if (seconds < 86400UL) {
        snprintf(msg, sizeof(msg), "Wartungsintervall gespeichert: %lu Minuten", static_cast<unsigned long>(seconds / 60UL));
    } else {
        snprintf(msg, sizeof(msg), "Wartungsintervall gespeichert: %lu Tage", static_cast<unsigned long>(seconds / 86400UL));
    }
    update_status(msg);
    return true;
}

static bool ui_t4s3_web_reset_maintenance(const char *cmd)
{
    if (!cmd) {
        return false;
    }

    const bool isMaintenanceReset =
        strcmp(cmd, "maintenance_reset_machine") == 0 ||
        strcmp(cmd, "maintenance_reset_grinder") == 0 ||
        strcmp(cmd, "maintenance_reset_filter") == 0;

    if (!isMaintenanceReset) {
        return false;
    }

    if (t4s3_time_now_epoch() == 0) {
        update_status("Wartung kann erst nach NTP-Sync zurückgesetzt werden");
        return false;
    }

    perform_maintenance_reset(cmd);
    return true;
}

static bool ui_t4s3_web_delete_gefaess(uint8_t idx)
{
    if (idx >= T4S3_GEFAESS_SLOT_COUNT) {
        return false;
    }

    gefaessWeightTenths[idx] = -1;
    autodetectDetectedGefaess = kNoDetectedGefaess;
    autodetectPendingGefaess = kNoDetectedGefaess;
    autodetectAutoTaredGefaess = kNoDetectedGefaess;
    autodetectPendingSinceMs = 0;
    save_current_ui_settings();
    update_gefaess_storage_display();
    update_gefaess_manage_display();
    update_status("Gefäß gelöscht");
    return true;
}


static bool parse_web_float_suffix(const char *cmd, const char *prefix, float &value)
{
    if (!cmd || !prefix) {
        return false;
    }
    const size_t prefixLen = strlen(prefix);
    if (strncmp(cmd, prefix, prefixLen) != 0) {
        return false;
    }

    char *end = nullptr;
    value = strtof(cmd + prefixLen, &end);
    return end && *end == '\0' && isfinite(value);
}

static bool ui_t4s3_web_wizard_begin()
{
    webWizardActive = true;
    autodetectDetectedGefaess = kNoDetectedGefaess;
    autodetectPendingGefaess = kNoDetectedGefaess;
    autodetectAutoTaredGefaess = kNoDetectedGefaess;
    autodetectPendingSinceMs = 0;
    manualTareSaveArmed = false;
    set_save_ready(false);
    update_autodetect_display();
    update_status("Web-Assistent gestartet - Autodetect pausiert");
    return true;
}

static bool ui_t4s3_web_wizard_end()
{
    webWizardActive = false;
    autodetectDetectedGefaess = kNoDetectedGefaess;
    autodetectPendingGefaess = kNoDetectedGefaess;
    autodetectPendingSinceMs = 0;
    update_autodetect_display();
    update_status("Web-Assistent beendet");
    return true;
}

static bool ui_t4s3_web_set_calibration_weight(float grams)
{
    if (!isfinite(grams) || grams <= 0.0f || grams > 5000.0f) {
        update_status("Kalibriergewicht ungültig");
        return false;
    }

    scaleCalibrationTargetTenths = static_cast<int32_t>(grams * 10.0f + 0.5f);
    update_scale_calibration_display();

    char gramsText[24];
    char msg[80];
    format_grams(gramsText, sizeof(gramsText), scaleCalibrationTargetTenths);
    snprintf(msg, sizeof(msg), "Kalibriergewicht gesetzt: %s", gramsText);
    update_status(msg);
    return true;
}

static bool ui_t4s3_web_apply_calibration(float grams)
{
    if (!ui_t4s3_web_set_calibration_weight(grams)) {
        return false;
    }

    const float calibrationWeight = static_cast<float>(scaleCalibrationTargetTenths) / 10.0f;
    if (!t4s3_scale_calibrate(calibrationWeight)) {
        update_status("Kalibrierung fehlgeschlagen");
        return false;
    }

    char gramsText[24];
    char msg[96];
    format_grams(gramsText, sizeof(gramsText), scaleCalibrationTargetTenths);
    snprintf(msg, sizeof(msg), "Kalibrierung mit %s gespeichert", gramsText);
    update_status(msg);
    return true;
}

static bool ui_t4s3_web_select_gefaess(uint8_t idx)
{
    if (idx >= T4S3_GEFAESS_SLOT_COUNT) {
        return false;
    }

    webSelectedGefaessSlot = idx;
    gefaessMeasureSlot = idx;

    char msg[64];
    snprintf(msg, sizeof(msg), "Gefäß %u ausgewählt", static_cast<unsigned>(idx + 1));
    update_status(msg);
    return true;
}

static bool ui_t4s3_web_save_measured_gefaess()
{
    if (webSelectedGefaessSlot >= T4S3_GEFAESS_SLOT_COUNT) {
        return false;
    }

    const int32_t measuredTenths = current_gefaess_measure_tenths();
    if (measuredTenths <= 0) {
        update_status("Gefäßgewicht ungültig");
        return false;
    }

    gefaessWeightTenths[webSelectedGefaessSlot] = measuredTenths;
    autodetectDetectedGefaess = kNoDetectedGefaess;
    autodetectPendingGefaess = kNoDetectedGefaess;
    autodetectAutoTaredGefaess = kNoDetectedGefaess;
    autodetectPendingSinceMs = 0;
    save_current_ui_settings();
    update_gefaess_storage_display();
    update_gefaess_manage_display();

    char gramsText[24];
    char msg[80];
    format_grams(gramsText, sizeof(gramsText), measuredTenths);
    snprintf(msg, sizeof(msg), "Gefäß %u gespeichert: %s", static_cast<unsigned>(webSelectedGefaessSlot + 1), gramsText);
    update_status(msg);
    return true;
}

bool ui_t4s3_handle_web_command(const char *cmd)
{
    if (!cmd || cmd[0] == '\0') {
        return false;
    }

    ui_t4s3_notify_activity();

    if (strcmp(cmd, "web_wizard_begin") == 0) {
        return ui_t4s3_web_wizard_begin();
    }
    if (strcmp(cmd, "web_wizard_end") == 0) {
        return ui_t4s3_web_wizard_end();
    }
    if (strcmp(cmd, "web_wizard_tare") == 0) {
        return ui_t4s3_web_tare();
    }
    if (strcmp(cmd, "tare") == 0) {
        return ui_t4s3_web_tare();
    }
    if (strcmp(cmd, "save_dose") == 0) {
        return ui_t4s3_web_save_dose();
    }
    if (strcmp(cmd, "autodetect_on") == 0) {
        return ui_t4s3_web_set_autodetect(true);
    }
    if (strcmp(cmd, "autodetect_off") == 0) {
        return ui_t4s3_web_set_autodetect(false);
    }
    if (strcmp(cmd, "stopwatch_start_stop") == 0) {
        set_timer_running(!timerRunning);
        return true;
    }
    if (strcmp(cmd, "stopwatch_reset") == 0) {
        reset_timer();
        return true;
    }
    if (strncmp(cmd, "select_siebtraeger_", 19) == 0) {
        const char indexChar = cmd[19];
        if (indexChar < '0' || indexChar > '9') {
            return false;
        }
        return ui_t4s3_web_select_siebtraeger(static_cast<uint8_t>(indexChar - '0'));
    }
    if (strncmp(cmd, "select_gefaess_", 15) == 0) {
        const char indexChar = cmd[15];
        if (indexChar < '0' || indexChar > '9') {
            return false;
        }
        return ui_t4s3_web_select_gefaess(static_cast<uint8_t>(indexChar - '0'));
    }
    const char *siebtraegerNamePrefix = "set_siebtraeger_name_";
    const size_t siebtraegerNamePrefixLen = strlen(siebtraegerNamePrefix);
    if (strncmp(cmd, siebtraegerNamePrefix, siebtraegerNamePrefixLen) == 0) {
        const char *cursor = cmd + siebtraegerNamePrefixLen;
        char *end = nullptr;
        const long idx = strtol(cursor, &end, 10);
        if (end == cursor || !end || *end != '_' || idx < 0 || idx >= T4S3_SIEBTRAEGER_SLOT_COUNT) {
            return false;
        }
        return ui_t4s3_web_set_siebtraeger_name(static_cast<uint8_t>(idx), end + 1);
    }

    float parsedGrams = 0.0f;
    if (parse_web_float_suffix(cmd, "set_selected_siebtraeger_weight_", parsedGrams)) {
        return ui_t4s3_web_set_selected_weight(parsedGrams);
    }
    if (parse_web_float_suffix(cmd, "scale_calibration_set_weight_", parsedGrams)) {
        return ui_t4s3_web_set_calibration_weight(parsedGrams);
    }
    if (parse_web_float_suffix(cmd, "scale_calibration_apply_", parsedGrams)) {
        return ui_t4s3_web_apply_calibration(parsedGrams);
    }
    if (strcmp(cmd, "measure_gefaess_save") == 0) {
        return ui_t4s3_web_save_measured_gefaess();
    }
    if (strncmp(cmd, "delete_gefaess_", 15) == 0) {
        const char indexChar = cmd[15];
        if (indexChar < '0' || indexChar > '9') {
            return false;
        }
        return ui_t4s3_web_delete_gefaess(static_cast<uint8_t>(indexChar - '0'));
    }
    if (strncmp(cmd, "set_stats_totals_", 17) == 0) {
        const char *cursor = cmd + 17;
        char *end = nullptr;
        const unsigned long shots = strtoul(cursor, &end, 10);
        if (end == cursor || !end || *end != '_') {
            return false;
        }
        cursor = end + 1;
        const long gramsTenths = strtol(cursor, &end, 10);
        if (end == cursor || !end || *end != '\0') {
            return false;
        }
        return ui_t4s3_web_set_totals(static_cast<uint32_t>(shots), static_cast<int32_t>(gramsTenths));
    }


    if (strncmp(cmd, "set_maintenance_enabled_", 24) == 0) {
        const char *cursor = cmd + 24;
        const char *sep = strchr(cursor, '_');
        if (!sep || sep == cursor) {
            return false;
        }
        char item[12];
        const size_t itemLen = static_cast<size_t>(sep - cursor);
        if (itemLen >= sizeof(item)) {
            return false;
        }
        memcpy(item, cursor, itemLen);
        item[itemLen] = '\0';
        const char *value = sep + 1;
        if (strcmp(value, "1") == 0 || strcmp(value, "on") == 0) {
            return ui_t4s3_web_set_maintenance_enabled(item, true);
        }
        if (strcmp(value, "0") == 0 || strcmp(value, "off") == 0) {
            return ui_t4s3_web_set_maintenance_enabled(item, false);
        }
        return false;
    }

    if (strncmp(cmd, "set_maintenance_interval_", 25) == 0) {
        const char *cursor = cmd + 25;
        const char *sep = strchr(cursor, '_');
        if (!sep || sep == cursor) {
            return false;
        }
        char item[12];
        const size_t itemLen = static_cast<size_t>(sep - cursor);
        if (itemLen >= sizeof(item)) {
            return false;
        }
        memcpy(item, cursor, itemLen);
        item[itemLen] = '\0';
        char *end = nullptr;
        const unsigned long seconds = strtoul(sep + 1, &end, 10);
        if (end == sep + 1 || !end || *end != '\0') {
            return false;
        }
        return ui_t4s3_web_set_maintenance_interval(item, static_cast<uint32_t>(seconds));
    }

    if (ui_t4s3_web_reset_maintenance(cmd)) {
        return true;
    }

    return false;
}

void ui_t4s3_create(uint16_t width, uint16_t height)
{
    screenWidth = width;
    screenHeight = height;
    activePage = Page::Waage;
    load_saved_ui_settings();
    load_maintenance_epochs();
    update_maintenance_due_state();
    lastUserActivityMs = millis();
    build_current_page();
}

void ui_t4s3_tick()
{
    process_pending_maintenance_reset();

    const uint32_t now = millis();

    if (timerRunning && now - lastTimerMs >= 100) {
        lastTimerMs = now;
        update_timer_display();
    }

    static uint32_t lastClockMs = 0;
    if (now - lastClockMs >= 1000) {
        lastClockMs = now;

        update_clock_display();
        update_system_uptime_display();
        update_header_wifi_display();
        update_wlan_page_display();
        process_wlan_setup_overlay_state();

        // Hintergrundstatus immer aktualisieren, nicht nur auf der Wartungsseite.
        // Dadurch kann auf der Waage-Seite sofort eine Warnung erscheinen,
        // sobald eine Wartung faellig wird.
        if (static_cast<int32_t>(now - maintenanceResetCooldownUntilMs) >= 0) {
            update_maintenance_due_state();

            if (maintenanceRefreshPending) {
                maintenanceRefreshPending = false;
                update_maintenance_warning_display();
            }

            if (activePage == Page::SettingsWartung) {
                update_maintenance_display();
            }
        }
    }

    if (now - lastSimMs < 350) {
        return;
    }
    lastSimMs = now;

    if (!hx711DisplayValid) {
        simTenths += simDir;
        if (simTenths >= 42) {
            simDir = -1;
        } else if (simTenths <= 0) {
            simDir = 1;
        }
        update_sim_weight();
    }
    update_gefaess_measure_weight_display();
}
























