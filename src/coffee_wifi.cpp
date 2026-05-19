#include "coffee_wifi.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#include "wifi_secrets.h"

namespace {
constexpr const char* WIFI_PREF_NAMESPACE = "coffee_wifi";
constexpr const char* WIFI_PREF_KEY_SSID = "ssid";
constexpr const char* WIFI_PREF_KEY_PASS = "pass";
constexpr const char* WIFI_PREF_KEY_ACTIVE = "active";
constexpr const char* WIFI_SETUP_AP_SSID = "Waagen-Setup";

CoffeeWifiCredentials activeCredentials;
bool activeCredentialsLoaded = false;
bool storedCredentialsKnown = false;
bool storedCredentialsAvailable = false;
bool storedCredentialsActive = false;
String storedCredentialsSsid;
bool storedCredentialsPasswordAvailable = false;
bool setupApActive = false;
uint32_t activeCredentialsConnectStartedMs = 0;
bool activeCredentialsGotIp = false;
uint8_t activeCredentialsDisconnects = 0;

void resetActiveConnectionAttempt()
{
  activeCredentialsConnectStartedMs = millis();
  activeCredentialsGotIp = false;
  activeCredentialsDisconnects = 0;
}

void refreshStoredCredentialsCache()
{
  Preferences prefs;
  storedCredentialsAvailable = false;
  storedCredentialsActive = false;
  storedCredentialsSsid = String();
  storedCredentialsPasswordAvailable = false;

  if (!prefs.begin(WIFI_PREF_NAMESPACE, true)) {
    storedCredentialsKnown = true;
    return;
  }

  String ssid = prefs.getString(WIFI_PREF_KEY_SSID, "");
  const String password = prefs.getString(WIFI_PREF_KEY_PASS, "");
  ssid.trim();
  storedCredentialsSsid = ssid;
  storedCredentialsAvailable = ssid.length() > 0;
  storedCredentialsPasswordAvailable = storedCredentialsAvailable && password.length() > 0;
  storedCredentialsActive = storedCredentialsAvailable && prefs.getBool(WIFI_PREF_KEY_ACTIVE, false);
  prefs.end();

  storedCredentialsKnown = true;
}

bool readStoredCredentials(CoffeeWifiCredentials& credentials)
{
  Preferences prefs;
  if (!prefs.begin(WIFI_PREF_NAMESPACE, true)) {
    return false;
  }

  String ssid = prefs.getString(WIFI_PREF_KEY_SSID, "");
  const String password = prefs.getString(WIFI_PREF_KEY_PASS, "");
  const bool active = prefs.getBool(WIFI_PREF_KEY_ACTIVE, false);
  prefs.end();

  ssid.trim();
  if (ssid.length() == 0 || !active) {
    return false;
  }

  credentials.ssid = ssid;
  credentials.password = password;
  credentials.fromPreferences = true;
  credentials.source = CoffeeWifiCredentialSource::Preferences;
  return true;
}

CoffeeWifiCredentials fallbackCredentials()
{
  CoffeeWifiCredentials credentials;
  credentials.ssid = WIFI_SSID;
  credentials.password = WIFI_PASS;
  credentials.fromPreferences = false;
  credentials.source = credentials.ssid.length() > 0
                         ? CoffeeWifiCredentialSource::WifiSecrets
                         : CoffeeWifiCredentialSource::None;
  return credentials;
}

const CoffeeWifiCredentials& ensureActiveCredentials()
{
  if (!activeCredentialsLoaded) {
    coffeeWifiLoadCredentials(activeCredentials);
    activeCredentialsLoaded = true;
  }
  return activeCredentials;
}
}  // namespace

bool coffeeWifiLoadCredentials(CoffeeWifiCredentials& credentials)
{
  if (readStoredCredentials(credentials)) {
    return true;
  }

  credentials = fallbackCredentials();
  return credentials.ssid.length() > 0;
}

bool coffeeWifiHasStoredCredentials()
{
  if (!storedCredentialsKnown) {
    refreshStoredCredentialsCache();
  }
  return storedCredentialsAvailable;
}

bool coffeeWifiStoredCredentialsAreActive()
{
  if (!storedCredentialsKnown) {
    refreshStoredCredentialsCache();
  }
  return storedCredentialsActive;
}

bool coffeeWifiSaveCredentials(const String& ssid, const String& password)
{
  String trimmedSsid = ssid;
  trimmedSsid.trim();
  if (trimmedSsid.length() == 0) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(WIFI_PREF_NAMESPACE, false)) {
    return false;
  }

  String passwordToStore = password;
  if (passwordToStore.length() == 0 && prefs.isKey(WIFI_PREF_KEY_PASS)) {
    passwordToStore = prefs.getString(WIFI_PREF_KEY_PASS, "");
  }

  const size_t ssidBytes = prefs.putString(WIFI_PREF_KEY_SSID, trimmedSsid);
  const size_t passBytes = prefs.putString(WIFI_PREF_KEY_PASS, passwordToStore);

  // Sicherheitsentscheidung nach Phase-2a-Test:
  // Neu gespeicherte Credentials werden zunaechst nur abgelegt, aber nicht
  // automatisch beim Boot bevorzugt. Die Aktivierung kommt erst, wenn eine
  // saubere Validierungs-/Fallback-Logik vorhanden ist.
  const size_t activeBytes = prefs.putBool(WIFI_PREF_KEY_ACTIVE, false);
  prefs.end();

  refreshStoredCredentialsCache();

  return ssidBytes > 0 && (passwordToStore.length() == 0 || passBytes > 0) && activeBytes > 0;
}

String coffeeWifiStoredSsid()
{
  if (!storedCredentialsKnown) {
    refreshStoredCredentialsCache();
  }
  return storedCredentialsSsid;
}

