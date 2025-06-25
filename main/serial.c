#include <string.h>
#include "esp_log.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "sensor.pb.h"
#include "serial.h"

static const char *TAG = "SERIAL";

// ==============================
// Serialização Protobuf
// ==============================
bool serializeSensorData(uint8_t *buffer, size_t *length, float temp, float hum, uint64_t timestamp, uint64_t interv) {
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, *length);
    SensorData data = SensorData_init_zero;

    data.temperature = temp;
    data.humidity = hum;
    data.timestamp = timestamp;
    data.interval = interv;

    if (!pb_encode(&stream, SensorData_fields, &data)) {
        ESP_LOGE(TAG, "Erro na serialização Protobuf");
        return false;
    }

    *length = stream.bytes_written;
    return true;
}

bool deserializeSensorData(const uint8_t *buffer, size_t length, SensorData *data) {
    pb_istream_t stream = pb_istream_from_buffer(buffer, length);
    return pb_decode(&stream, SensorData_fields, data);
}
