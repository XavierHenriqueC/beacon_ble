
#include "ble.h"
#include "log_ble.h"
#include "temp_hum.h"

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "sensor.pb.h"

static const char *TAG = "BLE";

// UUIDs das características de log
#define UUID_LOG_CHAR BLE_UUID128_DECLARE(0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef, \
                                          0x12, 0x34, 0x56, 0x78, 0x90, 0xab, 0xcd, 0xef)

#define UUID_LOG_CTRL_CHAR BLE_UUID128_DECLARE(0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12, \
                                               0xef, 0xcd, 0xab, 0x90, 0x78, 0x56, 0x34, 0x12)

// ==============================
// Variáveis internas BLE
// ==============================
static uint8_t own_addr_type;
static uint16_t temp_char_handle;
static uint16_t interval_char_handle;
static uint16_t log_char_handle;
static uint16_t log_ctrl_char_handle;

static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool log_transmission_active = false;

// Intervalo configurável (em segundos)
uint64_t interval = 60;

// ==============================
// Serialização Protobuf
// ==============================
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
        ESP_LOGE(TAG, "Erro na serialização Protobuf");
        return false;
    }

    *length = stream.bytes_written;
    return true;
}

bool deserializeSensorData(const uint8_t *buffer, size_t length, SensorData *data)
{
    pb_istream_t stream = pb_istream_from_buffer(buffer, length);
    return pb_decode(&stream, SensorData_fields, data);
}

// ==============================
// Notify BLE
// ==============================
void ble_notify_sensor(void)
{
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE)
        return;

    uint8_t buffer[64];
    size_t len = sizeof(buffer);

    if (serializeSensorData(buffer, &len, get_temperature(), get_humidity(),
                            esp_timer_get_time() / 1000, interval))
    {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(buffer, len);
        if (om)
        {
            ble_gattc_notify_custom(conn_handle, temp_char_handle, om);
            ESP_LOGI(TAG, "Notify Temp/Hum enviado");
        }
    }
}

void ble_notify_interval(void)
{
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE)
        return;

    uint8_t buffer[64];
    size_t len = sizeof(buffer);

    if (serializeSensorData(buffer, &len, get_temperature(), get_humidity(),
                            esp_timer_get_time() / 1000, interval))
    {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(buffer, len);
        if (om)
        {
            ble_gattc_notify_custom(conn_handle, interval_char_handle, om);
            ESP_LOGI(TAG, "Notify Interval enviado");
        }
    }
}

void ble_notify_log(const uint8_t *data, size_t length)
{
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE)
        return;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
    if (om)
    {
        ble_gattc_notify_custom(conn_handle, log_char_handle, om);
        ESP_LOGI(TAG, "Notify Log enviado (%d bytes)", (int)length);
    }
}

void ble_send_log_via_notify(void) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGW(TAG, "Sem conexão BLE ativa.");
        return;
    }

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
    }

    ESP_LOGI(TAG, "Envio de log concluído.");
}

