#include "internet_ip.h"
#include "network.h"
#include <WiFi.h>
#include <HTTPClient.h>

static String public_ip = "checking...";

static void internet_ip_task(void *) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      network_lock();
      HTTPClient http;
      if (http.begin("http://api.ipify.org")) {
        int response = http.GET();
        if (response == HTTP_CODE_OK) {
          String value = http.getString();
          value.trim();
          if (value.length()) {
            public_ip = value;
          }
        }
        http.end();
      }
      network_unlock();
    }
    vTaskDelay(pdMS_TO_TICKS(3600000));
  }
}

void internet_ip_begin() {
  xTaskCreatePinnedToCore(internet_ip_task, "internet_ip", 4096, nullptr, 1, nullptr, 0);
}

String internet_ip_display() {
  network_lock();
  String value = public_ip;
  network_unlock();
  return value;
}
