#include "ble_hid.h"
#include <NimBLEDevice.h>
#include <queue>
#include <vector>

struct ScannedDevice {
  String name;
  String address;
  int rssi;
};

struct KeyEvent {
  uint8_t key_code;
  bool is_pressed;
};

static std::vector<ScannedDevice> scanned_devices;
static std::queue<KeyEvent> key_events;
static NimBLEClient *connected_client = nullptr;
static String connected_device_name;
static NimBLERemoteCharacteristic *keyboard_input = nullptr;
static bool scanning = false;

// Callback class for discovered devices
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
public:
  void onResult(NimBLEAdvertisedDevice *advDevice) override {
    // Look for HID service (UUID 0x1812)
    if (advDevice->haveServiceUUID()) {
      NimBLEUUID svcUUID = advDevice->getServiceUUID();
      if (svcUUID.equals(NimBLEUUID((uint16_t)0x1812))) {
        if (advDevice->haveName() && scanned_devices.size() < 20) {
          ScannedDevice dev;
          dev.name = String(advDevice->getName().c_str());
          dev.address = String(advDevice->getAddress().toString().c_str());
          dev.rssi = advDevice->getRSSI();
          scanned_devices.push_back(dev);
          Serial.printf("[BLE] Found HID device: %s (%s, RSSI: %d)\n",
            dev.name.c_str(), dev.address.c_str(), dev.rssi);
        }
      }
    }
  }
};

static AdvertisedDeviceCallbacks advertised_device_callbacks;

// Callback class for HID client
class HIDClientCallbacks : public NimBLEClientCallbacks {
public:
  void onConnect(NimBLEClient *pClient) override {
    Serial.printf("[BLE] Connected to %s\n", connected_device_name.c_str());
  }

  void onDisconnect(NimBLEClient *pClient) override {
    Serial.println("[BLE] Disconnected from keyboard");
    connected_client = nullptr;
    keyboard_input = nullptr;
  }
};

static HIDClientCallbacks hid_client_callbacks;

// Global notify callback for HID reports
static void hid_notify_callback(NimBLERemoteCharacteristic *pRemoteCharacteristic,
                                 uint8_t *pData, size_t length, bool isNotify) {
  if (length < 8) return;
  
  // HID keyboard report format (8 bytes):
  // Byte 0: Modifier keys (Ctrl, Shift, Alt, Win)
  // Byte 1: Reserved
  // Bytes 2-7: Key codes
  uint8_t *report = pData;
  
  for (int i = 2; i < 8; i++) {
    if (report[i] != 0x00) {
      KeyEvent evt;
      evt.key_code = report[i];
      evt.is_pressed = true;
      key_events.push(evt);
      Serial.printf("[BLE] Key pressed: 0x%02X\n", report[i]);
    }
  }
}

void ble_hid_begin() {
  // Initialize NimBLE device (empty string = not advertising)
  NimBLEDevice::init("");
  
  // Set security settings
  NimBLEDevice::setSecurityAuth(true, true, true);
  
  // Set TX power
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);
  
  Serial.println("[BLE] HID client initialized");
}

void ble_hid_tick() {
  // NimBLE callbacks handle most of the work in this implementation
  // This is here for future polling-based functionality if needed
}

int ble_hid_scan(uint32_t scan_duration_ms) {
  if (scanning) return scanned_devices.size();
  
  scanning = true;
  scanned_devices.clear();
  
  Serial.printf("[BLE] Starting scan for %u ms\n", scan_duration_ms);
  
  NimBLEScan *pScan = NimBLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(&advertised_device_callbacks, true);
  pScan->setInterval(45);
  pScan->setWindow(15);
  pScan->setActiveScan(true);
  
  // Scan for duration_ms / 1000 seconds (0 = forever)
  pScan->start(scan_duration_ms / 1000, nullptr, false);
  
  scanning = false;
  Serial.printf("[BLE] Scan complete. Found %d HID devices\n", (int)scanned_devices.size());
  return scanned_devices.size();
}

