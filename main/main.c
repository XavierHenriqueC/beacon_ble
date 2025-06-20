#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "sensor.pb.h"
#include "pb_encode.h"
#include "pb_decode.h"

static const char *TAG = "BEACON_BLE";

// Protótipos das funções
void ble_app_advertise(void);
void ble_host_task(void *param);
void ble_app_on_sync(void);

/// Dados simulados
float simulatedTemperature = 25.0;
float simulatedHumidity = 50.0;
uint64_t interval = 60;

/// Handle das características
uint16_t temp_char_handle;
uint16_t interval_char_handle;

/// Tipo de endereço BLE
static uint8_t own_addr_type;

/// ============================
/// Serializa dados usando Protobuf
/// ============================
bool serializeSensorData(uint8_t *buffer, size_t *length, float temp, float hum, uint64_t timestamp, uint64_t interv)
{
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, *length);
    SensorData data = SensorData_init_zero;

    data.temperature = temp;
    data.humidity = hum;
    data.timestamp = timestamp;
    data.interval = interv;

    if (!pb_encode(&stream, SensorData_fields, &data))
    {
        ESP_LOGE(TAG, "Falha na serialização!");
        return false;
    }

    *length = stream.bytes_written;
    return true;
}

/// ============================
/// Desserializa dados recebidos
/// ============================
bool deserializeSensorData(const uint8_t *buffer, size_t length, SensorData *data)
{
    pb_istream_t stream = pb_istream_from_buffer(buffer, length);
    return pb_decode(&stream, SensorData_fields, data);
}

/// ============================
/// Callback de eventos GATT Server
/// ============================
static int gatt_svr_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t buffer[64];
    size_t len = sizeof(buffer);

    if (attr_handle == temp_char_handle)
    {
        if (serializeSensorData(buffer, &len, simulatedTemperature, simulatedHumidity, esp_timer_get_time() / 1000, interval))
        {
            os_mbuf_append(ctxt->om, buffer, len);
            ESP_LOGI(TAG, "Temp/Hum enviado via BLE");
            return 0;
        }
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (attr_handle == interval_char_handle)
    {
        switch (ctxt->op)
        {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            if (serializeSensorData(buffer, &len, simulatedTemperature, simulatedHumidity, esp_timer_get_time() / 1000, interval))
            {
                os_mbuf_append(ctxt->om, buffer, len);
                ESP_LOGI(TAG, "Interval enviado via BLE");
                return 0;
            }
            return BLE_ATT_ERR_UNLIKELY;

        case BLE_GATT_ACCESS_OP_WRITE_CHR:
        {
            SensorData data = SensorData_init_zero;
            uint16_t data_len = OS_MBUF_PKTLEN(ctxt->om);
            uint8_t temp_buf[64];

            os_mbuf_copydata(ctxt->om, 0, data_len, temp_buf);

            if (deserializeSensorData(temp_buf, data_len, &data))
            {
                interval = data.interval;
                ESP_LOGI(TAG, "Novo interval recebido via BLE: %llu", interval);
                return 0;
            }
            else
            {
                ESP_LOGE(TAG, "Falha na desserialização do interval.");
                return BLE_ATT_ERR_UNLIKELY;
            }
        }
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

/// ============================
/// Serviço GATT
/// ============================
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(0x1809), // Health Thermometer
     .characteristics = (struct ble_gatt_chr_def[]){
         {.uuid = BLE_UUID16_DECLARE(0x2A1C), // Temperature Measurement
          .access_cb = gatt_svr_access_cb,
          .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
          .val_handle = &temp_char_handle},
         {.uuid = BLE_UUID16_DECLARE(0x2A1E), // Interval
          .access_cb = gatt_svr_access_cb,
          .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
          .val_handle = &interval_char_handle},
         {0}}},
    {0}};

/// ============================
/// Evento GAP (Conexão / Desconexão)
/// ============================
static int ble_gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0)
        {
            ESP_LOGI(TAG, "Dispositivo conectado.");
        }
        else
        {
            ESP_LOGI(TAG, "Falha na conexão; status=%d", event->connect.status);
            ble_app_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Dispositivo desconectado.");
        ble_app_advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "Advertising completo.");
        ble_app_advertise();
        break;

    default:
        break;
    }

    return 0;
}

/// ============================
/// Advertising BLE
/// ============================
void ble_app_advertise(void)
{
    struct ble_gap_adv_params adv_params = {0};

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    // Flags obrigatórios
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Nome do dispositivo
    fields.name = (uint8_t *)"Beacon_ESP32";
    fields.name_len = strlen("Beacon_ESP32");
    fields.name_is_complete = 1;

    // UUID do serviço (Health Thermometer = 0x1809)
    const uint16_t svc_uuid = 0x1809;
    fields.uuids16 = (ble_uuid16_t[]){
        {.u = {.type = BLE_UUID_TYPE_16}, .value = svc_uuid},
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;  // Indica que é a lista completa de UUIDs

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Erro ble_gap_adv_set_fields; rc=%d", rc);
        return;
    }

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                            &adv_params, ble_gap_event_cb, NULL);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Erro ao iniciar advertising; rc=%d", rc);
    }
    else
    {
        ESP_LOGI(TAG, "Advertising iniciado");
    }
}

/// ============================
/// Inicializa BLE
/// ============================
void ble_app_on_sync(void)
{
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    assert(rc == 0);

    uint8_t addr_val[6] = {0};
    ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

    ESP_LOGI(TAG, "Endereço BLE: %02x:%02x:%02x:%02x:%02x:%02x",
             addr_val[5], addr_val[4], addr_val[3],
             addr_val[2], addr_val[1], addr_val[0]);

    ble_app_advertise();
}

/// ============================
/// Task principal BLE
/// ============================
void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/// ============================
/// App Main
/// ============================
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_LOGI(TAG, "Iniciando BLE...");
    nimble_port_init();

    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_gatts_count_cfg(gatt_svr_svcs);
    ble_gatts_add_svcs(gatt_svr_svcs);

    ble_hs_cfg.sync_cb = ble_app_on_sync;

    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "BLE rodando...");
}
