#pragma once
#include <Arduino.h>
void webconfig_begin();
void webconfig_tick();
bool webconfig_saved();
bool webconfig_is_ap();
String webconfig_ip();
String webconfig_ap_ssid();
