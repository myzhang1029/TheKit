/*
 *  ntp.h
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

/* Bridge between NTP protocol shenanigans, LwIP, and RPi Pico RTC, mostly
 * functions that will only be used once in the code but too cluttered
 */

#ifndef _NTP_H
#define _NTP_H

#include <stdint.h>
#include <time.h>

#include "pico/util/datetime.h"

// The endianess of this structure is flexible
struct ntp_message {
    /// Leap indicator, version number, mode
    uint8_t flags;
    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;
    uint32_t root_delay;
    uint32_t root_dispersion;
    uint32_t ref_id;
    uint32_t ref_ts_sec;
    uint32_t ref_ts_frac;
    uint32_t orig_ts_sec;
    uint32_t orig_ts_frac;
    uint32_t rx_ts_sec;
    uint32_t rx_ts_frac;
    uint32_t tx_ts_sec;
    uint32_t tx_ts_frac;
};

static const uint16_t NTP_MSG_LEN = sizeof(struct ntp_message);
// Seconds between 1 Jan 1900 and 1 Jan 1970
static const uint32_t NTP_DELTA = 2208988800;
// (S)NTP version this code is compatible with
static const uint8_t NTP_VERSION = 4;
// Minimum server version we can accept
static const uint8_t NTP_VERSION_OK = 3;
// "GPS\0" in host byte order
static const uint32_t NTP_REF_GPS = 0x47505300;

// ntp_common.c
uint8_t ntp_get_stratum(void);
uint32_t ntp_get_ref(void);
absolute_time_t ntp_get_last_sync(void);

void ntp_update_time(uint64_t now, uint8_t stratum, uint32_t ref);
void ntp_update_time_by_offset(int64_t offset, uint8_t stratum, uint32_t ref);
uint64_t ntp_get_utc_us(void);
bool ntp_update_rtc(datetime_t *dt);

void unix_to_local_datetime(time_t result, datetime_t *dt);
uint32_t ntp_make_ref(const ip_addr_t *addr);
bool ntp_from_pbuf(const struct pbuf *p, struct ntp_message *dest);
void ntp_dump_debug(const struct ntp_message *msg);

// ntp_client.c
bool ntp_client_init(struct ntp_client *state);
void ntp_client_check_run(struct ntp_client *state);

#endif
