/*
 *  ntp_server.c
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
#include "thekit4_pico_w.h"
#include "log.h"
#include "ntp.h"

#include "pico/divider.h"

#include "lwip/pbuf.h"
#include "lwip/udp.h"

static void ntp_server_recv_cb(void *arg, struct udp_pcb *upcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    uint64_t now = ntp_get_utc_us(), now_uspart;
    uint64_t now_spart = divmod_u64u64_rem(now, 1000000, &now_uspart);
    now_spart += NTP_DELTA;
    now_uspart = (now_uspart << 26) / 15625;
    struct ntp_message received;
    bool result = pbuf_copy_partial(p, &received, NTP_MSG_LEN, 0);
    pbuf_free(p);
    if (!result) {
        LOG_ERR1("Failed to parse NTP message");
        return;
    }
    LOG_INFO("Received NTP request from [%s]:%u\n", ipaddr_ntoa(addr), port);
    ntp_dump_debug(&received);
    p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    assert(p != NULL);
    struct ntp_message *outgoing = (struct ntp_message *) p->payload;
    outgoing->flags = (NTP_VERSION << 3) | 0x4;
    outgoing->stratum = ntp_get_stratum();
    outgoing->poll = 0x03;
    outgoing->precision = 0xfa; // TODO: calculate this
    outgoing->root_delay = 0;
    outgoing->root_dispersion = 0;
    outgoing->ref_id = ntp_get_ref();
    outgoing->ref_ts_sec = 0; // TODO: provide this
    outgoing->ref_ts_frac = 0;
    outgoing->orig_ts_sec = received.tx_ts_sec;
    outgoing->orig_ts_frac = received.tx_ts_frac;
    outgoing->rx_ts_sec = lwip_htonl((uint32_t) now_spart);
    outgoing->rx_ts_frac = lwip_htonl((uint32_t) now_uspart);
    now = ntp_get_utc_us();
    now_spart = divmod_u64u64_rem(now, 1000000, &now_uspart);
    now_spart += NTP_DELTA;
    now_uspart = (now_uspart << 26) / 15625;
    outgoing->tx_ts_sec = lwip_htonl((uint32_t) now_spart);
    outgoing->tx_ts_frac = lwip_htonl((uint32_t) now_uspart);
    udp_sendto(upcb, p, addr, port);
    pbuf_free(p);
}

static bool ntp_server_open_one(struct udp_pcb *ntp_server_udp_pcb, uint8_t lwip_type, const ip_addr_t *ipaddr) {
    LOG_INFO("Starting NTP server on [%s]:%u\n", ipaddr_ntoa(ipaddr), NTP_PORT);
    ntp_server_udp_pcb = udp_new_ip_type(lwip_type);
    if (!ntp_server_udp_pcb) {
        LOG_ERR1("Failed to create NTP server UDP PCB");
        return false;
    }
    err_t err = udp_bind(ntp_server_udp_pcb, ipaddr, NTP_PORT);
    if (err != ERR_OK) {
        LOG_ERR1("Failed to bind NTP server UDP PCB");
        udp_remove(ntp_server_udp_pcb);
        return false;
    }
    udp_recv(ntp_server_udp_pcb, ntp_server_recv_cb, NULL);
    return true;
}

#if LWIP_IPV4
// Marker: static variable
static struct udp_pcb *ntp_server_udp_pcb4;
#endif
#if LWIP_IPV6
// Marker: static variable
static struct udp_pcb *ntp_server_udp_pcb6;
#endif

bool ntp_server_open(void) {
    bool success = true;
#if LWIP_IPV4
    success &= ntp_server_open_one(ntp_server_udp_pcb4, IPADDR_TYPE_V4, IP4_ADDR_ANY);
#endif
#if LWIP_IPV6
    success &= ntp_server_open_one(ntp_server_udp_pcb6, IPADDR_TYPE_V6, IP6_ADDR_ANY);
#endif
    return success;
}
