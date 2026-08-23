#pragma once
#include "settings.h"
static inline bool imperial() { return g_settings.units == UNITS_IMPERIAL; }
static inline float displayTemp(float c) { return imperial() ? c * 9.0f / 5.0f + 32.0f : c; }
static inline float displayWind(float kph) { return imperial() ? kph * 0.621371f : kph; }
static inline float displayPressure(float hpa) { return imperial() ? hpa * 0.02953f : hpa; }
static inline const char *tempUnit() { return imperial() ? "F" : "C"; }
static inline const char *windUnit() { return imperial() ? "mph" : "km/h"; }
static inline const char *pressureUnit() { return imperial() ? "inHg" : "hPa"; }
