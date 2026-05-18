#include "coffee_wifi.h"

#include <Preferences.h>
#include <WiFi.h>

#include "wifi_secrets.h"

namespace {
constexpr const char* WIFI_PREF_NAMESPACE = "coffee_wifi";
constexpr const char* WIFI_PREF_KEY_SSID = "ssid";
constexpr const char* WIFI_PREF_KEY_PASS = "pass";

CoffeeWifiCredentials activeCredentials;
bool activeCredentialsLoaded = false;

bool readStoredCredentials(CoffeeWifiCredentials& credentials)
{
  Preferences prefs;
  if (!prefs.begin(WIFI_PREF_NAMESPACE, true)) {
    return false;
  }

  String ssid = prefs.getString(WIFI_PREF_KEY_SSID, "");
  const String password = prefs.getString(WIFI_PREF_KEY_PASS, "");
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
  CoffeeWifiCredentials credentials;
  return readStoredCredentials(credentials);
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

  const size_t ssidBytes = prefs.putString(WIFI_PREF_KEY_SSID, trimmedSsid);
  const size_t passBytes = prefs.putString(WIFI_PREF_KEY_PASS, password);
  prefs.end();

  activeCredentials.ssid = trimmedSsid;
  activeCredentials.password = password;
  activeCredentials.fromPreferences = true;
  activeCredentials.source = CoffeeWifiCredentialSource::Preferences;
  activeCredentialsLoaded = true;

  return ssidBytes > 0 && (password.length() == 0 || passBytes > 0);
}

bool coffeeWifiClearCredentials()
{
  Preferences prefs;
  if (!prefs.begin(WIFI_PREF_NAMESPACE, false)) {
    return false;
  }

  const bool ssidRemoved = prefs.remove(WIFI_PREF_KEY_SSID);
  const bool passRemoved = prefs.remove(WIFI_PREF_KEY_PASS);
  prefs.end();

  activeCredentialsLoaded = false;
  activeCredentials = CoffeeWifiCredentials{};

  return ssidRemoved || passRemoved;
}

CoffeeWifiCredentialSource coffeeWifiCredentialSource()
{
  return ensureActiveCredentials().source;
}

const char* coffeeWifiCredentialSourceLabel()
{
  switch (coffeeWifiCredentialSource()) {
    case CoffeeWifiCredentialSource::Preferences:
      return "NVS/Preferences";
    case CoffeeWifiCredentialSource::WifiSecrets:
      return "wifi_secrets.h";
    case CoffeeWifiCredentialSource::None:
    default:
      return "none";
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
  WiFi.mode(WIFI_STA);
  WiFi.begin(credentials.ssid.c_str(), credentials.password.c_str());
}

void coffeeWifiReconnect()
{
  const CoffeeWifiCredentials& credentials = ensureActiveCredentials();
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
