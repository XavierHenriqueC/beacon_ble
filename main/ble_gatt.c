#include "ble_gatt.h"
#include "ble_live.h"
#include "ble_log.h"
#include "temp_hum.h"

static const char *TAG = "BLE_GATT";

// ==============================
// Variáveis BLE
// ==============================
uint8_t own_addr_type;
uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static void ble_app_advertise(void);

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
             .val_handle = &config_char_handle,
         },
         {
             .uuid = BLE_UUID16_DECLARE(0x2A1D),
             .access_cb = log_gatt_access_cb,
             .flags = BLE_GATT_CHR_F_NOTIFY,
             .val_handle = &log_char_handle,
         },
         {
             .uuid = BLE_UUID16_DECLARE(0x2A1F),
             .access_cb = log_gatt_access_cb,
             .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
             .val_handle = &log_ctrl_char_handle,
         },
         {0},
     }},
    {0},
};

// ==============================
// GAP Events
// ==============================
static int ble_gap_event_cb(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "Dispositivo conectado.");
        } else {
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
static void ble_app_advertise(void) {
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
    if (rc != 0) {
        ESP_LOGE(TAG, "Erro ble_gap_adv_set_fields; rc=%d", rc);
        return;
    }

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Erro ao iniciar advertising; rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising iniciado");
    }
}


// ==============================
// Sync Callback
// ==============================
static void ble_on_sync(void) {
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
void ble_init(void) {
    nimble_port_init();

    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_gatts_count_cfg(gatt_svr_svcs);
    ble_gatts_add_svcs(gatt_svr_svcs);

    ble_hs_cfg.sync_cb = ble_on_sync;
}

void ble_start(void) {
    nimble_port_freertos_init(ble_host_task);
}

void ble_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}
