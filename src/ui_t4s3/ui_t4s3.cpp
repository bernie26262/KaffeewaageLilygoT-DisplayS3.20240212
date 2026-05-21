#include "ui_t4s3.h"

#include <Arduino.h>
#include <lvgl.h>
#include <stdio.h>

namespace {

constexpr uint32_t COLOR_BG = 0x000000;
constexpr uint32_t COLOR_PANEL = 0x07120a;
constexpr uint32_t COLOR_GREEN = 0x2E8B57;
constexpr uint32_t COLOR_GREEN_DARK = 0x1F5F3D;
constexpr uint32_t COLOR_WHITE = 0xFFFFFF;
constexpr uint32_t COLOR_MUTED = 0xA8B8A8;

lv_obj_t *weightLabel = nullptr;
lv_obj_t *statusLabel = nullptr;
lv_obj_t *touchLabel = nullptr;
lv_obj_t *simLabel = nullptr;
lv_obj_t *timerLabel = nullptr;
lv_obj_t *timerButtonLabel = nullptr;

uint32_t lastSimMs = 0;
uint32_t lastTimerMs = 0;
uint32_t timerBaseMs = 0;
uint32_t timerStartedMs = 0;
bool timerRunning = false;
int32_t simTenths = 0;
int8_t simDir = 1;

void set_text(lv_obj_t *obj, const char *text)
{
    if (obj) {
        lv_label_set_text(obj, text);
    }
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

void update_status(const char *msg)
{
    Serial.printf("[T4S3] %s\n", msg);
    set_text(statusLabel, msg);
}

void update_timer_display()
{
    uint32_t elapsed = timerBaseMs;
    if (timerRunning) {
        elapsed += millis() - timerStartedMs;
    }

    const uint32_t tenths = (elapsed / 100) % 10;
    const uint32_t seconds = (elapsed / 1000) % 60;
    const uint32_t minutes = (elapsed / 60000) % 60;

    char buf[24];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%lu",
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds),
             static_cast<unsigned long>(tenths));
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
        update_status("Timer gestartet - Demo-Stoppuhr laeuft");
    } else {
        timerBaseMs += millis() - timerStartedMs;
        timerRunning = false;
        set_text(timerButtonLabel, "Timer");
        update_status("Timer gestoppt - Demo-Stoppuhr pausiert");
    }
    update_timer_display();
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
        update_status("Save gedrueckt - noch Demo-Modus ohne Waegezelle");
    } else if (strcmp(action, "timer") == 0) {
        set_timer_running(!timerRunning);
    } else if (strcmp(action, "setup") == 0) {
        update_status("Setup gedrueckt - LVGL-Touch ist bereit");
    }
}

lv_obj_t *create_button(lv_obj_t *parent, const char *text, const char *action)
{
    lv_obj_t *btn = lv_btn_create(parent);
    style_button(btn);
    lv_obj_set_width(btn, 170);
    lv_obj_set_height(btn, 58);
    lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, const_cast<char *>(action));

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    style_label(label, COLOR_WHITE);
    lv_obj_center(label);
    if (strcmp(action, "timer") == 0) {
        timerButtonLabel = label;
    }
    return btn;
}

void update_sim_weight()
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%ld,%d g", static_cast<long>(simTenths / 10), abs(simTenths % 10));
    set_text(weightLabel, buf);

    char sim[64];
    snprintf(sim, sizeof(sim), "Demo-Gewicht: %s", buf);
    set_text(simLabel, sim);
}

}  // namespace

