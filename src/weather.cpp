#include "weather.h"
#include "settings.h"
#include "app_data.h"
#include "network.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

static uint32_t nextAttempt;
static uint32_t backoff;

struct NetworkGuard {
  NetworkGuard() { network_lock(); }
  ~NetworkGuard() { network_unlock(); }
};

static bool fetch_weather() {
  NetworkGuard guard;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(g_settings.latitude, 4) +
    "&longitude=" + String(g_settings.longitude, 4) +
    "&current=temperature_2m,apparent_temperature,weather_code,cloud_cover,relative_humidity_2m,wind_speed_10m,surface_pressure,uv_index"+
    "&daily=temperature_2m_max,temperature_2m_min&forecast_days=1&timezone=auto";
  if (!http.begin(client, url) || http.GET() != HTTP_CODE_OK) { http.end(); return false; }
  String payload = http.getString();
  http.end();
  JsonDocument filter;
  filter["current"]["temperature_2m"] = true;
  filter["current"]["apparent_temperature"] = true;
  filter["current"]["weather_code"] = true;
  filter["current"]["cloud_cover"] = true;
  filter["current"]["relative_humidity_2m"] = true;
  filter["current"]["wind_speed_10m"] = true;
  filter["current"]["surface_pressure"] = true;
  filter["current"]["uv_index"] = true;
  filter["daily"]["temperature_2m_max"] = true;
  filter["daily"]["temperature_2m_min"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, payload, DeserializationOption::Filter(filter))) return false;
  JsonObject cur = doc["current"];
  if (cur.isNull()) return false;
  g_data.tempC = cur["temperature_2m"] | g_data.tempC;
  g_data.feelsLikeC = cur["apparent_temperature"] | g_data.feelsLikeC;
  g_data.weatherCode = cur["weather_code"] | g_data.weatherCode;
  g_data.cloudCoverPct = cur["cloud_cover"] | g_data.cloudCoverPct;
  g_data.humidityPct = cur["relative_humidity_2m"] | g_data.humidityPct;
  g_data.windKph = cur["wind_speed_10m"] | g_data.windKph;
  float pressure = cur["surface_pressure"] | g_data.pressureHpa;
  g_data.pressureTrend = g_data.pressureHpa > 0 ? pressure - g_data.pressureHpa : 0;
  g_data.pressureHpa = pressure;
  g_data.uvIndex = cur["uv_index"] | g_data.uvIndex;
  JsonArray high = doc["daily"]["temperature_2m_max"];
  JsonArray low = doc["daily"]["temperature_2m_min"];
  if (high.size() && low.size()) { g_data.tempMaxC = high[0]; g_data.tempMinC = low[0]; g_data.dailyValid = true; }
  uint32_t now = (uint32_t)time(nullptr);
  g_data.weatherUpdatedAt = g_data.uvUpdatedAt = now;
  g_data.weatherValid = g_data.uvValid = true;
  Serial.printf("weather %.1fC code %d heap %u\n", g_data.tempC, g_data.weatherCode, ESP.getFreeHeap());
  return true;
}

void weather_begin() { nextAttempt = 0; backoff = 0; }
void weather_tick() {
  if ((int32_t)(millis() - nextAttempt) < 0 || WiFi.status() != WL_CONNECTED) return;
  if (fetch_weather()) { backoff = 0; nextAttempt = millis() + 900000UL; }
  else {
    backoff = backoff ? backoff * 2 : 1;
    if (backoff > 15) backoff = 15;
    nextAttempt = millis() + backoff * 60000UL;
  }
}
