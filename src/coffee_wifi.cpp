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
  prefs.begin(WIFI_PREF_NAMESPACE, true);
  const String ssid = prefs.getString(WIFI_PREF_KEY_SSID, "");
  const String password = prefs.getString(WIFI_PREF_KEY_PASS, "");
  prefs.end();

  if (ssid.length() == 0) {
    return false;
  }

  credentials.ssid = ssid;
  credentials.password = password;
  credentials.fromPreferences = true;
  return true;
}

CoffeeWifiCredentials fallbackCredentials()
{
  CoffeeWifiCredentials credentials;
  credentials.ssid = WIFI_SSID;
  credentials.password = WIFI_PASS;
  credentials.fromPreferences = false;
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
  const String trimmedSsid = ssid;
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
  activeCredentialsLoaded = true;

  return ssidBytes > 0 && (password.length() == 0 || passBytes > 0);
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
