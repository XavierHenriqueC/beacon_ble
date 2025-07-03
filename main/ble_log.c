#include "ble_gatt.h"
#include "ble_log.h"
#include "serial.h"
#include "nvs_controller.h"


static const char *TAG = "BLE_LOG";

// Handles BLE
uint16_t log_char_handle;
uint16_t log_ctrl_char_handle;

// Controle interno
static bool transfer_active = false;
static size_t transfer_index = 0;
static size_t transfer_total = 0;
static SensorData *log_data_array = NULL;

// ==========================
// Envia um LogControl serializado com a quantidade de dados
// ==========================
static int notify_log_control_count(uint32_t count)
{
    LogControl response = LogControl_init_zero;
    response.command = LogControl_Command_GETLENGTH;
    response.length = count;

    uint8_t buffer[64];
    size_t len = sizeof(buffer);

    if (!serializeLogControl(buffer, &len, response.command, response.length))
    {
        ESP_LOGE(TAG, "Falha ao serializar LogControl");
        return -1;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(buffer, len);
    if (!om)
    {
        ESP_LOGE(TAG, "Erro ao criar os_mbuf");
        return -1;
    }

    int rc = ble_gattc_notify_custom(conn_handle, log_ctrl_char_handle, om);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Erro ao enviar notificação (rc=%d)", rc);
    }

    return rc;
}

// ==========================
// Envia o próximo dado SensorData via notify
// ==========================
static int send_next_log_entry()
{
    if (!log_data_array || transfer_index >= transfer_total)
    {
        ESP_LOGW(TAG, "Nenhum dado para enviar ou todos os dados já foram enviados.");
        return -1;
    }

    uint8_t buffer[64];
    size_t len = sizeof(buffer);

    if (!serializeSensorDataFromStruct(buffer, &len, &log_data_array[transfer_index]))
    {
        ESP_LOGE(TAG, "Falha ao serializar SensorData existente");
        return -1;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(buffer, len);
    if (!om)
    {
        ESP_LOGE(TAG, "Erro ao criar os_mbuf");
        return -1;
    }

    int rc = ble_gattc_notify_custom(conn_handle, log_char_handle, om);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Erro ao enviar log (rc=%d)", rc);
        return rc;
    }

    transfer_index++; // Avança para o próximo
    return 0;
}

// ==========================
// Manipulador da característica de controle
// ==========================
int log_gatt_access_cb(uint16_t conn_handle_cb, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
    {
        LogControl command;
        if (!deserializeLogControl(ctxt->om->om_data, ctxt->om->om_len, &command))
        {
            ESP_LOGE(TAG, "Erro ao decodificar comando LogControl");
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        switch (command.command)
        {
        case LogControl_Command_GETLENGTH:
        {
            uint32_t count = 0;
            nvs_get_sensor_data_count(&count);
            notify_log_control_count(count);
            break;
        }

        case LogControl_Command_START:
        {
            if (log_data_array)
                free(log_data_array);

            uint32_t count = 0;
            nvs_get_sensor_data_count(&count);
            transfer_total = count;

            log_data_array = calloc(transfer_total, sizeof(SensorData));
            if (!log_data_array)
            {
                ESP_LOGE(TAG, "Erro ao alocar memória para logs");
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }

            nvs_read_all_sensor_data(log_data_array, transfer_total, &transfer_total);
            transfer_index = 0;
            transfer_active = true;

            send_next_log_entry();
            break;
        }

        case LogControl_Command_NEXT:
        {
            if (transfer_active)
            {
                if (transfer_index < transfer_total)
                {
                    send_next_log_entry();
                }
                else
                {
                    ESP_LOGI(TAG, "Todos os dados foram enviados.");
                }
            }
            break;
        }

        case LogControl_Command_STOP:
        case LogControl_Command_CLEAR:
        {
            transfer_active = false;
            transfer_index = 0;
            transfer_total = 0;
            if (log_data_array)
            {
                free(log_data_array);
                log_data_array = NULL;
            }

            if (command.command == LogControl_Command_CLEAR)
            {
                nvs_clear_all_sensor_data();
                ESP_LOGI(TAG, "Todos os logs foram apagados.");
            }
            else
            {
                ESP_LOGI(TAG, "Transferência interrompida.");
            }
            break;
        }

        default:
            ESP_LOGW(TAG, "Comando desconhecido: %d", command.command);
            break;
        }

        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}
