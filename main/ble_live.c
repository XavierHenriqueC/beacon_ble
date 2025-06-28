#include "ble_live.h"
#include "ble_gatt.h"
#include "ble_log.h"
#include "temp_hum.h"
#include "serial.h"

static const char *TAG = "BLE_LIVE";

uint64_t interval = 60;
uint16_t temp_char_handle;
uint16_t interval_char_handle;

// ==============================
// Notify BLE
// ==============================
void ble_notify_sensor(void) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) return;

    uint8_t buffer[64];
    size_t len = sizeof(buffer);

    if (serializeSensorData(buffer, &len, get_temperature(), get_humidity(),
                            esp_timer_get_time() / 1000, interval)) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(buffer, len);
        if (om) {
            ble_gattc_notify_custom(conn_handle, temp_char_handle, om);
            ESP_LOGI(TAG, "Notify Temp/Hum enviado");
        }
    }
}

void ble_notify_interval(void) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) return;

    uint8_t buffer[64];
    size_t len = sizeof(buffer);

    if (serializeSensorData(buffer, &len, get_temperature(), get_humidity(),
                            esp_timer_get_time() / 1000, interval)) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(buffer, len);
        if (om) {
            ble_gattc_notify_custom(conn_handle, interval_char_handle, om);
            ESP_LOGI(TAG, "Notify Interval enviado");
        }
    }
}


// ==============================
// Callback GATT
// ==============================
int gatt_svr_access_cb(uint16_t conn, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    uint8_t buffer[64];
    size_t len = sizeof(buffer);

    if (attr_handle == temp_char_handle) {
        if (serializeSensorData(buffer, &len, get_temperature(), get_humidity(),
                                esp_timer_get_time() / 1000, interval)) {
            os_mbuf_append(ctxt->om, buffer, len);
            ESP_LOGI(TAG, "Read Temp/Hum");
            return 0;
        }
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (attr_handle == interval_char_handle) {
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            if (serializeSensorData(buffer, &len, get_temperature(), get_humidity(),
                                    esp_timer_get_time() / 1000, interval)) {
                os_mbuf_append(ctxt->om, buffer, len);
                ESP_LOGI(TAG, "Read Interval");
                return 0;
            }
            return BLE_ATT_ERR_UNLIKELY;

        case BLE_GATT_ACCESS_OP_WRITE_CHR: {
            SensorData data = SensorData_init_zero;
            uint16_t data_len = OS_MBUF_PKTLEN(ctxt->om);
            uint8_t temp_buf[64];

            os_mbuf_copydata(ctxt->om, 0, data_len, temp_buf);

            if (deserializeSensorData(temp_buf, data_len, &data)) {
                interval = data.interval;
                ESP_LOGI(TAG, "Interval atualizado via BLE: %llu", interval);

                ble_notify_interval();
                return 0;
            } else {
                ESP_LOGE(TAG, "Erro desserializando Interval.");
                return BLE_ATT_ERR_UNLIKELY;
            }
        }
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}



