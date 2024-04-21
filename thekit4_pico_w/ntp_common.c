/*
 *  ntp_common.c
 *  Copyright (C) 2024 Zhang Maiyun <me@maiyun.me>
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
#include "ntp.h"
#include "log.h"

#include <time.h>

#include "pico/time.h"
#include "hardware/rtc.h"

// Our current position in the stratum system
// used in http_server.c and tasks.c
// It remains 16 if NTP nor GPS is enabled
// Marker: static variable
static volatile uint8_t ntp_stratum = 16;
// NTP reference identifier
// Marker: static variable
static volatile uint32_t ntp_ref = 0;
// Marker: static variable
static volatile absolute_time_t last_sync;
// to_us_since_boot(get_absolute_time()) plus this value is the number of microseconds since the UNIX epoch
// This is likely a large number, so 0 means the system has not been synchronized
// Marker: static variable
static volatile uint64_t ntp_boot_us = 0;

// Some getters
uint8_t ntp_get_stratum(void) {
    return ntp_stratum;
}

uint32_t ntp_get_ref(void) {
    return ntp_ref;
}

absolute_time_t ntp_get_last_sync(void) {
    return last_sync;
}

// We should allow calling these from an ISR
/// time: UTC time as UNIX timestamp
/// us: microsecond part of the UTC time
void ntp_update_time(uint64_t now, uint8_t stratum, uint32_t ref) {
    ntp_boot_us = now - to_us_since_boot(get_absolute_time());
    ntp_stratum = stratum;
    ntp_ref = ref;
    last_sync = get_absolute_time();
}

/// Update with an offset
void ntp_update_time_by_offset(int64_t offset, uint8_t stratum, uint32_t ref) {
    ntp_boot_us += offset;
    ntp_stratum = stratum;
    ntp_ref = ref;
    last_sync = get_absolute_time();
}

uint64_t ntp_get_utc_us(void) {
    return ntp_boot_us + to_us_since_boot(get_absolute_time());
}

/// Update the RTC with our version and store the current time in `dt`
bool ntp_update_rtc(datetime_t *dt) {
    time_t t = ntp_get_utc_us() / 1000000;
    unix_to_local_datetime(t, dt);
    return rtc_set_datetime(dt);
}

/// Convert UNIX timestamp to `datetime_t`
void unix_to_local_datetime(time_t result, datetime_t *dt) {
    result += TZ_DIFF_SEC;
    struct tm *lt = gmtime(&result);
    dt->year = lt->tm_year + 1900;
    dt->month = lt->tm_mon + 1;
    dt->day = lt->tm_mday;
    dt->dotw = lt->tm_wday;
    dt->hour = lt->tm_hour;
    dt->min = lt->tm_min;
    dt->sec = lt->tm_sec;
}

/// Make NTP Reference Identifier from a IP address, result is in host byte order
uint32_t ntp_make_ref(const ip_addr_t *addr) {
    if (IP_IS_V4(addr))
        return ip4_addr_get_u32(ip_2_ip4(addr));
    else if (IP_IS_V6(addr)) {
        // I don't want to implement MD5 on this platform
        const ip6_addr_t *ip6 = ip_2_ip6(addr);
        return ip6->addr[0] ^ ip6->addr[1] ^ ip6->addr[2] ^ ip6->addr[3];
    }
    return 0;
}

/// Demodulate a NTP message with a `struct pbuf`, result is in host byte order
bool ntp_from_pbuf(const struct pbuf *p, struct ntp_message *dest) {
    if (!p || p->tot_len != NTP_MSG_LEN || !dest)
        return false;
    bool result = pbuf_copy_partial(p, dest, sizeof(struct ntp_message), 0);
    // Convert to native byte order
    dest->root_delay = lwip_ntohl(dest->root_delay);
    dest->root_dispersion = lwip_ntohl(dest->root_dispersion);
    dest->ref_id = lwip_ntohl(dest->ref_id);
    dest->ref_ts_sec = lwip_ntohl(dest->ref_ts_sec);
    dest->ref_ts_frac = lwip_ntohl(dest->ref_ts_frac);
    dest->orig_ts_sec = lwip_ntohl(dest->orig_ts_sec);
    dest->orig_ts_frac = lwip_ntohl(dest->orig_ts_frac);
    dest->rx_ts_sec = lwip_ntohl(dest->rx_ts_sec);
    dest->rx_ts_frac = lwip_ntohl(dest->rx_ts_frac);
    dest->tx_ts_sec = lwip_ntohl(dest->tx_ts_sec);
    dest->tx_ts_frac = lwip_ntohl(dest->tx_ts_frac);
    return result;
}

/// Dump the contents of an NTP message to the debug log
void ntp_dump_debug(const struct ntp_message *msg) {
    LOG_DEBUG1("NTP message:");
    LOG_DEBUG("\tHeader: %02x\n", (int)msg->flags);
    LOG_DEBUG("\tStratum: %02x\n", (int)msg->stratum);
    LOG_DEBUG("\tPoll: %02x\n", (int)msg->poll);
    LOG_DEBUG("\tPrecision: %02x\n", (int)msg->precision);
    LOG_DEBUG("\tRoot Delay: %04x.%04x\n", (int)(msg->root_delay >> 16), (int)(msg->root_delay & 0xffff));
    LOG_DEBUG("\tRoot Dispersion: %04x.%04x\n", (int)(msg->root_dispersion >> 16), (int)(msg->root_dispersion & 0xffff));
    LOG_DEBUG("\tReference ID: %08x\n", (int)msg->ref_id);
    LOG_DEBUG("\tReference Timestamp: %08x.%08x\n", (int)msg->ref_ts_sec, (int)msg->ref_ts_frac);
    LOG_DEBUG("\tOriginate Timestamp: %08x.%08x\n", (int)msg->orig_ts_sec, (int)msg->orig_ts_frac);
    LOG_DEBUG("\tReceive Timestamp: %08x.%08x\n", (int)msg->rx_ts_sec, (int)msg->rx_ts_frac);
    LOG_DEBUG("\tTransmit Timestamp: %08x.%08x\n", (int)msg->tx_ts_sec, (int)msg->tx_ts_frac);
}
