#include <Arduino.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>
#include <math.h>
#include <string.h>

#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#if defined(COFFEE_USE_HX711) && COFFEE_USE_HX711
#include <HX711_ADC.h>
#endif

#include "ui_t4s3/ui_t4s3.h"
#include "t4s3_pins.h"
#include "t4s3_wifi.h"
#include "t4s3_time.h"
#include "t4s3_settings.h"
#include "t4s3_scale.h"
#include "app_state.h"
#include "coffee_web.h"
#include "coffee_ota.h"

LilyGo_Class amoled;

static constexpr uint8_t kDisplayBrightnessAwake = 128;
static constexpr uint8_t kDisplayBrightnessSleep = 0;
static constexpr uint32_t kWakeGraceMs = 3000UL;
static constexpr float kDisplayWakeWeightDeltaGrams = 30.0f;

static bool g_displaySleeping = false;
static uint32_t g_sleepAllowedAfterWakeMs = 0;
static lv_obj_t *g_sleepOverlay = nullptr;
static bool g_sleepWeightReferenceValid = false;
static float g_sleepWeightReferenceGrams = 0.0f;
static bool g_weightActivityReferenceValid = false;
static float g_weightActivityReferenceGrams = 0.0f;

static AsyncWebServer g_server(80);
static AppState g_webState;
static T4S3UiSettings g_webSettingsCache;
static bool g_webSettingsCacheValid = false;
static uint32_t g_lastWebSettingsRefreshMs = 0;
static uint32_t g_lastWebMaintenanceRefreshMs = 0;
static uint32_t g_lastWebBroadcastMs = 0;
static bool g_webUiStarted = false;
static constexpr uint32_t kWebStartDelayMs = 5000UL;
static uint32_t g_webMaintenanceMachineEpoch = 0;
static uint32_t g_webMaintenanceGrinderEpoch = 0;
static uint32_t g_webMaintenanceFilterEpoch = 0;
static constexpr uint32_t kWebSettingsRefreshMs = 5000UL;
static constexpr uint32_t kWebMaintenanceRefreshMs = 10000UL;
static constexpr uint32_t kWebBroadcastIntervalMs = 500UL;

#if COFFEE_T4S3_MAINTENANCE_TEST_30S
static constexpr uint32_t kMaintenanceMachineIntervalSec = 30UL;
static constexpr uint32_t kMaintenanceGrinderIntervalSec = 30UL;
static constexpr uint32_t kMaintenanceFilterIntervalSec = 30UL;
#else
static constexpr uint32_t kMaintenanceMachineIntervalSec = 864000UL;   // 10 Tage
static constexpr uint32_t kMaintenanceGrinderIntervalSec = 2419200UL;  // 28 Tage
static constexpr uint32_t kMaintenanceFilterIntervalSec = 7257600UL;   // 12 Wochen / 84 Tage
#endif


#if defined(COFFEE_USE_HX711) && COFFEE_USE_HX711
static HX711_ADC g_loadCell(coffee_t4s3_pins::HX711_DOUT_PIN,
                            coffee_t4s3_pins::HX711_SCK_PIN);
static bool g_hx711Ready = false;
static uint32_t g_lastHx711LogMs = 0;
static float g_lastHx711Raw = 0.0f;
static float g_lastHx711Filtered = 0.0f;

// Separate HX711 paths:
// - fast: responsive UI / shot dynamics
// - stable: calm reference for tare/autodetect/save decisions later
// - display: adaptive value, fast during movement and calm when settled
static float g_fastWeightRaw = 0.0f;
static float g_stableWeightRaw = 0.0f;
static float g_displayWeightRaw = 0.0f;
static bool g_weightPipelineInitialized = false;
static bool g_weightMoving = false;
static bool g_weightStable = false;
static uint32_t g_lastMovementMs = 0;

static constexpr float kMovementThresholdRaw = 120.0f;
static constexpr uint32_t kStableDelayMs = 700UL;

static float g_hx711CalFactorRawPerGram = 1000.0f;
static float g_hxHistory[5] = {0};
static uint8_t g_hxHistoryIndex = 0;
static bool g_hxHistoryFilled = false;
#endif

