#include "calendar.h"
#include "settings.h"
#include "network.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>

static SemaphoreHandle_t calendar_mutex;
static String display_text;
static String updated_text;
static TaskHandle_t calendar_task_handle;

static const char *CALDAV_ROOT = "https://caldav.icloud.com/";

struct NetworkGuard {
  NetworkGuard() { network_lock(); }
  ~NetworkGuard() { network_unlock(); }
};

// ---------------------------------------------------------------------------
// Minimal namespace-agnostic XML helpers.
// Only ASCII structural markers are searched, so a lowercased copy of the
// document can be used for locating tags while the original text (with its
// case preserved) is used to pull out the actual content.
// ---------------------------------------------------------------------------
static int find_open_tag(const String &lowerXml, const char *localName, int from) {
  String name(localName);
  int searchFrom = from;
  while (true) {
    int idx = lowerXml.indexOf(name, searchFrom);
    if (idx < 0) return -1;
    int lt = lowerXml.lastIndexOf('<', idx);
    if (lt < 0) { searchFrom = idx + 1; continue; }
    if (lowerXml.charAt(lt + 1) == '/') { searchFrom = idx + 1; continue; }
    bool prefixOk = true;
    for (int i = lt + 1; i < idx; i++) {
      char c = lowerXml.charAt(i);
      if (!isAlphaNumeric(c) && c != ':') { prefixOk = false; break; }
    }
    if (!prefixOk) { searchFrom = idx + 1; continue; }
    int after = idx + name.length();
    if (after >= (int)lowerXml.length()) return -1;
    char c = lowerXml.charAt(after);
    if (c != '>' && c != ' ' && c != '/' && c != '\r' && c != '\n' && c != '\t') { searchFrom = idx + 1; continue; }
    return lt;
  }
}

// Extracts the inner content of the next <localName>...</localName> element,
// ignoring whatever namespace prefix it uses. searchPos advances past it.
static String extract_block(const String &xml, const String &lowerXml, const char *localName, int &searchPos) {
  int openStart = find_open_tag(lowerXml, localName, searchPos);
  if (openStart < 0) { searchPos = lowerXml.length(); return String(); }
  int openTagEnd = lowerXml.indexOf('>', openStart);
  if (openTagEnd < 0) { searchPos = lowerXml.length(); return String(); }
  if (lowerXml.charAt(openTagEnd - 1) == '/') { // self-closing, no content
    searchPos = openTagEnd + 1;
    return String();
  }
  String closeNeedle = String("/") + localName;
  int closeIdx = lowerXml.indexOf(closeNeedle, openTagEnd);
  if (closeIdx < 0) { searchPos = lowerXml.length(); return String(); }
  int closeTagGt = lowerXml.indexOf('>', closeIdx);
  if (closeTagGt < 0) { searchPos = lowerXml.length(); return String(); }
  int closeTagLt = lowerXml.lastIndexOf('<', closeTagGt);
  searchPos = closeTagGt + 1;
  if (closeTagLt <= openTagEnd) return String();
  return xml.substring(openTagEnd + 1, closeTagLt);
}

static String extract_first_text(const String &xml, const char *localName) {
  String lowerXml = xml; lowerXml.toLowerCase();
  int pos = 0;
  return extract_block(xml, lowerXml, localName, pos);
}

