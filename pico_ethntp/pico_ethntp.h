#ifndef _THEKIT4_PICO_W_H
#define _THEKIT4_PICO_W_H

#include <stdint.h>

#include "lwip/ip_addr.h"

#include "gps_util.h"

void gps_init(void);
bool gps_get_location(float *lat, float *lon, float *alt, timestamp_t *age);
bool gps_get_time(time_t *time, timestamp_t *age);
uint8_t gps_get_sat_num(void);
void gps_parse_available(void);

#endif
