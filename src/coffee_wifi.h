#pragma once

#include <Arduino.h>

enum class CoffeeWifiCredentialSource {
  None,
  Preferences,
  WifiSecrets
};

struct CoffeeWifiCredentials {
  String ssid;
  String password;
  bool fromPreferences = false;
  CoffeeWifiCredentialSource source = CoffeeWifiCredentialSource::None;
};

// Phase A1: NVS schema v1 supports two stored WLAN profiles.
// Existing single-profile NVS credentials are migrated once to WLAN 1.
// The current T4 connection/fallback logic remains unchanged until phase A2.
bool coffeeWifiLoadCredentials(CoffeeWifiCredentials& credentials);
bool coffeeWifiHasStoredCredentials();
bool coffeeWifiStoredCredentialsAreActive();
String coffeeWifiStoredSsid();
bool coffeeWifiStoredPasswordAvailable();

// WLAN profiles (1/2), preferred profile and automatic connection switch.
bool coffeeWifiProfileAvailable(uint8_t profile);
String coffeeWifiProfileSsid(uint8_t profile);
bool coffeeWifiProfilePasswordAvailable(uint8_t profile);
bool coffeeWifiSaveProfile(uint8_t profile, const String& ssid, const String& password);
bool coffeeWifiClearProfile(uint8_t profile);
uint8_t coffeeWifiPreferredProfile();
bool coffeeWifiSetPreferredProfile(uint8_t profile);
bool coffeeWifiAutomaticConnectionEnabled();
bool coffeeWifiSetAutomaticConnectionEnabled(bool enabled);

// Compatibility API for the existing T4 single-profile WebUI (WLAN 1).
bool coffeeWifiSaveCredentials(const String& ssid, const String& password);
bool coffeeWifiSetStoredCredentialsActive(bool active);
bool coffeeWifiClearCredentials();
uint32_t coffeeWifiStoredCredentialsRevision();
CoffeeWifiCredentialSource coffeeWifiCredentialSource();
const char* coffeeWifiCredentialSourceLabel();
String coffeeWifiStatusSummary();
void coffeeWifiBegin();
void coffeeWifiMarkConnected();
void coffeeWifiReconnect();
String coffeeWifiCurrentSsid();
bool coffeeWifiUsingStoredCredentials();
String coffeeWifiResetReasonLabel();
String coffeeWifiSetupApDiagPhaseLabel();
bool coffeeWifiSetupApDiagInterrupted();
const char* coffeeWifiSetupApSsid();
bool coffeeWifiStartSetupAp();
bool coffeeWifiStopSetupAp();
bool coffeeWifiSetupApActive();
String coffeeWifiSetupApIp();
void coffeeWifiMarkSetupCredentialsSaved();
bool coffeeWifiSetupCredentialsSavedPendingRestart();
void coffeeWifiClearSetupCredentialsSavedPendingRestart();