// ---------------------------------------------------------------------------
// CalDAV request with basic-auth and manual redirect handling. iCloud routes
// the well-known https://caldav.icloud.com/ endpoint to a region-specific
// pNN-caldav.icloud.com host via a 301, so every request has to be able to
// follow that once.
// ---------------------------------------------------------------------------
static bool caldav_request(String url, const char *method, const char *depth, const String &body,
                            int &statusOut, String &responseOut, String &finalUrlOut) {
  for (int redirect = 0; redirect < 5; redirect++) {
    WiFiClientSecure client;
    client.setInsecure();
    NetworkGuard guard;
    HTTPClient http;
    if (!http.begin(client, url)) return false;
    http.setAuthorization(g_settings.icloudEmail.c_str(), g_settings.icloudAppPassword.c_str());
    if (depth) http.addHeader("Depth", depth);
    http.addHeader("Content-Type", "application/xml; charset=utf-8");
    const char *headerKeys[] = { "Location" };
    http.collectHeaders(headerKeys, 1);
    int code = http.sendRequest(method, (uint8_t *)body.c_str(), body.length());
    if (code == 301 || code == 302 || code == 307 || code == 308) {
      String location = http.header("Location");
      http.end();
      if (location.isEmpty()) return false;
      url = location;
      continue;
    }
    statusOut = code;
    responseOut = http.getString();
    finalUrlOut = url;
    http.end();
    return code > 0;
  }
  return false;
}

static String host_of(const String &url) {
  int pathStart = url.indexOf('/', 8); // skip past "https://"
  return pathStart < 0 ? url : url.substring(0, pathStart);
}

static String resolve_href(const String &href, const String &baseHost) {
  if (href.startsWith("http")) return href;
  return baseHost + href;
}

// ---------------------------------------------------------------------------
// One-time discovery: principal -> calendar-home-set -> the first real
// calendar collection in it. The result is cached in NVS so this only runs
// again if credentials change or the calendar disappears.
// ---------------------------------------------------------------------------
static bool discover_calendar() {
  int status;
  String resp, finalUrl;

  String principalBody = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                          "<D:propfind xmlns:D=\"DAV:\"><D:prop><D:current-user-principal/></D:prop></D:propfind>";
  if (!caldav_request(CALDAV_ROOT, "PROPFIND", "0", principalBody, status, resp, finalUrl) || status < 200 || status >= 300) {
    Serial.printf("calendar: principal discovery failed (%d)\n", status);
    return false;
  }
  String principalHref = extract_first_text(resp, "href");
  if (principalHref.isEmpty()) { Serial.println("calendar: no principal href in response"); return false; }
  String principalUrl = resolve_href(principalHref, host_of(finalUrl));

  String homeBody = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                     "<D:propfind xmlns:D=\"DAV:\" xmlns:C=\"urn:ietf:params:xml:ns:caldav\">"
                     "<D:prop><C:calendar-home-set/></D:prop></D:propfind>";
  if (!caldav_request(principalUrl, "PROPFIND", "0", homeBody, status, resp, finalUrl) || status < 200 || status >= 300) {
    Serial.printf("calendar: calendar-home discovery failed (%d)\n", status);
    return false;
  }
  String homeHref = extract_first_text(resp, "href");
  if (homeHref.isEmpty()) { Serial.println("calendar: no calendar-home href in response"); return false; }
  String homeUrl = resolve_href(homeHref, host_of(finalUrl));

  String listBody = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                     "<D:propfind xmlns:D=\"DAV:\"><D:prop><D:resourcetype/><D:displayname/></D:prop></D:propfind>";
  if (!caldav_request(homeUrl, "PROPFIND", "1", listBody, status, resp, finalUrl) || status < 200 || status >= 300) {
    Serial.printf("calendar: calendar list failed (%d)\n", status);
    return false;
  }
  String baseHost = host_of(finalUrl);
  String lowerResp = resp; lowerResp.toLowerCase();
  int pos = 0;
  String chosenHref;
  while (true) {
    String block = extract_block(resp, lowerResp, "response", pos);
    if (block.isEmpty()) break;
    String blockLower = block; blockLower.toLowerCase();
    if (blockLower.indexOf("calendar/>") < 0) continue;
    if (blockLower.indexOf("schedule-inbox") >= 0 || blockLower.indexOf("schedule-outbox") >= 0) continue;
    String href = extract_first_text(block, "href");
    if (href.isEmpty() || href == homeHref) continue;
    chosenHref = href;
    break;
  }
  if (chosenHref.isEmpty()) { Serial.println("calendar: no calendar collection found under home"); return false; }
  g_settings.icloudCalendarUrl = resolve_href(chosenHref, baseHost);
  settings_save();
  Serial.printf("calendar: discovered %s\n", g_settings.icloudCalendarUrl.c_str());
  return true;
}

