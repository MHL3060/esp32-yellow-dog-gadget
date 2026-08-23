#include "webconfig.h"
#include "settings.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

static WebServer server(80);
static DNSServer dns;
static bool running = false, ap = false, saved = false;
static String ap_name;

static String html_escape(const String &value) {
  String out;
  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if (c == '&') out += "&amp;"; else if (c == '<') out += "&lt;"; else if (c == '"') out += "&quot;"; else out += c;
  }
  return out;
}
static void root() {
  String page = "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'><title>Weather Clock</title>";
  page += "<style>body{font:16px sans-serif;max-width:520px;margin:24px auto;padding:0 16px;background:#101820;color:#eee}input,select{box-sizing:border-box;width:100%;padding:10px;margin:5px 0 14px;background:#202c35;color:#fff;border:1px solid #52606b;border-radius:4px}button{padding:12px;width:100%;background:#16b8a6;color:#fff;border:0;border-radius:4px}</style>";
  page += "<h1>Weather Clock</h1><form method=post action=/save>Wi-Fi SSID<input name=ssid required value='" + html_escape(g_settings.wifiSsid) + "'>Password<input name=pass type=password placeholder='leave blank to keep current'>Latitude<input name=lat type=number step=any min=-90 max=90 required value='" + String(g_settings.latitude, 4) + "'>Longitude<input name=lon type=number step=any min=-180 max=180 required value='" + String(g_settings.longitude, 4) + "'>POSIX time zone<input name=tz value='" + html_escape(g_settings.tz) + "'>Units<select name=units><option value=0" + String(g_settings.units == UNITS_METRIC ? " selected" : "") + ">Metric (C, km/h, hPa)</option><option value=1" + String(g_settings.units == UNITS_IMPERIAL ? " selected" : "") + ">Imperial (F, mph, inHg)</option></select><button>Save and restart</button></form>";
  server.send(200, "text/html", page);
}
static void save() {
  String ssid = server.arg("ssid"); ssid.trim();
  float lat = server.arg("lat").toFloat(), lon = server.arg("lon").toFloat();
  if (ssid.isEmpty() || lat < -90 || lat > 90 || lon < -180 || lon > 180) { server.send(400, "text/plain", "Invalid settings"); return; }
  g_settings.wifiSsid = ssid;
  if (server.arg("pass").length()) g_settings.wifiPass = server.arg("pass");
  g_settings.latitude = lat; g_settings.longitude = lon; g_settings.tz = server.arg("tz");
  g_settings.units = server.arg("units").toInt() ? UNITS_IMPERIAL : UNITS_METRIC;
  settings_save(); saved = true;
  server.send(200, "text/html", "<h1>Saved</h1><p>Restarting...</p>");
}
static void not_found() { server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true); server.send(302, "text/plain", ""); }
void webconfig_begin() {
  if (running) return;
  WiFi.mode(WIFI_AP_STA);
  ap_name = "WeatherClock-" + String((uint32_t)(ESP.getEfuseMac() & 0xffff), HEX);
  ap = WiFi.status() != WL_CONNECTED;
  if (ap) { WiFi.softAP(ap_name.c_str()); dns.start(53, "*", WiFi.softAPIP()); }
  server.on("/", HTTP_GET, root); server.on("/save", HTTP_POST, save); server.onNotFound(not_found); server.begin(); running = true;
}
void webconfig_tick() { if (!running) return; if (ap) dns.processNextRequest(); server.handleClient(); }
bool webconfig_saved() { return saved; }
bool webconfig_is_ap() { return ap; }
String webconfig_ip() { return ap ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }
String webconfig_ap_ssid() { return ap_name; }
