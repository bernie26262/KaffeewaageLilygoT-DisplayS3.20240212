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

// One-time migration from the original single-profile NVS keys above.
constexpr const char* WIFI_PREF_KEY_SCHEMA = "schema";
constexpr uint8_t WIFI_PREF_SCHEMA_PROFILES = 1;
constexpr const char* WIFI_PREF_KEY_P1_SSID = "p1_ssid";
constexpr const char* WIFI_PREF_KEY_P1_PASS = "p1_pass";
constexpr const char* WIFI_PREF_KEY_P2_SSID = "p2_ssid";
constexpr const char* WIFI_PREF_KEY_P2_PASS = "p2_pass";
constexpr const char* WIFI_PREF_KEY_PREFERRED = "pref";
constexpr const char* WIFI_PREF_KEY_AUTO = "auto";
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
bool profileAvailable[2] = {false, false};
String profileSsid[2];
bool profilePasswordAvailable[2] = {false, false};
uint8_t preferredProfile = 1;
bool automaticConnectionEnabled = false;
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

bool profileNumberValid(uint8_t profile)
{
  return profile == 1 || profile == 2;
}

const char* profileSsidKey(uint8_t profile)
{
  return profile == 2 ? WIFI_PREF_KEY_P2_SSID : WIFI_PREF_KEY_P1_SSID;
}

const char* profilePassKey(uint8_t profile)
{
  return profile == 2 ? WIFI_PREF_KEY_P2_PASS : WIFI_PREF_KEY_P1_PASS;
}

bool ensureProfileStorageMigrated()
{
  Preferences prefs;
  if (!prefs.begin(WIFI_PREF_NAMESPACE, false)) {
    return false;
  }

  const uint8_t schema = prefs.getUChar(WIFI_PREF_KEY_SCHEMA, 0);
  if (schema >= WIFI_PREF_SCHEMA_PROFILES) {
    prefs.end();
    return true;
  }

  String legacySsid = prefs.getString(WIFI_PREF_KEY_SSID, "");
  const String legacyPassword = prefs.getString(WIFI_PREF_KEY_PASS, "");
  const bool legacyActive = prefs.getBool(WIFI_PREF_KEY_ACTIVE, false);
  legacySsid.trim();

  // Do not erase the old keys: they remain intact for rollback.
  bool migrated = true;
  if (legacySsid.length() > 0) {
    if (!prefs.isKey(WIFI_PREF_KEY_P1_SSID)) {
      migrated = prefs.putString(WIFI_PREF_KEY_P1_SSID, legacySsid) > 0 && migrated;
    }
    if (!prefs.isKey(WIFI_PREF_KEY_P1_PASS)) {
      migrated = (legacyPassword.length() == 0 ||
                  prefs.putString(WIFI_PREF_KEY_P1_PASS, legacyPassword) > 0) && migrated;
    }
  }

  migrated = prefs.putUChar(WIFI_PREF_KEY_PREFERRED, 1) > 0 && migrated;
  migrated = prefs.putBool(WIFI_PREF_KEY_AUTO, legacySsid.length() > 0 && legacyActive) > 0 && migrated;
  if (migrated) {
    migrated = prefs.putUChar(WIFI_PREF_KEY_SCHEMA, WIFI_PREF_SCHEMA_PROFILES) > 0;
  }
  prefs.end();
  return migrated;
}

void refreshStoredCredentialsCache()
{
  storedCredentialsAvailable = false;
  storedCredentialsActive = false;
  storedCredentialsSsid = String();
  storedCredentialsPasswordAvailable = false;
  profileAvailable[0] = profileAvailable[1] = false;
  profileSsid[0] = profileSsid[1] = String();
  profilePasswordAvailable[0] = profilePasswordAvailable[1] = false;
  preferredProfile = 1;
  automaticConnectionEnabled = false;

  if (!ensureProfileStorageMigrated()) {
    storedCredentialsKnown = true;
    return;
  }

  Preferences prefs;
  if (!prefs.begin(WIFI_PREF_NAMESPACE, true)) {
    storedCredentialsKnown = true;
    return;
  }

  for (uint8_t profile = 1; profile <= 2; ++profile) {
    String ssid = prefs.getString(profileSsidKey(profile), "");
    const String password = prefs.getString(profilePassKey(profile), "");
    ssid.trim();
    const uint8_t index = profile - 1;
    profileSsid[index] = ssid;
    profileAvailable[index] = ssid.length() > 0;
    profilePasswordAvailable[index] = profileAvailable[index] && password.length() > 0;
  }

  preferredProfile = prefs.getUChar(WIFI_PREF_KEY_PREFERRED, 1);
  if (!profileNumberValid(preferredProfile)) {
    preferredProfile = 1;
  }
  automaticConnectionEnabled = prefs.getBool(WIFI_PREF_KEY_AUTO, false);
  prefs.end();

  const uint8_t preferredIndex = preferredProfile - 1;
  storedCredentialsAvailable = profileAvailable[0] || profileAvailable[1];
  storedCredentialsSsid = profileSsid[preferredIndex];
  storedCredentialsPasswordAvailable = profilePasswordAvailable[preferredIndex];
  storedCredentialsActive = automaticConnectionEnabled && profileAvailable[preferredIndex];
  storedCredentialsKnown = true;
}

bool readProfileCredentials(uint8_t profile, CoffeeWifiCredentials& credentials)
{
  if (!profileNumberValid(profile) || !ensureProfileStorageMigrated()) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(WIFI_PREF_NAMESPACE, true)) {
    return false;
  }

  String ssid = prefs.getString(profileSsidKey(profile), "");
  const String password = prefs.getString(profilePassKey(profile), "");
  prefs.end();

  ssid.trim();
  if (ssid.length() == 0) {
    return false;
  }

  credentials.ssid = ssid;
  credentials.password = password;
  credentials.fromPreferences = true;
  credentials.source = CoffeeWifiCredentialSource::Preferences;
  return true;
}

