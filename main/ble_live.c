#include "ble_live.h"
#include "ble_gatt.h"
#include "ble_log.h"
#include "temp_hum.h"
#include "serial.h"
#include "nvs_controller.h"

static const char *TAG = "BLE_LIVE";

uint64_t interval;
uint64_t log_mode = 1; //Always
uint16_t date_time_init = 0;
uint16_t date_time_stop = 0;

uint16_t temp_char_handle;
uint16_t config_char_handle;


// ==============================
// Notify BLE
// ==============================
void ble_notify_sensor(void) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) return;

    uint8_t buffer[64];
    size_t len = sizeof(buffer);

    if (serializeSensorData(buffer, &len, get_temperature(), get_humidity())) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(buffer, len);
        if (om) {
            ble_gattc_notify_custom(conn_handle, temp_char_handle, om);
            ESP_LOGI(TAG, "Notify Temp/Hum enviado");
        }
    }
}


void ble_notify_config(void) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) return;

    SensorConfig cfg = SensorConfig_init_zero;
    cfg.interval = interval;
    cfg.log_mode = log_mode;
    cfg.date_time_init = date_time_init;
    cfg.date_time_stop = date_time_stop;

    uint8_t buffer[64];
    size_t len = sizeof(buffer);

    if (serializeSensorConfig(buffer, &len, &cfg)) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(buffer, len);
        if (om) {
            ble_gattc_notify_custom(conn_handle, config_char_handle, om);
            ESP_LOGI(TAG, "Notify Config enviado");
        }
    }
}


// ==============================
// Callback GATT
// ==============================
int gatt_svr_access_cb(
    uint16_t conn, 
    uint16_t attr_handle,
    struct ble_gatt_access_ctxt *ctxt, 
    void *arg
) {
    uint8_t buffer[64];
    size_t len = sizeof(buffer);

    if (attr_handle == temp_char_handle) {
        if (serializeSensorData(buffer, &len, get_temperature(), get_humidity())) {
            os_mbuf_append(ctxt->om, buffer, len);
            ESP_LOGI(TAG, "Read Temp/Hum");
            return 0;
        }
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (attr_handle == config_char_handle) {
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR: {
            SensorConfig cfg = SensorConfig_init_zero;
            cfg.interval = interval;
            cfg.log_mode = log_mode;
            cfg.date_time_init = date_time_init;
            cfg.date_time_stop = date_time_stop;

            if (serializeSensorConfig(buffer, &len, &cfg)) {
                os_mbuf_append(ctxt->om, buffer, len);
                ESP_LOGI(TAG, "Read config");
                return 0;
            }
            return BLE_ATT_ERR_UNLIKELY;
        }

        case BLE_GATT_ACCESS_OP_WRITE_CHR: {
            SensorConfig data = SensorConfig_init_zero;

            uint16_t data_len = OS_MBUF_PKTLEN(ctxt->om);
            uint8_t temp_buf[64];
            os_mbuf_copydata(ctxt->om, 0, data_len, temp_buf);

            if (deserializeSensorConfig(temp_buf, data_len, &data)) {
                interval = data.interval;
                log_mode = data.log_mode;
                date_time_init = data.date_time_init;
                date_time_stop = data.date_time_stop;

                nvs_save_sensor_config(&data); // Salva o que recebeu

                ESP_LOGI(TAG, 
                    "Configurações atualizadas via BLE:\nInterval: %llu\nLog_mode: %d\nDate_time_init: %llu\nDate_time_stop: %llu", 
                    interval, 
                    log_mode, 
                    date_time_init, 
                    date_time_stop
                );

                ble_notify_config();
                return 0;
            } else {
                ESP_LOGE(TAG, "Erro desserializando Config.");
                return BLE_ATT_ERR_UNLIKELY;
            }
        }
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}



