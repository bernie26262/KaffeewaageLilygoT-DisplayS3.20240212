#include "coffee_wifi.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_attr.h>
#include <esp_system.h>

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
constexpr uint32_t WIFI_PROFILE_CONNECT_TIMEOUT_MS = 15000UL;
constexpr uint32_t WIFI_RECOVERY_RETRY_MS = 30000UL;
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
  SoftApFailed = 8,
  RecoveryModeAp = 9,
  RecoveryModeApDone = 10,
  RecoveryApOk = 11,
  RecoveryApFailed = 12
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
bool automaticRecoveryMode = false;
uint32_t recoveryLastStartAttemptMs = 0;
bool setupCredentialsSavedPendingRestart = false;
uint32_t storedCredentialsRevision = 0;
uint32_t activeCredentialsConnectStartedMs = 0;
bool activeCredentialsGotIp = false;
uint8_t activeCredentialsDisconnects = 0;
uint8_t activeConnectionProfile = 0;
uint8_t fallbackConnectionProfile = 0;
bool fallbackConnectionStarted = false;

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
    case SetupApDiagPhase::RecoveryModeAp:
      return "vor WiFi.mode(WIFI_AP) (Recovery)";
    case SetupApDiagPhase::RecoveryModeApDone:
      return "WiFi.mode(WIFI_AP) abgeschlossen (Recovery)";
    case SetupApDiagPhase::RecoveryApOk:
      return "Recovery-AP erfolgreich gestartet";
    case SetupApDiagPhase::RecoveryApFailed:
      return "Recovery-AP-Start fehlgeschlagen";
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
  recoveryLastStartAttemptMs = millis();

  // Im automatischen Recovery-Fall ist keine funktionierende STA-Verbindung
  // vorhanden. WIFI_AP beendet den laufenden STA-Verbindungsversuch vollstaendig
  // und verhindert, dass weitere Scans/Reconnects die AP-Beacons stoeren.
  // Der manuell gestartete Setup-AP bleibt dagegen bewusst WIFI_AP_STA.
  markSetupApDiag(SetupApDiagPhase::RecoveryModeAp, true);
  WiFi.mode(WIFI_AP);
  markSetupApDiag(SetupApDiagPhase::RecoveryModeApDone, true);
  const bool ok = WiFi.softAP(WIFI_SETUP_AP_SSID);
  markSetupApDiag(ok ? SetupApDiagPhase::RecoveryApOk : SetupApDiagPhase::RecoveryApFailed, false);
  Serial.printf("[T4S3][WiFi] Recovery AP: %s\n", ok ? "active" : "failed");
  setupApActive = ok;
  // Stay in recovery state even when softAP fails. Otherwise each loop
  // would immediately repeat WiFi.mode() and softAP(), potentially causing
  // a restart loop. Retry at a controlled interval instead.
  return ok;
}

bool profileAvailableForAutomaticConnection(uint8_t profile)
{
  if (!profileNumberValid(profile)) {
    return false;
  }
  if (!storedCredentialsKnown) {
    refreshStoredCredentialsCache();
  }
  return automaticConnectionEnabled && profileAvailable[profile - 1];
}

bool beginProfileConnection(uint8_t profile)
{
  CoffeeWifiCredentials credentials;
  if (!profileAvailableForAutomaticConnection(profile) ||
      !readProfileCredentials(profile, credentials)) {
    return false;
  }

  activeCredentials = credentials;
  activeCredentialsLoaded = true;
  activeConnectionProfile = profile;
  resetActiveConnectionAttempt();
  WiFi.begin(activeCredentials.ssid.c_str(), activeCredentials.password.c_str());
  Serial.printf("[T4S3][WiFi] connecting profile %u: SSID='%s'\n", profile, activeCredentials.ssid.c_str());
  return true;
}

void advanceAutomaticConnectionCycle()
{
  if (!fallbackConnectionStarted && fallbackConnectionProfile != 0) {
    fallbackConnectionStarted = true;
    if (beginProfileConnection(fallbackConnectionProfile)) {
      return;
    }
  }

  enterAutomaticRecoveryMode();
}

