#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"



extern uint64_t interval;
extern uint64_t log_mode; //Always
extern uint16_t date_time_init;
extern uint16_t date_time_stop;

extern uint16_t temp_char_handle;
extern uint16_t config_char_handle;

void ble_notify_sensor(void);
void ble_notify_config(void);
int gatt_svr_access_cb(uint16_t conn, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
