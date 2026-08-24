#include "webconfig.h"
#include "settings.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

static WebServer server(80);
static DNSServer dns;
static bool running = false, ap = false, saved = false;
static String ap_name;

struct TimeZoneOption {
  const char *label;
  const char *value;
};

static const TimeZoneOption time_zone_options[] = {
  { "UTC", "UTC0" },
  { "UK / London", "GMT0BST,M3.5.0/1,M10.5.0" },
  { "Central Europe", "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Eastern Europe", "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "US Eastern", "EST5EDT,M3.2.0,M11.1.0" },
  { "US Central", "CST6CDT,M3.2.0,M11.1.0" },
  { "US Mountain", "MST7MDT,M3.2.0,M11.1.0" },
  { "US Pacific", "PST8PDT,M3.2.0,M11.1.0" },
  { "Arizona", "MST7" },
  { "Hawaii", "HST10" },
  { "Japan", "JST-9" },
  { "China / Hong Kong", "CST-8" },
  { "India", "IST-5:30" },
  { "Sydney / Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3" },
  { "New Zealand", "NZST-12NZDT,M9.5.0,M4.1.0/3" }
};
static constexpr size_t time_zone_option_count = sizeof(time_zone_options) / sizeof(time_zone_options[0]);

static String html_escape(const String &value) {
  String out;
  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if (c == '&') out += "&amp;"; else if (c == '<') out += "&lt;"; else if (c == '"') out += "&quot;"; else out += c;
  }
  return out;
}
static void root() {
  String page = "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'><title>Dashboard</title>";
  page += "<style>body{font:16px sans-serif;max-width:520px;margin:24px auto;padding:0 16px;background:#101820;color:#eee}input,select{box-sizing:border-box;width:100%;padding:10px;margin:5px 0 14px;background:#202c35;color:#fff;border:1px solid #52606b;border-radius:4px}button{padding:12px;width:100%;background:#16b8a6;color:#fff;border:0;border-radius:4px}</style>";
  page += "<h1>Dashboard</h1><form method=post action=/save>Wi-Fi SSID<input name=ssid required value='" + html_escape(g_settings.wifiSsid) + "'>Password<input name=pass type=password placeholder='leave blank to keep current'>Latitude<input name=lat type=number step=any min=-90 max=90 required value='" + String(g_settings.latitude, 4) + "'>Longitude<input name=lon type=number step=any min=-180 max=180 required value='" + String(g_settings.longitude, 4) + "'>Time zone<select name=tzsel>";
  bool time_zone_matched = false;
  for (size_t option = 0; option < time_zone_option_count; option++) {
    bool selected = g_settings.tz == time_zone_options[option].value;
    time_zone_matched = time_zone_matched || selected;
    page += "<option value='" + String(time_zone_options[option].value) + "'" + (selected ? " selected" : "") + ">" + time_zone_options[option].label + "</option>";
  }
  page += "</select><input name=tzcustom placeholder='Custom POSIX TZ string' value='" + (time_zone_matched ? "" : html_escape(g_settings.tz)) + "'>Stocks<input name=stocks value='" + html_escape(g_settings.stockSymbols) + "' placeholder='AAPL,MSFT,GOOGL'><small>Comma-separated Yahoo Finance symbols, for example AAPL,MSFT or 7203.T</small>Units<select name=units><option value=0" + String(g_settings.units == UNITS_METRIC ? " selected" : "") + ">Metric (C, km/h, hPa)</option><option value=1" + String(g_settings.units == UNITS_IMPERIAL ? " selected" : "") + ">Imperial (F, mph, inHg)</option></select><button>Save and restart</button></form>";
  server.send(200, "text/html", page);
}
static void save() {
  String ssid = server.arg("ssid"); ssid.trim();
  float lat = server.arg("lat").toFloat(), lon = server.arg("lon").toFloat();
  if (ssid.isEmpty() || lat < -90 || lat > 90 || lon < -180 || lon > 180) { server.send(400, "text/plain", "Invalid settings"); return; }
  g_settings.wifiSsid = ssid;
  if (server.arg("pass").length()) g_settings.wifiPass = server.arg("pass");
  g_settings.latitude = lat; g_settings.longitude = lon;
  String customTimeZone = server.arg("tzcustom"); customTimeZone.trim();
  g_settings.tz = customTimeZone.length() ? customTimeZone : server.arg("tzsel");
  g_settings.stockSymbols = server.arg("stocks"); g_settings.stockSymbols.trim();
  if (g_settings.stockSymbols.isEmpty()) g_settings.stockSymbols = "AAPL,MSFT,GOOGL";
  g_settings.units = server.arg("units").toInt() ? UNITS_IMPERIAL : UNITS_METRIC;
  settings_save(); saved = true;
  server.send(200, "text/html", "<h1>Saved</h1><p>Restarting...</p>");
}
static void not_found() { server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true); server.send(302, "text/plain", ""); }
void webconfig_begin() {
  if (running) return;
  WiFi.mode(WIFI_AP_STA);
  ap_name = "hub-" + String((uint32_t)(ESP.getEfuseMac() & 0xffff), HEX);
  ap = WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0);
  if (ap) {
    WiFi.softAP(ap_name.c_str());
    dns.start(53, "*", WiFi.softAPIP());
    Serial.printf("Setup AP: SSID=%s IP=%s\n", ap_name.c_str(), WiFi.softAPIP().toString().c_str());
  }
  server.on("/", HTTP_GET, root); server.on("/save", HTTP_POST, save); server.onNotFound(not_found); server.begin(); running = true;
}
void webconfig_tick() { if (!running) return; if (ap) dns.processNextRequest(); server.handleClient(); }
bool webconfig_saved() { return saved; }
bool webconfig_is_ap() { return ap; }
String webconfig_ip() { return ap ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }
String webconfig_ap_ssid() { return ap_name; }
