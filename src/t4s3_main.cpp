#include <Arduino.h>
#include <LilyGo_AMOLED.h>
#include <LV_Helper.h>

#include "ui_t4s3/ui_t4s3.h"

LilyGo_Class amoled;

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
    beginLvglHelper(amoled);

    Serial.printf("[T4S3] Board: %s, rotation=%u, width=%u, height=%u\n",
                  amoled.getName(),
                  amoled.getRotation(),
                  amoled.width(),
                  amoled.height());

    ui_t4s3_create(amoled.width(), amoled.height());
}

void loop()
{
    ui_t4s3_tick();
    lv_task_handler();
    delay(5);
}