static void deleteSleepOverlay()
{
    if (g_sleepOverlay) {
        lv_obj_del_async(g_sleepOverlay);
        g_sleepOverlay = nullptr;
    }
}

static void wakeDisplay(const char *reason, bool keepOverlay = false)
{
    if (!g_displaySleeping) {
        return;
    }

    amoled.setBrightness(kDisplayBrightnessAwake);
    g_displaySleeping = false;
    g_sleepWeightReferenceValid = false;

    if (!keepOverlay) {
        deleteSleepOverlay();
    }

    // Reset LVGL's own inactivity timer and our grace timer.
    lv_disp_trig_activity(nullptr);
    ui_t4s3_notify_activity();
    g_sleepAllowedAfterWakeMs = millis() + kWakeGraceMs;

    Serial.printf("[T4S3] Display wakeup by %s\n", reason ? reason : "unknown");
}

static void sleepOverlayEvent(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_PRESSED) {
        // The first touch after timeout is handled by this fullscreen overlay.
        // It wakes the display, but it cannot hit any real button underneath.
        wakeDisplay("touch", true);
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST || code == LV_EVENT_CLICKED) {
        // Keep the overlay until the wake-touch is released. Only then reveal
        // the real UI, so the wake-touch can never become an accidental click.
        deleteSleepOverlay();
        lv_disp_trig_activity(nullptr);
        ui_t4s3_notify_activity();
        g_sleepAllowedAfterWakeMs = millis() + kWakeGraceMs;
    }
}

#if defined(COFFEE_USE_HX711) && COFFEE_USE_HX711
static void updateDisplayWakeByWeight();

static void beginHx711Test()
{
    Serial.printf("[HX711] init start: DOUT=IO%u, SCK=IO%u\n",
                  coffee_t4s3_pins::HX711_DOUT_PIN,
                  coffee_t4s3_pins::HX711_SCK_PIN);

    g_loadCell.begin();

    constexpr unsigned long kStabilizingTimeMs = 2000UL;
    constexpr bool kDoTareAtStartup = true;

    g_loadCell.start(kStabilizingTimeMs, kDoTareAtStartup);

    if (g_loadCell.getTareTimeoutFlag()) {
        Serial.println("[HX711] start failed: tare timeout / no signal");
        g_hx711Ready = false;
        return;
    }

    // Keep the HX711_ADC calibration factor at 1.0 so getData() remains a raw-like
    // value. We calculate grams ourselves from filtered raw / raw-per-gram factor.
    g_loadCell.setCalFactor(1.0f);
    g_hx711CalFactorRawPerGram = t4s3_settings_load_hx711_cal_factor(1000.0f);
    g_hx711Ready = true;
    Serial.printf("[HX711] ready; cal=%.4f raw/g; logging raw values on new samples\n",
                  g_hx711CalFactorRawPerGram);
}

