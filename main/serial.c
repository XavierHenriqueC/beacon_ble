#include "serial.h"
#include "sensor.pb.h"           // Definições Protobuf: SensorData, SensorConfig, LogControl
#include "esp_timer.h"           // Para esp_timer_get_time()
#include "esp_log.h"             // Para ESP_LOGE
#include "pb_encode.h"
#include "pb_decode.h"

static const char *TAG = "SERIAL";

// ==============================
// Serialização Protobuf
// ==============================

// --- SensorData com timestamp atual (para coleta) ---
bool serializeSensorData(uint8_t *buffer, size_t *length, float temp, float hum) {
    if (!buffer || !length) {
        ESP_LOGE(TAG, "Parâmetros inválidos em serializeSensorData");
        return false;
    }

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, *length);
    SensorData data = SensorData_init_zero;

    data.timestamp = esp_timer_get_time() / 1000000ULL; // Em segundos
    data.temperature = temp;
    data.humidity = hum;

    if (!pb_encode(&stream, SensorData_fields, &data)) {
        ESP_LOGE(TAG, "Erro na serialização SensorData: %s", PB_GET_ERROR(&stream));
        return false;
    }

    *length = stream.bytes_written;
    return true;
}

// --- SensorData a partir de estrutura existente (preserva timestamp) ---
bool serializeSensorDataFromStruct(uint8_t *buffer, size_t *length, const SensorData *data_in) {
    if (!buffer || !length || !data_in) {
        ESP_LOGE(TAG, "Parâmetros inválidos em serializeSensorDataFromStruct");
        return false;
    }

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, *length);

    if (!pb_encode(&stream, SensorData_fields, data_in)) {
        ESP_LOGE(TAG, "Erro na serialização SensorData (from struct): %s", PB_GET_ERROR(&stream));
        return false;
    }

    *length = stream.bytes_written;
    return true;
}

bool deserializeSensorData(const uint8_t *buffer, size_t length, SensorData *data) {
    if (!buffer || !data) {
        ESP_LOGE(TAG, "Parâmetros inválidos em deserializeSensorData");
        return false;
    }

    pb_istream_t stream = pb_istream_from_buffer(buffer, length);
    if (!pb_decode(&stream, SensorData_fields, data)) {
        ESP_LOGE(TAG, "Erro na desserialização SensorData: %s", PB_GET_ERROR(&stream));
        return false;
    }

    return true;
}

// --- SensorConfig ---
bool serializeSensorConfig(uint8_t *buffer, size_t *len, const SensorConfig *cfg)
{
    SensorConfig proto = SensorConfig_init_zero;
    proto.interval = cfg->interval;
    proto.log_mode = cfg->log_mode;
    proto.date_time_init = cfg->date_time_init;
    proto.date_time_stop = cfg->date_time_stop;

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, *len);
    if (!pb_encode(&stream, SensorConfig_fields, &proto))
        return false;

    *len = stream.bytes_written;
    return true;
}

bool deserializeSensorConfig(const uint8_t *buffer, size_t length, SensorConfig *data) {
    if (!buffer || !data) {
        ESP_LOGE(TAG, "Parâmetros inválidos em deserializeSensorConfig");
        return false;
    }

    pb_istream_t stream = pb_istream_from_buffer(buffer, length);
    if (!pb_decode(&stream, SensorConfig_fields, data)) {
        ESP_LOGE(TAG, "Erro na desserialização SensorConfig: %s", PB_GET_ERROR(&stream));
        return false;
    }

    return true;
}

// --- LogControl ---
bool serializeLogControl(uint8_t *buffer, size_t *length, LogControl_Command command, uint32_t length_val) {
    if (!buffer || !length) {
        ESP_LOGE(TAG, "Parâmetros inválidos em serializeLogControl");
        return false;
    }

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, *length);
    LogControl data = LogControl_init_zero;

    data.command = command;
    data.length = length_val;

    if (!pb_encode(&stream, LogControl_fields, &data)) {
        ESP_LOGE(TAG, "Erro na serialização LogControl: %s", PB_GET_ERROR(&stream));
        return false;
    }

    *length = stream.bytes_written;
    return true;
}

bool deserializeLogControl(const uint8_t *buffer, size_t length, LogControl *data) {
    if (!buffer || !data) {
        ESP_LOGE(TAG, "Parâmetros inválidos em deserializeLogControl");
        return false;
    }

    pb_istream_t stream = pb_istream_from_buffer(buffer, length);
    if (!pb_decode(&stream, LogControl_fields, data)) {
        ESP_LOGE(TAG, "Erro na desserialização LogControl: %s", PB_GET_ERROR(&stream));
        return false;
    }

    return true;
}
