#include "weather_ui.h"
#include "app_data.h"
#include "settings.h"
#include "units.h"
#include "time_manager.h"
#include "sun_moon.h"
#include "webconfig.h"
#include "stocks.h"
#include "internet_ip.h"
#include "ssh_terminal.h"
#include "calendar.h"
#include <lvgl.h>
#include <WiFi.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

static lv_obj_t *clock_time, *clock_seconds, *clock_meridiem, *clock_date, *weather_icon, *weather_content, *air_content, *stock_content, *footer;
static lv_obj_t *stock_cells[5][6];
static lv_obj_t *stock_header_bold[6];
static lv_obj_t *dashboard_panel, *stock_panel, *ssh_panel;
static lv_obj_t *dashboard_tab, *ssh_tab;
static lv_obj_t *settings_tab, *settings_panel, *brightness_value;
static lv_obj_t *calendar_tab, *calendar_panel, *calendar_content, *calendar_updated_label;
static lv_obj_t *ssh_output, *ssh_command;
static lv_obj_t *ssh_modal, *ssh_host_input, *ssh_port_input, *ssh_user_input, *ssh_password_input;
static uint32_t last_draw;
static char previous_clock_time[32] = "";
static char previous_clock_seconds[8] = "";
static char previous_clock_meridiem[8] = "";
static char previous_clock_date[64] = "";
static char previous_weather[512] = "";
static char previous_air[256] = "";
static char previous_stocks[256] = "";
static char previous_footer[160] = "";
static char previous_calendar[512] = "";
static lv_style_t style_bg, style_heading, style_content, style_clock, style_footer;
static lv_style_t style_panel, style_small_heading, style_small_content;
static lv_style_t style_sun, style_cloud, style_rain;
static lv_style_t style_stock;
static lv_style_t style_stock_updated;
static lv_style_t style_tab, style_tab_active;
static lv_style_t style_calendar;

static void brightness_event(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED && lv_event_get_code(event) != LV_EVENT_RELEASED) return;
  lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(event);
  if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED) {
    g_settings.brightness = lv_slider_get_value(slider);
    ledcWrite(0, g_settings.brightness);
    char value[8]; snprintf(value, sizeof(value), "%u%%", (unsigned)(g_settings.brightness * 100 / 255));
    lv_label_set_text(brightness_value, value);
  } else {
    settings_save();
  }
}