static void tickHx711Test(uint32_t now)
{
    const bool newData = g_loadCell.update();

    if (!g_hx711Ready) {
        if (now - g_lastHx711LogMs >= 1000UL) {
            g_lastHx711LogMs = now;
            Serial.println("[HX711] not ready");
        }
        return;
    }

    if (!newData) {
        return;
    }

    g_lastHx711Raw = g_loadCell.getData();

    g_hxHistory[g_hxHistoryIndex] = g_lastHx711Raw;
    g_hxHistoryIndex = (g_hxHistoryIndex + 1) % 5;
    if (g_hxHistoryIndex == 0) {
        g_hxHistoryFilled = true;
    }

    float median = g_lastHx711Raw;
    if (g_hxHistoryFilled) {
        float temp[5];
        memcpy(temp, g_hxHistory, sizeof(temp));
        for (int i = 0; i < 4; ++i) {
            for (int j = i + 1; j < 5; ++j) {
                if (temp[j] < temp[i]) {
                    float t = temp[i];
                    temp[i] = temp[j];
                    temp[j] = t;
                }
            }
        }
        median = temp[2];
    }

    if (!g_weightPipelineInitialized) {
        g_fastWeightRaw = median;
        g_stableWeightRaw = median;
        g_displayWeightRaw = median;
        g_lastHx711Filtered = median;
        g_weightPipelineInitialized = true;
        g_lastMovementMs = now;
    } else {
        const float previousFast = g_fastWeightRaw;

        // Fast path: follows changes quickly, but still suppresses sample noise.
        g_fastWeightRaw = g_fastWeightRaw * 0.65f + median * 0.35f;

        // Stable path: calm, slow reference for future stability decisions.
        g_stableWeightRaw = g_stableWeightRaw * 0.92f + median * 0.08f;

        const float movementDelta = fabsf(g_fastWeightRaw - previousFast);
        if (movementDelta > kMovementThresholdRaw) {
            g_lastMovementMs = now;
            g_weightMoving = true;
            g_weightStable = false;
        } else if ((now - g_lastMovementMs) > kStableDelayMs) {
            g_weightMoving = false;
            g_weightStable = true;
        } else {
            g_weightMoving = false;
            g_weightStable = false;
        }

        // Adaptive display path:
        // - during movement: follow fast path
        // - while settling: blend toward stable path
        // - once stable: strongly favor stable path
        if (g_weightMoving) {
            g_displayWeightRaw = g_fastWeightRaw;
        } else if (g_weightStable) {
            g_displayWeightRaw = g_displayWeightRaw * 0.80f + g_stableWeightRaw * 0.20f;
        } else {
            g_displayWeightRaw = g_displayWeightRaw * 0.85f + g_stableWeightRaw * 0.15f;
        }

        g_lastHx711Filtered = g_displayWeightRaw;
    }

    ui_t4s3_set_hx711_raw_value(static_cast<int32_t>(g_lastHx711Raw));
    ui_t4s3_set_hx711_grams_value(t4s3_scale_current_grams(), g_hx711Ready);
    updateDisplayWakeByWeight();

    if (now - g_lastHx711LogMs >= 250UL) {
        g_lastHx711LogMs = now;
        Serial.printf("[HX711] raw=%.2f fast=%.2f stable=%.2f display=%.2f grams=%.2f moving=%d stable=%d cal=%.4f\n",
                      g_lastHx711Raw,
                      g_fastWeightRaw,
                      g_stableWeightRaw,
                      g_displayWeightRaw,
                      t4s3_scale_current_grams(),
                      g_weightMoving ? 1 : 0,
                      g_weightStable ? 1 : 0,
                      g_hx711CalFactorRawPerGram);
    }
}

bool t4s3_scale_is_ready()
{
    return g_hx711Ready;
}

bool t4s3_scale_is_stable()
{
    return g_hx711Ready && g_weightStable;
}

bool t4s3_scale_tare()
{
    if (!g_hx711Ready) {
        return false;
    }

    g_loadCell.tare();
    g_lastHx711Raw = 0.0f;
    g_lastHx711Filtered = 0.0f;
    g_fastWeightRaw = 0.0f;
    g_stableWeightRaw = 0.0f;
    g_displayWeightRaw = 0.0f;
    g_weightPipelineInitialized = false;
    g_weightMoving = false;
    g_weightStable = false;
    g_lastMovementMs = millis();
    memset(g_hxHistory, 0, sizeof(g_hxHistory));
    g_hxHistoryIndex = 0;
    g_hxHistoryFilled = false;
    g_sleepWeightReferenceValid = false;
    g_weightActivityReferenceValid = false;
    Serial.println("[HX711] tare done");
    return true;
}

bool t4s3_scale_calibrate(float knownGrams)
{
    if (!g_hx711Ready || knownGrams <= 0.0f) {
        return false;
    }

    const float raw = fabsf(g_displayWeightRaw);
    if (!isfinite(raw) || raw < 100.0f) {
        Serial.printf("[HX711] calibration rejected: raw=%.2f known=%.2f\n", raw, knownGrams);
        return false;
    }

    g_hx711CalFactorRawPerGram = raw / knownGrams;
    t4s3_settings_save_hx711_cal_factor(g_hx711CalFactorRawPerGram);
    Serial.printf("[HX711] calibration set: %.4f raw/g from %.2f raw and %.2f g\n",
                  g_hx711CalFactorRawPerGram, raw, knownGrams);
    return true;
}

