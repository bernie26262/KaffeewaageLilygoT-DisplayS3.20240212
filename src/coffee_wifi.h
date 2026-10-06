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
// Gespeicherte NVS-Daten nur verwenden, wenn sie explizit als aktiv markiert sind.
// Wenn keine aktiven Daten vorhanden sind oder die Verbindung scheitert,
// startet automatisch der Recovery-AP "Waagen-Setup".
bool coffeeWifiLoadCredentials(CoffeeWifiCredentials& credentials);
bool coffeeWifiHasStoredCredentials();
bool coffeeWifiStoredCredentialsAreActive();
String coffeeWifiStoredSsid();
bool coffeeWifiStoredPasswordAvailable();
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
