#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void log_ble_init(uint64_t *interval_ptr);
void log_ble_add_entry(float temp, float hum, uint64_t timestamp);
void log_ble_clear(void);
size_t log_ble_get_packet(uint8_t *buffer, size_t buffer_size, size_t start_index, size_t *next_index);
size_t log_ble_get_total_entries(void);