float t4s3_scale_current_grams()
{
    if (!g_hx711Ready || g_hx711CalFactorRawPerGram <= 0.0f) {
        return 0.0f;
    }
    return g_displayWeightRaw / g_hx711CalFactorRawPerGram;
}

static void updateDisplayWakeByWeight()
{
    if (!g_hx711Ready || g_hx711CalFactorRawPerGram <= 0.0f) {
        return;
    }

    const float currentGrams = t4s3_scale_current_grams();
    if (!isfinite(currentGrams)) {
        return;
    }

    if (!g_weightActivityReferenceValid) {
        g_weightActivityReferenceGrams = currentGrams;
        g_weightActivityReferenceValid = true;
    }

    if (g_displaySleeping) {
        if (!g_sleepWeightReferenceValid) {
            g_sleepWeightReferenceGrams = currentGrams;
            g_sleepWeightReferenceValid = true;
            return;
        }

        if (fabsf(currentGrams - g_sleepWeightReferenceGrams) >= kDisplayWakeWeightDeltaGrams) {
            g_weightActivityReferenceGrams = currentGrams;
            g_weightActivityReferenceValid = true;
            wakeDisplay("weight");
        }
        return;
    }

    if (fabsf(currentGrams - g_weightActivityReferenceGrams) >= kDisplayWakeWeightDeltaGrams) {
        g_weightActivityReferenceGrams = currentGrams;
        g_weightActivityReferenceValid = true;

        lv_disp_trig_activity(nullptr);
        ui_t4s3_notify_activity();

        Serial.println("[T4S3] Display timeout postponed by weight change");
    }
}

float t4s3_scale_calibration_factor()
{
    return g_hx711CalFactorRawPerGram;
}
#endif

static void createSleepOverlay()
{
    deleteSleepOverlay();

    g_sleepOverlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_sleepOverlay);
    lv_obj_set_size(g_sleepOverlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(g_sleepOverlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_sleepOverlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_sleepOverlay, LV_OPA_COVER, 0);
    lv_obj_add_flag(g_sleepOverlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_sleepOverlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_sleepOverlay, sleepOverlayEvent, LV_EVENT_ALL, nullptr);
    lv_obj_move_foreground(g_sleepOverlay);
}

static void sleepDisplay()
{
    if (g_displaySleeping) {
        return;
    }

    Serial.println("[T4S3] Display timeout reached, dimming AMOLED");

    createSleepOverlay();
    lv_refr_now(nullptr);

#if defined(COFFEE_USE_HX711) && COFFEE_USE_HX711
    if (g_hx711Ready) {
        g_sleepWeightReferenceGrams = t4s3_scale_current_grams();
        g_sleepWeightReferenceValid = isfinite(g_sleepWeightReferenceGrams);
        if (g_sleepWeightReferenceValid) {
            g_weightActivityReferenceGrams = g_sleepWeightReferenceGrams;
            g_weightActivityReferenceValid = true;
        }
    } else {
        g_sleepWeightReferenceValid = false;
    }
#endif

    amoled.setBrightness(kDisplayBrightnessSleep);
    g_displaySleeping = true;
}


static void refreshWebSettingsCache(uint32_t now, bool force = false)
{
    if (!force && g_webSettingsCacheValid && (now - g_lastWebSettingsRefreshMs) < kWebSettingsRefreshMs) {
        return;
    }

    t4s3_settings_load(g_webSettingsCache);
    g_webSettingsCacheValid = true;
    g_lastWebSettingsRefreshMs = now;
}

static void refreshWebMaintenanceCache(uint32_t now, bool force = false)
{
    if (!force && (now - g_lastWebMaintenanceRefreshMs) < kWebMaintenanceRefreshMs) {
        return;
    }

    t4s3_settings_load_maintenance(g_webMaintenanceMachineEpoch,
                                   g_webMaintenanceGrinderEpoch,
                                   g_webMaintenanceFilterEpoch);
    g_lastWebMaintenanceRefreshMs = now;
}

