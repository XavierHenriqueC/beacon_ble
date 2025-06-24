#include "log_ble.h"
#include "sensor.pb.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "pb_encode.h"

#define LOG_NAMESPACE "log_data"
#define MAX_ENTRIES   100

static const char *TAG = "LOG_BLE";

typedef struct {
    float temperature;
    float humidity;
    uint64_t timestamp;
} LogEntry;

static LogEntry log_entries[MAX_ENTRIES];
static size_t log_count = 0;

void log_ble_init(uint64_t *interval_ptr) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(LOG_NAMESPACE, NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        size_t required_size = sizeof(log_entries);
        err = nvs_get_blob(nvs, "entries", log_entries, &required_size);
        if (err == ESP_OK) {
            log_count = required_size / sizeof(LogEntry);
            ESP_LOGI(TAG, "Log restaurado com %d entradas", (int)log_count);
        }
        nvs_close(nvs);
    }
}

void log_ble_add_entry(float temp, float hum, uint64_t timestamp) {
    if (log_count < MAX_ENTRIES) {
        log_entries[log_count++] = (LogEntry){temp, hum, timestamp};
    } else {
        memmove(&log_entries[0], &log_entries[1], sizeof(LogEntry) * (MAX_ENTRIES - 1));
        log_entries[MAX_ENTRIES - 1] = (LogEntry){temp, hum, timestamp};
    }

    nvs_handle_t nvs;
    if (nvs_open(LOG_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_blob(nvs, "entries", log_entries, log_count * sizeof(LogEntry));
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

void log_ble_clear(void) {
    log_count = 0;
    memset(log_entries, 0, sizeof(log_entries));

    nvs_handle_t nvs;
    if (nvs_open(LOG_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_erase_all(nvs);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

size_t log_ble_get_packet(uint8_t *buffer, size_t buffer_size, size_t start_index, size_t *next_index) {
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, buffer_size);
    size_t i;

    for (i = start_index; i < log_count; i++) {
        SensorData entry = SensorData_init_zero;
        entry.temperature = log_entries[i].temperature;
        entry.humidity = log_entries[i].humidity;
        entry.timestamp = log_entries[i].timestamp;
        entry.interval = 0;

        pb_size_t current_size = stream.bytes_written;
        if (!pb_encode_delimited(&stream, SensorData_fields, &entry)) {
            stream.bytes_written = current_size;
            break;
        }
    }

    *next_index = i;
    return stream.bytes_written;
}

size_t log_ble_get_total_entries(void) {
    return log_count;
}
