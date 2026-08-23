#pragma once

#include <Arduino.h>
#include <time.h>

struct AppData {
  float tempC = 0, feelsLikeC = 0, tempMaxC = 0, tempMinC = 0;
  int weatherCode = 0, cloudCoverPct = 0, humidityPct = 0;
  float windKph = 0, pressureHpa = 0, pressureTrend = 0, uvIndex = 0;
  uint32_t weatherUpdatedAt = 0, aqiUpdatedAt = 0, uvUpdatedAt = 0;
  bool weatherValid = false, dailyValid = false, aqiValid = false, uvValid = false;
  int aqi = 0, pm25 = 0;
  time_t sunriseToday = 0, sunsetToday = 0;
  time_t sunriseTomorrow = 0, sunsetTomorrow = 0;
  float moonPhase = 0, moonIlluminationPct = 0;
};

extern AppData g_data;