static int32_t maintenanceSecondsToDue(uint32_t lastEpoch, uint32_t intervalSec, uint32_t nowEpoch)
{
    if (nowEpoch == 0 || lastEpoch == 0) {
        return 0;
    }

    const uint32_t dueEpoch = lastEpoch + intervalSec;
    if (nowEpoch < dueEpoch) {
        return static_cast<int32_t>(dueEpoch - nowEpoch);
    }
    return -static_cast<int32_t>(nowEpoch - dueEpoch);
}

static bool maintenanceIsDue(uint32_t lastEpoch, uint32_t intervalSec, uint32_t nowEpoch)
{
    return nowEpoch != 0 && lastEpoch != 0 && nowEpoch >= (lastEpoch + intervalSec);
}

static const char *webWifiSignalLabel(uint8_t bars)
{
    if (bars >= 4) {
        return "sehr gut";
    }
    if (bars == 3) {
        return "gut";
    }
    if (bars == 2) {
        return "schwach";
    }
    if (bars == 1) {
        return "sehr schwach";
    }
    return "nicht verbunden";
}

static void updateWebState(uint32_t now)
{
    refreshWebSettingsCache(now);
    refreshWebMaintenanceCache(now);

    g_webState.weight.actual_g = t4s3_scale_current_grams();
    g_webState.weight.set_g = g_webSettingsCache.targetTenthsBySiebtraeger[g_webSettingsCache.selectedSiebtraeger] / 10.0f;
    g_webState.weight.stable = t4s3_scale_is_stable();

    g_webState.status.save_ready = ui_t4s3_web_save_ready();
    g_webState.status.mode = g_webState.weight.stable ? APP_STATUS_STABLE : APP_STATUS_MEASURING;

    g_webState.stopwatch.ms = ui_t4s3_web_stopwatch_ms();
    g_webState.stopwatch.running = ui_t4s3_web_stopwatch_running();

    g_webState.selection.siebtraeger = g_webSettingsCache.selectedSiebtraeger;
    g_webState.selection.gefaess = ui_t4s3_web_selected_gefaess();
    g_webState.selection.autodetect = g_webSettingsCache.autodetectEnabled;
    for (uint8_t i = 0; i < T4S3_SIEBTRAEGER_SLOT_COUNT && i < 4; ++i) {
        strlcpy(g_webState.selection.siebtraeger_names[i], ui_t4s3_web_siebtraeger_name(i), sizeof(g_webState.selection.siebtraeger_names[i]));
        g_webState.selection.siebtraeger_targets_g[i] = g_webSettingsCache.targetTenthsBySiebtraeger[i] / 10.0f;
    }

    g_webState.calibration.set_weight_g = ui_t4s3_web_calibration_weight_g();
    g_webState.calibration.factor = t4s3_scale_calibration_factor();

    for (int i = 0; i < 4; ++i) {
        g_webState.gefaesse.weights_g[i] = 0.0f;
    }
    for (uint8_t i = 0; i < T4S3_GEFAESS_SLOT_COUNT && i < 4; ++i) {
        g_webState.gefaesse.weights_g[i] = g_webSettingsCache.gefaessWeightTenths[i] >= 0
                                             ? g_webSettingsCache.gefaessWeightTenths[i] / 10.0f
                                             : 0.0f;
    }

    g_webState.stats.ground.total_g = g_webSettingsCache.totalGramsTenths / 10.0f;
    g_webState.stats.ground.since_grinder_clean_g = g_webSettingsCache.grinderGramsTenths / 10.0f;
    g_webState.stats.ground.since_machine_clean_g = g_webSettingsCache.machineGramsTenths / 10.0f;
    g_webState.stats.ground.since_filter_change_g = g_webSettingsCache.filterGramsTenths / 10.0f;

    g_webState.stats.shots.total = g_webSettingsCache.totalShots;
    g_webState.stats.shots.since_grinder_clean = g_webSettingsCache.grinderShots;
    g_webState.stats.shots.since_machine_clean = g_webSettingsCache.machineShots;
    g_webState.stats.shots.since_filter_change = g_webSettingsCache.filterShots;

    const uint32_t nowEpoch = t4s3_time_now_epoch();
    g_webState.maintenance.machine_clean_due = maintenanceIsDue(g_webMaintenanceMachineEpoch, g_webSettingsCache.maintenanceMachineIntervalSec, nowEpoch);
    g_webState.maintenance.grinder_clean_due = maintenanceIsDue(g_webMaintenanceGrinderEpoch, g_webSettingsCache.maintenanceGrinderIntervalSec, nowEpoch);
    g_webState.maintenance.filter_change_due = maintenanceIsDue(g_webMaintenanceFilterEpoch, g_webSettingsCache.maintenanceFilterIntervalSec, nowEpoch);
    g_webState.maintenance.machine_seconds_to_due = maintenanceSecondsToDue(g_webMaintenanceMachineEpoch, g_webSettingsCache.maintenanceMachineIntervalSec, nowEpoch);
    g_webState.maintenance.grinder_seconds_to_due = maintenanceSecondsToDue(g_webMaintenanceGrinderEpoch, g_webSettingsCache.maintenanceGrinderIntervalSec, nowEpoch);
    g_webState.maintenance.filter_seconds_to_due = maintenanceSecondsToDue(g_webMaintenanceFilterEpoch, g_webSettingsCache.maintenanceFilterIntervalSec, nowEpoch);
    g_webState.maintenance.machine_interval_sec = g_webSettingsCache.maintenanceMachineIntervalSec;
    g_webState.maintenance.grinder_interval_sec = g_webSettingsCache.maintenanceGrinderIntervalSec;
    g_webState.maintenance.filter_interval_sec = g_webSettingsCache.maintenanceFilterIntervalSec;

    g_webState.maintenance.due_count = 0;
    if (g_webState.maintenance.machine_clean_due) ++g_webState.maintenance.due_count;
    if (g_webState.maintenance.grinder_clean_due) ++g_webState.maintenance.due_count;
    if (g_webState.maintenance.filter_change_due) ++g_webState.maintenance.due_count;

    T4S3WifiStatus wifi;
    t4s3_wifi_get_status(wifi);
    g_webState.time.valid = t4s3_time_is_valid();
    g_webState.time.epoch = nowEpoch;
    g_webState.system.wifi_connected = wifi.connected;
    g_webState.system.ip = wifi.ip;
    g_webState.system.wifi_rssi_dbm = wifi.rssi;
    g_webState.system.wifi_signal_level = wifi.qualityBars;
    g_webState.system.wifi_signal_label = webWifiSignalLabel(wifi.qualityBars);
    g_webState.system.uptime_ms = now;
    g_webState.system.web_wizard_active = ui_t4s3_web_wizard_active();
    g_webState.system.autodetect_paused = ui_t4s3_web_wizard_active();
}

