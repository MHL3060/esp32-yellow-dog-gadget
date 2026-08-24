#include "time_manager.h"
#include <Arduino.h>

void time_manager_begin(const char *tz) {
  configTzTime(tz, "pool.ntp.org", "time.nist.gov");
  struct tm currentTime = {};
  uint32_t started = millis();
  while (!getLocalTime(&currentTime, 1000) && millis() - started < 10000) {
    delay(100);
  }
  if (currentTime.tm_year >= (2024 - 1900)) {
    Serial.printf("NTP time synchronized: %04d-%02d-%02d %02d:%02d:%02d\n",
                  currentTime.tm_year + 1900, currentTime.tm_mon + 1, currentTime.tm_mday,
                  currentTime.tm_hour, currentTime.tm_min, currentTime.tm_sec);
  } else {
    Serial.println("NTP synchronization not available yet");
  }
}

bool time_manager_now(struct tm &out) {
  time_t currentTime = time(nullptr);
  if (currentTime < 1704067200) return false;
  return localtime_r(&currentTime, &out) != nullptr;
}
