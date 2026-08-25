#pragma once

#include <Arduino.h>

void ssh_terminal_begin();
void ssh_terminal_tick();
String ssh_terminal_output();
void ssh_terminal_send(const String &command);
void ssh_terminal_connect_now();