static bool handleT4S3WebCommand(const char *cmd)
{
    if (!cmd || cmd[0] == '\0') {
        return false;
    }

    if (ui_t4s3_handle_web_command(cmd)) {
        const uint32_t now = millis();
        g_webSettingsCacheValid = false;
        refreshWebSettingsCache(now, true);
        refreshWebMaintenanceCache(now, true);
        updateWebState(now);
        coffeeWebBroadcastState(g_webState);
        return true;
    }

    if (strcmp(cmd, "restart_device") == 0) {
        Serial.println("[T4S3][WebUI] restart requested");
        coffeeOtaRequestReboot();
        return true;
    }

    // Die WebUI ist jetzt mit den einfachen T4-S3-HMI-Aktionen und
    // Wartungs-Resets verbunden. Komplexere Assistenten wie Kalibrierung
    // und Gefäß-Einmess-Wizard werden in separaten Schritten gemappt.
    Serial.printf("[T4S3][WebUI] command not mapped yet: %s\n", cmd);
    return false;
}

static void beginWebUi()
{
    if (g_webUiStarted) {
        return;
    }

    if (!SPIFFS.begin(false)) {
        Serial.println("[T4S3][WebUI] SPIFFS mount failed or no filesystem uploaded; PWA icons may be unavailable");
    } else {
        Serial.println("[T4S3][WebUI] SPIFFS mounted");
    }

    refreshWebSettingsCache(millis(), true);
    refreshWebMaintenanceCache(millis(), true);

    g_server.on("/", HTTP_GET, coffeeWebHandleRoot);
    coffeeWebSetCommandHandler(handleT4S3WebCommand);
    coffeeWebBegin(g_server);
    coffeeOtaBegin(g_server);
    g_server.begin();
    g_webUiStarted = true;
    Serial.println("[T4S3][WebUI] HTTP server started on port 80");
}

