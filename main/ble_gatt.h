#pragma once

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"


extern uint8_t own_addr_type;
extern uint16_t conn_handle;

void ble_init(void);
void ble_start(void);
void ble_host_task(void *param);