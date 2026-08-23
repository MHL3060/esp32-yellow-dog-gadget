#include "weather_ui.h"
#include "app_data.h"
#include "settings.h"
#include "units.h"
#include "time_manager.h"
#include "sun_moon.h"
#include "webconfig.h"
#include <lvgl.h>
#include <WiFi.h>
#include <time.h>
#include <stdio.h>

static lv_obj_t *panel, *heading, *content, *footer;
static int scene = 0;
static bool pinned = false;
static uint32_t last_change;
static lv_style_t style_bg, style_heading, style_content, style_footer;

static void set_label(lv_obj_t *obj, const char *text, const lv_style_t *style) {
  lv_label_set_text(obj, text); lv_obj_add_style(obj, style, 0);
}
static const char *condition(int code) {
  if (code == 0) return "Clear"; if (code <= 2) return "Partly cloudy"; if (code == 3) return "Cloudy";
  if (code == 45 || code == 48) return "Fog"; if (code >= 51 && code <= 67 || code >= 80 && code <= 82) return "Rain";
  if (code >= 71 && code <= 86) return "Snow"; if (code >= 95) return "Thunderstorm"; return "Cloudy";
}
static void touch_event(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) { if (!pinned) scene = (scene + 1) % 4; last_change = millis(); }
  if (lv_event_get_code(event) == LV_EVENT_LONG_PRESSED) { pinned = !pinned; last_change = millis(); }
}
static void draw_clock() {
  struct tm t; char text[256];
  if (time_manager_now(t)) strftime(text, sizeof(text), "%H:%M:%S\n%A, %d %B %Y", &t);
  else snprintf(text, sizeof(text), "--:--:--\nWaiting for network time");
  set_label(heading, "CLOCK", &style_heading); set_label(content, text, &style_content);
}
static void draw_weather() {
  char text[512];
  if (!g_data.weatherValid) snprintf(text, sizeof(text), "Waiting for weather data...\n\nOpen the setup page to configure Wi-Fi and location.");
  else snprintf(text, sizeof(text), "%s\n\n%.1f deg %s   feels %.1f deg %s\nHigh %.1f / Low %.1f\n\nCloud %d%%   Humidity %d%%\nWind %.1f %s   Pressure %.1f %s\nUV index %.1f", condition(g_data.weatherCode), displayTemp(g_data.tempC), tempUnit(), displayTemp(g_data.feelsLikeC), tempUnit(), displayTemp(g_data.tempMaxC), displayTemp(g_data.tempMinC), g_data.cloudCoverPct, g_data.humidityPct, displayWind(g_data.windKph), windUnit(), displayPressure(g_data.pressureHpa), pressureUnit(), g_data.uvIndex);
  set_label(heading, "WEATHER", &style_heading); set_label(content, text, &style_content);
}
static void format_event(char *out, size_t size, time_t value) {
  if (!value) { snprintf(out, size, "--:--"); return; }
  strftime(out, size, "%H:%M", localtime(&value));
}
static void draw_sun() {
  char rise[8], set[8], rise2[8], set2[8], text[320];
  format_event(rise, sizeof(rise), g_data.sunriseToday); format_event(set, sizeof(set), g_data.sunsetToday);
  format_event(rise2, sizeof(rise2), g_data.sunriseTomorrow); format_event(set2, sizeof(set2), g_data.sunsetTomorrow);
  snprintf(text, sizeof(text), "SUN & MOON\n\nToday     sunrise %s   sunset %s\nTomorrow  sunrise %s   sunset %s\n\nMoon: %s\nIllumination: %.0f%%", rise, set, rise2, set2, moon_phase_name(g_data.moonPhase), g_data.moonIlluminationPct);
  set_label(heading, "SUN & MOON", &style_heading); set_label(content, text, &style_content);
}
static void draw_air() {
  char text[256];
  if (!g_data.aqiValid) snprintf(text, sizeof(text), "Waiting for air-quality data...");
  else snprintf(text, sizeof(text), "AIR QUALITY\n\nUS AQI: %d\nPM2.5: %d ug/m3\n\nPressure trend: %+.1f hPa", g_data.aqi, g_data.pm25, g_data.pressureTrend);
  set_label(heading, "AIR QUALITY", &style_heading); set_label(content, text, &style_content);
}
static void redraw() {
  if (scene == 0) draw_clock(); else if (scene == 1) draw_weather(); else if (scene == 2) draw_sun(); else draw_air();
  char status[160];
  snprintf(status, sizeof(status), "Wi-Fi: %s   %s   Scene %d/4   %s", WiFi.status() == WL_CONNECTED ? webconfig_ip().c_str() : "offline", webconfig_is_ap() ? webconfig_ap_ssid().c_str() : "", scene + 1, pinned ? "PINNED" : "tap next | hold pin");
  set_label(footer, status, &style_footer);
}
void weather_ui_begin() {
  lv_style_init(&style_bg); lv_style_set_bg_color(&style_bg, lv_color_hex(0x07151c));
  lv_style_init(&style_heading); lv_style_set_text_color(&style_heading, lv_color_hex(0x35d0c2)); lv_style_set_text_font(&style_heading, &lv_font_montserrat_32);
  lv_style_init(&style_content); lv_style_set_text_color(&style_content, lv_color_hex(0xf4f7f5)); lv_style_set_text_font(&style_content, &lv_font_montserrat_28); lv_style_set_text_align(&style_content, LV_TEXT_ALIGN_CENTER);
  lv_style_init(&style_footer); lv_style_set_text_color(&style_footer, lv_color_hex(0x91a8ad)); lv_style_set_text_font(&style_footer, &lv_font_montserrat_16);
  lv_obj_t *screen = lv_screen_active(); lv_obj_add_style(screen, &style_bg, 0);
  panel = lv_btn_create(screen); lv_obj_set_size(panel, 800, 480); lv_obj_center(panel); lv_obj_add_event_cb(panel, touch_event, LV_EVENT_ALL, NULL);
  lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(panel, 0, 0); lv_obj_set_style_shadow_width(panel, 0, 0);
  heading = lv_label_create(panel); lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 28);
  content = lv_label_create(panel); lv_obj_set_width(content, 760); lv_obj_align(content, LV_ALIGN_CENTER, 0, 8);
  footer = lv_label_create(panel); lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -16);
  last_change = millis(); redraw();
}
void weather_ui_tick() {
  static uint32_t last_draw = 0;
  if (millis() - last_draw < 1000) return;
  last_draw = millis();
  if (!pinned && millis() - last_change >= 15000) { scene = (scene + 1) % 4; last_change = millis(); }
  if (scene == 0 || scene == 1 || scene == 3) redraw();
}
