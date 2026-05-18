#pragma once

#include <Arduino.h>

struct CoffeeWifiCredentials {
  String ssid;
  String password;
  bool fromPreferences = false;
};

// Phase 1: WLAN-Zugangsdaten zentral verwalten.
// - bevorzugt gespeicherte Daten aus NVS/Preferences nutzen
// - wenn keine gespeicherten Daten vorhanden sind, unverändert auf wifi_secrets.h zurückfallen
bool coffeeWifiLoadCredentials(CoffeeWifiCredentials& credentials);
bool coffeeWifiHasStoredCredentials();
bool coffeeWifiSaveCredentials(const String& ssid, const String& password);
void coffeeWifiBegin();
void coffeeWifiReconnect();
String coffeeWifiCurrentSsid();
bool coffeeWifiUsingStoredCredentials();
