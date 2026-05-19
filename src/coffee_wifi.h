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

// WLAN-Zugangsdaten zentral verwalten.
// Sicherheitsstand nach Phase-2a-Test:
// - gespeicherte NVS-Daten nur verwenden, wenn sie explizit als aktiv markiert sind
// - sonst unverändert auf wifi_secrets.h zurückfallen
bool coffeeWifiLoadCredentials(CoffeeWifiCredentials& credentials);
bool coffeeWifiHasStoredCredentials();
bool coffeeWifiStoredCredentialsAreActive();
bool coffeeWifiSaveCredentials(const String& ssid, const String& password);
bool coffeeWifiSetStoredCredentialsActive(bool active);
bool coffeeWifiClearCredentials();
CoffeeWifiCredentialSource coffeeWifiCredentialSource();
const char* coffeeWifiCredentialSourceLabel();
String coffeeWifiStatusSummary();
void coffeeWifiBegin();
void coffeeWifiMarkConnected();
void coffeeWifiReconnect();
String coffeeWifiCurrentSsid();
bool coffeeWifiUsingStoredCredentials();
