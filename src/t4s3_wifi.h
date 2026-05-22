#pragma once

#include <stdint.h>

struct T4S3WifiStatus {
    bool connected = false;
    bool setupApActive = false;
    int32_t rssi = 0;
    uint8_t qualityBars = 0;
    char status[32] = "";
    char ssid[64] = "";
    char ip[24] = "";
    char signal[40] = "";
    char setupApSsid[32] = "";
    char setupApIp[24] = "";
    char webUiAddress[24] = "";
};

void t4s3_wifi_begin();
void t4s3_wifi_tick();
bool t4s3_wifi_start_setup_ap();
void t4s3_wifi_get_status(T4S3WifiStatus& out);
