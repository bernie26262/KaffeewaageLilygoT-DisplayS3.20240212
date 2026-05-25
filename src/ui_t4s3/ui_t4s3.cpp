#include "ui_t4s3.h"
#include "coffee_ui_umlaut_font.h"

#include "../t4s3_wifi.h"
#include "../t4s3_time.h"
#include "../t4s3_settings.h"

#include <Arduino.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr uint32_t COLOR_BG = 0x000000;
constexpr uint32_t COLOR_PANEL = 0x07120a;
constexpr uint32_t COLOR_GREEN = 0x2E8B57;
constexpr uint32_t COLOR_GREEN_DARK = 0x1F5F3D;
constexpr uint32_t COLOR_WHITE = 0xFFFFFF;
constexpr uint32_t COLOR_MUTED = 0xA8B8A8;
constexpr uint32_t COLOR_DIM = 0x5F705F;

constexpr uint16_t SCREEN_W = 600;
constexpr uint16_t SCREEN_H = 450;

enum class Page : uint8_t {
    Waage,
    Stoppuhr,
    Daten,
    Settings,
    SettingsWartung,
    SettingsWaage,
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
lv_obj_t *restartOverlay = nullptr;
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
lv_obj_t *headerWifiBars[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t *wlanStatusLabel = nullptr;
lv_obj_t *wlanSsidLabel = nullptr;
lv_obj_t *wlanIpLabel = nullptr;
lv_obj_t *wlanSignalLabel = nullptr;
lv_obj_t *wlanSetupApLabel = nullptr;
lv_obj_t *wlanWebUiLabel = nullptr;
lv_obj_t *wlanQualityBars[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t *maintenanceWarningButton = nullptr;


uint32_t lastSimMs = 0;
uint32_t lastTimerMs = 0;
uint32_t timerBaseMs = 0;
uint32_t timerStartedMs = 0;
bool timerRunning = false;
int32_t simTenths = 0;
int8_t simDir = 1;
bool autodetectEnabled = true;
uint16_t demoTotalShots = 0;
uint16_t demoMachineShots = 0;
uint16_t demoGrinderShots = 0;
uint16_t demoFilterShots = 0;
int32_t demoTotalGramsTenths = 0;
int32_t demoMachineGramsTenths = 0;
int32_t demoGrinderGramsTenths = 0;
int32_t demoFilterGramsTenths = 0;
int32_t targetTenthsBySiebtraeger[4] = {180, 90, 180, 180};
int32_t gefaessWeightTenths[4] = {-1, -1, -1, -1};
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
void open_restart_overlay();
void close_restart_overlay();


void open_maintenance_reset_overlay(const char *action);
void close_maintenance_reset_overlay();
void process_pending_maintenance_reset();
void perform_maintenance_reset(const char *action);
lv_obj_t *create_button(lv_obj_t *parent, const char *text, const char *action, int width, int height);
void update_status(const char *msg);
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
    screenTimeoutValueLabel = nullptr;
    systemTimeStatusLabel = nullptr;
    systemTimeLocalLabel = nullptr;
    systemTimeZoneLabel = nullptr;
    systemTimeSourceLabel = nullptr;
    systemUptimeLabel = nullptr;
    systemDataSsidLabel = nullptr;
    systemDataIpLabel = nullptr;
    systemDataSignalLabel = nullptr;
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
    for (uint8_t i = 0; i < 4; ++i) {
        vesselOptionLabels[i] = nullptr;
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
    settings.targetStepTenths = targetStepTenths;

    for (uint8_t i = 0; i < 4; ++i) {
        settings.gefaessWeightTenths[i] = gefaessWeightTenths[i];
    }

    settings.totalShots = demoTotalShots;
    settings.machineShots = demoMachineShots;
    settings.grinderShots = demoGrinderShots;
    settings.filterShots = demoFilterShots;
    settings.totalGramsTenths = demoTotalGramsTenths;
    settings.machineGramsTenths = demoMachineGramsTenths;
    settings.grinderGramsTenths = demoGrinderGramsTenths;
    settings.filterGramsTenths = demoFilterGramsTenths;t4s3_settings_save(settings);
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

    targetStepTenths = settings.targetStepTenths;
    draftTargetStepTenths = targetStepTenths;

    demoTargetTenths = targetTenthsBySiebtraeger[currentVesselIndex];
    draftTargetTenths = demoTargetTenths;

    for (uint8_t i = 0; i < 4; ++i) {
        gefaessWeightTenths[i] = settings.gefaessWeightTenths[i];
    }

    demoTotalShots = settings.totalShots;
    demoMachineShots = settings.machineShots;
    demoGrinderShots = settings.grinderShots;
    demoFilterShots = settings.filterShots;
    demoTotalGramsTenths = settings.totalGramsTenths;
    demoMachineGramsTenths = settings.machineGramsTenths;
    demoGrinderGramsTenths = settings.grinderGramsTenths;
    demoFilterGramsTenths = settings.filterGramsTenths;}
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
    switch (index) {
    case 0: return "Bodenloser ST";
    case 1: return "1er-Siebträger";
    case 2: return "2er-Siebträger";
    case 3: return "Custom ST";
    default: return "Bodenloser ST";
    }
}

void update_autodetect_display()
{
    set_text(autodetectStateLabel, "Auto");
    if (autodetectLed) {
        lv_obj_set_style_bg_color(autodetectLed, lv_color_hex(autodetectEnabled ? COLOR_GREEN : COLOR_DIM), 0);
    }
}

void update_vessel_display()
{
    set_text(vesselLabel, vessel_name(currentVesselIndex));
}

void update_vessel_overlay_display()
{
    for (uint8_t i = 0; i < 4; ++i) {
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


uint8_t stored_gefaess_count()
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < 4; ++i) {
        if (gefaessWeightTenths[i] >= 0) {
            ++count;
        }
    }
    return count;
}

void update_gefaess_storage_display()
{
    char buf[40];
    snprintf(buf, sizeof(buf), "%u/4 eingemessen", stored_gefaess_count());
    set_text(gefaessStoredLabel, buf);
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
constexpr uint32_t MAINTENANCE_MACHINE_INTERVAL_SEC = 30UL;  // Test: 30 Sekunden
constexpr uint32_t MAINTENANCE_GRINDER_INTERVAL_SEC = 30UL;  // Test: 30 Sekunden
constexpr uint32_t MAINTENANCE_FILTER_INTERVAL_SEC  = 30UL;  // Test: 30 Sekunden
#else
constexpr uint32_t MAINTENANCE_MACHINE_INTERVAL_SEC = 864000UL;     // 10 Tage
constexpr uint32_t MAINTENANCE_GRINDER_INTERVAL_SEC = 2419200UL;    // 28 Tage
constexpr uint32_t MAINTENANCE_FILTER_INTERVAL_SEC  = 7257600UL;    // 12 Wochen / 84 Tage
#endif

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
                                    uint32_t intervalSec)
{
    if (!leftLabel || !timeLabel) {
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
                                   MAINTENANCE_MACHINE_INTERVAL_SEC);

    update_maintenance_row_display(maintenanceGrinderLeftLabel,
                                   maintenanceGrinderTimeLabel,
                                   "Kaffeemühle",
                                   maintenanceGrinderEpoch,
                                   MAINTENANCE_GRINDER_INTERVAL_SEC);

    update_maintenance_row_display(maintenanceFilterLeftLabel,
                                   maintenanceFilterTimeLabel,
                                   "Filter",
                                   maintenanceFilterEpoch,
                                   MAINTENANCE_FILTER_INTERVAL_SEC);
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
        maintenance_item_due(maintenanceMachineEpoch, MAINTENANCE_MACHINE_INTERVAL_SEC, now) ||
        maintenance_item_due(maintenanceGrinderEpoch, MAINTENANCE_GRINDER_INTERVAL_SEC, now) ||
        maintenance_item_due(maintenanceFilterEpoch, MAINTENANCE_FILTER_INTERVAL_SEC, now);

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
    } else if (strcmp(action, "maintenance_reset_grinder") == 0) {
        maintenanceGrinderEpoch = nowEpoch;
    } else if (strcmp(action, "maintenance_reset_filter") == 0) {
        maintenanceFilterEpoch = nowEpoch;
    } else {
        return;
    }

    save_maintenance_epochs();

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
        simTenths = 0;
        set_text(weightLabel, "0,0 g");
        update_status("Tara gedrückt - Demo-Gewicht auf 0,0 g gesetzt");
    } else if (strcmp(action, "save") == 0) {
        if (simTenths <= 0) {
            update_status("Save ignoriert - Demo-Gewicht ist 0,0 g");
            return;
        }
        demoTotalShots++;
        demoMachineShots++;
        demoGrinderShots++;
        demoFilterShots++;
        demoTotalGramsTenths += simTenths;
        demoMachineGramsTenths += simTenths;
        demoGrinderGramsTenths += simTenths;
        demoFilterGramsTenths += simTenths;
        update_demo_stats_display();

        char msg[96];
        char grams[24];
        format_grams(grams, sizeof(grams), simTenths);
        snprintf(msg, sizeof(msg), "Save gedrückt - Demo-Bezug %s gespeichert", grams);
        update_status(msg);
        save_current_ui_settings();
    } else if (strcmp(action, "autodetect") == 0) {
        autodetectEnabled = !autodetectEnabled;
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
        update_status("Kalibrierung: Assistent folgt später");
    } else if (strcmp(action, "vessels_measure") == 0) {
        update_status("Gefäße einmessen: Assistent folgt später");
    } else if (strcmp(action, "vessels_manage") == 0) {
        update_status("Gefäße verwalten: Liste / Löschen folgt später");
    } else if (strcmp(action, "totals_edit") == 0) {
        update_status("Gesamtwerte ändern: Eingabe folgt später");
    } else if (strcmp(action, "settings_wlan") == 0) {
        navigate_to(Page::SettingsWlan);
    } else if (strcmp(action, "wlan_start_setup") == 0) {
        if (t4s3_wifi_start_setup_ap()) {
            update_wlan_page_display();
            update_status("Setup-AP aktiv: Waagen-Setup / WebUI 192.168.4.1");
        } else {
            update_status("Setup-AP konnte nicht gestartet werden");
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
    lv_label_set_text(hint, "Nicht gespeicherte Demo-Werte gehen verloren.");
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
    lv_obj_set_size(panel, 430, 330);
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
    if (save) {
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
        vesselOverlay = nullptr;
        for (uint8_t i = 0; i < 4; ++i) {
            vesselOptionLabels[i] = nullptr;
        }
    }
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
    lv_obj_set_size(panel, 430, 295);
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

    for (uint8_t i = 0; i < 4; ++i) {
        lv_obj_t *option = create_button(panel, "", actions[i], 350, 48);
        lv_obj_align(option, LV_ALIGN_TOP_MID, 0, 48 + i * 55);

        vesselOptionLabels[i] = lv_label_create(option);
        lv_obj_set_width(vesselOptionLabels[i], 310);
        lv_obj_set_style_text_align(vesselOptionLabels[i], LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align(vesselOptionLabels[i], LV_ALIGN_LEFT_MID, 10, 0);
    }
    update_vessel_overlay_display();

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
    lv_obj_set_size(btn, 128, 46);
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
    char buf[24];
    format_grams(buf, sizeof(buf), simTenths);
    set_text(weightLabel, buf);

    char sim[64];
    snprintf(sim, sizeof(sim), "Demo-Gewicht: %s", buf);
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
    statusLabel = lv_label_create(screen);
    lv_label_set_text(statusLabel, statusText);
    lv_obj_set_width(statusLabel, 560);
    lv_label_set_long_mode(statusLabel, LV_LABEL_LONG_DOT);
    style_label(statusLabel, COLOR_WHITE);
    lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_LEFT, 18, -84);

    touchLabel = lv_label_create(screen);
    lv_label_set_text(touchLabel, hintText);
    lv_obj_set_width(touchLabel, 560);
    lv_label_set_long_mode(touchLabel, LV_LABEL_LONG_DOT);
    style_label(touchLabel, COLOR_MUTED);
    lv_obj_align(touchLabel, LV_ALIGN_BOTTOM_LEFT, 18, -62);
}

void create_nav(lv_obj_t *screen)
{
    lv_obj_t *navWaage = create_nav_button(screen, "Waage", "nav_waage", activePage == Page::Waage);
    lv_obj_align(navWaage, LV_ALIGN_BOTTOM_LEFT, 18, -8);

    lv_obj_t *navStoppuhr = create_nav_button(screen, "Stoppuhr", "nav_stoppuhr", activePage == Page::Stoppuhr);
    lv_obj_align(navStoppuhr, LV_ALIGN_BOTTOM_LEFT, 158, -8);

    lv_obj_t *navDaten = create_nav_button(screen, "Daten", "nav_daten", activePage == Page::Daten);
    lv_obj_align(navDaten, LV_ALIGN_BOTTOM_LEFT, 298, -8);

    const bool settingsActive = activePage == Page::Settings ||
                                activePage == Page::SettingsWartung ||
                                activePage == Page::SettingsWaage ||
                                activePage == Page::SettingsWlan ||
                                activePage == Page::SettingsSystem;
    lv_obj_t *navSettings = create_nav_button(screen, "Settings", "nav_settings", settingsActive);
    lv_obj_align(navSettings, LV_ALIGN_BOTTOM_LEFT, 438, -8);
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
    lv_obj_set_size(autoBox, 82, 32);
    lv_obj_align(autoBox, LV_ALIGN_TOP_RIGHT, 0, -4);
    lv_obj_set_style_bg_color(autoBox, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(autoBox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(autoBox, 0, 0);
    lv_obj_set_style_radius(autoBox, 12, 0);
    lv_obj_set_style_pad_all(autoBox, 4, 0);
    lv_obj_add_event_cb(autoBox, button_event_cb, LV_EVENT_CLICKED, const_cast<char *>("autodetect"));

    autodetectStateLabel = lv_label_create(autoBox);
    lv_obj_set_width(autodetectStateLabel, 48);
    lv_obj_set_style_text_align(autodetectStateLabel, LV_TEXT_ALIGN_RIGHT, 0);
    style_label(autodetectStateLabel, COLOR_MUTED);
    lv_obj_align(autodetectStateLabel, LV_ALIGN_LEFT_MID, 0, 0);

    autodetectLed = lv_obj_create(autoBox);
    style_plain_block(autodetectLed, autodetectEnabled ? COLOR_GREEN : COLOR_DIM);
    lv_obj_set_size(autodetectLed, 12, 12);
    lv_obj_set_style_radius(autodetectLed, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(autodetectLed, LV_ALIGN_RIGHT_MID, -2, 0);
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

    lv_obj_t *save = create_button(inputPanel, "Save", "save", 175, 78);
    lv_obj_align(save, LV_ALIGN_TOP_MID, 0, 90);

    lv_obj_t *vesselBtn = create_button(inputPanel, vessel_name(currentVesselIndex), "vessel_select", 175, 78);
    lv_obj_align(vesselBtn, LV_ALIGN_TOP_MID, 0, 180);

    create_footer(screen,
                  "Status: Waage-Demo bereit - keine Wägezelle erforderlich",
                  "Sollgewicht links antippen. Siebträger rechts wählen. Auto oben im Gewichtsfeld antippen.");
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
                  "Status: Stoppuhr bereit",
                  "Start/Stop und Reset sind als Touch-Bedienung vorbereitet.");
}

void create_daten_page(lv_obj_t *screen)
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

    lv_obj_t *shotsPanel = lv_obj_create(screen);
    style_panel(shotsPanel);
    lv_obj_set_size(shotsPanel, 270, 180);
    lv_obj_align(shotsPanel, LV_ALIGN_TOP_LEFT, 18, 54);
    lv_obj_clear_flag(shotsPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(shotsPanel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(shotsPanel, "Shots");
    addRow(shotsPanel, "Gesamt", totalShots, 30, &totalShotsLabel);
    addSection(shotsPanel, "seit Reinigung / Wechsel", 58);
    addRow(shotsPanel, "Kaffeemaschine", machineShots, 84, &shotsMachineLabel);
    addRow(shotsPanel, "Kaffeemühle", grinderShots, 110, &shotsGrinderLabel);
    addRow(shotsPanel, "Filter", filterShots, 136, &shotsFilterLabel);

    lv_obj_t *gramsPanel = lv_obj_create(screen);
    style_panel(gramsPanel);
    lv_obj_set_size(gramsPanel, 270, 180);
    lv_obj_align(gramsPanel, LV_ALIGN_TOP_RIGHT, -18, 54);
    lv_obj_clear_flag(gramsPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(gramsPanel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(gramsPanel, "Mahlgut");
    addRow(gramsPanel, "Gesamt", totalGrams, 30, &totalGramsLabel);
    addSection(gramsPanel, "seit Reinigung / Wechsel", 58);
    addRow(gramsPanel, "Kaffeemaschine", machineGrams, 84, &gramsMachineLabel);
    addRow(gramsPanel, "Kaffeemühle", grinderGrams, 110, &gramsGrinderLabel);
    addRow(gramsPanel, "Filter", filterGrams, 136, &gramsFilterLabel);

    lv_obj_t *systemPanel = lv_obj_create(screen);
    style_panel(systemPanel);
    lv_obj_set_size(systemPanel, 564, 80);
    lv_obj_align(systemPanel, LV_ALIGN_TOP_LEFT, 18, 248);
    lv_obj_clear_flag(systemPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(systemPanel, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *systemTitle = lv_label_create(systemPanel);
    lv_label_set_text(systemTitle, "System");
    style_label(systemTitle, COLOR_MUTED);
    lv_obj_align(systemTitle, LV_ALIGN_TOP_LEFT, 0, 0);

    T4S3WifiStatus wifi;
    t4s3_wifi_get_status(wifi);

    lv_obj_t *left = lv_label_create(systemPanel);
    lv_label_set_text(left, "Uptime:\nWLAN:");
    lv_obj_set_width(left, 92);
    lv_label_set_long_mode(left, LV_LABEL_LONG_DOT);
    style_label(left, COLOR_WHITE);
    lv_obj_align(left, LV_ALIGN_TOP_LEFT, 0, 26);

    systemUptimeLabel = lv_label_create(systemPanel);
    lv_obj_set_width(systemUptimeLabel, 168);
    lv_label_set_long_mode(systemUptimeLabel, LV_LABEL_LONG_DOT);
    style_label(systemUptimeLabel, COLOR_WHITE);
    lv_obj_align(systemUptimeLabel, LV_ALIGN_TOP_LEFT, 92, 26);

    systemDataSsidLabel = lv_label_create(systemPanel);
    lv_obj_set_width(systemDataSsidLabel, 168);
    lv_label_set_long_mode(systemDataSsidLabel, LV_LABEL_LONG_DOT);
    style_label(systemDataSsidLabel, COLOR_WHITE);
    lv_obj_align(systemDataSsidLabel, LV_ALIGN_TOP_LEFT, 92, 46);

    lv_obj_t *right = lv_label_create(systemPanel);
    lv_label_set_text(right, "IP:\nSignal:");
    lv_obj_set_width(right, 66);
    lv_label_set_long_mode(right, LV_LABEL_LONG_DOT);
    style_label(right, COLOR_WHITE);
    lv_obj_align(right, LV_ALIGN_TOP_LEFT, 280, 26);

    systemDataIpLabel = lv_label_create(systemPanel);
    lv_obj_set_width(systemDataIpLabel, 205);
    lv_label_set_long_mode(systemDataIpLabel, LV_LABEL_LONG_DOT);
    style_label(systemDataIpLabel, COLOR_WHITE);
    lv_obj_align(systemDataIpLabel, LV_ALIGN_TOP_LEFT, 346, 26);

    systemDataSignalLabel = lv_label_create(systemPanel);
    lv_obj_set_width(systemDataSignalLabel, 205);
    lv_label_set_long_mode(systemDataSignalLabel, LV_LABEL_LONG_DOT);
    style_label(systemDataSignalLabel, COLOR_WHITE);
    lv_obj_align(systemDataSignalLabel, LV_ALIGN_TOP_LEFT, 346, 46);

    update_system_uptime_display();
    update_data_system_wifi_display();

    create_footer(screen,
                  "Status: Daten-Demo bereit",
                  "Daten aktiv: Shots/Mahlgut Demo, WLAN echt");
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
                  "Status kommt aus WiFi / Setup-AP bleibt ohne Scan");
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
        {"Gefäße einmessen", "Gewicht erfassen", "für Autodetect", "vessels_measure", 274, 42},
        {"Gefäße verwalten", "anzeigen / löschen", "0/4 eingemessen", "vessels_manage", 0, 142},
        {"Gesamtwerte", "Shots und Mahlgut", "korrigieren", "totals_edit", 274, 142},
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
                  "Status: Waage / Gefäße-Demo bereit",
                  "Kalibrierung, Gefäße und Gesamtwerte folgen als Detailseiten");
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
    lv_label_set_text(hint, "Intervalle: Kaffeemaschine 10 Tage, Mühle 28 Tage, Filter 12 Wochen");
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

    maintenance_status(maintenanceMachineEpoch, MAINTENANCE_MACHINE_INTERVAL_SEC,
                       dirMachine, sizeof(dirMachine), timeMachine, sizeof(timeMachine));
    maintenance_status(maintenanceGrinderEpoch, MAINTENANCE_GRINDER_INTERVAL_SEC,
                       dirGrinder, sizeof(dirGrinder), timeGrinder, sizeof(timeGrinder));
    maintenance_status(maintenanceFilterEpoch, MAINTENANCE_FILTER_INTERVAL_SEC,
                       dirFilter, sizeof(dirFilter), timeFilter, sizeof(timeFilter));

    create_maintenance_row(panel, "Kaffeemaschine", dirMachine, timeMachine, "maintenance_reset_machine", 72);
    create_maintenance_row(panel, "Kaffeemühle", dirGrinder, timeGrinder, "maintenance_reset_grinder", 128);
    create_maintenance_row(panel, "Filter", dirFilter, timeFilter, "maintenance_reset_filter", 184);

    
    update_maintenance_display();create_footer(screen,
                  "Wartung bereit",
                  "Reset speichert den aktuellen NTP-Zeitpunkt");
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
                  "Zeitstatus, Bildschirmtimeout und Neustart");
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
                  "Status: Settings-Demo bereit",
                  "Settings: Wartung, Waage / Gefäße, WLAN und System");
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
    case Page::Settings:
        create_settings_page(screen);
        break;
    case Page::SettingsWartung:
        create_settings_wartung_page(screen);
        break;
    case Page::SettingsWaage:
        create_settings_waage_page(screen);
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

    simTenths += simDir;
    if (simTenths >= 42) {
        simDir = -1;
    } else if (simTenths <= 0) {
        simDir = 1;
    }
    update_sim_weight();
}