static void ssh_send_event(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
    const char *command = lv_textarea_get_text(ssh_command);
    if (command && command[0]) {
      ssh_terminal_send(command);
      lv_textarea_set_text(ssh_command, "");
    }
  }
}
static void ssh_connect_event(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  g_settings.sshHost = lv_textarea_get_text(ssh_host_input);
  g_settings.sshPort = constrain(atoi(lv_textarea_get_text(ssh_port_input)), 1, 65535);
  g_settings.sshUser = lv_textarea_get_text(ssh_user_input);
  g_settings.sshPassword = lv_textarea_get_text(ssh_password_input);
  settings_save();
  ssh_terminal_connect_now();
  lv_obj_del(ssh_modal);
  ssh_modal = nullptr;
}
static void open_ssh_modal(lv_event_t *) {
  if (ssh_modal) return;
  ssh_modal = lv_obj_create(lv_screen_active());
  lv_obj_set_size(ssh_modal, 800, 440); lv_obj_set_pos(ssh_modal, 0, 0);
  lv_obj_set_style_bg_color(ssh_modal, lv_color_hex(0x07151c), 0);
  lv_obj_set_style_bg_opa(ssh_modal, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(ssh_modal, 10, 0);
  lv_obj_clear_flag(ssh_modal, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *title = lv_label_create(ssh_modal); lv_label_set_text(title, "SSH CONNECTION"); lv_obj_set_pos(title, 10, 8);
  ssh_host_input = lv_textarea_create(ssh_modal); lv_obj_set_size(ssh_host_input, 380, 38); lv_obj_set_pos(ssh_host_input, 10, 38); lv_textarea_set_one_line(ssh_host_input, true); lv_textarea_set_text(ssh_host_input, g_settings.sshHost.c_str()); lv_textarea_set_placeholder_text(ssh_host_input, "Hostname or IP address");
  ssh_port_input = lv_textarea_create(ssh_modal); lv_obj_set_size(ssh_port_input, 100, 38); lv_obj_set_pos(ssh_port_input, 400, 38); lv_textarea_set_one_line(ssh_port_input, true); lv_textarea_set_text(ssh_port_input, String(g_settings.sshPort).c_str()); lv_textarea_set_placeholder_text(ssh_port_input, "Port");
  ssh_user_input = lv_textarea_create(ssh_modal); lv_obj_set_size(ssh_user_input, 380, 38); lv_obj_set_pos(ssh_user_input, 10, 82); lv_textarea_set_one_line(ssh_user_input, true); lv_textarea_set_text(ssh_user_input, g_settings.sshUser.c_str()); lv_textarea_set_placeholder_text(ssh_user_input, "Username");
  ssh_password_input = lv_textarea_create(ssh_modal); lv_obj_set_size(ssh_password_input, 380, 38); lv_obj_set_pos(ssh_password_input, 10, 126); lv_textarea_set_one_line(ssh_password_input, true); lv_textarea_set_password_mode(ssh_password_input, true); lv_textarea_set_text(ssh_password_input, g_settings.sshPassword.c_str()); lv_textarea_set_placeholder_text(ssh_password_input, "Password");
  lv_obj_t *connect = lv_btn_create(ssh_modal); lv_obj_set_size(connect, 160, 38); lv_obj_set_pos(connect, 520, 82); lv_obj_add_event_cb(connect, ssh_connect_event, LV_EVENT_CLICKED, nullptr); lv_obj_t *connect_label = lv_label_create(connect); lv_label_set_text(connect_label, "CONNECT"); lv_obj_center(connect_label);
  lv_obj_t *cancel = lv_btn_create(ssh_modal); lv_obj_set_size(cancel, 160, 38); lv_obj_set_pos(cancel, 520, 126); lv_obj_add_event_cb(cancel, [](lv_event_t *) { lv_obj_del(ssh_modal); ssh_modal = nullptr; }, LV_EVENT_CLICKED, nullptr); lv_obj_t *cancel_label = lv_label_create(cancel); lv_label_set_text(cancel_label, "CANCEL"); lv_obj_center(cancel_label);
  lv_obj_t *keyboard = lv_keyboard_create(ssh_modal); lv_obj_set_size(keyboard, 780, 260); lv_obj_set_pos(keyboard, 10, 170); lv_keyboard_set_textarea(keyboard, ssh_host_input);
  auto select_ssh_field = [](lv_event_t *event) {
    lv_keyboard_set_textarea((lv_obj_t *)lv_event_get_user_data(event), (lv_obj_t *)lv_event_get_target(event));
  };
  lv_obj_add_event_cb(ssh_host_input, select_ssh_field, LV_EVENT_FOCUSED, keyboard);
  lv_obj_add_event_cb(ssh_port_input, select_ssh_field, LV_EVENT_FOCUSED, keyboard);
  lv_obj_add_event_cb(ssh_user_input, select_ssh_field, LV_EVENT_FOCUSED, keyboard);
  lv_obj_add_event_cb(ssh_password_input, select_ssh_field, LV_EVENT_FOCUSED, keyboard);
}

static void select_tab(int tab) {
  if (ssh_modal) {
    lv_obj_del(ssh_modal);
    ssh_modal = nullptr;
  }
  if (tab == 0) {
    lv_obj_clear_flag(dashboard_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(stock_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ssh_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(calendar_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_style(dashboard_tab, &style_tab_active, 0);
    lv_obj_add_style(ssh_tab, &style_tab, 0);
    lv_obj_add_style(calendar_tab, &style_tab, 0);
    lv_obj_add_style(settings_tab, &style_tab, 0);
  } else if (tab == 1) {
    lv_obj_add_flag(dashboard_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(stock_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ssh_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(calendar_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_style(dashboard_tab, &style_tab, 0);
    lv_obj_add_style(ssh_tab, &style_tab_active, 0);
    lv_obj_add_style(calendar_tab, &style_tab, 0);
    lv_obj_add_style(settings_tab, &style_tab, 0);
  } else if (tab == 2) {
    calendar_start_fetch();
    lv_obj_add_flag(dashboard_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(stock_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ssh_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(calendar_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_style(dashboard_tab, &style_tab, 0);
    lv_obj_add_style(ssh_tab, &style_tab, 0);
    lv_obj_add_style(calendar_tab, &style_tab_active, 0);
    lv_obj_add_style(settings_tab, &style_tab, 0);
  } else {
    lv_obj_add_flag(dashboard_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(stock_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ssh_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(calendar_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_style(dashboard_tab, &style_tab, 0);
    lv_obj_add_style(ssh_tab, &style_tab, 0);
    lv_obj_add_style(calendar_tab, &style_tab, 0);
    lv_obj_add_style(settings_tab, &style_tab_active, 0);
  }
}
static void tab_event(lv_event_t *event) {
  if (lv_event_get_code(event) == LV_EVENT_RELEASED) {
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
  struct tm t; char time_text[16], seconds_text[8], meridiem_text[8], date_text[64];
  static const char *weekdays[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
  if (time_manager_now(t)) {
    strftime(time_text, sizeof(time_text), "%I:%M", &t);
    strftime(seconds_text, sizeof(seconds_text), ":%S", &t);
    strftime(meridiem_text, sizeof(meridiem_text), "%p", &t);
    snprintf(date_text, sizeof(date_text), "%s %04d/%02d/%02d",
             weekdays[t.tm_wday], t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  } else if (WiFi.status() != WL_CONNECTED) {
    snprintf(time_text, sizeof(time_text), "--:--");
    snprintf(seconds_text, sizeof(seconds_text), ":--");
    meridiem_text[0] = '\0';
    snprintf(date_text, sizeof(date_text), "Wi-Fi offline");
  } else {
    snprintf(time_text, sizeof(time_text), "--:--");
    snprintf(seconds_text, sizeof(seconds_text), ":--");
    meridiem_text[0] = '\0';
    snprintf(date_text, sizeof(date_text), "Waiting for network time");
  }
  set_label(clock_time, previous_clock_time, sizeof(previous_clock_time), time_text, &style_clock);
  set_label(clock_seconds, previous_clock_seconds, sizeof(previous_clock_seconds), seconds_text, &style_clock);
  set_label(clock_meridiem, previous_clock_meridiem, sizeof(previous_clock_meridiem), meridiem_text, &style_clock);
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
      if (row > 0 && (column == 3 || column == 4) && strcmp(values[column], "--") != 0 && values[column][0] != '\0') {
        lv_obj_set_style_text_color(stock_cells[row][column], atof(values[column]) >= 0 ? lv_color_hex(0x55d68a) : lv_color_hex(0xff6b6b), 0);
      } else {
        lv_obj_set_style_text_color(stock_cells[row][column], lv_color_hex(0xf4f7f5), 0);
      }
      lv_obj_invalidate(stock_cells[row][column]);
      if (row == 0) {
        lv_label_set_text(stock_header_bold[column], values[column]);
        lv_obj_set_style_text_color(stock_header_bold[column], lv_color_hex(0xf4f7f5), 0);
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
static void draw_ssh() {
  static char previous_output[1900] = "";
  String output = ssh_terminal_output();
  if (output != previous_output) {
    lv_textarea_set_text(ssh_output, output.c_str());
    lv_textarea_set_cursor_pos(ssh_output, LV_TEXTAREA_CURSOR_LAST);
    strncpy(previous_output, output.c_str(), sizeof(previous_output) - 1);
    previous_output[sizeof(previous_output) - 1] = '\0';
  }
}
static void draw_calendar() {
  String text = calendar_display();
  String updated = calendar_updated_display();
  if (updated.length()) { text += "\n\n"; text += updated; }
  set_label(calendar_content, previous_calendar, sizeof(previous_calendar), text.c_str(), &style_calendar);
}
static void redraw() {
  draw_clock();
  draw_weather();
  draw_air();
  draw_stocks();
  draw_ssh();
  draw_calendar();
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
  lv_style_init(&style_calendar); lv_style_set_text_color(&style_calendar, lv_color_hex(0xf4f7f5)); lv_style_set_text_font(&style_calendar, &lv_font_montserrat_20); lv_style_set_text_align(&style_calendar, LV_TEXT_ALIGN_LEFT);
  lv_style_init(&style_sun); lv_style_set_bg_color(&style_sun, lv_color_hex(0xf6c945)); lv_style_set_bg_opa(&style_sun, LV_OPA_COVER);
  lv_style_init(&style_cloud); lv_style_set_bg_color(&style_cloud, lv_color_hex(0x9bb4bd)); lv_style_set_bg_opa(&style_cloud, LV_OPA_COVER);
  lv_style_init(&style_rain); lv_style_set_bg_color(&style_rain, lv_color_hex(0x4aa8df)); lv_style_set_bg_opa(&style_rain, LV_OPA_COVER);
  lv_obj_t *screen = lv_screen_active(); lv_obj_add_style(screen, &style_bg, 0);
  dashboard_panel = lv_obj_create(screen); lv_obj_set_size(dashboard_panel, 400, 440); lv_obj_set_pos(dashboard_panel, 0, 0); lv_obj_add_style(dashboard_panel, &style_panel, 0);
  stock_panel = lv_obj_create(screen); lv_obj_set_size(stock_panel, 400, 440); lv_obj_set_pos(stock_panel, 400, 0); lv_obj_add_style(stock_panel, &style_panel, 0);
  ssh_panel = lv_obj_create(screen); lv_obj_set_size(ssh_panel, 800, 440); lv_obj_set_pos(ssh_panel, 0, 0); lv_obj_add_style(ssh_panel, &style_panel, 0);
  settings_panel = lv_obj_create(screen); lv_obj_set_size(settings_panel, 800, 440); lv_obj_set_pos(settings_panel, 0, 0); lv_obj_add_style(settings_panel, &style_panel, 0);
  calendar_panel = lv_obj_create(screen); lv_obj_set_size(calendar_panel, 800, 440); lv_obj_set_pos(calendar_panel, 0, 0); lv_obj_add_style(calendar_panel, &style_panel, 0);
  lv_obj_t *clock_panel = dashboard_panel;
  lv_obj_t *data_panel = stock_panel;
  lv_obj_clear_flag(clock_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(data_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(ssh_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(settings_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(calendar_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(clock_panel, 0, 0);
  lv_obj_set_style_pad_all(data_panel, 0, 0);

  clock_time = lv_label_create(clock_panel); lv_obj_set_size(clock_time, 190, 60); lv_obj_set_pos(clock_time, 10, 20); lv_obj_set_style_text_align(clock_time, LV_TEXT_ALIGN_RIGHT, 0);
  clock_seconds = lv_label_create(clock_panel); lv_obj_set_size(clock_seconds, 76, 60); lv_obj_set_pos(clock_seconds, 204, 20); lv_obj_set_style_text_align(clock_seconds, LV_TEXT_ALIGN_LEFT, 0);
  clock_meridiem = lv_label_create(clock_panel); lv_obj_set_size(clock_meridiem, 100, 60); lv_obj_set_pos(clock_meridiem, 286, 20); lv_obj_set_style_text_align(clock_meridiem, LV_TEXT_ALIGN_LEFT, 0);
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
  ssh_output = lv_textarea_create(ssh_panel);
  lv_obj_set_size(ssh_output, 780, 280);
  lv_obj_set_pos(ssh_output, 10, 14);
  lv_textarea_set_text(ssh_output, "SSH terminal\n");
  lv_textarea_set_cursor_click_pos(ssh_output, false);
  lv_textarea_set_one_line(ssh_output, false);
  ssh_command = lv_textarea_create(ssh_panel);
  lv_obj_set_size(ssh_command, 520, 42);
  lv_obj_set_pos(ssh_command, 140, 306);
  lv_textarea_set_one_line(ssh_command, true);
  lv_textarea_set_placeholder_text(ssh_command, "Command");
  lv_obj_t *ssh_send = lv_btn_create(ssh_panel);
  lv_obj_set_size(ssh_send, 80, 42);
  lv_obj_set_pos(ssh_send, 670, 306);
  lv_obj_add_event_cb(ssh_send, ssh_send_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ssh_send_label = lv_label_create(ssh_send); lv_label_set_text(ssh_send_label, "SEND"); lv_obj_center(ssh_send_label);
  lv_obj_t *ssh_connect = lv_btn_create(ssh_panel); lv_obj_set_size(ssh_connect, 120, 42); lv_obj_set_pos(ssh_connect, 10, 306); lv_obj_add_event_cb(ssh_connect, open_ssh_modal, LV_EVENT_CLICKED, nullptr); lv_obj_t *ssh_connect_label = lv_label_create(ssh_connect); lv_label_set_text(ssh_connect_label, "CONNECT"); lv_obj_center(ssh_connect_label);
  lv_obj_t *keyboard = lv_keyboard_create(ssh_panel);
  lv_obj_set_size(keyboard, 780, 100);
  lv_obj_set_pos(keyboard, 10, 354);
  lv_keyboard_set_textarea(keyboard, ssh_command);
  lv_obj_t *settings_title = lv_label_create(settings_panel); lv_label_set_text(settings_title, "SETTINGS"); lv_obj_set_pos(settings_title, 24, 24);
  lv_obj_t *brightness_label = lv_label_create(settings_panel); lv_label_set_text(brightness_label, "Screen brightness"); lv_obj_set_pos(brightness_label, 24, 82);
  lv_obj_t *brightness_slider = lv_slider_create(settings_panel); lv_obj_set_size(brightness_slider, 600, 24); lv_obj_set_pos(brightness_slider, 24, 116); lv_slider_set_range(brightness_slider, 0, 255); lv_slider_set_value(brightness_slider, g_settings.brightness, LV_ANIM_OFF); lv_obj_add_event_cb(brightness_slider, brightness_event, LV_EVENT_VALUE_CHANGED, nullptr);
  brightness_value = lv_label_create(settings_panel); char brightness_text[8]; snprintf(brightness_text, sizeof(brightness_text), "%u%%", (unsigned)(g_settings.brightness * 100 / 255)); lv_label_set_text(brightness_value, brightness_text); lv_obj_set_pos(brightness_value, 640, 112);
  lv_obj_t *bluetooth_label = lv_label_create(settings_panel); lv_label_set_text(bluetooth_label, "BLE keyboard pairing: requires a BLE HID host adapter"); lv_obj_set_pos(bluetooth_label, 24, 190);
  lv_obj_t *calendar_title = lv_label_create(calendar_panel); lv_label_set_text(calendar_title, "UPCOMING REMINDERS"); lv_obj_set_pos(calendar_title, 24, 20);
  calendar_content = lv_label_create(calendar_panel); lv_obj_set_size(calendar_content, 752, 380); lv_obj_set_pos(calendar_content, 24, 64); lv_label_set_long_mode(calendar_content, LV_LABEL_LONG_WRAP);
  lv_obj_t *tab_bar = lv_obj_create(screen); lv_obj_remove_style_all(tab_bar); lv_obj_set_size(tab_bar, 800, 40); lv_obj_set_pos(tab_bar, 0, 440); lv_obj_clear_flag(tab_bar, LV_OBJ_FLAG_SCROLLABLE);
  dashboard_tab = lv_btn_create(tab_bar); lv_obj_set_size(dashboard_tab, 200, 40); lv_obj_set_pos(dashboard_tab, 0, 0); lv_obj_add_style(dashboard_tab, &style_tab_active, 0); lv_obj_add_event_cb(dashboard_tab, tab_event, LV_EVENT_RELEASED, (void *)(intptr_t)0);
  lv_obj_t *dashboard_label = lv_label_create(dashboard_tab); lv_label_set_text(dashboard_label, "DASHBOARD"); lv_obj_center(dashboard_label);
  ssh_tab = lv_btn_create(tab_bar); lv_obj_set_size(ssh_tab, 200, 40); lv_obj_set_pos(ssh_tab, 200, 0); lv_obj_add_style(ssh_tab, &style_tab, 0); lv_obj_add_event_cb(ssh_tab, tab_event, LV_EVENT_RELEASED, (void *)(intptr_t)1);
  lv_obj_t *ssh_label = lv_label_create(ssh_tab); lv_label_set_text(ssh_label, "SSH"); lv_obj_center(ssh_label);
  calendar_tab = lv_btn_create(tab_bar); lv_obj_set_size(calendar_tab, 200, 40); lv_obj_set_pos(calendar_tab, 400, 0); lv_obj_add_style(calendar_tab, &style_tab, 0); lv_obj_add_event_cb(calendar_tab, tab_event, LV_EVENT_RELEASED, (void *)(intptr_t)2);
  lv_obj_t *calendar_tab_label = lv_label_create(calendar_tab); lv_label_set_text(calendar_tab_label, "CALENDAR"); lv_obj_center(calendar_tab_label);
  settings_tab = lv_btn_create(tab_bar); lv_obj_set_size(settings_tab, 200, 40); lv_obj_set_pos(settings_tab, 600, 0); lv_obj_add_style(settings_tab, &style_tab, 0); lv_obj_add_event_cb(settings_tab, tab_event, LV_EVENT_RELEASED, (void *)(intptr_t)3);
  lv_obj_t *settings_tab_label = lv_label_create(settings_tab); lv_label_set_text(settings_tab_label, "SETTINGS"); lv_obj_center(settings_tab_label);
  select_tab(0);
  redraw();
}

static uint32_t compute_content_hash() {
  uint32_t hash = 0;
  struct tm now;
  if (!time_manager_now(now)) return 0;
  
  // Hash time (minutes only, so hour/minute changes trigger redraw)
  hash = hash * 31 + now.tm_hour;
  hash = hash * 31 + now.tm_min;
  
  // Hash weather data
  hash = hash * 31 + (uint32_t)(g_data.tempC * 10);
  hash = hash * 31 + g_data.weatherCode;
  hash = hash * 31 + g_data.aqi;
  
  // Hash calendar/stocks/SSH to detect their updates
  hash = hash * 31 + calendar_display().length();
  hash = hash * 31 + stocks_display().length();
  hash = hash * 31 + ssh_terminal_output().length();
  
  return hash;
}

void weather_ui_tick() {
  if (!time_manager_second_elapsed()) return;
  
  static uint32_t last_hash = 0;
  uint32_t current_hash = compute_content_hash();
  
  if (current_hash != last_hash) {
    last_hash = current_hash;
    redraw();
  }
}
