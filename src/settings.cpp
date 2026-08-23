#include "settings.h"
#include <Preferences.h>

Settings g_settings;

void settings_begin() {
  Preferences prefs;
  prefs.begin("weather", true);
  g_settings.wifiSsid = prefs.getString("ssid", "");
  g_settings.wifiPass = prefs.getString("pass", "");
  g_settings.latitude = prefs.getFloat("lat", g_settings.latitude);
  g_settings.longitude = prefs.getFloat("lon", g_settings.longitude);
  g_settings.tz = prefs.getString("tz", g_settings.tz);
  g_settings.units = prefs.getUChar("units", g_settings.units);
  g_settings.provisioned = prefs.getBool("ok", false);
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
