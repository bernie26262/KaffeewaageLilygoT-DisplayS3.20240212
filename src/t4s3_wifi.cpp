#include "t4s3_wifi.h"

#include <Arduino.h>
#include <WiFi.h>
#include <stdio.h>
#include <string.h>

#include "coffee_wifi.h"

namespace {

bool wifiStarted = false;
bool connectedMarked = false;
wl_status_t lastWifiStatus = WL_IDLE_STATUS;
uint32_t lastReconnectAttemptMs = 0;
uint32_t lastStatusLogMs = 0;
uint32_t wifiBeginMs = 0;
uint32_t firstApStartAttemptMs = 0;
bool autoSetupApStarted = false;

constexpr uint32_t kSetupApNoSsidDelayMs = 5000UL;
constexpr uint32_t kSetupApFallbackDelayMs = 45000UL;

void copy_text(char *dst, size_t dstSize, const String& src)
{
    if (!dst || dstSize == 0) {
        return;
    }
    strlcpy(dst, src.c_str(), dstSize);
}

const char *quality_text(int32_t rssi)
{
    if (rssi >= -60) {
        return "sehr gut";
    }
    if (rssi >= -70) {
        return "gut";
    }
    if (rssi >= -80) {
        return "schwach";
    }
    return "sehr schwach";
}

uint8_t quality_bars(int32_t rssi)
{
    if (rssi >= -60) {
        return 4;
    }
    if (rssi >= -70) {
        return 3;
    }
    if (rssi >= -80) {
        return 2;
    }
    if (rssi >= -90) {
        return 1;
    }
    return 0;
}

void log_status_if_changed()
{
    const wl_status_t currentStatus = WiFi.status();
    const uint32_t now = millis();

    if (currentStatus != lastWifiStatus || now - lastStatusLogMs >= 15000UL) {
        lastWifiStatus = currentStatus;
        lastStatusLogMs = now;

        if (currentStatus == WL_CONNECTED) {
            Serial.printf("[T4S3][WiFi] connected: SSID='%s', IP=%s, RSSI=%ld dBm\n",
                          WiFi.SSID().c_str(),
                          WiFi.localIP().toString().c_str(),
                          static_cast<long>(WiFi.RSSI()));
        } else {
            Serial.printf("[T4S3][WiFi] status=%d, target SSID='%s'\n",
                          static_cast<int>(currentStatus),
                          coffeeWifiCurrentSsid().c_str());
        }
    }
}

}  // namespace

void t4s3_wifi_begin()
{
    if (wifiStarted) {
        return;
    }

    Serial.printf("[T4S3][WiFi] begin: %s\n", coffeeWifiStatusSummary().c_str());
    coffeeWifiBegin();
    wifiStarted = true;
    connectedMarked = false;
    wifiBeginMs = millis();
    firstApStartAttemptMs = 0;
    autoSetupApStarted = false;
    lastReconnectAttemptMs = wifiBeginMs;
    lastWifiStatus = WiFi.status();
}

void t4s3_wifi_tick()
{
    if (!wifiStarted) {
        return;
    }

    const wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED) {
        if (!connectedMarked) {
            coffeeWifiMarkConnected();
            connectedMarked = true;
        }
    } else {
        connectedMarked = false;

        const uint32_t now = millis();
        if (now - lastReconnectAttemptMs >= 15000UL) {
            lastReconnectAttemptMs = now;
            Serial.println("[T4S3][WiFi] reconnect");
            coffeeWifiReconnect();
        }

        const String targetSsid = coffeeWifiCurrentSsid();
        const bool noConfiguredSsid = targetSsid.length() == 0;
        const uint32_t setupDelayMs = noConfiguredSsid ? kSetupApNoSsidDelayMs : kSetupApFallbackDelayMs;
        if (!coffeeWifiSetupApActive() && !autoSetupApStarted && now - wifiBeginMs >= setupDelayMs) {
            autoSetupApStarted = true;
            firstApStartAttemptMs = now;
            Serial.printf("[T4S3][WiFi] no STA connection after %lu ms, starting setup AP\n",
                          static_cast<unsigned long>(now - wifiBeginMs));
            t4s3_wifi_start_setup_ap();
        }

        if (!coffeeWifiSetupApActive() && autoSetupApStarted && firstApStartAttemptMs != 0 && now - firstApStartAttemptMs >= 30000UL) {
            firstApStartAttemptMs = now;
            Serial.println("[T4S3][WiFi] setup AP retry");
            t4s3_wifi_start_setup_ap();
        }
    }

    log_status_if_changed();
}

bool t4s3_wifi_start_setup_ap()
{
    const bool ok = coffeeWifiStartSetupAp();
    if (ok) {
        autoSetupApStarted = true;
        firstApStartAttemptMs = millis();
        Serial.printf("[T4S3][WiFi] setup AP active: SSID='%s', IP=%s\n",
                      coffeeWifiSetupApSsid(),
                      coffeeWifiSetupApIp().c_str());
    } else {
        Serial.println("[T4S3][WiFi] setup AP start failed");
    }
    return ok;
}

void t4s3_wifi_get_status(T4S3WifiStatus& out)
{
    memset(&out, 0, sizeof(out));

    out.connected = WiFi.status() == WL_CONNECTED;
    out.setupApActive = coffeeWifiSetupApActive();

    strlcpy(out.setupApSsid, coffeeWifiSetupApSsid(), sizeof(out.setupApSsid));

    if (out.setupApActive) {
        copy_text(out.setupApIp, sizeof(out.setupApIp), coffeeWifiSetupApIp());
        copy_text(out.webUiAddress, sizeof(out.webUiAddress), coffeeWifiSetupApIp());
    } else {
        strlcpy(out.setupApIp, "-", sizeof(out.setupApIp));
        strlcpy(out.webUiAddress, "192.168.4.1", sizeof(out.webUiAddress));
    }

    if (out.connected) {
        out.rssi = WiFi.RSSI();
        out.qualityBars = quality_bars(out.rssi);

        strlcpy(out.status, "verbunden", sizeof(out.status));
        copy_text(out.ssid, sizeof(out.ssid), WiFi.SSID());
        copy_text(out.ip, sizeof(out.ip), WiFi.localIP().toString());

        snprintf(out.signal, sizeof(out.signal), "%ld dBm, %s",
                 static_cast<long>(out.rssi),
                 quality_text(out.rssi));
        return;
    }

    out.rssi = 0;
    out.qualityBars = 0;
    strlcpy(out.ip, "-", sizeof(out.ip));
    strlcpy(out.signal, "-", sizeof(out.signal));

    const String configuredSsid = coffeeWifiCurrentSsid();
    if (configuredSsid.length() > 0) {
        copy_text(out.ssid, sizeof(out.ssid), configuredSsid);
    } else {
        strlcpy(out.ssid, "-", sizeof(out.ssid));
    }

    if (out.setupApActive) {
        strlcpy(out.status, "Setup-AP aktiv", sizeof(out.status));
        return;
    }

    const wl_status_t status = WiFi.status();
    if (status == WL_NO_SSID_AVAIL) {
        strlcpy(out.status, "SSID nicht gefunden", sizeof(out.status));
    } else if (status == WL_CONNECT_FAILED) {
        strlcpy(out.status, "Verbindung fehlgeschlagen", sizeof(out.status));
    } else if (status == WL_DISCONNECTED) {
        strlcpy(out.status, "nicht verbunden", sizeof(out.status));
    } else {
        strlcpy(out.status, "verbinden ...", sizeof(out.status));
    }
}
