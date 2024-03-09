/* The entire The Kit on Raspberry Pi Pico W */
/*
 *  thekit4_pico_w.c
 *  Copyright (C) 2022-2024 Zhang Maiyun <me@maiyun.me>
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
#include "thekit4_pico_w.h"

#include <stdio.h>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "hardware/adc.h"
#include "hardware/rtc.h"
#if ENABLE_WATCHDOG
#include "hardware/watchdog.h"
#endif

#if ENABLE_NTP
static struct ntp_client ntp_state;
#endif
static struct http_server http_state;

static void init() {
    stdio_init_all();
    sleep_ms(1000);

#if ENABLE_WATCHDOG
    if (watchdog_caused_reboot())
        puts("Rebooted by watchdog");
#endif

    rtc_init();
    // ADC (before light and temperature)
    adc_init();
    adc_gpio_init(ADC_ZERO_PIN);
#if ENABLE_LIGHT
    light_init();
#endif
#if ENABLE_TEMPERATURE_SENSOR
    temperature_init();
#endif
#if ENABLE_GPS
    gps_init();
#endif
    irq_init();

#if ENABLE_WATCHDOG
    // Needs to be larger than `wifi_connect`'s timeout
    watchdog_enable(60000, 1);
#endif

    if (cyw43_arch_init() != 0)
        panic("ERROR: Cannot init CYW43");
    // Depends on cyw43
    cyw43_arch_enable_sta_mode();
    wifi_connect();

#if ENABLE_NTP
    if (!ntp_client_init(&ntp_state))
        puts("WARNING: Cannot init NTP client");
#endif
    // Start HTTP server
    if (!http_server_open(&http_state))
        puts("WARNING: Cannot open HTTP server");

    puts("Successfully initialized everything");

#if ENABLE_TEMPERATURE_SENSOR
    printf("Temperature: %f\n", temperature_measure());
#endif
}

int main() {
    init();

    while (1) {
        int wifi_state = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
#if ENABLE_WATCHDOG
        watchdog_update();
#endif
        if (wifi_state != CYW43_LINK_JOIN) {
            printf("Wi-Fi link status is %d, reconnecting\n", wifi_state);
            wifi_connect();
        }
#if ENABLE_WATCHDOG
        watchdog_update();
#endif
#if ENABLE_NTP
        ntp_client_check_run(&ntp_state);
#endif
#if ENABLE_WATCHDOG
        watchdog_update();
#endif
        tasks_check_run();
#if ENABLE_WATCHDOG
        watchdog_update();
#endif
#if ENABLE_GPS
        gps_parse_available();
#endif
#if ENABLE_WATCHDOG
        watchdog_update();
#endif
#if PICO_CYW43_ARCH_POLL
        if (has_cyw43)
            cyw43_arch_poll();
#endif
#if ENABLE_GPS || PICO_CYW43_ARCH_POLL
        sleep_ms(1);
#else
        sleep_ms(100);
#endif
    }
    http_server_close(&http_state);
    cyw43_arch_deinit();
}
