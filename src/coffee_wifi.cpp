#include "coffee_wifi.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>


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
bool automaticRecoveryMode = false;
uint32_t storedCredentialsRevision = 0;
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

const CoffeeWifiCredentials& ensureActiveCredentials()
{
  if (!activeCredentialsLoaded) {
    coffeeWifiLoadCredentials(activeCredentials);
    activeCredentialsLoaded = true;
  }
  return activeCredentials;
}

bool enterAutomaticRecoveryMode()
{
  automaticRecoveryMode = true;

  // Im automatischen Recovery-Fall ist keine funktionierende STA-Verbindung
  // vorhanden. WIFI_AP beendet den laufenden STA-Verbindungsversuch vollstaendig
  // und verhindert, dass weitere Scans/Reconnects die AP-Beacons stoeren.
  // Der manuell gestartete Setup-AP bleibt dagegen bewusst WIFI_AP_STA.
  WiFi.mode(WIFI_AP);
  const bool ok = WiFi.softAP(WIFI_SETUP_AP_SSID);
  setupApActive = ok;
  if (!ok) {
    automaticRecoveryMode = false;
  }
  return ok;
}
}  // namespace

bool coffeeWifiLoadCredentials(CoffeeWifiCredentials& credentials)
{
  if (readStoredCredentials(credentials)) {
    return true;
  }

  credentials = CoffeeWifiCredentials{};
  return false;
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
  storedCredentialsRevision++;

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
  storedCredentialsRevision++;
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
  storedCredentialsRevision++;

  (void)ssidRemoved;
  (void)passRemoved;
  (void)activeRemoved;
  return true;
}

uint32_t coffeeWifiStoredCredentialsRevision()
{
  return storedCredentialsRevision;
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
      return "nicht verwendet";
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

  // WiFi sleep is configured once in normal STA mode. Do not call
  // WiFi.setSleep(false) directly after switching to WIFI_AP_STA.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  automaticRecoveryMode = false;
  resetActiveConnectionAttempt();

  if (credentials.ssid.length() == 0) {
    enterAutomaticRecoveryMode();
    return;
  }

  WiFi.begin(credentials.ssid.c_str(), credentials.password.c_str());
}

void coffeeWifiLoop()
{
  const CoffeeWifiCredentials& credentials = ensureActiveCredentials();
  if (automaticRecoveryMode || activeCredentialsGotIp || credentials.ssid.length() == 0) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    coffeeWifiMarkConnected();
    return;
  }

  if (millis() - activeCredentialsConnectStartedMs >= 20000UL) {
    enterAutomaticRecoveryMode();
  }
}

void coffeeWifiMarkConnected()
{
  activeCredentialsGotIp = true;
  activeCredentialsDisconnects = 0;
}

void coffeeWifiReconnect()
{
  const CoffeeWifiCredentials& credentials = ensureActiveCredentials();

  if (automaticRecoveryMode || credentials.ssid.length() == 0) {
    return;
  }

  // Nach einer zuvor erfolgreichen Verbindung beginnt bei einem Disconnect
  // ein neues 20-s-Fenster. Bleibt die Verbindung weg, uebernimmt der
  // Recovery-AP. Gespeicherte Credentials werden dabei nie deaktiviert.
  if (activeCredentialsGotIp) {
    resetActiveConnectionAttempt();
  }

  activeCredentialsDisconnects++;
  if (!activeCredentialsGotIp && millis() - activeCredentialsConnectStartedMs >= 20000UL) {
    enterAutomaticRecoveryMode();
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

  // WiFi sleep is already disabled by coffeeWifiBegin(). Do not re-apply
  // WiFi.setSleep(false) directly after switching into AP+STA mode.
  const bool ok = WiFi.softAP(WIFI_SETUP_AP_SSID);
  setupApActive = ok;
  return ok;
}

bool coffeeWifiStopSetupAp()
{
  // softAPdisconnect(true) kann auf manchen ESP32-Arduino-Versionen false
  // liefern, obwohl der AP danach bereits beendet bzw. nicht mehr aktiv ist.
  // Stoppen soll daher idempotent sein.
  const bool wasAutomaticRecovery = automaticRecoveryMode;
  WiFi.softAPdisconnect(true);
  delay(50);
  setupApActive = false;
  automaticRecoveryMode = false;
  WiFi.mode(WIFI_STA);

  // Wird ein automatisch gestarteter Recovery-AP bewusst beendet, darf die
  // Station das gespeicherte WLAN erneut versuchen.
  if (wasAutomaticRecovery && activeCredentialsLoaded && activeCredentials.ssid.length() > 0) {
    resetActiveConnectionAttempt();
    WiFi.begin(activeCredentials.ssid.c_str(), activeCredentials.password.c_str());
  }
  return true;
}

bool coffeeWifiSetupApActive()
{
  return setupApActive;
}

bool coffeeWifiRecoveryModeActive()
{
  return automaticRecoveryMode && setupApActive;
}

String coffeeWifiSetupApIp()
{
  if (!setupApActive) {
    return String();
  }
  return WiFi.softAPIP().toString();
}
