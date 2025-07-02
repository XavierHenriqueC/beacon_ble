#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "sensor.pb.h"

// Inicializa a NVS
esp_err_t nvs_controller_init(void);

// Série temporal SensorData
esp_err_t nvs_save_sensor_data(float temp, float hum);
esp_err_t nvs_read_all_sensor_data(SensorData *out_array, size_t max_items, size_t *read_items);
esp_err_t nvs_get_sensor_data_count(uint32_t *count);
esp_err_t nvs_clear_all_sensor_data(void);

// Configuração SensorConfig
esp_err_t nvs_save_sensor_config(SensorConfig *cfg);
esp_err_t nvs_update_sensor_config(SensorConfig *cfg);
SensorConfig nvs_read_sensor_config(void);
void load_sensor_config(void);