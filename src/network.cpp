#include "network.h"
#include <Arduino.h>

static SemaphoreHandle_t network_mutex;

void network_begin() {
  network_mutex = xSemaphoreCreateMutex();
}

void network_lock() {
  if (network_mutex) xSemaphoreTake(network_mutex, portMAX_DELAY);
}

void network_unlock() {
  if (network_mutex) xSemaphoreGive(network_mutex);
}
