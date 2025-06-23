#include "temp_hum.h"
#include <stdlib.h>
#include <time.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ble.h"

static const char *TAG = "TEMP_HUM";

// Dados simulados
static float temperature = 25.0;
static float humidity = 50.0;

// Referência para o interval configurado via BLE
static uint64_t *interval_ptr = NULL;

/// ================================
/// Task que atualiza dados periodicamente
/// ================================
void temp_hum_task(void *param)
{
    while (1)
    {
        // Atualiza dados simulados
        float temp_variation = ((float)(rand() % 100) / 100.0f) - 0.5f;
        float hum_variation = ((float)(rand() % 100) / 100.0f) - 0.5f;

        temperature += temp_variation;
        humidity += hum_variation;

        if (temperature < 15.0) temperature = 15.0;
        if (temperature > 35.0) temperature = 35.0;

        if (humidity < 30.0) humidity = 30.0;
        if (humidity > 90.0) humidity = 90.0;

        ESP_LOGI(TAG, "Temp: %.2f ºC, Hum: %.2f %%", temperature, humidity);

        //Gera notificação para o BLE
        ble_notify_sensor();

        // Delay baseado no interval configurado via BLE
        uint64_t interval_ms = (*interval_ptr) * 1000;
        if (interval_ms < 1000)
        {
            interval_ms = 1000; // Intervalo mínimo de segurança
        }

        vTaskDelay(pdMS_TO_TICKS(interval_ms));
    }
}

/// ================================
/// Inicializa o gerador de dados
/// ================================
void temp_hum_init(uint64_t *interval_ref)
{
    srand(time(NULL));
    interval_ptr = interval_ref;

    ESP_LOGI(TAG, "Iniciando sensor simulado de Temp/Hum com intervalo de %llu segundos", *interval_ref);

    xTaskCreate(temp_hum_task, "temp_hum_task", 2048, NULL, 5, NULL);
}

/// ================================
/// Retorna a temperatura atual
/// ================================
float get_temperature(void)
{
    return temperature;
}

/// ================================
/// Retorna a umidade atual
/// ================================
float get_humidity(void)
{
    return humidity;
}
