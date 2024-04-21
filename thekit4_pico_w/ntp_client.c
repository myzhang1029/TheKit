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
#include "log.h"
#include "ntp.h"

#include <assert.h>
#include <math.h>
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
// NTP reference identifier
// Marker: static variable
static volatile uint32_t ntp_ref = 0;

uint8_t ntp_get_stratum(void) {
    return ntp_stratum;
}

uint32_t ntp_get_ref(void) {
    return ntp_ref;
}

#if ENABLE_NTP || ENABLE_GPS
// Marker: static variable
static volatile absolute_time_t sync_expiry;
// Difference for calculating UTC time microsecond part by
// absolute_time_diff_us(ntp_us_offset, get_absolute_time())
// Then, assuming that the RTC works properly, this value % 1000000
// is the microsecond part
// In other words, this timestamp marks the start of a UTC second
// Only for NTP internal use
static volatile absolute_time_t ntp_us_offset;

// We should allow calling these from an ISR
void ntp_update_rtc(time_t result, uint8_t stratum, uint32_t ref) {
    datetime_t dt;
    unix_to_datetime(result, &dt);
    // Let the RTC follow UTC
    if (rtc_set_datetime(&dt)) {
        ntp_stratum = stratum + 1;
        ntp_ref = ref;
        // So that NTP is not run when we have GPS time
        sync_expiry = make_timeout_time_ms(NTP_INTERVAL_MS);
    }
}

void ntp_set_utc_us(uint32_t value) {
    assert(value < 1000000);
    ntp_us_offset = delayed_by_us(get_absolute_time(), 1000000 - value);
}

uint32_t ntp_get_utc_us(void) {
    return absolute_time_diff_us(ntp_us_offset, get_absolute_time()) % 1000000;
}

void ntp_apply_us_offset(int32_t offset) {
    uint64_t new_us = ntp_get_utc_us() + offset;
    uint32_t second_correction = 0;
    if (new_us < 0) {
        new_us += 1000000;
        second_correction = -1;
    } else if (new_us >= 1000000) {
        new_us -= 1000000;
        second_correction = 1;
    }
    ntp_set_utc_us(new_us);
    if (second_correction) {
        datetime_t dt;
        if (rtc_get_datetime(&dt)) {
            unix_to_datetime(datetime_to_unix(&dt) + second_correction, &dt);
            rtc_set_datetime(&dt);
        }
    }
}
#endif

#if ENABLE_NTP
/// Fill the current time into `tx_ts_*`, in network byte order.
/// Call this as close to sending the request as possible.
static void ntp_fill_tx(struct ntp_message *outgoing) {
    datetime_t dt;
    if (!rtc_get_datetime(&dt)) {
        LOG_WARN1("Failed to get datetime, falling back to simple request");
        // Assuming that the structure is zeroed
        return;
    }
    time_t t = datetime_to_unix(&dt);
    outgoing->tx_ts_sec = lwip_htonl(t + NTP_DELTA);
    outgoing->tx_ts_frac = lwip_htonl(((uint64_t)ntp_get_utc_us() << 26) / 15625);
}

/// Fill the current time into `ref_ts_*`, in host byte order.
/// Call this as close to receiving the response as possible.
static void ntp_fill_rx_as_ref(struct ntp_message *incoming) {
    datetime_t dt;
    if (!rtc_get_datetime(&dt)) {
        LOG_WARN1("Failed to get datetime, falling back to simple request");
        // Assuming that the structure is zeroed
        return;
    }
    time_t t = datetime_to_unix(&dt);
    incoming->ref_ts_sec = t + NTP_DELTA;
    incoming->ref_ts_frac = ((uint64_t)ntp_get_utc_us() << 26) / 15625;
}

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

