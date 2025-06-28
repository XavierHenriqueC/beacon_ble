#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_log.h"

#include "nvs_flash.h"
#include "nvs.h"

#include "host/ble_hs.h"
#include "host/ble_gatt.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "sensor.pb.h"

extern uint16_t log_char_handle;
extern uint16_t log_ctrl_char_handle;

void log_ble_init(uint64_t *interval_ptr);
void log_ble_add_entry(float temp, float hum, uint64_t timestamp);
void log_ble_clear(void);
void ble_send_log_via_notify(void);

int log_gatt_access_cb(uint16_t conn, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
size_t log_ble_get_packet(uint8_t *buffer, size_t buffer_size, size_t start_index, size_t *next_index);
size_t log_ble_get_total_entries(void);