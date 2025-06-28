#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "esp_timer.h"


void temp_hum_init(uint64_t *interval_ref);
float get_temperature(void);
float get_humidity(void);

