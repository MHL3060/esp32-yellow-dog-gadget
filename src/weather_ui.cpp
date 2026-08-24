#include "weather_ui.h"
#include "app_data.h"
#include "settings.h"
#include "units.h"
#include "time_manager.h"
#include "sun_moon.h"
#include "webconfig.h"
#include "stocks.h"
#include "internet_ip.h"
#include <lvgl.h>
#include <WiFi.h>
#include <time.h>
#include <stdio.h>

static lv_obj_t *clock_time, *clock_date, *weather_icon, *weather_content, *air_content, *stock_content, *footer;
static lv_obj_t *stock_cells[5][6];
static lv_obj_t *stock_header_bold[6];
static lv_obj_t *dashboard_panel, *stock_panel;
static uint32_t last_draw;
static char previous_clock_time[32] = "";
static char previous_clock_date[64] = "";
static char previous_weather[512] = "";
static char previous_air[256] = "";
static char previous_stocks[256] = "";
static char previous_footer[160] = "";
static lv_style_t style_bg, style_heading, style_content, style_clock, style_footer;
static lv_style_t style_panel, style_small_heading, style_small_content;
static lv_style_t style_sun, style_cloud, style_rain;
static lv_style_t style_stock;
static lv_style_t style_stock_updated;
static lv_style_t style_tab, style_tab_active;

