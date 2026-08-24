#pragma once

#include <Arduino.h>

constexpr uint8_t UNITS_METRIC = 0;
constexpr uint8_t UNITS_IMPERIAL = 1;

struct Settings {
  String wifiSsid;
  String wifiPass;
  float latitude = 51.4779f;
  float longitude = -0.0015f;
  String tz = "UTC0";
  String stockSymbols = "AAPL,MSFT,GOOGL";
  String sshHost;
  uint16_t sshPort = 22;
  String sshUser;
  String sshPassword;
  uint8_t brightness = 255;
  uint8_t units = UNITS_METRIC;
  bool provisioned = false;
};

extern Settings g_settings;
void settings_begin();
bool settings_save();