bool readStoredCredentials(CoffeeWifiCredentials& credentials)
{
  if (!storedCredentialsKnown) {
    refreshStoredCredentialsCache();
  }
  if (!automaticConnectionEnabled || !profileNumberValid(preferredProfile)) {
    return false;
  }
  return readProfileCredentials(preferredProfile, credentials);
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

bool coffeeWifiProfileAvailable(uint8_t profile)
{
  if (!profileNumberValid(profile)) {
    return false;
  }
  if (!storedCredentialsKnown) {
    refreshStoredCredentialsCache();
  }
  return profileAvailable[profile - 1];
}

String coffeeWifiProfileSsid(uint8_t profile)
{
  if (!profileNumberValid(profile)) {
    return String();
  }
  if (!storedCredentialsKnown) {
    refreshStoredCredentialsCache();
  }
  return profileSsid[profile - 1];
}

bool coffeeWifiProfilePasswordAvailable(uint8_t profile)
{
  if (!profileNumberValid(profile)) {
    return false;
  }
  if (!storedCredentialsKnown) {
    refreshStoredCredentialsCache();
  }
  return profilePasswordAvailable[profile - 1];
}

uint8_t coffeeWifiPreferredProfile()
{
  if (!storedCredentialsKnown) {
    refreshStoredCredentialsCache();
  }
  return preferredProfile;
}

bool coffeeWifiAutomaticConnectionEnabled()
{
  if (!storedCredentialsKnown) {
    refreshStoredCredentialsCache();
  }
  return automaticConnectionEnabled;
}

bool coffeeWifiSaveProfile(uint8_t profile, const String& ssid, const String& password)
{
  if (!profileNumberValid(profile)) {
    return false;
  }

  String trimmedSsid = ssid;
  trimmedSsid.trim();
  if (trimmedSsid.length() == 0 || !ensureProfileStorageMigrated()) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(WIFI_PREF_NAMESPACE, false)) {
    return false;
  }

  String passwordToStore = password;
  if (passwordToStore.length() == 0 && prefs.isKey(profilePassKey(profile))) {
    passwordToStore = prefs.getString(profilePassKey(profile), "");
  }

  const size_t ssidBytes = prefs.putString(profileSsidKey(profile), trimmedSsid);
  const size_t passBytes = prefs.putString(profilePassKey(profile), passwordToStore);
  prefs.end();

  refreshStoredCredentialsCache();
  storedCredentialsRevision++;
  return ssidBytes > 0 && (passwordToStore.length() == 0 || passBytes > 0);
}

bool coffeeWifiClearProfile(uint8_t profile)
{
  if (!profileNumberValid(profile) || !ensureProfileStorageMigrated()) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(WIFI_PREF_NAMESPACE, false)) {
    return false;
  }

  prefs.remove(profileSsidKey(profile));
  prefs.remove(profilePassKey(profile));

  const uint8_t currentPreferred = prefs.getUChar(WIFI_PREF_KEY_PREFERRED, 1);
  if (currentPreferred == profile) {
    const uint8_t other = profile == 1 ? 2 : 1;
    String otherSsid = prefs.getString(profileSsidKey(other), "");
    otherSsid.trim();
    if (otherSsid.length() > 0) {
      prefs.putUChar(WIFI_PREF_KEY_PREFERRED, other);
    } else {
      prefs.putBool(WIFI_PREF_KEY_AUTO, false);
    }
  }
  prefs.end();

  activeCredentialsLoaded = false;
  activeCredentials = CoffeeWifiCredentials{};
  refreshStoredCredentialsCache();
  storedCredentialsRevision++;
  return true;
}

bool coffeeWifiSetPreferredProfile(uint8_t profile)
{
  if (!profileNumberValid(profile) || !coffeeWifiProfileAvailable(profile) || !ensureProfileStorageMigrated()) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(WIFI_PREF_NAMESPACE, false)) {
    return false;
  }
  const size_t bytes = prefs.putUChar(WIFI_PREF_KEY_PREFERRED, profile);
  prefs.end();

  activeCredentialsLoaded = false;
  activeCredentials = CoffeeWifiCredentials{};
  refreshStoredCredentialsCache();
  storedCredentialsRevision++;
  return bytes > 0;
}

bool coffeeWifiSetAutomaticConnectionEnabled(bool enabled)
{
  if (!ensureProfileStorageMigrated()) {
    return false;
  }
  if (enabled && !coffeeWifiProfileAvailable(coffeeWifiPreferredProfile())) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(WIFI_PREF_NAMESPACE, false)) {
    return false;
  }
  const size_t bytes = prefs.putBool(WIFI_PREF_KEY_AUTO, enabled);
  prefs.end();

  activeCredentialsLoaded = false;
  activeCredentials = CoffeeWifiCredentials{};
  refreshStoredCredentialsCache();
  storedCredentialsRevision++;
  return bytes > 0;
}

// Backwards-compatible T4 WebUI: edit WLAN 1, leave inactive until enabled.
bool coffeeWifiSaveCredentials(const String& ssid, const String& password)
{
  if (!coffeeWifiSaveProfile(1, ssid, password)) {
    return false;
  }
  return coffeeWifiSetAutomaticConnectionEnabled(false);
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
  if (active && !coffeeWifiSetPreferredProfile(1)) {
    return false;
  }
  return coffeeWifiSetAutomaticConnectionEnabled(active);
}

bool coffeeWifiClearCredentials()
{
  return coffeeWifiClearProfile(1);
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
