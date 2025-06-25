#pragma once

bool serializeSensorData(uint8_t *buffer, size_t *length, float temp, float hum, uint64_t timestamp, uint64_t interv);
bool deserializeSensorData(const uint8_t *buffer, size_t length, SensorData *data);