void beginAutomaticConnectionCycle(uint8_t firstProfile = 0)
{
  if (!storedCredentialsKnown) {
    refreshStoredCredentialsCache();
  }

  automaticRecoveryMode = false;
  fallbackConnectionProfile = 0;
  fallbackConnectionStarted = false;

  if (!automaticConnectionEnabled) {
    activeConnectionProfile = 0;
    enterAutomaticRecoveryMode();
    return;
  }

  uint8_t selectedFirstProfile = 0;
  if (profileAvailableForAutomaticConnection(firstProfile)) {
    selectedFirstProfile = firstProfile;
  } else if (profileAvailableForAutomaticConnection(preferredProfile)) {
    selectedFirstProfile = preferredProfile;
  } else {
    const uint8_t otherProfile = preferredProfile == 1 ? 2 : 1;
    if (profileAvailableForAutomaticConnection(otherProfile)) {
      selectedFirstProfile = otherProfile;
    }
  }

  if (selectedFirstProfile == 0) {
    activeConnectionProfile = 0;
    enterAutomaticRecoveryMode();
    return;
  }

  const uint8_t otherProfile = selectedFirstProfile == 1 ? 2 : 1;
  if (profileAvailableForAutomaticConnection(otherProfile)) {
    fallbackConnectionProfile = otherProfile;
  }

  if (!beginProfileConnection(selectedFirstProfile)) {
    advanceAutomaticConnectionCycle();
  }
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
  // WiFi sleep is configured once in normal STA mode. Do not call
  // WiFi.setSleep(false) directly after switching to WIFI_AP_STA.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  automaticRecoveryMode = false;
  beginAutomaticConnectionCycle();
}

void coffeeWifiLoop()
{
  if (automaticRecoveryMode) {
    if (!setupApActive && millis() - recoveryLastStartAttemptMs >= WIFI_RECOVERY_RETRY_MS) {
      enterAutomaticRecoveryMode();
    }
    return;
  }
  if (activeCredentialsGotIp || activeConnectionProfile == 0) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    coffeeWifiMarkConnected();
    return;
  }

  if (millis() - activeCredentialsConnectStartedMs >= WIFI_PROFILE_CONNECT_TIMEOUT_MS) {
    advanceAutomaticConnectionCycle();
  }
}

void coffeeWifiMarkConnected()
{
  activeCredentialsGotIp = true;
  activeCredentialsDisconnects = 0;
}

void coffeeWifiReconnect()
{
  if (automaticRecoveryMode) {
    return;
  }

  activeCredentialsDisconnects++;

  // Nach einer zuvor erfolgreichen Verbindung beginnt ein neuer Zyklus mit
  // dem zuletzt verwendeten Profil. Waehrend eines laufenden Verbindungs-
  // versuchs steuert coffeeWifiLoop() Timeout und Profilwechsel.
  if (activeCredentialsGotIp) {
    beginAutomaticConnectionCycle(activeConnectionProfile);
    return;
  }

  // Waehrend des laufenden 15-s-Fensters das aktuelle Profil wie bisher nach
  // einem Disconnect erneut anstossen. Der Profilwechsel selbst bleibt allein
  // Aufgabe von coffeeWifiLoop(), damit der Timeout deterministisch bleibt.
  if (activeConnectionProfile != 0 &&
      millis() - activeCredentialsConnectStartedMs < WIFI_PROFILE_CONNECT_TIMEOUT_MS) {
    WiFi.begin(activeCredentials.ssid.c_str(), activeCredentials.password.c_str());
  }
}

String coffeeWifiCurrentSsid()
{
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.SSID();
  }
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
  // A running automatic recovery AP already serves the setup page. Do not
  // switch back to AP+STA unless recovery has been explicitly stopped.
  if (setupApActive) {
    return true;
  }

  setupCredentialsSavedPendingRestart = false;
  markSetupApDiag(SetupApDiagPhase::Requested, true);
  markSetupApDiag(SetupApDiagPhase::ModeApSta, true);
  WiFi.mode(WIFI_AP_STA);
  markSetupApDiag(SetupApDiagPhase::ModeApStaDone, true);

  // Keep the tested T4-S3 fix: never call WiFi.setSleep(false) here.
  markSetupApDiag(SetupApDiagPhase::SoftApStart, true);
  const bool ok = WiFi.softAP(WIFI_SETUP_AP_SSID);
  setupApActive = ok;
  markSetupApDiag(ok ? SetupApDiagPhase::SoftApOk : SetupApDiagPhase::SoftApFailed, false);
  return ok;
}

bool coffeeWifiStopSetupAp()
{
  const bool wasAutomaticRecovery = automaticRecoveryMode;
  WiFi.softAPdisconnect(true);
  delay(50);
  setupApActive = false;
  automaticRecoveryMode = false;
  WiFi.mode(WIFI_STA);

  // Explicit recovery retry without reboot. If automatic connection is
  // disabled, this returns straight to AP-only recovery.
  if (wasAutomaticRecovery) {
    beginAutomaticConnectionCycle();
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
