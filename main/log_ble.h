#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "host/ble_gatt.h"

void log_ble_init(uint64_t *interval_ptr);
void log_ble_add_entry(float temp, float hum, uint64_t timestamp);
void log_ble_clear(void);
size_t log_ble_get_packet(uint8_t *buffer, size_t buffer_size, size_t start_index, size_t *next_index);
size_t log_ble_get_total_entries(void);
void ble_send_log_via_notify(void);
int log_gatt_access_cb(uint16_t conn, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
