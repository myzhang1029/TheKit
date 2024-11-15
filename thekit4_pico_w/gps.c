/*
 *  gps.c
 *  Copyright (C) 2023-2024 Zhang Maiyun <me@maiyun.me>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "gps_util.h"

#include <time.h>

#include "pico/stdlib.h"

#include "hardware/gpio.h"
#include "hardware/rtc.h"
#include "hardware/uart.h"

#if ENABLE_GPS
// Marker: static variable
static struct gps_status gps_status = GPS_STATUS_INIT;

void gps_init(void) {
    uart_init(GPS_UART, GPS_BAUD);
    gpio_set_function(GPS_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(GPS_RX_PIN, GPIO_FUNC_UART);
    // We don't need TX
    // Turn off flow control CTS/RTS
    uart_set_hw_flow(GPS_UART, false, false);
    // Set up EN
    gpio_init(GPS_EN_PIN);
    gpio_set_dir(GPS_EN_PIN, GPIO_OUT);
    // Enable GPS
    gpio_put(GPS_EN_PIN, 1);
    // PPS is set up in irq.c
}

bool gps_get_location(float *lat, float *lon, float *alt, timestamp_t *age) {
    timestamp_t timestamp, now;
    now = timestamp_micros();
    if (!gpsutil_get_location(&gps_status, lat, lon, alt, &timestamp)) {
        return false;
    }
    *age = now - timestamp;
    return true;
}

bool gps_get_time(time_t *time, timestamp_t *age) {
    timestamp_t timestamp, now;
    now = timestamp_micros();
    if (!gpsutil_get_time(&gps_status, time, &timestamp)) {
        return false;
    }
    *age = now - timestamp;
    return true;
}

uint8_t gps_get_sat_num(void) {
    return gps_status.gps_sat_num;
}

/// Read whatever is in the UART, and parse it if it's a sentence
void gps_parse_available(void) {
    while (uart_is_readable(GPS_UART)) {
        char c = uart_getc(GPS_UART);
        gpsutil_feed(&gps_status, c);
    }
}


#endif