static String format_utc_basic(time_t value) {
  struct tm timeValue;
  gmtime_r(&value, &timeValue);
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y%m%dT%H%M%SZ", &timeValue);
  return String(buffer);
}

struct CalendarEntry {
  time_t when;
  bool allDay;
  String text;
};

static bool fetch_events() {
  if (g_settings.icloudCalendarUrl.isEmpty()) return false;
  time_t now = time(nullptr);
  if (now < 1704067200) return false; // wait for a valid clock

  String body = String("<?xml version=\"1.0\" encoding=\"utf-8\" ?>") +
    "<C:calendar-query xmlns:D=\"DAV:\" xmlns:C=\"urn:ietf:params:xml:ns:caldav\">"
    "<D:prop><C:calendar-data/></D:prop>"
    "<C:filter><C:comp-filter name=\"VCALENDAR\"><C:comp-filter name=\"VEVENT\">"
    "<C:time-range start=\"" + format_utc_basic(now - 3600) + "\" end=\"" + format_utc_basic(now + 14 * 86400) + "\"/>"
    "</C:comp-filter></C:comp-filter></C:filter></C:calendar-query>";

  int status;
  String resp, finalUrl;
  if (!caldav_request(g_settings.icloudCalendarUrl, "REPORT", "1", body, status, resp, finalUrl)) return false;
  if (status < 200 || status >= 300) {
    Serial.printf("calendar: event fetch HTTP %d\n", status);
    if (status == 404 || status == 410) { g_settings.icloudCalendarUrl = ""; settings_save(); }
    return false;
  }

  static const int maxEntries = 24;
  CalendarEntry entries[maxEntries];
  int entryCount = 0;

  String lowerResp = resp; lowerResp.toLowerCase();
  int pos = 0;
  while (entryCount < maxEntries) {
    String calendarData = extract_block(resp, lowerResp, "calendar-data", pos);
    if (calendarData.isEmpty()) break;

    int searchFrom = 0;
    while (entryCount < maxEntries) {
      int vStart = calendarData.indexOf("BEGIN:VEVENT", searchFrom);
      if (vStart < 0) break;
      int vEnd = calendarData.indexOf("END:VEVENT", vStart);
      if (vEnd < 0) break;
      String vevent = calendarData.substring(vStart, vEnd);
      searchFrom = vEnd + 10;

      String summary = "(no title)";
      int summaryIdx = vevent.indexOf("\nSUMMARY");
      if (summaryIdx >= 0) {
        int colon = vevent.indexOf(':', summaryIdx);
        int lineEnd = vevent.indexOf('\n', summaryIdx + 1);
        if (colon >= 0 && (lineEnd < 0 || colon < lineEnd)) {
          int end = lineEnd < 0 ? vevent.length() : lineEnd;
          summary = vevent.substring(colon + 1, end);
          summary.trim();
        }
      }

      int startIdx = vevent.indexOf("\nDTSTART");
      if (startIdx < 0) continue;
      int colon = vevent.indexOf(':', startIdx);
      if (colon < 0) continue;
      int lineEnd = vevent.indexOf('\n', startIdx + 1);
      int end = lineEnd < 0 ? vevent.length() : lineEnd;
      String value = vevent.substring(colon + 1, end);
      value.trim();

      struct tm timeValue = {};
      bool allDay = value.length() == 8;
      if (allDay) {
        timeValue.tm_year = value.substring(0, 4).toInt() - 1900;
        timeValue.tm_mon = value.substring(4, 6).toInt() - 1;
        timeValue.tm_mday = value.substring(6, 8).toInt();
      } else if (value.length() >= 15) {
        timeValue.tm_year = value.substring(0, 4).toInt() - 1900;
        timeValue.tm_mon = value.substring(4, 6).toInt() - 1;
        timeValue.tm_mday = value.substring(6, 8).toInt();
        timeValue.tm_hour = value.substring(9, 11).toInt();
        timeValue.tm_min = value.substring(11, 13).toInt();
        timeValue.tm_sec = value.substring(13, 15).toInt();
      } else {
        continue;
      }
      time_t when = mktime(&timeValue);
      if (when < now - 3600) continue;

      entries[entryCount].when = when;
      entries[entryCount].allDay = allDay;
      entries[entryCount].text = summary.substring(0, 30);
      entryCount++;
    }
  }

  for (int i = 1; i < entryCount; i++) {
    CalendarEntry key = entries[i];
    int j = i - 1;
    while (j >= 0 && entries[j].when > key.when) { entries[j + 1] = entries[j]; j--; }
    entries[j + 1] = key;
  }

  String result;
  int shown = min(entryCount, 6);
  if (shown == 0) {
    result = "No upcoming events in the next 14 days";
  } else {
    for (int i = 0; i < shown; i++) {
      struct tm timeValue;
      localtime_r(&entries[i].when, &timeValue);
      char line[64];
      if (entries[i].allDay) {
        snprintf(line, sizeof(line), "%02d/%02d  All day  %s", timeValue.tm_mon + 1, timeValue.tm_mday, entries[i].text.c_str());
      } else {
        snprintf(line, sizeof(line), "%02d/%02d %02d:%02d  %s", timeValue.tm_mon + 1, timeValue.tm_mday, timeValue.tm_hour, timeValue.tm_min, entries[i].text.c_str());
      }
      result += line;
      if (i < shown - 1) result += "\n";
    }
  }

  struct tm nowTm;
  char stamp[32] = "";
  if (localtime_r(&now, &nowTm)) strftime(stamp, sizeof(stamp), "Updated %I:%M:%S %p", &nowTm);

  if (xSemaphoreTake(calendar_mutex, portMAX_DELAY) == pdTRUE) {
    display_text = result;
    updated_text = stamp;
    xSemaphoreGive(calendar_mutex);
  }
  return true;
}

