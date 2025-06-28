#include "ble_log.h"
#include "ble_live.h"
#include "ble_gatt.h"

#define LOG_NAMESPACE "log_data"
#define MAX_ENTRIES   100


static const char *TAG = "LOG_BLE";
uint16_t log_char_handle = 0;
uint16_t log_ctrl_char_handle = 0;

static bool log_transmission_active = false;
static bool log_sending_in_progress = false;

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
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Nenhum log encontrado na NVS.");
            log_count = 0;
        } else {
            ESP_LOGE(TAG, "Erro ao ler log da NVS: %s", esp_err_to_name(err));
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

void ble_notify_log_ctrl(uint32_t total_entries) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) return;

    LogControl ctrl = LogControl_init_zero;
    ctrl.command = LogControl_Command_START;
    ctrl.total_entries = total_entries;

    uint8_t buffer[64];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

    if (pb_encode(&stream, LogControl_fields, &ctrl)) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(buffer, stream.bytes_written);
        if (om) {
            ble_gattc_notify_custom(conn_handle, log_ctrl_char_handle, om);
            ESP_LOGI(TAG, "Notify LogControl enviado (total_entries: %d)", total_entries);
        }
    } else {
        ESP_LOGE(TAG, "Erro na serialização do LogControl");
    }
}

void ble_send_log_via_notify(void) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGW(TAG, "Sem conexão BLE ativa.");
        return;
    }

    if (log_sending_in_progress) {
        ESP_LOGW(TAG, "Log já está sendo enviado.");
        return;
    }

    log_sending_in_progress = true;

    size_t index = 0;
    size_t next_index = 0;

    while (index < log_ble_get_total_entries()) {
        uint8_t buffer[256];
        size_t len = log_ble_get_packet(buffer, sizeof(buffer), index, &next_index);

        if (len > 0) {
            struct os_mbuf *om = ble_hs_mbuf_from_flat(buffer, len);
            if (om) {
                ble_gattc_notify_custom(conn_handle, log_char_handle, om);
                ESP_LOGI(TAG, "Notify Log (%d bytes) enviado. Index: %d", (int)len, (int)index);
            }
        }

        index = next_index;

        // Pequeno delay para não saturar o link BLE
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    ESP_LOGI(TAG, "Envio de log concluído.");
    log_sending_in_progress = false;
}

// ==============================
// Callback Log GATT
// ==============================
int log_gatt_access_cb(uint16_t conn, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (attr_handle == log_char_handle) {
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }

    if (attr_handle == log_ctrl_char_handle) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            LogControl ctrl = LogControl_init_zero;
            uint8_t temp_buf[32];
            uint16_t data_len = OS_MBUF_PKTLEN(ctxt->om);

            os_mbuf_copydata(ctxt->om, 0, data_len, temp_buf);

            pb_istream_t stream = pb_istream_from_buffer(temp_buf, data_len);
            if (pb_decode(&stream, LogControl_fields, &ctrl)) {
                ESP_LOGI(TAG, "Comando LogControl recebido: %d", ctrl.command);

                switch (ctrl.command) {
                case LogControl_Command_START:
                    log_transmission_active = true;
                    ESP_LOGI(TAG, "Log START");

                    // Envia total de registros antes de iniciar envio
                    ble_notify_log_ctrl(log_ble_get_total_entries());
                    ble_send_log_via_notify();
                    break;

                case LogControl_Command_STOP:
                    log_transmission_active = false;
                    ESP_LOGI(TAG, "Log STOP");
                    break;

                case LogControl_Command_CLEAR:
                    log_ble_clear();
                    ESP_LOGI(TAG, "Log CLEAR");
                    break;

                default:
                    ESP_LOGW(TAG, "Comando desconhecido: %d", ctrl.command);
                    break;
                }
                return 0;
            } else {
                ESP_LOGE(TAG, "Falha na desserialização LogControl");
                return BLE_ATT_ERR_UNLIKELY;
            }
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