/// Process an incoming NTP response and update the RTC
/// `incoming` should have been modified before calling this function such that:
/// - Fields are in host byte order
/// - `ref_ts` should be replaced with the time the server received the request (from `ntp_fill_rx_as_ref`)
static void ntp_process_response(const struct ntp_message *incoming, uint8_t stratum, uint32_t ref) {
    uint32_t t1s = incoming->orig_ts_sec;
    uint32_t t2s = incoming->rx_ts_sec;
    uint32_t t3s = incoming->tx_ts_sec;
    uint32_t t4s = incoming->ref_ts_sec;
    uint32_t t1f = incoming->orig_ts_frac;
    uint32_t t2f = incoming->rx_ts_frac;
    uint32_t t3f = incoming->tx_ts_frac;
    uint32_t t4f = incoming->ref_ts_frac;
    // RFC 5905 calculation
    // Since we use a `uint64_t` to keep the microseconds, we can completely eliminate floating
    // point operations
    // These are twice the correct values, the division is deferred
    int64_t soffset2 = (t2s - t1s) + (t3s - t4s);
    if (soffset2 > NTP_EPSILON2 || soffset2 < -NTP_EPSILON2) {
        LOG_WARN1("Big offset, assuming initial sync");
        ntp_update_rtc(t3s - NTP_DELTA, stratum, ref);
        ntp_set_utc_us((t3f * 15625) >> 27);
    }
    else {
        int64_t foffset2 = (t2f - t1f) + (t3f - t4f);
        // factor = 10^6 2^-32 = 5^6 2^-26, divide one more time so it is no longer twice the offset
        int32_t foffset_us = (foffset2 * 15625) >> 27;
        // If `soffset2` is odd, add 0.5 seconds to foffset_us
        if (soffset2 & 1) {
            if (soffset2 > 0)
                foffset_us += 500000;
            else
                foffset_us -= 500000;
        }
        ntp_apply_us_offset(foffset_us);
    }
}

// NTP data received callback
static void ntp_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    struct ntp_client *state = (struct ntp_client *)arg;
    // This struct should use host byte order
    struct ntp_message incoming;
    cyw43_arch_lwip_check();
    // Sanity check
    if (!ip_addr_cmp(addr, &state->server_address) || port != NTP_PORT) {
        LOG_ERR1("Invalid NTP response");
        goto bad;
    }
    if (!ntp_fill_pbuf(p, &incoming)) {
        LOG_ERR1("Failed to copy NTP response");
        goto bad;
    }
    ntp_dump_debug(&incoming);
    ntp_fill_rx_as_ref(&incoming);
    uint8_t mode = incoming.flags & 0x7;
    uint8_t version = (incoming.flags >> 3) & 0x7;
    if (incoming.stratum == 0 || mode != 0x4 || version != NTP_VERSION) {
        LOG_ERR1("Invalid or unsupported NTP response");
        goto bad;
    }
    uint32_t new_ref = lwip_htonl(ntp_make_ref(addr));
    ntp_process_response(&incoming, incoming.stratum, new_ref);
bad:
    ntp_req_close(state);
    pbuf_free(p);
}

// Make an NTP request
static void do_send_ntp_request(const char *_hostname, const ip_addr_t *ipaddr, void *arg) {
    struct ntp_client *state = (struct ntp_client *)arg;
    if (ipaddr) {
        state->server_address = *ipaddr;
        LOG_DEBUG("NTP address %s\n", ipaddr_ntoa(ipaddr));
    }
    else {
        LOG_ERR1("NTP DNS request failed");
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
        LOG_ERR1("Failed to create pcb");
        ntp_req_close(state);
        return;
    }
    udp_recv(state->pcb, ntp_recv_cb, state);
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    // This struct should use network byte order
    struct ntp_message *outgoing = (struct ntp_message *) p->payload;
    memset(outgoing, 0, NTP_MSG_LEN);
    outgoing->flags = (NTP_VERSION << 3) | 0x3; // client mode
    ntp_fill_tx(outgoing);
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
        LOG_ERR1("NTP request timed out");
        ntp_req_close(state);
    }
    if (absolute_time_diff_us(get_absolute_time(), sync_expiry) >= 0)
        // Not time to sync yet
        // Successful GPS syncs renew `sync_expiry` so we also get here
        return;

    if (state->in_progress)
        return;
    // Time to close the connection in case UDP requests are lost
    state->deadline = make_timeout_time_ms(NTP_UDP_TIMEOUT_TIME_MS);

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
        LOG_ERR1("DNS request for NTP failed");
        ntp_req_close(state);
    }
    // Let `ntp_update_rtc` update `sync_expiry`, so that a failed NTP request
    // is retried as soon as we discover that it has timed out
}

#endif