bool ble_hid_get_device(int index, String &name, String &address) {
  if (index >= 0 && index < (int)scanned_devices.size()) {
    name = scanned_devices[index].name;
    address = scanned_devices[index].address;
    return true;
  }
  return false;
}

bool ble_hid_connect(int index) {
  if (index < 0 || index >= (int)scanned_devices.size()) {
    Serial.println("[BLE] Invalid device index");
    return false;
  }
  
  // Disconnect existing client if any
  if (connected_client && connected_client->isConnected()) {
    Serial.println("[BLE] Disconnecting from previous device");
    connected_client->disconnect();
  }
  
  // Get or create a client
  if (!connected_client) {
    if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS) {
      Serial.println("[BLE] Max clients reached");
      return false;
    }
    connected_client = NimBLEDevice::createClient();
    if (!connected_client) {
      Serial.println("[BLE] Failed to create client");
      return false;
    }
    connected_client->setClientCallbacks(&hid_client_callbacks, false);
    connected_client->setConnectionParams(12, 12, 0, 51);
    connected_client->setConnectTimeout(5);
  }
  
  // Convert address string to NimBLEAddress
  NimBLEAddress targetAddr(scanned_devices[index].address.c_str());
  
  Serial.printf("[BLE] Connecting to %s (%s)\n", 
    scanned_devices[index].name.c_str(),
    scanned_devices[index].address.c_str());
  
  // Attempt connection directly with address
  if (!connected_client->connect(targetAddr)) {
    Serial.println("[BLE] Failed to connect");
    return false;
  }
  
  Serial.println("[BLE] Connected! Discovering HID service...");
  
  // Get HID service (UUID 0x1812)
  NimBLERemoteService *pSvc = connected_client->getService(NimBLEUUID((uint16_t)0x1812));
  if (!pSvc) {
    Serial.println("[BLE] HID service not found");
    connected_client->disconnect();
    return false;
  }
  
  Serial.println("[BLE] HID service found. Looking for keyboard input characteristic...");
  
  // Get keyboard input characteristic (UUID 0x2A4D = HID Report)
  // Note: We need to check all characteristics with this UUID since devices may report multiple
  std::vector<NimBLERemoteCharacteristic *> *charvector = pSvc->getCharacteristics(true);
  keyboard_input = nullptr;
  
  for (auto &it : *charvector) {
    if (it->getUUID().equals(NimBLEUUID((uint16_t)0x2A4D))) {
      Serial.printf("[BLE] Found keyboard characteristic at handle %d\n", it->getHandle());
      if (it->canNotify()) {
        keyboard_input = it;
        break;  // Use first keyboard input characteristic
      }
    }
  }
  
  if (!keyboard_input) {
    Serial.println("[BLE] Keyboard input characteristic not found or doesn't support notify");
    connected_client->disconnect();
    return false;
  }
  
  // Subscribe to notifications
  connected_device_name = scanned_devices[index].name;
  
  if (!keyboard_input->subscribe(true, hid_notify_callback)) {
    Serial.println("[BLE] Failed to subscribe to keyboard notifications");
    connected_client->disconnect();
    keyboard_input = nullptr;
    return false;
  }
  
  Serial.printf("[BLE] Successfully connected and subscribed to %s\n", connected_device_name.c_str());
  return true;
}

bool ble_hid_is_connected() {
  return connected_client != nullptr && connected_client->isConnected();
}

String ble_hid_connected_device() {
  return connected_device_name;
}

void ble_hid_disconnect() {
  if (connected_client) {
    if (connected_client->isConnected()) {
      Serial.printf("[BLE] Disconnecting from %s\n", connected_device_name.c_str());
      connected_client->disconnect();
    }
    connected_client = nullptr;
    keyboard_input = nullptr;
    connected_device_name = "";
  }
}

bool ble_hid_get_key(uint8_t &key_code, bool &is_pressed) {
  if (key_events.empty()) return false;
  
  KeyEvent evt = key_events.front();
  key_events.pop();
  key_code = evt.key_code;
  is_pressed = evt.is_pressed;
  return true;
}