static void tickWebUi(uint32_t now)
{
    if (!g_webUiStarted) {
        return;
    }

    coffeeWebLoop();

    if (!coffeeWebHasClients()) {
        return;
    }

    if (now - g_lastWebBroadcastMs < kWebBroadcastIntervalMs) {
        return;
    }

    g_lastWebBroadcastMs = now;
    updateWebState(now);
    coffeeWebBroadcastState(g_webState);
}

void setup()
{
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println("[T4S3] Kaffeewaage LVGL touch lab starting");

    const bool ok = amoled.begin();
    if (!ok) {
        Serial.println("[T4S3] ERROR: AMOLED board could not be detected");
        while (true) {
            delay(1000);
        }
    }

    amoled.setRotation(0);  // T4-S3 Kaffeewaage: landscape 600x450, USB unten
    amoled.setBrightness(kDisplayBrightnessAwake);
    beginLvglHelper(amoled);

    t4s3_settings_begin();

    Serial.printf("[T4S3] Board: %s, rotation=%u, width=%u, height=%u\n",
                  amoled.getName(),
                  amoled.getRotation(),
                  amoled.width(),
                  amoled.height());
#if defined(COFFEE_USE_HX711) && COFFEE_USE_HX711
    Serial.printf("[T4S3] Reserved HX711 pins: DOUT=IO%u, SCK=IO%u (HX711 test active)\n",
                  coffee_t4s3_pins::HX711_DOUT_PIN,
                  coffee_t4s3_pins::HX711_SCK_PIN);
    beginHx711Test();
#else
    Serial.printf("[T4S3] Reserved HX711 pins: DOUT=IO%u, SCK=IO%u (simulator active)\n",
                  coffee_t4s3_pins::HX711_DOUT_PIN,
                  coffee_t4s3_pins::HX711_SCK_PIN);
#endif

    ui_t4s3_create(amoled.width(), amoled.height());
    lv_task_handler();
    lv_refr_now(nullptr);

    t4s3_wifi_begin();
    t4s3_time_begin();
    ui_t4s3_notify_activity();
    lv_disp_trig_activity(nullptr);
    g_sleepAllowedAfterWakeMs = millis() + kWakeGraceMs;
}

void loop()
{
    const uint32_t now = millis();

    t4s3_wifi_tick();
    t4s3_time_tick();
#if defined(COFFEE_USE_HX711) && COFFEE_USE_HX711
    tickHx711Test(now);
#endif
    ui_t4s3_tick();
    lv_task_handler();

    if (!g_webUiStarted && now >= kWebStartDelayMs) {
        beginWebUi();
    }
    tickWebUi(now);
    coffeeOtaLoop();

    if (!g_displaySleeping) {
        const uint16_t timeoutMinutes = ui_t4s3_get_screen_timeout_minutes();

        if (timeoutMinutes > 0 &&
            static_cast<int32_t>(now - g_sleepAllowedAfterWakeMs) >= 0) {
            const uint32_t timeoutMs = static_cast<uint32_t>(timeoutMinutes) * 60UL * 1000UL;
            const uint32_t inactiveMs = lv_disp_get_inactive_time(nullptr);

            if (inactiveMs >= timeoutMs) {
                sleepDisplay();
            }
        }
    }

    delay(5);
}