// ==============================
// Callback Log GATT
// ==============================
static int log_gatt_access_cb(uint16_t conn, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (attr_handle == log_char_handle)
    {
        if (!log_transmission_active)
            return BLE_ATT_ERR_READ_NOT_PERMITTED;

        uint8_t buffer[256];
        static size_t index = 0;
        size_t next_index = 0;

        size_t len = log_ble_get_packet(buffer, sizeof(buffer), index, &next_index);

        if (len > 0)
        {
            os_mbuf_append(ctxt->om, buffer, len);
            index = next_index;
            ESP_LOGI(TAG, "Read Log %d bytes (index %d)", (int)len, (int)index);

            if (index >= log_ble_get_total_entries())
            {
                index = 0; // reset ao final do log
            }

            return 0;
        }
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (attr_handle == log_ctrl_char_handle)
    {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
        {
            LogControl ctrl = LogControl_init_zero;
            uint8_t temp_buf[32];
            uint16_t data_len = OS_MBUF_PKTLEN(ctxt->om);

            os_mbuf_copydata(ctxt->om, 0, data_len, temp_buf);

            pb_istream_t stream = pb_istream_from_buffer(temp_buf, data_len);
            if (pb_decode(&stream, LogControl_fields, &ctrl))
            {
                ESP_LOGI(TAG, "Comando recebido no Log Control: %d", ctrl.command);

                switch (ctrl.command)
                {
                case LogControl_Command_START:
                    log_transmission_active = true;
                    ESP_LOGI(TAG, "Log START");
                    ble_send_log_via_notify(); // 🚀 Envia todo o log por Notify
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
                    ESP_LOGW(TAG, "Comando desconhecido recebido: %d", ctrl.command);
                    break;
                }
                return 0;
            }
            else
            {
                ESP_LOGE(TAG, "Falha na desserialização do LogControl");
            }

            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// ==============================
// Callback GATT
// ==============================
static int gatt_svr_access_cb(uint16_t conn, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t buffer[64];
    size_t len = sizeof(buffer);

    if (attr_handle == temp_char_handle)
    {
        if (serializeSensorData(buffer, &len, get_temperature(), get_humidity(),
                                esp_timer_get_time() / 1000, interval))
        {
            os_mbuf_append(ctxt->om, buffer, len);
            ESP_LOGI(TAG, "Read Temp/Hum");
            return 0;
        }
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (attr_handle == interval_char_handle)
    {
        switch (ctxt->op)
        {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            if (serializeSensorData(buffer, &len, get_temperature(), get_humidity(),
                                    esp_timer_get_time() / 1000, interval))
            {
                os_mbuf_append(ctxt->om, buffer, len);
                ESP_LOGI(TAG, "Read Interval");
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
                ESP_LOGI(TAG, "Interval atualizado via BLE: %llu", interval);

                ble_notify_interval();
                return 0;
            }
            else
            {
                ESP_LOGE(TAG, "Erro desserializando Interval.");
                return BLE_ATT_ERR_UNLIKELY;
            }
        }
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// ==============================
// Serviço GATT
// ==============================
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(0x1809),
     .characteristics = (struct ble_gatt_chr_def[]){
         {
             .uuid = BLE_UUID16_DECLARE(0x2A1C),
             .access_cb = gatt_svr_access_cb,
             .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
             .val_handle = &temp_char_handle,
         },
         {
             .uuid = BLE_UUID16_DECLARE(0x2A1E),
             .access_cb = gatt_svr_access_cb,
             .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
             .val_handle = &interval_char_handle,
         },
         {
             .uuid = BLE_UUID16_DECLARE(0x2A1D),
             .access_cb = log_gatt_access_cb,
             .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
             .val_handle = &log_char_handle,
         },
         {
             .uuid = BLE_UUID16_DECLARE(0x2A1F),
             .access_cb = log_gatt_access_cb,
             .flags = BLE_GATT_CHR_F_WRITE,
             .val_handle = &log_ctrl_char_handle,
         },
         {0},
     }},
    {0},
};

// ==============================
// GAP Events
// ==============================
static void ble_app_advertise(void);

static int ble_gap_event_cb(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0)
        {
            conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "Dispositivo conectado.");
        }
        else
        {
            ESP_LOGI(TAG, "Falha na conexão. Status=%d", event->connect.status);
            ble_app_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
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

// ==============================
// Advertising
// ==============================
static void ble_app_advertise(void)
{
    struct ble_gap_adv_params adv_params = {0};

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)"Beacon_ESP32";
    fields.name_len = strlen("Beacon_ESP32");
    fields.name_is_complete = 1;

    const uint16_t svc_uuid = 0x1809;
    fields.uuids16 = (ble_uuid16_t[]){
        {.u = {.type = BLE_UUID_TYPE_16}, .value = svc_uuid}};
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

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

// ==============================
// Sync Callback
// ==============================
static void ble_on_sync(void)
{
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    assert(rc == 0);

    uint8_t addr_val[6];
    ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

    ESP_LOGI(TAG, "Endereço BLE: %02X:%02X:%02X:%02X:%02X:%02X",
             addr_val[5], addr_val[4], addr_val[3],
             addr_val[2], addr_val[1], addr_val[0]);

    ble_app_advertise();
}

// ==============================
// Init e Tasks
// ==============================
void ble_init(void)
{
    nimble_port_init();

    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_gatts_count_cfg(gatt_svr_svcs);
    ble_gatts_add_svcs(gatt_svr_svcs);

    ble_hs_cfg.sync_cb = ble_on_sync;

    temp_hum_init(&interval);
}

void ble_start(void)
{
    nimble_port_freertos_init(ble_host_task);
}

void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}
