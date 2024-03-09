/*
 *  ntp.c
 *  Heavily refactored from BSD-3-Clause picow_ntp_client.c
 *  Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
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

#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "hardware/rtc.h"

#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

// Our current position in the stratum system
// used in http_server.c and tasks.c
// It remains 16 if NTP nor GPS is enabled
// Marker: static variable
static volatile uint8_t ntp_stratum = 16;

uint8_t ntp_get_stratum(void) {
    return ntp_stratum;
}

#if ENABLE_NTP || ENABLE_GPS
// Marker: static variable
static volatile absolute_time_t sync_expiry;

// We should allow calling this from an ISR
void update_rtc(time_t result, uint8_t stratum) {
    // Timezone correction
    time_t lresult = result + TZ_DIFF_SEC;
    struct tm *lt = gmtime(&lresult);
    datetime_t dt = {
        .year  = lt->tm_year + 1900,
        .month = lt->tm_mon + 1,
        .day = lt->tm_mday,
        .dotw = lt->tm_wday,
        .hour = lt->tm_hour,
        .min = lt->tm_min,
        .sec = lt->tm_sec
    };
    if (rtc_set_datetime(&dt)) {
        ntp_stratum = stratum + 1;
        // So that NTP is not run when we have GPS time
        sync_expiry = make_timeout_time_ms(NTP_INTERVAL_MS);
    }
}
#endif

#if ENABLE_NTP

static const uint16_t NTP_MSG_LEN = 48;
// Seconds between 1 Jan 1900 and 1 Jan 1970
static const uint32_t NTP_DELTA = 2208988800;
// Time to wait in case UDP requests are lost
static const uint32_t UDP_TIMEOUT_TIME_MS = 5 * 1000;

/// Close this request
static void ntp_req_close(struct ntp_client *state) {
    if (!state)
        return;
    if (state->pcb) {
        cyw43_arch_lwip_begin();
        udp_remove(state->pcb);
        cyw43_arch_lwip_end();
        state->pcb = NULL;
    }
    state->in_progress = false;
}

// NTP data received
static void ntp_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    struct ntp_client *state = (struct ntp_client *)arg;
    cyw43_arch_lwip_check();
    uint8_t mode = pbuf_get_at(p, 0) & 0x7;
    uint8_t stratum = pbuf_get_at(p, 1);

    // Check the result
    if (ip_addr_cmp(addr, &state->server_address) && port == NTP_PORT && p->tot_len == NTP_MSG_LEN &&
        mode == 0x4 && stratum != 0) {
        uint8_t seconds_buf[4] = {0};
        pbuf_copy_partial(p, seconds_buf, sizeof(seconds_buf), 40);
        uint32_t seconds_since_1900 = seconds_buf[0] << 24 | seconds_buf[1] << 16 | seconds_buf[2] << 8 | seconds_buf[3];
        uint32_t seconds_since_1970 = seconds_since_1900 - NTP_DELTA;
        time_t epoch = seconds_since_1970;
        update_rtc(epoch, stratum);
    } else {
        puts("Invalid NTP response");
    }
    ntp_req_close(state);
    pbuf_free(p);
}

// Make an NTP request
static void do_send_ntp_request(const char *_hostname, const ip_addr_t *ipaddr, void *arg) {
    struct ntp_client *state = (struct ntp_client *)arg;
    if (ipaddr) {
        state->server_address = *ipaddr;
        printf("NTP address %s\n", ipaddr_ntoa(ipaddr));
    }
    else {
        puts("NTP DNS request failed");
        ntp_req_close(state);
        return;
    }
    cyw43_arch_lwip_begin();
    // Create a new UDP PCB structure. That this function is called
    // should be enough evidence that we are the only one working on
    // the `state` structure
    state->pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!state->pcb) {
        cyw43_arch_lwip_end();
        puts("Failed to create pcb");
        ntp_req_close(state);
        return;
    }
    udp_recv(state->pcb, ntp_recv_cb, state);
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    uint8_t *payload = (uint8_t *) p->payload;
    memset(payload, 0, NTP_MSG_LEN);
    payload[0] = 0x1b;
    udp_sendto(state->pcb, p, &state->server_address, NTP_PORT);
    pbuf_free(p);
    cyw43_arch_lwip_end();
}

/// Perform initialisation
bool ntp_client_init(struct ntp_client *state) {
    if (!state)
        return false;
    // Meaningful init values
    state->in_progress = false;
    state->pcb = NULL;
    // Expires immediately, so when `ntp_client_check_run` is called for the
    // first time, it will always execute an NTP request
    sync_expiry = get_absolute_time();
    return true;
}

/// Check and see if the time should be synchronized
void ntp_client_check_run(struct ntp_client *state) {
    if (!state)
        return;

    // Check for timed-out requests
    if (state->in_progress && absolute_time_diff_us(get_absolute_time(), state->deadline) < 0) {
        puts("NTP request timed out");
        ntp_req_close(state);
    }
    if (absolute_time_diff_us(get_absolute_time(), sync_expiry) >= 0)
        // Not time to sync yet
        // Successful GPS syncs renew `sync_expiry` so we also get here
        return;

    if (state->in_progress) {
        // Timeout is already checked for
        puts("Skipping NTP request, another is still in progress");
        return;
    }
    // Time to close the connection in case UDP requests are lost
    state->deadline = make_timeout_time_ms(UDP_TIMEOUT_TIME_MS);

    // Now we actually do the UDP stuff
    // Mark this before actually calling any lwIP functions,
    // so we don't overwrite stuff and cause a memory leak
    state->in_progress = true;
    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname(NTP_SERVER, &state->server_address, do_send_ntp_request, state);
    cyw43_arch_lwip_end();

    if (err == ERR_OK) {
        // Cached result
        do_send_ntp_request(NTP_SERVER, &state->server_address, state);
    } else if (err != ERR_INPROGRESS) { // ERR_INPROGRESS means expect a callback
        puts("DNS request for NTP failed");
        ntp_req_close(state);
    }
    // Let `update_rtc` update `sync_expiry`, so that a failed NTP request
    // is retried as soon as we discover that it has timed out
}

#endif
