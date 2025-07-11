#pragma once

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

#include "host/ble_gatt.h"
#include "host/ble_hs.h"


extern uint16_t log_char_handle;
extern uint16_t log_ctrl_char_handle;
extern bool transfer_active;

// Callback BLE para característica de controle de log
int log_gatt_access_cb(uint16_t conn_handle_cb, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg);
