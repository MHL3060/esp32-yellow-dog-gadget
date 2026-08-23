#include "time_manager.h"
#include <Arduino.h>

void time_manager_begin(const char *tz) {
  configTzTime(tz, "pool.ntp.org", "time.nist.gov");
}

bool time_manager_now(struct tm &out) {
  return getLocalTime(&out, 10) && out.tm_year >= (2024 - 1900);
}
