#pragma once

#include <stddef.h>
#include <stdint.h>

void t4s3_time_begin();
void t4s3_time_tick();
bool t4s3_time_is_valid();
const char *t4s3_time_zone_label();
const char *t4s3_time_source_label();
void t4s3_time_format_header(char *buf, size_t len);
void t4s3_time_format_local(char *buf, size_t len);
uint32_t t4s3_time_now_epoch();



