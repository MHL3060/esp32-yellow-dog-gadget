#pragma once
#include <time.h>
void time_manager_begin(const char *tz);
bool time_manager_now(struct tm &out);
bool time_manager_second_elapsed();
