#include "sun_moon.h"
#include "settings.h"
#include "app_data.h"
#include <Arduino.h>
#include <math.h>

static double rad(double value) { return value * M_PI / 180.0; }
static double julian(int year, int month, int day) {
  if (month <= 2) { year--; month += 12; }
  int a = year / 100; int b = 2 - a + a / 4;
  return floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) + day + b - 1524.5;
}
static time_t sun_event(int year, int month, int day, double elevation, bool morning) {
  double jd = julian(year, month, day), t = (jd - 2451545.0) / 36525.0;
  double l0 = fmod(280.46646 + t * (36000.76983 + .0003032 * t), 360.0);
  double m = 357.52911 + t * (35999.05029 - .0001537 * t), mr = rad(m);
  double e = .016708634 - t * (.000042037 + .0000001267 * t);
  double c = sin(mr) * (1.914602 - t * (.004817 + .000014 * t)) + sin(2 * mr) * (.019993 - .000101 * t) + sin(3 * mr) * .000289;
  double omega = 125.04 - 1934.136 * t, lambda = l0 + c - .00569 - .00478 * sin(rad(omega));
  double eps = 23.0 + (26.0 + (21.448 - t * (46.815 + t * (.00059 - t * .001813))) / 60.0) / 60.0 + .00256 * cos(rad(omega));
  double decl = asin(sin(rad(eps)) * sin(rad(lambda)));
  double y = tan(rad(eps / 2)); y *= y;
  double eq = 4 * (180 / M_PI) * (y * sin(2 * rad(l0)) - 2 * e * sin(mr) + 4 * e * y * sin(mr) * cos(2 * rad(l0)) - .5 * y * y * sin(4 * rad(l0)) - 1.25 * e * e * sin(2 * mr));
  double zenith = rad(90 - elevation), lat = rad(g_settings.latitude);
  double cosH = (cos(zenith) - sin(lat) * sin(decl)) / (cos(lat) * cos(decl));
  if (cosH > 1 || cosH < -1) return 0;
  double h = acos(cosH) * 180 / M_PI;
  double minutes = 720 - 4 * (g_settings.longitude + (morning ? h : -h)) - eq;
  return (time_t)llround((jd - 2440587.5) * 86400 + minutes * 60);
}
const char *moon_phase_name(float p) {
  if (p < .03f || p > .97f) return "New Moon";
  if (p < .22f) return "Waxing Crescent"; if (p < .28f) return "First Quarter";
  if (p < .47f) return "Waxing Gibbous"; if (p < .53f) return "Full Moon";
  if (p < .72f) return "Waning Gibbous"; if (p < .78f) return "Last Quarter";
  return "Waning Crescent";
}
void sunmoon_recompute() {
  struct tm local;
  if (!getLocalTime(&local, 10)) return;
  int y = local.tm_year + 1900, m = local.tm_mon + 1, d = local.tm_mday;
  g_data.sunriseToday = sun_event(y, m, d, -.833, true);
  g_data.sunsetToday = sun_event(y, m, d, -.833, false);
  local.tm_mday++; local.tm_hour = 12; local.tm_min = local.tm_sec = 0;
  time_t tomorrow = mktime(&local); struct tm next; localtime_r(&tomorrow, &next);
  g_data.sunriseTomorrow = sun_event(next.tm_year + 1900, next.tm_mon + 1, next.tm_mday, -.833, true);
  g_data.sunsetTomorrow = sun_event(next.tm_year + 1900, next.tm_mon + 1, next.tm_mday, -.833, false);
  double phase = (2440587.5 + (double)time(nullptr) / 86400.0 - 2451550.1) / 29.530588853;
  g_data.moonPhase = phase - floor(phase);
  g_data.moonIlluminationPct = (1 - cos(2 * M_PI * g_data.moonPhase)) * 50;
}
