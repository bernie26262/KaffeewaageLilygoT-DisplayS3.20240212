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

// Phase 1: WLAN-Zugangsdaten zentral verwalten.
// - bevorzugt gespeicherte Daten aus NVS/Preferences nutzen
// - wenn keine gespeicherten Daten vorhanden sind, unverändert auf wifi_secrets.h zurückfallen
bool coffeeWifiLoadCredentials(CoffeeWifiCredentials& credentials);
bool coffeeWifiHasStoredCredentials();
bool coffeeWifiSaveCredentials(const String& ssid, const String& password);
bool coffeeWifiClearCredentials();
CoffeeWifiCredentialSource coffeeWifiCredentialSource();
const char* coffeeWifiCredentialSourceLabel();
String coffeeWifiStatusSummary();
void coffeeWifiBegin();
void coffeeWifiReconnect();
String coffeeWifiCurrentSsid();
bool coffeeWifiUsingStoredCredentials();
