#include <Arduino.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>

#include "ui_t4s3/ui_t4s3.h"
#include "t4s3_pins.h"
#include "t4s3_wifi.h"
#include "t4s3_time.h"
#include "t4s3_settings.h"

LilyGo_Class amoled;

static constexpr uint8_t kDisplayBrightnessAwake = 128;
static constexpr uint8_t kDisplayBrightnessSleep = 0;
static constexpr uint32_t kWakeGraceMs = 3000UL;

static bool g_displaySleeping = false;
static uint32_t g_sleepAllowedAfterWakeMs = 0;
static lv_obj_t *g_sleepOverlay = nullptr;

static void deleteSleepOverlay()
{
    if (g_sleepOverlay) {
        lv_obj_del_async(g_sleepOverlay);
        g_sleepOverlay = nullptr;
    }
}

static void wakeDisplay(const char *reason)
{
    if (!g_displaySleeping) {
        return;
    }

    amoled.setBrightness(kDisplayBrightnessAwake);
    g_displaySleeping = false;

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
        wakeDisplay("touch");
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

    amoled.setBrightness(kDisplayBrightnessSleep);
    g_displaySleeping = true;
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
    Serial.printf("[T4S3] Reserved HX711 pins: DOUT=IO%u, SCK=IO%u (simulator active)\n",
                  coffee_t4s3_pins::HX711_DOUT_PIN,
                  coffee_t4s3_pins::HX711_SCK_PIN);

    ui_t4s3_create(amoled.width(), amoled.height());
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
    ui_t4s3_tick();
    lv_task_handler();

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

