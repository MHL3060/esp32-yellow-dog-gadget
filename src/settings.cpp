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
  if (prefs.isKey("stocks")) g_settings.stockSymbols = prefs.getString("stocks");
  if (prefs.isKey("ssh_host")) g_settings.sshHost = prefs.getString("ssh_host");
  if (prefs.isKey("ssh_port")) g_settings.sshPort = prefs.getUShort("ssh_port");
  if (prefs.isKey("ssh_user")) g_settings.sshUser = prefs.getString("ssh_user");
  if (prefs.isKey("ssh_pass")) g_settings.sshPassword = prefs.getString("ssh_pass");
  if (prefs.isKey("ic_email")) g_settings.icloudEmail = prefs.getString("ic_email");
  if (prefs.isKey("ic_pass")) g_settings.icloudAppPassword = prefs.getString("ic_pass");
  if (prefs.isKey("ic_cal_url")) g_settings.icloudCalendarUrl = prefs.getString("ic_cal_url");
  if (prefs.isKey("brightness")) g_settings.brightness = prefs.getUChar("brightness");
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
  prefs.putString("stocks", g_settings.stockSymbols);
  prefs.putString("ssh_host", g_settings.sshHost);
  prefs.putUShort("ssh_port", g_settings.sshPort);
  prefs.putString("ssh_user", g_settings.sshUser);
  prefs.putString("ssh_pass", g_settings.sshPassword);
  prefs.putString("ic_email", g_settings.icloudEmail);
  prefs.putString("ic_pass", g_settings.icloudAppPassword);
  prefs.putString("ic_cal_url", g_settings.icloudCalendarUrl);
  prefs.putUChar("brightness", g_settings.brightness);
  prefs.putUChar("units", g_settings.units);
  prefs.putBool("ok", true);
  prefs.end();
  g_settings.provisioned = true;
  return true;
}
