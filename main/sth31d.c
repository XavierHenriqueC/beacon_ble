#include <stdio.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_MASTER_NUM             I2C_NUM_0
#define I2C_MASTER_SCL_IO          9
#define I2C_MASTER_SDA_IO          8
#define I2C_MASTER_FREQ_HZ         100000
#define I2C_MASTER_TX_BUF_DISABLE  0
#define I2C_MASTER_RX_BUF_DISABLE  0
#define STH31_SENSOR_ADDR          0x44

static const char *TAG = "STH31";
static bool i2c_initialized = false;

esp_err_t i2c_master_init_once(void) {
    if (i2c_initialized) return ESP_OK;

    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    ESP_LOGI(TAG, "Configurando I2C...");
    esp_err_t err;

    err = i2c_param_config(I2C_MASTER_NUM, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao configurar I2C: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(I2C_MASTER_NUM, config.mode, 
                             I2C_MASTER_RX_BUF_DISABLE, 
                             I2C_MASTER_TX_BUF_DISABLE, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao instalar driver I2C: %s", esp_err_to_name(err));
        return err;
    }

    i2c_initialized = true;
    ESP_LOGI(TAG, "I2C inicializado com sucesso");
    return ESP_OK;
}

esp_err_t sth31_get_temp_hum(float *temperature, float *humidity) {
    if (!temperature || !humidity) return ESP_ERR_INVALID_ARG;

    esp_err_t ret = i2c_master_init_once();
    if (ret != ESP_OK) return ret;

    ESP_LOGI(TAG, "Iniciando leitura do sensor");

    // Comando de medição simples (high repeatability, clock stretching disabled)
    uint8_t cmd[] = { 0x24, 0x00 };
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (STH31_SENSOR_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd_handle, cmd, sizeof(cmd), true);
    i2c_master_stop(cmd_handle);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao enviar comando de medição: %s", esp_err_to_name(ret));
        return ret;
    }

    // Espera pela conversão (~15ms típico, recomenda-se até 20ms)
    vTaskDelay(pdMS_TO_TICKS(20));

    // Lê 6 bytes (Temp[2] + CRC + Hum[2] + CRC)
    uint8_t data[6];
    i2c_cmd_handle_t read_handle = i2c_cmd_link_create();
    i2c_master_start(read_handle);
    i2c_master_write_byte(read_handle, (STH31_SENSOR_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(read_handle, data, sizeof(data), I2C_MASTER_LAST_NACK);
    i2c_master_stop(read_handle);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, read_handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(read_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao ler dados do sensor: %s", esp_err_to_name(ret));
        return ret;
    }

    // Conversão sem verificação CRC
    uint16_t raw_temp = (data[0] << 8) | data[1];
    uint16_t raw_hum = (data[3] << 8) | data[4];

    *temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    *humidity = 100.0f * ((float)raw_hum / 65535.0f);

    ESP_LOGI(TAG, "Temp: %.2f °C | Hum: %.2f %%", *temperature, *humidity);

    return ESP_OK;
}
