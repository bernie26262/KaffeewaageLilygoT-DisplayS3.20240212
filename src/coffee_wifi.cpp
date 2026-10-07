#include "coffee_wifi.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>


namespace {
constexpr const char* WIFI_PREF_NAMESPACE = "coffee_wifi";
// Legacy single-profile keys. Kept for one-time migration only.
constexpr const char* WIFI_PREF_KEY_SSID = "ssid";
constexpr const char* WIFI_PREF_KEY_PASS = "pass";
constexpr const char* WIFI_PREF_KEY_ACTIVE = "active";

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
uint32_t storedCredentialsRevision = 0;
uint32_t activeCredentialsConnectStartedMs = 0;
bool activeCredentialsGotIp = false;
uint8_t activeCredentialsDisconnects = 0;
uint8_t activeConnectionProfile = 0;
uint8_t fallbackConnectionProfile = 0;
bool fallbackConnectionStarted = false;

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

  // Existing single-profile installations become WLAN 1. The legacy keys are
  // retained as rollback safety for the pre-migration credentials.
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
  profileAvailable[0] = false;
  profileAvailable[1] = false;
  profileSsid[0] = String();
  profileSsid[1] = String();
  profilePasswordAvailable[0] = false;
  profilePasswordAvailable[1] = false;
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

  uint8_t currentPreferred = prefs.getUChar(WIFI_PREF_KEY_PREFERRED, 1);
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

// Compatibility wrappers for the current single-profile WebUI. Until the
// profile UI lands, its single credentials form edits WLAN 1.
bool coffeeWifiSaveCredentials(const String& ssid, const String& password)
{
  const bool saved = coffeeWifiSaveProfile(1, ssid, password);
  if (!saved) {
    return false;
  }

  // Preserve the current WebUI contract: saving alone does not activate the
  // newly entered credentials. The setup portal activates them explicitly.
  return coffeeWifiSetAutomaticConnectionEnabled(false);
}

bool coffeeWifiSetStoredCredentialsActive(bool active)
{
  if (active) {
    if (!coffeeWifiSetPreferredProfile(1)) {
      return false;
    }
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
  if (automaticRecoveryMode || activeCredentialsGotIp || activeConnectionProfile == 0) {
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

  // Wird ein automatischer Recovery-AP bewusst beendet, beginnt bei aktivierter
  // Automatik ein frischer Zyklus: bevorzugtes Profil, zweites Profil, Recovery.
  if (wasAutomaticRecovery && automaticConnectionEnabled) {
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
