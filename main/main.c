#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "ble_live.h"
#include "ble_gatt.h"
#include "ble_log.h"


void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_LOGI("MAIN", "Iniciando BLE...");
    ble_init();
    ble_start();
    ESP_LOGI("MAIN", "BLE rodando...");
}