void ui_t4s3_create(uint16_t width, uint16_t height)
{
    lv_obj_t *screen = lv_scr_act();
    style_screen(screen);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Kaffeewaage T4-S3  |  LVGL Touch Lab");
    style_label(title, COLOR_GREEN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 18, 12);

    lv_obj_t *mode = lv_label_create(screen);
    lv_label_set_text_fmt(mode, "%ux%u  |  Rotation 0  |  ohne Waegezelle", width, height);
    style_label(mode, COLOR_MUTED);
    lv_obj_align(mode, LV_ALIGN_TOP_RIGHT, -18, 12);

    lv_obj_t *weightPanel = lv_obj_create(screen);
    style_panel(weightPanel);
    lv_obj_set_size(weightPanel, 365, 270);
    lv_obj_align(weightPanel, LV_ALIGN_TOP_LEFT, 18, 54);

    lv_obj_t *weightTitle = lv_label_create(weightPanel);
    lv_label_set_text(weightTitle, "Gewicht");
    style_label(weightTitle, COLOR_MUTED);
    lv_obj_align(weightTitle, LV_ALIGN_TOP_LEFT, 0, 0);

    weightLabel = lv_label_create(weightPanel);
    lv_label_set_text(weightLabel, "0,0 g");
    lv_obj_set_width(weightLabel, 230);
    lv_label_set_long_mode(weightLabel, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(weightLabel, LV_TEXT_ALIGN_RIGHT, 0);
    style_label(weightLabel, COLOR_WHITE);
    lv_obj_align(weightLabel, LV_ALIGN_CENTER, -20, -10);

    lv_obj_t *timerTitle = lv_label_create(weightPanel);
    lv_label_set_text(timerTitle, "Timer");
    style_label(timerTitle, COLOR_MUTED);
    lv_obj_align(timerTitle, LV_ALIGN_BOTTOM_RIGHT, 0, -30);

    timerLabel = lv_label_create(weightPanel);
    lv_label_set_text(timerLabel, "00:00:0");
    lv_obj_set_width(timerLabel, 105);
    lv_label_set_long_mode(timerLabel, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(timerLabel, LV_TEXT_ALIGN_RIGHT, 0);
    style_label(timerLabel, COLOR_WHITE);
    lv_obj_align(timerLabel, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    simLabel = lv_label_create(weightPanel);
    lv_label_set_text(simLabel, "Demo-Gewicht: 0,0 g");
    lv_obj_set_width(simLabel, 210);
    lv_label_set_long_mode(simLabel, LV_LABEL_LONG_DOT);
    style_label(simLabel, COLOR_GREEN);
    lv_obj_align(simLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *buttonPanel = lv_obj_create(screen);
    style_panel(buttonPanel);
    lv_obj_set_size(buttonPanel, 195, 270);
    lv_obj_align(buttonPanel, LV_ALIGN_TOP_RIGHT, -18, 54);

    lv_obj_t *buttonTitle = lv_label_create(buttonPanel);
    lv_label_set_text(buttonTitle, "Aktionen");
    style_label(buttonTitle, COLOR_MUTED);
    lv_obj_align(buttonTitle, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *tara = create_button(buttonPanel, "Tara", "tara");
    lv_obj_align(tara, LV_ALIGN_TOP_MID, 0, 35);

    lv_obj_t *save = create_button(buttonPanel, "Save", "save");
    lv_obj_align(save, LV_ALIGN_TOP_MID, 0, 100);

    lv_obj_t *timer = create_button(buttonPanel, "Timer", "timer");
    lv_obj_align(timer, LV_ALIGN_TOP_MID, 0, 165);

    lv_obj_t *setup = create_button(screen, "Setup", "setup");
    lv_obj_set_size(setup, 130, 48);
    lv_obj_align(setup, LV_ALIGN_BOTTOM_RIGHT, -18, -18);

    statusLabel = lv_label_create(screen);
    lv_label_set_text(statusLabel, "Status: Demo-Modus bereit - keine Waegezelle erforderlich");
    lv_obj_set_width(statusLabel, 420);
    lv_label_set_long_mode(statusLabel, LV_LABEL_LONG_DOT);
    style_label(statusLabel, COLOR_WHITE);
    lv_obj_align(statusLabel, LV_ALIGN_BOTTOM_LEFT, 18, -46);

    touchLabel = lv_label_create(screen);
    lv_label_set_text(touchLabel, "Touch: Buttons reagieren, echte Waagenfunktionen folgen spaeter");
    lv_obj_set_width(touchLabel, 420);
    lv_label_set_long_mode(touchLabel, LV_LABEL_LONG_DOT);
    style_label(touchLabel, COLOR_MUTED);
    lv_obj_align(touchLabel, LV_ALIGN_BOTTOM_LEFT, 18, -20);
}

void ui_t4s3_tick()
{
    const uint32_t now = millis();

    if (timerRunning && now - lastTimerMs >= 100) {
        lastTimerMs = now;
        update_timer_display();
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
