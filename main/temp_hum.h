#ifndef TEMP_HUM_H
#define TEMP_HUM_H

#include <stdint.h>

void temp_hum_init(uint64_t *interval_ref);
float get_temperature(void);
float get_humidity(void);

#endif // TEMP_HUM_H
