#include "coffee_wifi.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_attr.h>
#include <esp_system.h>

#include "wifi_secrets.h"

namespace {
constexpr const char* WIFI_PREF_NAMESPACE = "coffee_wifi";
constexpr const char* WIFI_PREF_KEY_SSID = "ssid";
constexpr const char* WIFI_PREF_KEY_PASS = "pass";
constexpr const char* WIFI_PREF_KEY_ACTIVE = "active";
constexpr const char* WIFI_SETUP_AP_SSID = "Waagen-Setup";
constexpr uint32_t SETUP_AP_DIAG_MAGIC = 0x57494649UL;  // "WIFI"

enum class SetupApDiagPhase : uint8_t {
  None = 0,
  Requested = 1,
  ModeApSta = 2,
  ModeApStaDone = 3,
  SleepOff = 4,
  SleepOffDone = 5,
  SoftApStart = 6,
  SoftApOk = 7,
  SoftApFailed = 8
};

RTC_NOINIT_ATTR volatile uint32_t setupApDiagMagicRtc;
RTC_NOINIT_ATTR volatile uint8_t setupApDiagPhaseRtc;
RTC_NOINIT_ATTR volatile uint8_t setupApDiagInterruptedRtc;

CoffeeWifiCredentials activeCredentials;
bool activeCredentialsLoaded = false;
bool storedCredentialsKnown = false;
bool storedCredentialsAvailable = false;
bool storedCredentialsActive = false;
String storedCredentialsSsid;
bool storedCredentialsPasswordAvailable = false;
bool setupApActive = false;
bool setupCredentialsSavedPendingRestart = false;
uint32_t storedCredentialsRevision = 0;
uint32_t activeCredentialsConnectStartedMs = 0;
bool activeCredentialsGotIp = false;
uint8_t activeCredentialsDisconnects = 0;

void ensureSetupApDiagRtcInitialized()
{
  if (setupApDiagMagicRtc == SETUP_AP_DIAG_MAGIC) {
    return;
  }

  setupApDiagMagicRtc = SETUP_AP_DIAG_MAGIC;
  setupApDiagPhaseRtc = static_cast<uint8_t>(SetupApDiagPhase::None);
  setupApDiagInterruptedRtc = 0;
}

void markSetupApDiag(SetupApDiagPhase phase, bool interrupted)
{
  ensureSetupApDiagRtcInitialized();
  setupApDiagPhaseRtc = static_cast<uint8_t>(phase);
  setupApDiagInterruptedRtc = interrupted ? 1 : 0;
}

const char* setupApDiagPhaseLabel(uint8_t phase)
{
  switch (static_cast<SetupApDiagPhase>(phase)) {
    case SetupApDiagPhase::Requested:
      return "Start angefordert";
    case SetupApDiagPhase::ModeApSta:
      return "vor WiFi.mode(WIFI_AP_STA)";
    case SetupApDiagPhase::ModeApStaDone:
      return "WiFi.mode(WIFI_AP_STA) abgeschlossen";
    case SetupApDiagPhase::SleepOff:
      return "vor WiFi.setSleep(false)";
    case SetupApDiagPhase::SleepOffDone:
      return "WiFi.setSleep(false) abgeschlossen";
    case SetupApDiagPhase::SoftApStart:
      return "vor WiFi.softAP()";
    case SetupApDiagPhase::SoftApOk:
      return "Setup-AP erfolgreich gestartet";
    case SetupApDiagPhase::SoftApFailed:
      return "WiFi.softAP() fehlgeschlagen";
    case SetupApDiagPhase::None:
    default:
      return "kein Setup-AP-Versuch gespeichert";
  }
}

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

String coffeeWifiResetReasonLabel()
{
  const int reason = static_cast<int>(esp_reset_reason());
  switch (reason) {
    case 0:
      return "unbekannt";
    case 1:
      return "Power-On";
    case 2:
      return "externer Reset";
    case 3:
      return "Software-Neustart";
    case 4:
      return "Panic / Exception";
    case 5:
      return "Interrupt-Watchdog";
    case 6:
      return "Task-Watchdog";
    case 7:
      return "Watchdog";
    case 8:
      return "Deep-Sleep";
    case 9:
      return "Brownout";
    case 10:
      return "SDIO-Reset";
    default:
      return String("anderer Reset (") + String(reason) + ")";
  }
}

String coffeeWifiSetupApDiagPhaseLabel()
{
  ensureSetupApDiagRtcInitialized();
  return String(setupApDiagPhaseLabel(setupApDiagPhaseRtc));
}

bool coffeeWifiSetupApDiagInterrupted()
{
  ensureSetupApDiagRtcInitialized();
  return setupApDiagInterruptedRtc != 0;
}

const char* coffeeWifiSetupApSsid()
{
  return WIFI_SETUP_AP_SSID;
}

bool coffeeWifiStartSetupAp()
{
  setupCredentialsSavedPendingRestart = false;

  markSetupApDiag(SetupApDiagPhase::Requested, true);
  markSetupApDiag(SetupApDiagPhase::ModeApSta, true);
  WiFi.mode(WIFI_AP_STA);
  markSetupApDiag(SetupApDiagPhase::ModeApStaDone, true);

  // WiFi sleep is already disabled during the normal STA startup in
  // coffeeWifiBegin(). Re-applying WiFi.setSleep(false) immediately after
  // switching from WIFI_STA to WIFI_AP_STA triggers a panic on the tested
  // ESP32-S3/Arduino-ESP32 2.0.17 setup, so do not touch the PS state here.
  markSetupApDiag(SetupApDiagPhase::SoftApStart, true);
  const bool ok = WiFi.softAP(WIFI_SETUP_AP_SSID);
  setupApActive = ok;
  markSetupApDiag(ok ? SetupApDiagPhase::SoftApOk : SetupApDiagPhase::SoftApFailed, false);
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

void coffeeWifiMarkSetupCredentialsSaved()
{
  setupCredentialsSavedPendingRestart = true;
}

bool coffeeWifiSetupCredentialsSavedPendingRestart()
{
  return setupCredentialsSavedPendingRestart;
}

void coffeeWifiClearSetupCredentialsSavedPendingRestart()
{
  setupCredentialsSavedPendingRestart = false;
}
