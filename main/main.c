#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "ble_live.h"
#include "ble_gatt.h"
#include "ble_log.h"
#include "nvs_controller.h"
#include "temp_hum.h"


void app_main(void)
{

    ESP_LOGI("MAIN", "Iniciando NVS...");
    nvs_controller_init();
    temp_hum_init(&interval);
    ESP_LOGI("MAIN", "NVS rodando...");

    ESP_LOGI("MAIN", "Iniciando BLE...");
    ble_init();
    ble_start();
    ESP_LOGI("MAIN", "BLE rodando...");
}
