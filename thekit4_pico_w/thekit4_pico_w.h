/*
 *  thekit4_pico_w.h
 *  Copyright (C) 2022-2023 Zhang Maiyun <me@maiyun.me>
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

#ifndef _THEKIT4_PICO_W_H
#define _THEKIT4_PICO_W_H

#include <stdint.h>

#include "lwip/ip_addr.h"

#include "gps_util.h"

#define WIFI_NETIF (cyw43_state.netif[CYW43_ITF_STA])

#undef uint32_t
typedef u32_t uint32_t;

struct ntp_client {
    bool in_progress;
    ip_addr_t server_address;
    struct udp_pcb *pcb;
    alarm_id_t timeout_alarm;
};

/// HTTP server. The entire structure exists throughout the program
/// but `conn` is re-initialized each time a request is received.
struct http_server {
    struct tcp_pcb *server_pcb;
    struct http_server_conn {
        struct tcp_pcb *client_pcb;
        enum {
            HTTP_OTHER = 0,
            HTTP_ACCEPTED,
            HTTP_RECEIVING
        } state;
        struct pbuf *received;
    } conn;
};

struct light_sched_entry {
    int8_t hour;
    int8_t min;
    bool on;
};

struct wifi_config_entry {
    const char *ssid;
    const char *password;
    uint32_t auth;
};

void irq_init(void);

void temperature_init(void);
float temperature_measure(void);

void light_init(void);
void light_dim(float intensity);
// Takes the current time to avoid wasting cycles waiting for RTC to be
// synchronised. Might modify it.
void light_register_next_alarm(datetime_t *current);

bool wifi_connect(void);

bool ntp_client_init(struct ntp_client *state);
void ntp_client_check_run(struct ntp_client *state);
void update_rtc(time_t result, uint8_t stratum);

bool http_server_open(struct http_server *state);
void http_server_close(struct http_server *arg);

void tasks_init(void);
bool tasks_check_run(void);

void gps_init(void);
bool gps_get_location(float *lat, float *lon, float *alt, timestamp_t *age);
bool gps_get_time(time_t *time, timestamp_t *age);
uint8_t gps_get_sat_num(void);
void gps_parse_available(void);

#endif
