# ESP32-8048S070 LVGL Weather Dashboard

A feature-rich embedded dashboard application for the Sunton ESP32-S3 7-inch display with weather, air quality, stocks, SSH terminal, and iCloud calendar integration.

## Features

### 📊 Dashboard
- **4-Tab Interface** with smooth navigation
  - **Dashboard**: Clock, weather, air quality, internet IP
  - **SSH**: Interactive SSH terminal with connection modal
  - **Calendar**: iCloud CalDAV reminders (lazy-loaded on demand)
  - **Settings**: Brightness control, configuration

### 🌤️ Weather
- Real-time weather from Open-Meteo API
- Temperature, feels-like, humidity, wind speed, UV index, cloud cover
- Weather icon matching WMO weather codes
- 30-minute update interval
- Metric/Imperial unit toggle

### 💨 Air Quality
- US AQI and PM2.5 from Open-Meteo air quality API
- 30-minute update interval
- Health category display

### 📈 Stock Ticker
- Yahoo Finance chart API integration
- Open/Last/Delta/% columns with color coding (green/red)
- 60-minute update interval
- **Market hours only** (6AM-1PM) to save power
- 6-stock display limit

### 🖥️ SSH Terminal
- LibSSH client with password authentication
- VT100 terminal emulation (80×24 character grid)
- ANSI escape sequence parsing (cursor movement, clear, scroll)
- On-demand connection via Connect button
- Command input with on-screen keyboard

### 📅 Calendar
- iCloud CalDAV integration
- Fetches upcoming calendar events (next 14 days)
- All-day and timed event support
- Lazy-loaded (fetches only when Calendar tab clicked)
- Auto-discovery of calendar URL from iCloud
- Requires app-specific password (not regular password)

### ⏰ Clock & Time
- NTP time sync via configurable timezone
- 1-second update via hardware timer (esp_timer)
- Fixed-position labels prevent digit jitter
- Shows weekday and full date

### 🌙 Brightness Scheduling
- Auto-dim to 50% at night (11PM-8AM)
- Smooth transition controlled by NVS persistence

### 📡 Connectivity
- WiFi provisioning via web setup portal
- WiFi light sleep mode (WIFI_PS_MAX_MODEM) for power saving
- Serialized HTTPS requests via global network lock (prevents concurrent TLS heap exhaustion)

### 🔋 Power Optimization
- **Event-driven UI refresh**: Only redraws when content changes (vs. every second)
- **WiFi light sleep**: Radio sleeps between beacons (~100ms wake cycles)
- **Stock market hours filtering**: Fetches only 6AM-1PM
- **Lazy calendar loading**: Fetches only when tab clicked
- ~20-30% total power reduction vs. continuous polling

### 📊 Memory Monitoring
- Periodic heap stats to serial console (every 10 seconds)
- Tracks free heap, minimum free, and max contiguous block

---

## Hardware Setup

### Display & Touch
- **Display**: Arduino_GFX_Library with RGB parallel panel (800×480)
- **Touch**: TAMC_GT911 capacitive touch (I2C: SDA=19, SCL=20)
  - INT pin: GPIO 18
  - RST pin: GPIO 38
- **Backlight**: GPIO 2 (PWM, 8-bit, ~160 Hz)

### Pinout Summary
| Function | GPIO |
|----------|------|
| TFT BL (Backlight) | 2 |
| I2C SDA (Touch) | 19 |
| I2C SCL (Touch) | 20 |
| GT911 INT | 18 |
| GT911 RST | 38 |

---

## Software Setup

### Prerequisites
- PlatformIO (VS Code extension recommended)
- ESP32-S3 board support (espressif32 6.4.0)
- Arduino framework

### Installation

1. **Clone and open the project**
   ```bash
   git clone <repo>
   cd ESP32-8048S070-LVGL-Demo
   ```

2. **Configure your settings** (optional before first boot):
   - Edit `src/settings.h` for default location, timezone, etc.
   - Or use the web setup portal after flashing

3. **Build and upload**
   ```bash
   platformio run --environment sunton_s3
   platformio run --target upload --environment sunton_s3
   ```

4. **Monitor serial output**
   ```bash
   platformio device monitor --environment sunton_s3 --speed 115200
   ```

---

## Initial Setup (First Boot)

1. **Device boots in AP mode**
   - SSID: `hub-XXXXXX` (where XXXXXX is part of MAC address)
   - No password required

2. **Connect to the AP from your phone/computer**

3. **Open browser and navigate to `http://192.168.4.1`**

