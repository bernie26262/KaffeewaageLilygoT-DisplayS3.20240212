#include "ui_t4s3.h"

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
lv_obj_t *vesselOptionLabels[4] = {nullptr, nullptr, nullptr, nullptr};

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
int32_t demoTargetTenths = 180;
int32_t draftTargetTenths = 180;
int32_t targetStepTenths = 5;
int32_t draftTargetStepTenths = 5;
uint8_t currentVesselIndex = 0;
uint8_t draftVesselIndex = 0;

void build_current_page();
void open_target_overlay();
void close_target_overlay(bool save);
void open_vessel_overlay();
void close_vessel_overlay(bool save);
lv_obj_t *create_button(lv_obj_t *parent, const char *text, const char *action, int width, int height);

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
    for (uint8_t i = 0; i < 4; ++i) {
        vesselOptionLabels[i] = nullptr;
    }
}

void set_text(lv_obj_t *obj, const char *text)
{
    if (obj) {
        lv_label_set_text(obj, text);
    }
}

const char *vessel_name(uint8_t index)
{
    switch (index) {
    case 0: return "Bodenloser ST";
    case 1: return "1er-Siebtraeger";
    case 2: return "2er-Siebtraeger";
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

void format_demo_datetime(char *buf, size_t len)
{
    const uint32_t baseSeconds = 17UL * 3600UL + 32UL * 60UL + 17UL;
    const uint32_t elapsedSeconds = millis() / 1000UL;
    const uint32_t totalSeconds = baseSeconds + elapsedSeconds;
    const uint32_t days = totalSeconds / 86400UL;
    const uint32_t secondsOfDay = totalSeconds % 86400UL;
    const uint32_t hours = secondsOfDay / 3600UL;
    const uint32_t minutes = (secondsOfDay / 60UL) % 60UL;
    const uint32_t seconds = secondsOfDay % 60UL;

    snprintf(buf, len, "%02lu.05.26  %02lu:%02lu:%02lu",
             static_cast<unsigned long>(21UL + days),
             static_cast<unsigned long>(hours),
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds));
}

void update_clock_display()
{
    char buf[32];
    format_demo_datetime(buf, sizeof(buf));
    set_text(clockLabel, buf);
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

static void button_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    const char *action = static_cast<const char *>(lv_event_get_user_data(event));
    if (!action) {
        return;
    }

    if (strcmp(action, "tara") == 0) {
        simTenths = 0;
        set_text(weightLabel, "0,0 g");
        update_status("Tara gedrueckt - Demo-Gewicht auf 0,0 g gesetzt");
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
        snprintf(msg, sizeof(msg), "Save gedrueckt - Demo-Bezug %s gespeichert", grams);
        update_status(msg);
    } else if (strcmp(action, "autodetect") == 0) {
        autodetectEnabled = !autodetectEnabled;
        update_autodetect_display();
        update_status(autodetectEnabled ? "Autodetect eingeschaltet" : "Autodetect ausgeschaltet");
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
        update_status("Kaffeemaschine: Reset folgt spaeter mit Bestaetigung");
    } else if (strcmp(action, "maintenance_reset_grinder") == 0) {
        update_status("Kaffeemuehle: Reset folgt spaeter mit Bestaetigung");
    } else if (strcmp(action, "maintenance_reset_filter") == 0) {
        update_status("Filterwechsel: Reset folgt spaeter mit Bestaetigung");
    } else if (strcmp(action, "settings_waage") == 0) {
        navigate_to(Page::SettingsWaage);
    } else if (strcmp(action, "scale_calibration") == 0) {
        update_status("Kalibrierung: Assistent folgt spaeter");
    } else if (strcmp(action, "vessels_measure") == 0) {
        update_status("Gefaesse einmessen: Assistent folgt spaeter");
    } else if (strcmp(action, "vessels_manage") == 0) {
        update_status("Gefaesse verwalten: Liste / Loeschen folgt spaeter");
    } else if (strcmp(action, "totals_edit") == 0) {
        update_status("Gesamtwerte aendern: Eingabe folgt spaeter");
    } else if (strcmp(action, "settings_wlan") == 0) {
        update_status("Settings: WLAN - Status und Setup-Assistent folgen");
    } else if (strcmp(action, "settings_system") == 0) {
        update_status("Settings: System - Neustart und Logs folgen");
    }
}


void close_target_overlay(bool save)
{
    if (save) {
        demoTargetTenths = draftTargetTenths;
        targetStepTenths = draftTargetStepTenths;
        update_target_display();
    lv_obj_move_foreground(weightLabel);

        char grams[24];
        char msg[72];
        format_grams(grams, sizeof(grams), demoTargetTenths);
        snprintf(msg, sizeof(msg), "Sollgewicht gespeichert: %s", grams);
        update_status(msg);
    } else {
        update_status("Sollgewicht nicht geaendert");
    }

    if (targetOverlay) {
        lv_obj_del(targetOverlay);
        targetOverlay = nullptr;
        targetOverlayValueLabel = nullptr;
        targetOverlayStepLabel = nullptr;
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
        update_vessel_display();

        char msg[72];
        snprintf(msg, sizeof(msg), "Siebtraeger gespeichert: %s", vessel_name(currentVesselIndex));
        update_status(msg);
    } else {
        update_status("Siebtraeger-Auswahl nicht geaendert");
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
    lv_label_set_text(title, "Siebtraeger waehlen");
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

    update_status("Siebtraeger direkt per Touch waehlen");
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

void create_wifi_quality_icon(lv_obj_t *screen, int x, int y, uint8_t quality)
{
    for (uint8_t i = 0; i < 4; ++i) {
        const int h = 6 + i * 4;
        lv_obj_t *bar = lv_obj_create(screen);
        style_plain_block(bar, i < quality ? COLOR_GREEN : COLOR_DIM);
        lv_obj_set_size(bar, 5, h);
        lv_obj_align(bar, LV_ALIGN_TOP_LEFT, x + i * 8, y + (18 - h));
    }
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

    create_wifi_quality_icon(screen, 540, 10, 3);
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

    const bool settingsActive = activePage == Page::Settings || activePage == Page::SettingsWartung || activePage == Page::SettingsWaage;
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
                  "Status: Waage-Demo bereit - keine Waegezelle erforderlich",
                  "Sollgewicht links antippen. Siebtraeger rechts waehlen. Auto oben im Gewichtsfeld antippen.");
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
    lv_label_set_text(timerInfo, timerRunning ? "laeuft" : "bereit");
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
    addRow(shotsPanel, "Kaffeemuehle", grinderShots, 110, &shotsGrinderLabel);
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
    addRow(gramsPanel, "Kaffeemuehle", grinderGrams, 110, &gramsGrinderLabel);
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

    lv_obj_t *left = lv_label_create(systemPanel);
    lv_label_set_text(left, "Uptime: 00:05:04\nWLAN: Demo-Netz");
    lv_obj_set_width(left, 265);
    lv_label_set_long_mode(left, LV_LABEL_LONG_DOT);
    style_label(left, COLOR_WHITE);
    lv_obj_align(left, LV_ALIGN_TOP_LEFT, 0, 26);

    lv_obj_t *right = lv_label_create(systemPanel);
    lv_label_set_text(right, "IP: 192.168.11.83\nSignal: gut (-63 dBm)");
    lv_obj_set_width(right, 265);
    lv_label_set_long_mode(right, LV_LABEL_LONG_DOT);
    style_label(right, COLOR_WHITE);
    lv_obj_align(right, LV_ALIGN_TOP_RIGHT, 0, 26);

    create_footer(screen,
                  "Status: Daten-Demo bereit",
                  "Daten aktiv: Shots, Mahlgut und Systemwerte noch mit Demo-Werten");
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

    lv_obj_t *button = create_button(parent, "Reset", action, 112, 42);
    lv_obj_align(button, LV_ALIGN_TOP_RIGHT, 0, y);

    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, 444, 1);
    lv_obj_set_style_bg_color(line, lv_color_hex(COLOR_DIM), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_50, 0);
    lv_obj_align(line, LV_ALIGN_TOP_LEFT, 0, y + 49);
}


void create_settings_waage_page(lv_obj_t *screen)
{
    lv_obj_t *panel = lv_obj_create(screen);
    style_panel(panel);
    lv_obj_set_size(panel, 564, 258);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

    create_panel_title(panel, "Waage / Gefaesse");

    lv_obj_t *back = create_button(panel, "Zurueck", "settings_back", 112, 40);
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
        {"Gefaesse einmessen", "Gewicht erfassen", "fuer Autodetect", "vessels_measure", 274, 42},
        {"Gefaesse verwalten", "anzeigen / loeschen", "gespeicherte Gefaesse", "vessels_manage", 0, 142},
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
    }

    create_footer(screen,
                  "Status: Waage / Gefaesse-Demo bereit",
                  "Kalibrierung, Gefaesse und Gesamtwerte folgen als Detailseiten");
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

    lv_obj_t *back = create_button(panel, "Zurueck", "settings_back", 112, 40);
    lv_obj_align(back, LV_ALIGN_TOP_RIGHT, 0, -4);

    lv_obj_t *hint = lv_label_create(panel);
    lv_label_set_text(hint, "Reinigung / Filterwechsel: Demo-Zeiten");
    lv_obj_set_width(hint, 390);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_DOT);
    style_label(hint, COLOR_MUTED);
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 0, 32);

    create_maintenance_row(panel, "Kaffeemaschine", "seit", "12 Tagen, 23:56:54", "maintenance_reset_machine", 72);
    create_maintenance_row(panel, "Kaffeemuehle", "in", "1 Tag, 03:10:08", "maintenance_reset_grinder", 128);
    create_maintenance_row(panel, "Filter", "in", "0 Tagen, 12:34:56", "maintenance_reset_filter", 184);

    create_footer(screen,
                  "Status: Wartung-Demo bereit",
                  "Reset-Buttons sind Platzhalter; spaeter mit Bestaetigungs-Overlay");
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
        {"Waage / Gefaesse", "Kalibrieren, einmessen", "Gesamtwerte korrigieren", "settings_waage", 274, 36},
        {"WLAN", "SSID, IP, Signal", "spaeter Setup-WLAN", "settings_wlan", 0, 138},
        {"System", "Neustart", "Logs / Diagnose", "settings_system", 274, 138},
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
                  "Settings: Wartung, Waage / Gefaesse, WLAN und System");
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
    }

    create_nav(screen);
}

}  // namespace

void ui_t4s3_create(uint16_t width, uint16_t height)
{
    screenWidth = width;
    screenHeight = height;
    activePage = Page::Waage;
    build_current_page();
}

void ui_t4s3_tick()
{
    const uint32_t now = millis();

    if (timerRunning && now - lastTimerMs >= 100) {
        lastTimerMs = now;
        update_timer_display();
    }

    static uint32_t lastClockMs = 0;
    if (now - lastClockMs >= 1000) {
        lastClockMs = now;
        update_clock_display();
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

