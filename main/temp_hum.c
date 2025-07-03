#include "temp_hum.h"
#include "ble_live.h"
#include "ble_log.h"
#include "sth31d.h"
#include "nvs_controller.h"

static const char *TAG = "TEMP_HUM";

static esp_timer_handle_t timer_handle;

static float temperature;
static float humidity;

static uint64_t *interval_ptr = NULL;

/// ============================
/// Geração dos dados simulados
/// ============================
static void generate_temp_hum_data(void)
{
    float temp_variation = ((float)(rand() % 100) / 100.0f) - 0.5f;
    float hum_variation = ((float)(rand() % 100) / 100.0f) - 0.5f;

    temperature += temp_variation;
    humidity += hum_variation;

    if (temperature < 15.0f)
        temperature = 15.0f;
    if (temperature > 35.0f)
        temperature = 35.0f;

    if (humidity < 20.0f)
        humidity = 20.0f;
    if (humidity > 80.0f)
        humidity = 80.0f;

    uint64_t timestamp = esp_timer_get_time() / 1000; // Em milissegundos

    ESP_LOGI(TAG, "Nova leitura -> Temp: %.2f C | Hum: %.2f %% | Timestamp: %llu", temperature, humidity, timestamp);
}

/// ============================
/// Callback do Timer
/// ============================
static void timer_callback(void *arg)
{
    // generate_temp_hum_data();


    // Leitura sensores STH31D
    esp_err_t err = sth31_get_temp_hum(&temperature, &humidity);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Temp: %.2f °C, Hum: %.2f %%", temperature, humidity);

        // Notifica BLE
        ble_notify_sensor();

        // Salva no log
        nvs_save_sensor_data(temperature, humidity);
    }
    else
    {
        ESP_LOGE(TAG, "Erro ao ler sensor: %s", esp_err_to_name(err));
    }

    if (interval_ptr)
    {
        uint64_t interval_ms = (*interval_ptr) * 1000;
        esp_timer_stop(timer_handle);
        esp_timer_start_periodic(timer_handle, interval_ms * 1000); // Intervalo em microssegundos
    }
}

/// ============================
/// Inicialização do módulo
/// ============================
void temp_hum_init(uint64_t *interval_reference)
{
    interval_ptr = interval_reference;

    srand((unsigned int)time(NULL)); // Inicializa seed do rand()

    const esp_timer_create_args_t timer_args = {
        .callback = &timer_callback,
        .name = "temp_hum_timer"};

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));

    uint64_t interval_ms = (*interval_ptr) * 1000;
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle, interval_ms * 1000));

    ESP_LOGI(TAG, "Módulo TEMP_HUM inicializado com intervalo de %llu segundos", *interval_ptr);
}

/// ============================
/// Acesso aos dados atuais
/// ============================
float get_temperature(void)
{
    return temperature;
}

float get_humidity(void)
{
    return humidity;
}
