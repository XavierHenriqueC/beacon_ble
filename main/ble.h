#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "sensor.pb.h"

void ble_init(void);
void ble_start(void);
void ble_host_task(void *param);
void ble_notify_sensor(void);

bool serializeSensorData(uint8_t *buffer, size_t *length, float temp, float hum, uint64_t timestamp, uint64_t interv);
bool deserializeSensorData(const uint8_t *buffer, size_t length, SensorData *data);
