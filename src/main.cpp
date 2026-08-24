#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include "app_data.h"
#include "settings.h"
#include "time_manager.h"
#include "weather.h"
#include "airquality.h"
#include "sun_moon.h"
#include "webconfig.h"
#include "weather_ui.h"
#define TFT_BL 2

AppData g_data;

static bool wifi_has_ip() {
  return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

/* Change to your screen resolution */
#define screenWidth 800
#define screenHeight 480
EXT_RAM_ATTR static lv_color_t disp_draw_buf[screenWidth * screenHeight / 10];

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    41 /* DE */, 40 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    14 /* R0 */, 21 /* R1 */, 47 /* R2 */, 48 /* R3 */, 45 /* R4 */,
    9 /* G0 */, 46 /* G1 */, 3 /* G2 */, 8 /* G3 */, 16 /* G4 */, 1 /* G5 */,
    15 /* B0 */, 7 /* B1 */, 6 /* B2 */, 5 /* B3 */, 4 /* B4 */,
    0 /* hsync_polarity */, 180 /* hsync_front_porch */, 30 /* hsync_pulse_width */, 16 /* hsync_back_porch */,
    0 /* vsync_polarity */, 12 /* vsync_front_porch */, 13 /* vsync_pulse_width */, 10 /* vsync_back_porch */);
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    screenWidth /* width */, screenHeight /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */);

#include "touch.h"

/* Display flushing */
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

  lv_display_flush_ready(disp);
}

void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data)
{
    if (touch_touched())
    {
      data->state = LV_INDEV_STATE_PR;

      /*Set the coordinates*/
      data->point.x = touch_last_x;
      data->point.y = touch_last_y;
    }
    else
    {
      data->state = LV_INDEV_STATE_REL;
    }
}

static uint32_t lvgl_tick() {
  return (uint32_t)millis();
}

void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial.println("Application setup started");

  // Init Display
  gfx->begin();
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  ledcSetup(0, 300, 8);
  ledcAttachPin(TFT_BL, 0);
  ledcWrite(0, 255); /* Screen brightness can be modified by adjusting this parameter. (0-255) */
  
#endif
  lv_init();
  lv_tick_set_cb(lvgl_tick);

    // Init touch device
  pinMode(TOUCH_GT911_RST, OUTPUT);
  digitalWrite(TOUCH_GT911_RST, LOW);
  delay(10);
  digitalWrite(TOUCH_GT911_RST, HIGH);
  delay(10);
  Serial.println("Touch init: starting");
  touch_init();
  Serial.println("Touch init: complete");

    lv_display_t *display = lv_display_create(screenWidth, screenHeight);
    lv_display_set_buffers(display, disp_draw_buf, NULL, sizeof(disp_draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, my_disp_flush);

    /* Initialize the (dummy) input device driver */
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    settings_begin();
    Serial.printf("WiFi config: provisioned=%s ssid=\"%s\"\n",
            g_settings.provisioned ? "yes" : "no", g_settings.wifiSsid.c_str());
    WiFi.mode(WIFI_AP_STA);
    WiFi.setAutoReconnect(true);
    if (g_settings.provisioned) {
      WiFi.begin(g_settings.wifiSsid.c_str(), g_settings.wifiPass.c_str());
      uint32_t started = millis();
      while (!wifi_has_ip() && millis() - started < 20000) delay(100);
      Serial.printf("WiFi status=%d IP=%s RSSI=%d\n", WiFi.status(), WiFi.localIP().toString().c_str(), WiFi.RSSI());
      if (!wifi_has_ip()) {
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(false, false);
        delay(100);
        int networkCount = WiFi.scanNetworks();
        if (networkCount >= 0) {
          Serial.printf("WiFi scan found %d networks:\n", networkCount);
          for (int network = 0; network < networkCount; network++) {
            Serial.printf("  %s (RSSI %d, channel %d)\n",
                          WiFi.SSID(network).c_str(), WiFi.RSSI(network), WiFi.channel(network));
          }
          WiFi.scanDelete();
        } else {
          Serial.printf("WiFi scan failed with code %d\n", networkCount);
        }
      }
    } else {
      Serial.println("WiFi credentials are not configured; use the WeatherClock setup AP.");
    }
    webconfig_begin();
    time_manager_begin(g_settings.tz.c_str());
    weather_begin();
    airquality_begin();
    weather_ui_begin();
}

void loop()
{
  webconfig_tick();
  if (webconfig_saved()) { delay(1000); ESP.restart(); }
  if (!wifi_has_ip() && g_settings.provisioned) {
    static uint32_t last_retry = 0;
    if (millis() - last_retry >= 10000) { last_retry = millis(); WiFi.reconnect(); }
  }
  static uint32_t last_sun = 0;
  if (millis() - last_sun >= 60000) { last_sun = millis(); sunmoon_recompute(); }
  weather_tick();
  airquality_tick();
  weather_ui_tick();
  lv_timer_handler(); /* let the GUI do its work */
  delay(5);
}