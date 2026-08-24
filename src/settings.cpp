#include "settings.h"
#include <Preferences.h>

Settings g_settings;

void settings_begin() {
  Preferences prefs;
  if (!prefs.begin("weather", false)) return;
  if (prefs.isKey("ssid")) g_settings.wifiSsid = prefs.getString("ssid");
  if (prefs.isKey("pass")) g_settings.wifiPass = prefs.getString("pass");
  if (prefs.isKey("lat")) g_settings.latitude = prefs.getFloat("lat");
  if (prefs.isKey("lon")) g_settings.longitude = prefs.getFloat("lon");
  if (prefs.isKey("tz")) g_settings.tz = prefs.getString("tz");
  if (prefs.isKey("units")) g_settings.units = prefs.getUChar("units");
  if (prefs.isKey("ok")) g_settings.provisioned = prefs.getBool("ok");
  prefs.end();
}

bool settings_save() {
  Preferences prefs;
  if (!prefs.begin("weather", false)) return false;
  prefs.putString("ssid", g_settings.wifiSsid);
  prefs.putString("pass", g_settings.wifiPass);
  prefs.putFloat("lat", g_settings.latitude);
  prefs.putFloat("lon", g_settings.longitude);
  prefs.putString("tz", g_settings.tz);
  prefs.putUChar("units", g_settings.units);
  prefs.putBool("ok", true);
  prefs.end();
  g_settings.provisioned = true;
  return true;
}