static void calendar_task(void *) {
  // Wait for user to click Calendar tab to start fetching
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  
  for (;;) {
    bool success = false;
    if (g_settings.icloudEmail.isEmpty() || g_settings.icloudAppPassword.isEmpty()) {
      if (xSemaphoreTake(calendar_mutex, portMAX_DELAY) == pdTRUE) {
        display_text = "iCloud calendar is not configured";
        xSemaphoreGive(calendar_mutex);
      }
    } else if (WiFi.status() == WL_CONNECTED) {
      success = true;
      if (g_settings.icloudCalendarUrl.isEmpty()) success = discover_calendar();
      if (success) success = fetch_events();
      if (!success && xSemaphoreTake(calendar_mutex, portMAX_DELAY) == pdTRUE) {
        display_text = "Unable to reach iCloud calendar";
        xSemaphoreGive(calendar_mutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(success ? 900000UL : 120000UL));
  }
}

void calendar_begin() {
  calendar_mutex = xSemaphoreCreateMutex();
  display_text = "Calendar (click to load)";
}

void calendar_start_fetch() {
  if (!calendar_task_handle) {
    xTaskCreatePinnedToCore(calendar_task, "calendar", 12288, nullptr, 1, &calendar_task_handle, 0);
  } else {
    xTaskNotifyGive(calendar_task_handle);
  }
}

void calendar_tick() {}

String calendar_display() {
  String copy;
  if (calendar_mutex && xSemaphoreTake(calendar_mutex, portMAX_DELAY) == pdTRUE) {
    copy = display_text;
    xSemaphoreGive(calendar_mutex);
  }
  return copy;
}

String calendar_updated_display() {
  String copy;
  if (calendar_mutex && xSemaphoreTake(calendar_mutex, portMAX_DELAY) == pdTRUE) {
    copy = updated_text;
    xSemaphoreGive(calendar_mutex);
  }
  return copy;
}
