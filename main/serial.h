#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "sensor.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "esp_timer.h"
#include "esp_log.h"

// SensorData
bool serializeSensorData(uint8_t *buffer, size_t *length, float temp, float hum);
bool serializeSensorDataFromStruct(uint8_t *buffer, size_t *length, const SensorData *data);
bool deserializeSensorData(const uint8_t *buffer, size_t length, SensorData *data);

// SensorConfig
bool serializeSensorConfig(uint8_t *buffer, size_t *len, const SensorConfig *cfg);
bool deserializeSensorConfig(const uint8_t *buffer, size_t length, SensorConfig *data);

// LogControl
bool serializeLogControl(uint8_t *buffer, size_t *length, LogControl_Command command, uint32_t length_val);
bool deserializeLogControl(const uint8_t *buffer, size_t length, LogControl *data);

