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

// WLAN-Zugangsdaten zentral verwalten. NVS schema v1 supports two stored
// profiles plus a preferred profile. Existing single-profile data is migrated
// once to WLAN 1. Automatic connection can be disabled independently.
bool coffeeWifiLoadCredentials(CoffeeWifiCredentials& credentials);
bool coffeeWifiHasStoredCredentials();
bool coffeeWifiStoredCredentialsAreActive();
String coffeeWifiStoredSsid();
bool coffeeWifiStoredPasswordAvailable();

// Two-profile backend. Profile numbers are 1 and 2.
bool coffeeWifiProfileAvailable(uint8_t profile);
String coffeeWifiProfileSsid(uint8_t profile);
bool coffeeWifiProfilePasswordAvailable(uint8_t profile);
bool coffeeWifiSaveProfile(uint8_t profile, const String& ssid, const String& password);
bool coffeeWifiClearProfile(uint8_t profile);
uint8_t coffeeWifiPreferredProfile();
bool coffeeWifiSetPreferredProfile(uint8_t profile);
bool coffeeWifiAutomaticConnectionEnabled();
bool coffeeWifiSetAutomaticConnectionEnabled(bool enabled);

// Compatibility API for the current single-profile WebUI (maps to WLAN 1).
bool coffeeWifiSaveCredentials(const String& ssid, const String& password);
bool coffeeWifiSetStoredCredentialsActive(bool active);
bool coffeeWifiClearCredentials();
uint32_t coffeeWifiStoredCredentialsRevision();
CoffeeWifiCredentialSource coffeeWifiCredentialSource();
const char* coffeeWifiCredentialSourceLabel();
String coffeeWifiStatusSummary();
void coffeeWifiBegin();
void coffeeWifiLoop();
void coffeeWifiMarkConnected();
void coffeeWifiReconnect();
String coffeeWifiCurrentSsid();
bool coffeeWifiUsingStoredCredentials();
const char* coffeeWifiSetupApSsid();
bool coffeeWifiStartSetupAp();
bool coffeeWifiStopSetupAp();
bool coffeeWifiSetupApActive();
bool coffeeWifiRecoveryModeActive();
String coffeeWifiSetupApIp();
