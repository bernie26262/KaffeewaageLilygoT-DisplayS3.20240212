#include "t4s3_time.h"

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <string.h>

namespace {

constexpr const char *NTP_SERVER_1 = "pool.ntp.org";
constexpr const char *NTP_SERVER_2 = "time.nist.gov";
constexpr const char *NTP_SERVER_3 = "time.google.com";

// Europe/Berlin with automatic daylight saving time.
constexpr const char *TZ_EUROPE_BERLIN = "CET-1CEST,M3.5.0/2,M10.5.0/3";
constexpr const char *TZ_LABEL = "Europe/Berlin";
constexpr const char *SOURCE_LABEL = "NTP pool.ntp.org";

bool ntpConfigured = false;
bool timeWasValid = false;
uint32_t lastSyncCheckMs = 0;

bool read_local_time(tm& out, uint32_t timeoutMs = 0)
{
    return getLocalTime(&out, timeoutMs);
}

void configure_ntp()
{
    if (ntpConfigured || WiFi.status() != WL_CONNECTED) {
        return;
    }

    Serial.println("[T4S3][Time] configuring NTP: Europe/Berlin");
    configTzTime(TZ_EUROPE_BERLIN, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    ntpConfigured = true;
    lastSyncCheckMs = 0;
}

}  // namespace

void t4s3_time_begin()
{
    ntpConfigured = false;
    timeWasValid = false;
    lastSyncCheckMs = 0;
}

void t4s3_time_tick()
{
    configure_ntp();

    if (!ntpConfigured) {
        return;
    }

    const uint32_t now = millis();
    if (now - lastSyncCheckMs < 1000UL) {
        return;
    }
    lastSyncCheckMs = now;

    tm timeinfo;
    const bool valid = read_local_time(timeinfo, 0);
    if (valid && !timeWasValid) {
        char buf[32];
        strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &timeinfo);
        Serial.printf("[T4S3][Time] synced: %s\n", buf);
    }
    timeWasValid = valid;
}

bool t4s3_time_is_valid()
{
    if (!ntpConfigured) {
        return false;
    }

    tm timeinfo;
    return read_local_time(timeinfo, 0);
}

const char *t4s3_time_zone_label()
{
    return TZ_LABEL;
}

const char *t4s3_time_source_label()
{
    return SOURCE_LABEL;
}

void t4s3_time_format_header(char *buf, size_t len)
{
    if (!buf || len == 0) {
        return;
    }

    tm timeinfo;
    if (!ntpConfigured || !read_local_time(timeinfo, 0)) {
        strlcpy(buf, "--.--.--  --:--:--", len);
        return;
    }

    strftime(buf, len, "%d.%m.%y  %H:%M:%S", &timeinfo);
}

void t4s3_time_format_local(char *buf, size_t len)
{
    if (!buf || len == 0) {
        return;
    }

    tm timeinfo;
    if (!ntpConfigured || !read_local_time(timeinfo, 0)) {
        strlcpy(buf, "--.--.---- --:--:--", len);
        return;
    }

    strftime(buf, len, "%d.%m.%Y %H:%M:%S", &timeinfo);
}

uint32_t t4s3_time_now_epoch()
{
    if (!ntpConfigured) {
        return 0;
    }

    time_t now = time(nullptr);
    if (now < 1700000000) {
        return 0;
    }

    return static_cast<uint32_t>(now);
}
