#pragma once

#include <stdint.h>

void ui_t4s3_create(uint16_t width, uint16_t height);
void ui_t4s3_tick();

uint16_t ui_t4s3_get_screen_timeout_minutes();
uint32_t ui_t4s3_get_last_activity_ms();
void ui_t4s3_notify_activity();
void ui_t4s3_prepare_wakeup_touch();
