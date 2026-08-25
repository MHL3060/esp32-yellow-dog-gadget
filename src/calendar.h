#pragma once

#include <Arduino.h>

void calendar_begin();
void calendar_tick();
String calendar_display();
String calendar_updated_display();
void calendar_start_fetch();
