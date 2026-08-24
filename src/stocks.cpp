#include "stocks.h"
#include "settings.h"
#include "time_manager.h"
#include "network.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static uint32_t next_attempt;
static String display_text;
static String updated_text;
static SemaphoreHandle_t display_mutex;

struct NetworkGuard {
  NetworkGuard() { network_lock(); }
  ~NetworkGuard() { network_unlock(); }
};

static bool fetch_quote(const String &input, String &line) {
  String symbol = input;
  symbol.trim();
  if (symbol.isEmpty()) return false;
  String query = symbol;
  query.toUpperCase();

  WiFiClientSecure client;
  NetworkGuard guard;
  client.setInsecure();
  HTTPClient http;
  String url = "https://query1.finance.yahoo.com/v8/finance/chart/" + query + "?range=1d&interval=1m";
  if (!http.begin(client, url)) return false;
  http.addHeader("User-Agent", "Mozilla/5.0");
  int responseCode = http.GET();
  if (responseCode != HTTP_CODE_OK) {
    Serial.printf("stock %s HTTP %d\n", symbol.c_str(), responseCode);
    http.end();
    return false;
  }
  String payload = http.getString();
  http.end();

  JsonDocument document;
  DeserializationError error = deserializeJson(document, payload);
  if (error) {
    Serial.printf("stock %s JSON error: %s\n", symbol.c_str(), error.c_str());
    return false;
  }
  JsonVariant price = document["chart"]["result"][0]["meta"]["regularMarketPrice"];
  if (price.isNull()) {
    JsonArray closes = document["chart"]["result"][0]["indicators"]["quote"][0]["close"];
    for (int index = closes.size() - 1; index >= 0; index--) {
      if (!closes[index].isNull()) {
        price = closes[index];
        break;
      }
    }
  }
  if (price.isNull()) return false;
  float current = price.as<float>();
  JsonVariant extendedPrice = document["chart"]["result"][0]["meta"]["postMarketPrice"];
  if (extendedPrice.isNull()) extendedPrice = document["chart"]["result"][0]["meta"]["preMarketPrice"];
  char extended[16] = "--";
  if (!extendedPrice.isNull()) snprintf(extended, sizeof(extended), "%.2f", extendedPrice.as<float>());
  JsonArray opens = document["chart"]["result"][0]["indicators"]["quote"][0]["open"];
  float start = 0;
  for (size_t index = 0; index < opens.size(); index++) {
    if (!opens[index].isNull()) {
      start = opens[index].as<float>();
      break;
    }
  }
  if (start > 0) {
    float delta = current - start;
    float percent = delta / start * 100.0f;
    char formatted[96];
    snprintf(formatted, sizeof(formatted), "%-6s %8.2f %8.2f %+8.2f %+7.2f%% %8s",
         symbol.c_str(), start, current, delta, percent,
         extended);
    line = formatted;
  } else {
    char formatted[96];
    snprintf(formatted, sizeof(formatted), "%-6s %8.2f %8.2f     --     -- %8s",
         symbol.c_str(), 0.0f, current,
         extended);
    line = formatted;
  }
  return true;
}

static bool fetch_stocks() {
  if (WiFi.status() != WL_CONNECTED) return false;
  String symbols = g_settings.stockSymbols;
  String result = "Symbol Open     Last    Delta      %      Ext";
  int start = 0;
  int shown = 0;
  while (start < symbols.length() && shown < 4) {
    int end = symbols.indexOf(',', start);
    if (end < 0) end = symbols.length();
    String line;
    if (fetch_quote(symbols.substring(start, end), line)) {
      if (result.length()) result += "\n";
      result += line;
      shown++;
    }
    start = end + 1;
  }
  if (shown == 0) return false;
  if (xSemaphoreTake(display_mutex, portMAX_DELAY) == pdTRUE) {
    display_text = result;
    xSemaphoreGive(display_mutex);
  }
  struct tm currentTime;
  if (time_manager_now(currentTime)) {
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "Updated %I:%M:%S %p", &currentTime);
    if (xSemaphoreTake(display_mutex, portMAX_DELAY) == pdTRUE) {
      updated_text = timestamp;
      xSemaphoreGive(display_mutex);
    }
  } else {
    if (xSemaphoreTake(display_mutex, portMAX_DELAY) == pdTRUE) {
      updated_text = "Updated just now";
      xSemaphoreGive(display_mutex);
    }
  }
  Serial.printf("stocks updated: %d quotes\n", shown);
  return true;
}

static void stock_task(void *) {
  for (;;) {
    if (!fetch_stocks()) {
      if (xSemaphoreTake(display_mutex, portMAX_DELAY) == pdTRUE) {
        display_text = "Stock feed unavailable";
        updated_text = "";
        xSemaphoreGive(display_mutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}
void stocks_begin() {
  display_mutex = xSemaphoreCreateMutex();
  if (!display_mutex) return;
  display_text = "Waiting for stock data...";
  updated_text = "";
  xTaskCreatePinnedToCore(stock_task, "stock_fetch", 8192, nullptr, 1, nullptr, 0);
}
void stocks_tick() {}
String stocks_display() {
  String copy;
  if (display_mutex && xSemaphoreTake(display_mutex, portMAX_DELAY) == pdTRUE) {
    copy = display_text;
    xSemaphoreGive(display_mutex);
  }
  return copy;
}
String stocks_updated_display() {
  String copy;
  if (display_mutex && xSemaphoreTake(display_mutex, portMAX_DELAY) == pdTRUE) {
    copy = updated_text;
    xSemaphoreGive(display_mutex);
  }
  return copy;
}