static void select_tab(int tab) {
  if (tab == 0) {
    lv_obj_clear_flag(dashboard_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(stock_panel, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(dashboard_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(stock_panel, LV_OBJ_FLAG_HIDDEN);
  }
}
static void tab_event(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    select_tab((int)(intptr_t)lv_event_get_user_data(event));
  }
}

static void set_label(lv_obj_t *obj, char *previous, size_t previous_size, const char *text, const lv_style_t *style) {
  if (strcmp(previous, text) == 0) return;
  lv_label_set_text(obj, text); lv_obj_add_style(obj, style, 0);
  lv_obj_invalidate(obj);
  strncpy(previous, text, previous_size - 1);
  previous[previous_size - 1] = '\0';
}
static const char *condition(int code) {
  if (code == 0) return "Clear"; if (code <= 2) return "Partly cloudy"; if (code == 3) return "Cloudy";
  if (code == 45 || code == 48) return "Fog"; if (code >= 51 && code <= 67 || code >= 80 && code <= 82) return "Rain";
  if (code >= 71 && code <= 86) return "Snow"; if (code >= 95) return "Thunderstorm"; return "Cloudy";
}
static void add_icon_part(lv_obj_t *parent, int x, int y, int width, int height, int radius, lv_style_t *style) {
  lv_obj_t *part = lv_obj_create(parent);
  lv_obj_remove_style_all(part);
  lv_obj_set_size(part, width, height);
  lv_obj_set_pos(part, x, y);
  lv_obj_set_style_radius(part, radius, 0);
  lv_obj_add_style(part, style, 0);
}
static void draw_weather_icon(int code) {
  lv_obj_clean(weather_icon);
  if (code == 0) {
    add_icon_part(weather_icon, 14, 14, 36, 36, LV_RADIUS_CIRCLE, &style_sun);
    add_icon_part(weather_icon, 30, 1, 5, 11, 2, &style_sun);
    add_icon_part(weather_icon, 30, 52, 5, 11, 2, &style_sun);
    add_icon_part(weather_icon, 1, 30, 11, 5, 2, &style_sun);
    add_icon_part(weather_icon, 53, 30, 11, 5, 2, &style_sun);
  } else {
    add_icon_part(weather_icon, 8, 28, 48, 24, 10, &style_cloud);
    add_icon_part(weather_icon, 20, 15, 28, 28, LV_RADIUS_CIRCLE, &style_cloud);
    if (code >= 51 && code <= 67 || code >= 80 && code <= 82 || code >= 95) {
      add_icon_part(weather_icon, 15, 54, 4, 9, 1, &style_rain);
      add_icon_part(weather_icon, 30, 54, 4, 9, 1, &style_rain);
      add_icon_part(weather_icon, 45, 54, 4, 9, 1, &style_rain);
    }
  }
}
static void draw_clock() {
  struct tm t; char time_text[32], date_text[64];
  static const char *weekdays[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
  if (time_manager_now(t)) {
    strftime(time_text, sizeof(time_text), "%I:%M:%S %p", &t);
    snprintf(date_text, sizeof(date_text), "%s %04d/%02d/%02d",
             weekdays[t.tm_wday], t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  } else if (WiFi.status() != WL_CONNECTED) {
    unsigned long elapsed = millis() / 1000;
    snprintf(time_text, sizeof(time_text), "%02lu:%02lu:%02lu",
             elapsed / 3600, (elapsed / 60) % 60, elapsed % 60);
    snprintf(date_text, sizeof(date_text), "Wi-Fi offline | uptime");
  } else {
    snprintf(time_text, sizeof(time_text), "--:--:--");
    snprintf(date_text, sizeof(date_text), "Waiting for network time");
  }
  set_label(clock_time, previous_clock_time, sizeof(previous_clock_time), time_text, &style_clock);
  set_label(clock_date, previous_clock_date, sizeof(previous_clock_date), date_text, &style_content);
}
static void draw_weather() {
  char text[512];
  if (!g_data.weatherValid) snprintf(text, sizeof(text), "Waiting for weather data...");
  else snprintf(text, sizeof(text), "%.1f %s  feels %.1f %s\nH %.1f / L %.1f %s\nHumidity %d%%  Wind %.1f %s", displayTemp(g_data.tempC), tempUnit(), displayTemp(g_data.feelsLikeC), tempUnit(), displayTemp(g_data.tempMaxC), displayTemp(g_data.tempMinC), tempUnit(), g_data.humidityPct, displayWind(g_data.windKph), windUnit());
  static int previous_code = -1;
  if (g_data.weatherValid && g_data.weatherCode != previous_code) {
    draw_weather_icon(g_data.weatherCode);
    previous_code = g_data.weatherCode;
  } else if (!g_data.weatherValid && previous_code != -2) {
    draw_weather_icon(1);
    previous_code = -2;
  }
  set_label(weather_content, previous_weather, sizeof(previous_weather), text, &style_small_content);
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
  set_label(weather_content, previous_weather, sizeof(previous_weather), text, &style_small_content);
}
static void draw_air() {
  char text[256];
  if (!g_data.aqiValid) snprintf(text, sizeof(text), "Waiting for air-quality data...");
  else snprintf(text, sizeof(text), "AQI %d  PM2.5 %d ug/m3\nPressure %+.1f hPa", g_data.aqi, g_data.pm25, g_data.pressureTrend);
  set_label(air_content, previous_air, sizeof(previous_air), text, &style_small_content);
}
static void draw_stocks() {
  String stockText = stocks_display();
  int row = 0;
  int lineStart = 0;
  while (row < 5 && lineStart <= stockText.length()) {
    int lineEnd = stockText.indexOf('\n', lineStart);
    if (lineEnd < 0) lineEnd = stockText.length();
    String line = stockText.substring(lineStart, lineEnd);
    char values[6][24] = {};
    sscanf(line.c_str(), "%23s %23s %23s %23s %23s %23s", values[0], values[1], values[2], values[3], values[4], values[5]);
    for (int column = 0; column < 6; column++) {
      lv_label_set_text(stock_cells[row][column], values[column]);
      lv_obj_invalidate(stock_cells[row][column]);
      if (row == 0) {
        lv_label_set_text(stock_header_bold[column], values[column]);
        lv_obj_invalidate(stock_header_bold[column]);
      }
    }
    row++;
    lineStart = lineEnd + 1;
  }
  while (row < 5) {
    for (int column = 0; column < 6; column++) lv_label_set_text(stock_cells[row][column], "");
    row++;
  }
  String updatedText = stocks_updated_display();
  set_label(stock_content, previous_stocks, sizeof(previous_stocks), updatedText.c_str(), &style_stock);
}
static void redraw() {
  draw_clock();
  draw_weather();
  draw_air();
  draw_stocks();
  String status = "Internet: " + internet_ip_display() + "  Wi-Fi: " +
                  (WiFi.status() == WL_CONNECTED ? webconfig_ip() : "offline");
  set_label(footer, previous_footer, sizeof(previous_footer), status.c_str(), &style_footer);
}
void weather_ui_begin() {
  lv_style_init(&style_bg); lv_style_set_bg_color(&style_bg, lv_color_hex(0x07151c));
  lv_style_init(&style_heading); lv_style_set_text_color(&style_heading, lv_color_hex(0x35d0c2)); lv_style_set_text_font(&style_heading, &lv_font_montserrat_32);
  lv_style_init(&style_content); lv_style_set_text_color(&style_content, lv_color_hex(0xf4f7f5)); lv_style_set_text_font(&style_content, &lv_font_montserrat_28); lv_style_set_text_align(&style_content, LV_TEXT_ALIGN_CENTER);
  lv_style_init(&style_clock); lv_style_set_text_color(&style_clock, lv_color_hex(0xf4f7f5)); lv_style_set_text_font(&style_clock, &lv_font_montserrat_48); lv_style_set_text_align(&style_clock, LV_TEXT_ALIGN_CENTER);
  lv_style_init(&style_footer); lv_style_set_text_color(&style_footer, lv_color_hex(0x91a8ad)); lv_style_set_text_font(&style_footer, &lv_font_montserrat_12);
  lv_style_init(&style_panel); lv_style_set_bg_color(&style_panel, lv_color_hex(0x10252d)); lv_style_set_border_width(&style_panel, 1); lv_style_set_border_color(&style_panel, lv_color_hex(0x29444d));
  lv_style_init(&style_tab); lv_style_set_bg_color(&style_tab, lv_color_hex(0x16333d)); lv_style_set_bg_opa(&style_tab, LV_OPA_COVER); lv_style_set_text_color(&style_tab, lv_color_hex(0x91a8ad));
  lv_style_init(&style_tab_active); lv_style_set_bg_color(&style_tab_active, lv_color_hex(0x35d0c2)); lv_style_set_bg_opa(&style_tab_active, LV_OPA_COVER); lv_style_set_text_color(&style_tab_active, lv_color_hex(0x07151c));
  lv_style_init(&style_small_heading); lv_style_set_text_color(&style_small_heading, lv_color_hex(0x35d0c2)); lv_style_set_text_font(&style_small_heading, &lv_font_montserrat_24);
  lv_style_init(&style_small_content); lv_style_set_text_color(&style_small_content, lv_color_hex(0xf4f7f5)); lv_style_set_text_font(&style_small_content, &lv_font_montserrat_18); lv_style_set_text_align(&style_small_content, LV_TEXT_ALIGN_CENTER);
  lv_style_init(&style_stock); lv_style_set_text_color(&style_stock, lv_color_hex(0xf4f7f5)); lv_style_set_text_font(&style_stock, &lv_font_montserrat_14); lv_style_set_text_align(&style_stock, LV_TEXT_ALIGN_LEFT);
  lv_style_init(&style_stock_updated); lv_style_set_text_color(&style_stock_updated, lv_color_hex(0x91a8ad)); lv_style_set_text_font(&style_stock_updated, &lv_font_montserrat_10); lv_style_set_text_align(&style_stock_updated, LV_TEXT_ALIGN_RIGHT);
  lv_style_init(&style_sun); lv_style_set_bg_color(&style_sun, lv_color_hex(0xf6c945)); lv_style_set_bg_opa(&style_sun, LV_OPA_COVER);
  lv_style_init(&style_cloud); lv_style_set_bg_color(&style_cloud, lv_color_hex(0x9bb4bd)); lv_style_set_bg_opa(&style_cloud, LV_OPA_COVER);
  lv_style_init(&style_rain); lv_style_set_bg_color(&style_rain, lv_color_hex(0x4aa8df)); lv_style_set_bg_opa(&style_rain, LV_OPA_COVER);
  lv_obj_t *screen = lv_screen_active(); lv_obj_add_style(screen, &style_bg, 0);
  dashboard_panel = lv_obj_create(screen); lv_obj_set_size(dashboard_panel, 400, 440); lv_obj_set_pos(dashboard_panel, 0, 0); lv_obj_add_style(dashboard_panel, &style_panel, 0);
  stock_panel = lv_obj_create(screen); lv_obj_set_size(stock_panel, 400, 440); lv_obj_set_pos(stock_panel, 400, 0); lv_obj_add_style(stock_panel, &style_panel, 0);
  lv_obj_t *clock_panel = dashboard_panel;
  lv_obj_t *data_panel = stock_panel;
  lv_obj_clear_flag(clock_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(data_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(clock_panel, 0, 0);
  lv_obj_set_style_pad_all(data_panel, 0, 0);

  clock_time = lv_label_create(clock_panel); lv_obj_set_size(clock_time, 380, 60); lv_obj_align(clock_time, LV_ALIGN_TOP_MID, 0, 20);
  clock_date = lv_label_create(clock_panel); lv_obj_set_size(clock_date, 380, 36); lv_obj_set_style_text_align(clock_date, LV_TEXT_ALIGN_CENTER, 0); lv_obj_align(clock_date, LV_ALIGN_TOP_MID, 0, 88);
  footer = lv_label_create(clock_panel); lv_obj_set_size(footer, 380, 24); lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0); lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -12);
  weather_icon = lv_obj_create(clock_panel); lv_obj_remove_style_all(weather_icon); lv_obj_set_size(weather_icon, 66, 66); lv_obj_set_pos(weather_icon, 8, 132);
  weather_content = lv_label_create(clock_panel); lv_obj_set_size(weather_content, 300, 96); lv_obj_add_style(weather_content, &style_small_content, 0); lv_obj_set_pos(weather_content, 88, 130);
  air_content = lv_label_create(clock_panel); lv_obj_set_size(air_content, 380, 54); lv_obj_add_style(air_content, &style_small_content, 0); lv_obj_align(air_content, LV_ALIGN_TOP_MID, 0, 226);
  stock_content = lv_label_create(data_panel); lv_obj_set_size(stock_content, 150, 18); lv_obj_add_style(stock_content, &style_stock_updated, 0); lv_obj_set_pos(stock_content, 240, 188);
  lv_obj_set_style_text_align(stock_content, LV_TEXT_ALIGN_RIGHT, 0);
  const int stock_x[] = { 4, 74, 136, 198, 260, 330 };
  const int stock_width[] = { 70, 62, 62, 62, 70, 60 };
  for (int row = 0; row < 5; row++) {
    for (int column = 0; column < 6; column++) {
      stock_cells[row][column] = lv_label_create(data_panel);
      lv_obj_set_size(stock_cells[row][column], stock_width[column], 28);
      lv_obj_set_pos(stock_cells[row][column], stock_x[column], 24 + row * 34);
      lv_obj_add_style(stock_cells[row][column], &style_stock, 0);
      if (column > 0) lv_obj_set_style_text_align(stock_cells[row][column], LV_TEXT_ALIGN_RIGHT, 0);
    }
  }
  for (int column = 0; column < 6; column++) {
    stock_header_bold[column] = lv_label_create(data_panel);
    lv_obj_set_size(stock_header_bold[column], stock_width[column], 28);
    lv_obj_set_pos(stock_header_bold[column], stock_x[column] + (column == 0 ? 1 : -1), 24);
    lv_obj_add_style(stock_header_bold[column], &style_stock, 0);
    if (column > 0) lv_obj_set_style_text_align(stock_header_bold[column], LV_TEXT_ALIGN_RIGHT, 0);
  }
  lv_obj_t *tab_bar = lv_obj_create(screen); lv_obj_remove_style_all(tab_bar); lv_obj_set_size(tab_bar, 800, 40); lv_obj_set_pos(tab_bar, 0, 440); lv_obj_clear_flag(tab_bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *dashboard_tab = lv_btn_create(tab_bar); lv_obj_set_size(dashboard_tab, 400, 40); lv_obj_set_pos(dashboard_tab, 0, 0); lv_obj_add_style(dashboard_tab, &style_tab_active, 0); lv_obj_add_event_cb(dashboard_tab, tab_event, LV_EVENT_CLICKED, (void *)(intptr_t)0);
  lv_obj_t *dashboard_label = lv_label_create(dashboard_tab); lv_label_set_text(dashboard_label, "DASHBOARD"); lv_obj_center(dashboard_label);
  lv_obj_t *stocks_tab = lv_btn_create(tab_bar); lv_obj_set_size(stocks_tab, 400, 40); lv_obj_set_pos(stocks_tab, 400, 0); lv_obj_add_style(stocks_tab, &style_tab, 0); lv_obj_add_event_cb(stocks_tab, tab_event, LV_EVENT_CLICKED, (void *)(intptr_t)1);
  lv_obj_t *stocks_label = lv_label_create(stocks_tab); lv_label_set_text(stocks_label, "EMPTY"); lv_obj_center(stocks_label);
  select_tab(0);
  redraw();
}
void weather_ui_tick() {
  if (millis() - last_draw < 1000) return;
  last_draw = millis();
  redraw();
}