4. **Fill in the setup form**:

   **WiFi**
   - SSID: Your network name
   - Password: Your WiFi password

   **Location & Time**
   - Latitude/Longitude: Your location for weather
   - Timezone: e.g., `EST5EDT`, `PST8PDT`, `UTC0`

   **SSH (Optional)**
   - Host: SSH server hostname/IP
   - Port: (default 22)
   - Username: SSH user
   - Password: SSH password

   **iCloud Calendar (Optional)**
   - Email: Your iCloud email address
   - App-specific password: Generate at [appleid.apple.com](https://appleid.apple.com) → Sign-In and Security → App-Specific Passwords

   **Stock Symbols (Optional)**
   - Comma-separated: `AAPL,MSFT,GOOGL` (max 6)

   **Display**
   - Brightness: 0-255 (255 = full brightness)
   - Units: Metric or Imperial

5. **Click SAVE** → Device restarts and connects to WiFi

---

## Web Configuration Portal

Access anytime (if provisioned) by:
1. Finding the device's IP on your WiFi network
2. Opening `http://<device-ip>/`
3. Modifying any settings and clicking SAVE

**Note**: Device reboots after saving configuration.

---

## Usage Guide

### Dashboard Tab
- Shows current weather, air quality, time, and public IP
- Auto-updates every 30 seconds

### SSH Tab
1. Click **CONNECT** button
2. Fill in host/port/user/password
3. Click **CONNECT** again
4. Type commands in the input field and press **SEND**
5. To disconnect, close the connection modal and reconnect as needed

### Calendar Tab
1. Click the **CALENDAR** tab
2. First click triggers fetch from iCloud CalDAV
3. Shows upcoming events for the next 14 days
4. Only fetches once per ~15 minutes while tab is active

### Settings Tab
- **Brightness slider**: Adjust display brightness (persists to NVS)
- Day/night auto-dim is applied on top of this setting

---

## Configuration Files

### `src/settings.h`
Defines the global `Settings` struct with all persistent configuration:
- WiFi credentials
- Location and timezone
- SSH host/user/password
- iCloud email and app-specific password
- Stock symbols
- Brightness and units

### `platformio.ini`
- **Platform**: espressif32 6.4.0 (Arduino framework)
- **Board**: sunton_s3
- **Build flags**: `-O3 -std=gnu++17`, LVGL configuration
- **Key libraries**:
  - lvgl 9.5.0 (UI framework)
  - ArduinoJson 7.0+ (JSON parsing)
  - GFX Library for Arduino 1.4.0 (display driver)
  - TAMC_GT911 1.0.2 (touch driver)
  - LibSSH-ESP32 5.9.0 (SSH client)

---

## Architecture Overview

### Task-Based Services
- **weather_task**: Fetches weather every 30s (after 25s boot delay)
- **airquality_task**: Fetches AQI every 30s (after 35s boot delay)
- **stock_task**: Fetches stocks every 60s (after 50s boot delay, 6AM-1PM only)
- **ssh_task**: Waits for user to click Connect, then maintains SSH connection
- **calendar_task**: Lazy-loads on Calendar tab click, fetches every 15 minutes

### Synchronization
- **NetworkGuard**: RAII wrapper around global `network_lock()` / `network_unlock()` to serialize all HTTPS/TLS requests and prevent concurrent heap exhaustion

### UI Framework
- **LVGL 9.5.0**: Lightweight embedded graphics library
- **Partial rendering**: Only redraws changed regions
- **Event-driven**: Content hashing prevents unnecessary redraws

### Time Management
- **esp_timer**: 1-second periodic hardware timer (not ISR-based for stability)
- **time_manager_now()**: Returns current `struct tm` via `localtime_r(time())`
- **time_manager_second_elapsed()**: Polls for 1-second tick events

---

## Troubleshooting

### Device stuck at boot
- Check that WiFi is enabled and credentials are correct
- Monitor serial output for error messages
- Press reset button or power-cycle if stuck

### "Failed to allocate buf memory" errors
- Indicates heap exhaustion during TLS handshake
- Services are staggered at boot to avoid this
- If persists, check that calendar lazy-loading is working (should not fetch until tab clicked)

### Touch not responding
- Verify I2C pins (SDA=19, SCL=20) are correct
- Check GT911 INT (GPIO 18) and RST (GPIO 38) pins
- Watch serial output for touch init messages

### Weather not updating
- Confirm WiFi is connected (check RSSI in status bar)
- Verify location (latitude/longitude) is set correctly
- Check serial output for `weather HTTP` error codes

### SSH connection fails
- Verify SSH host, port, username, password
- Ensure device can reach SSH server (try from computer first)
- Watch serial for connection error messages

### Calendar not showing events
- Generate **app-specific password** at [appleid.apple.com](https://appleid.apple.com)
- Do NOT use your regular iCloud password
- Ensure iCloud email and app password are set in Settings
- Calendar only fetches when Calendar tab is clicked (lazy-load)

---

## Serial Console Output

Monitor heap and system status:
```
[HEAP] Free: 145000 bytes, Min: 120000, Max alloc: 95000
weather updated: success
stock AAPL: 150.23 (delta +2.5%)
NTP time synchronized: 2026-08-24 16:42:48
```

---

## Power Consumption Estimates

| Mode | Power Draw |
|------|-----------|
| Full brightness, all updates | ~2.5W |
| With event-driven refresh + WiFi sleep | ~2.0W |
| With display off (future feature) | ~0.3W |

---

## Dependencies

See [platformio.ini](platformio.ini) for full dependency list. Key libraries:
- **lvgl**: UI rendering
- **ArduinoJson**: JSON parsing for APIs
- **libssh-esp32**: SSH client
- **Arduino-GFX**: Display driver abstraction
- **TAMC_GT911**: Touch controller driver

---

## Future Enhancements

- [ ] Display sleep mode (touch wake)
- [ ] Bluetooth remote control
- [ ] Local weather station data
- [ ] Multiple calendar support
- [ ] Custom theme/color schemes
- [ ] OTA firmware updates
- [ ] Voice assistant integration

---

## License

Project for embedded weather dashboard on Sunton ESP32-S3 display.

---

## Support

For issues, check:
1. Serial console output for error messages
2. Heap stats to confirm no memory exhaustion
3. WiFi connection status
4. Verify all configuration is saved (Settings tab → saved?)