bool coffeeWifiStoredPasswordAvailable()
{
  if (!storedCredentialsKnown) {
    refreshStoredCredentialsCache();
  }
  return storedCredentialsPasswordAvailable;
}

bool coffeeWifiSetStoredCredentialsActive(bool active)
{
  Preferences prefs;
  if (!prefs.begin(WIFI_PREF_NAMESPACE, false)) {
    return false;
  }

  String ssid = prefs.getString(WIFI_PREF_KEY_SSID, "");
  ssid.trim();
  if (active && ssid.length() == 0) {
    prefs.end();
    refreshStoredCredentialsCache();
    return false;
  }

  const size_t activeBytes = prefs.putBool(WIFI_PREF_KEY_ACTIVE, active);
  prefs.end();

  activeCredentialsLoaded = false;
  activeCredentials = CoffeeWifiCredentials{};
  refreshStoredCredentialsCache();
  return activeBytes > 0;
}

bool coffeeWifiClearCredentials()
{
  Preferences prefs;
  if (!prefs.begin(WIFI_PREF_NAMESPACE, false)) {
    return false;
  }

  const bool ssidRemoved = prefs.remove(WIFI_PREF_KEY_SSID);
  const bool passRemoved = prefs.remove(WIFI_PREF_KEY_PASS);
  const bool activeRemoved = prefs.remove(WIFI_PREF_KEY_ACTIVE);
  prefs.end();

  activeCredentialsLoaded = false;
  activeCredentials = CoffeeWifiCredentials{};
  refreshStoredCredentialsCache();

  (void)ssidRemoved;
  (void)passRemoved;
  (void)activeRemoved;
  return true;
}

CoffeeWifiCredentialSource coffeeWifiCredentialSource()
{
  return ensureActiveCredentials().source;
}

const char* coffeeWifiCredentialSourceLabel()
{
  switch (coffeeWifiCredentialSource()) {
    case CoffeeWifiCredentialSource::Preferences:
      return "gespeicherte WLAN-Daten";
    case CoffeeWifiCredentialSource::WifiSecrets:
      return "Standard-WLAN aus Firmware";
    case CoffeeWifiCredentialSource::None:
    default:
      return "keine Quelle";
  }
}

String coffeeWifiStatusSummary()
{
  const CoffeeWifiCredentials& credentials = ensureActiveCredentials();
  String summary = "SSID='";
  summary += credentials.ssid;
  summary += "', source=";
  summary += coffeeWifiCredentialSourceLabel();
  return summary;
}

void coffeeWifiBegin()
{
  const CoffeeWifiCredentials& credentials = ensureActiveCredentials();
  WiFi.mode(setupApActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.setSleep(false);
  resetActiveConnectionAttempt();
  WiFi.begin(credentials.ssid.c_str(), credentials.password.c_str());
}

void coffeeWifiMarkConnected()
{
  activeCredentialsGotIp = true;
  activeCredentialsDisconnects = 0;
}

void coffeeWifiReconnect()
{
  const CoffeeWifiCredentials& credentials = ensureActiveCredentials();

  // Sicherheitsnetz fuer Phase 2c:
  // Aktivierte NVS-Daten werden nicht mehr beim ersten Disconnect sofort
  // deaktiviert. Einige ESP32-/Router-Kombinationen erzeugen waehrend des
  // frischen Verbindungsaufbaus kurze Disconnect-Events, obwohl die Daten
  // korrekt sind. Erst wenn innerhalb eines Zeitfensters gar keine IP geholt
  // wurde, wird der Active-Marker geloescht und dauerhaft auf wifi_secrets.h
  // zurueckgefallen.
  if (credentials.source == CoffeeWifiCredentialSource::Preferences) {
    activeCredentialsDisconnects++;
    const uint32_t elapsedMs = millis() - activeCredentialsConnectStartedMs;
    if (!activeCredentialsGotIp && elapsedMs >= 20000UL) {
      coffeeWifiSetStoredCredentialsActive(false);
      activeCredentials = fallbackCredentials();
      activeCredentialsLoaded = true;
      resetActiveConnectionAttempt();
      WiFi.begin(activeCredentials.ssid.c_str(), activeCredentials.password.c_str());
      return;
    }

    WiFi.begin(credentials.ssid.c_str(), credentials.password.c_str());
    return;
  }

  WiFi.begin(credentials.ssid.c_str(), credentials.password.c_str());
}

String coffeeWifiCurrentSsid()
{
  return ensureActiveCredentials().ssid;
}

bool coffeeWifiUsingStoredCredentials()
{
  return ensureActiveCredentials().fromPreferences;
}


const char* coffeeWifiSetupApSsid()
{
  return WIFI_SETUP_AP_SSID;
}

bool coffeeWifiStartSetupAp()
{
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  const bool ok = WiFi.softAP(WIFI_SETUP_AP_SSID);
  setupApActive = ok;
  return ok;
}

bool coffeeWifiStopSetupAp()
{
  // softAPdisconnect(true) kann auf manchen ESP32-Arduino-Versionen false
  // liefern, obwohl der AP danach bereits beendet bzw. nicht mehr aktiv ist.
  // Stoppen soll daher idempotent sein: Der Bediener erwartet, dass der
  // Setup-AP danach aus ist, nicht dass ein bereits deaktivierter AP als
  // Fehler gemeldet wird.
  WiFi.softAPdisconnect(true);
  delay(50);
  setupApActive = false;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  return true;
}

bool coffeeWifiSetupApActive()
{
  return setupApActive;
}

String coffeeWifiSetupApIp()
{
  if (!setupApActive) {
    return String();
  }
  return WiFi.softAPIP().toString();
}
