#include "airquality.h"
#include "settings.h"
#include "app_data.h"
#include "network.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <math.h>

static uint32_t nextAttempt;
struct NetworkGuard {
  NetworkGuard() { network_lock(); }
  ~NetworkGuard() { network_unlock(); }
};
static bool fetch_aqi() {
  NetworkGuard guard;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  String url = "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=" + String(g_settings.latitude, 4) +
    "&longitude=" + String(g_settings.longitude, 4) + "&current=us_aqi,pm2_5&timezone=auto";
  if (!http.begin(client, url) || http.GET() != HTTP_CODE_OK) { http.end(); return false; }
  String payload = http.getString(); http.end();
  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  JsonObject cur = doc["current"];
  if (cur.isNull() || cur["us_aqi"].isNull()) return false;
  g_data.aqi = cur["us_aqi"] | g_data.aqi;
  g_data.pm25 = (int)lroundf(cur["pm2_5"] | (float)g_data.pm25);
  g_data.aqiUpdatedAt = (uint32_t)time(nullptr); g_data.aqiValid = true;
  return true;
}
void airquality_begin() { nextAttempt = millis() + 35000; }
void airquality_tick() {
  if ((int32_t)(millis() - nextAttempt) < 0 || WiFi.status() != WL_CONNECTED) return;
  nextAttempt = millis() + (fetch_aqi() ? 1800000UL : 60000UL);
}
