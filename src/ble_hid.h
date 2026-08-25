#pragma once

#include <Arduino.h>

// BLE HID device scanning and connection
void ble_hid_begin();
void ble_hid_tick();

// Scan for BLE HID devices (blocking, returns count of found devices)
int ble_hid_scan(uint32_t scan_duration_ms = 10000);

// Get discovered device info by index
bool ble_hid_get_device(int index, String &name, String &address);

// Connect to a discovered device by index
bool ble_hid_connect(int index);

// Get connection status
bool ble_hid_is_connected();
String ble_hid_connected_device();

// Disconnect
void ble_hid_disconnect();

// Get last key press
bool ble_hid_get_key(uint8_t &key_code, bool &is_pressed);
