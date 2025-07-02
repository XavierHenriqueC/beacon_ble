#include "nvs_controller.h"
#include "serial.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "ble_live.h"

#define TAG "NVS_CTRL"
#define NVS_NAMESPACE "storage"
#define NVS_SENSOR_KEY_PREFIX "sd_" // Ex: sd_0, sd_1, ...
#define NVS_SENSOR_COUNT_KEY "sd_count"
#define NVS_CONFIG_KEY "sensor_cfg"

void load_sensor_config(void);

// ==========================
// Inicialização da NVS
// ==========================
esp_err_t nvs_controller_init(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS corrompida ou versão incompatível. Formatando...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    if (err == ESP_OK)
    {
        load_sensor_config(); // Só executa se a NVS foi inicializada com sucesso
    }
    else
    {
        ESP_LOGE(TAG, "Falha ao inicializar a NVS: %s", esp_err_to_name(err));
    }

    return err;
}

// ==========================
// Série Temporal - SensorData
// ==========================
esp_err_t nvs_save_sensor_data(float temp, float hum)
{
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));

    // Obter contador atual
    uint32_t count = 0;
    nvs_get_u32(handle, NVS_SENSOR_COUNT_KEY, &count);

    // Serializar SensorData
    uint8_t buffer[SensorData_size];
    size_t len = sizeof(buffer);
    if (!serializeSensorData(buffer, &len, temp, hum))
    {
        ESP_LOGE(TAG, "Erro ao serializar SensorData.");
        nvs_close(handle);
        return ESP_FAIL;
    }

    // Criar chave única (sd_0, sd_1, ...)
    char key[16];
    snprintf(key, sizeof(key), NVS_SENSOR_KEY_PREFIX "%lu", (unsigned long)count);

    esp_err_t err = nvs_set_blob(handle, key, buffer, len);
    if (err == ESP_OK)
    {
        nvs_set_u32(handle, NVS_SENSOR_COUNT_KEY, count + 1);
        ESP_LOGI(TAG, "SensorData salvo com chave %s", key);
    }

    nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_read_all_sensor_data(SensorData *out_array, size_t max_items, size_t *read_items)
{
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle));

    uint32_t count = 0;
    nvs_get_u32(handle, NVS_SENSOR_COUNT_KEY, &count);
    *read_items = 0;

    for (uint32_t i = 0; i < count && i < max_items; i++)
    {
        char key[16];
        snprintf(key, sizeof(key), NVS_SENSOR_KEY_PREFIX "%lu", i);

        uint8_t buffer[SensorData_size];
        size_t len = sizeof(buffer);
        esp_err_t err = nvs_get_blob(handle, key, buffer, &len);

        if (err == ESP_OK)
        {
            SensorData data;
            if (deserializeSensorData(buffer, len, &data))
            {
                out_array[i] = data;
                (*read_items)++;
            }
            else
            {
                ESP_LOGW(TAG, "Falha ao desserializar item %s", key);
            }
        }
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t nvs_get_sensor_data_count(uint32_t *count)
{
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle));
    nvs_get_u32(handle, NVS_SENSOR_COUNT_KEY, count);
    nvs_close(handle);
    return ESP_OK;
}

esp_err_t nvs_clear_all_sensor_data(void)
{
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));

    uint32_t count = 0;
    esp_err_t err = nvs_get_u32(handle, NVS_SENSOR_COUNT_KEY, &count);
    if (err != ESP_OK)
        count = 0;

    for (uint32_t i = 0; i < count; i++)
    {
        char key[16];
        snprintf(key, sizeof(key), NVS_SENSOR_KEY_PREFIX "%lu", i);
        nvs_erase_key(handle, key);
    }

    nvs_erase_key(handle, NVS_SENSOR_COUNT_KEY);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "Todos os dados SensorData foram apagados.");
    return ESP_OK;
}

// ==========================
// SensorConfig
// ==========================
esp_err_t nvs_save_sensor_config(SensorConfig *cfg)
{
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));

    uint8_t buffer[SensorConfig_size];
    size_t len = sizeof(buffer);

    // ⚠️ Use struct inteira, não os campos individualmente!
    if (!serializeSensorConfig(buffer, &len, cfg))
    {
        ESP_LOGE(TAG, "Erro ao serializar SensorConfig.");
        nvs_close(handle);
        return ESP_FAIL;
    }

    esp_err_t err = nvs_set_blob(handle, NVS_CONFIG_KEY, buffer, len);
    if (err == ESP_OK)
        nvs_commit(handle);

    nvs_close(handle);
    return err;
}

esp_err_t nvs_update_sensor_config(SensorConfig *cfg)
{
    return nvs_save_sensor_config(cfg); // Mesmo comportamento
}

SensorConfig nvs_read_sensor_config(void)
{
    SensorConfig cfg;

    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle));

    uint8_t buffer[SensorConfig_size];
    size_t len = sizeof(buffer);

    esp_err_t err = nvs_get_blob(handle, NVS_CONFIG_KEY, buffer, &len);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Nenhuma configuração encontrada.");
        nvs_close(handle);
        return cfg;
    }

    if (deserializeSensorConfig(buffer, len, &cfg))
    {
        nvs_close(handle);
        return cfg;
    } else {
        ESP_LOGE(TAG, "Erro ao desserializar SensorConfig.");
        nvs_close(handle);
        return cfg;
    }
}

void load_sensor_config(void)
{

    SensorConfig data = nvs_read_sensor_config();

    if (data.interval == 0)
    {
        interval = 60;
        ESP_LOGI(TAG, "Configuração carregada: Interval: %llu", interval);
    }
    else
    {
        interval = data.interval;
        ESP_LOGI(TAG, "Configuração carregada: Interval: %llu", interval);
    }
}
