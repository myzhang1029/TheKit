/*
 *  pico_ethntp.c
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

#include <string.h>

#include "hardware/clocks.h"

#include "lwip/dhcp.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"

#include "pico_ethntp.h"
#include "pico_eth/ethpio_arch.h"
#include "log.h"
#include "ntp.h"

// Marker: static variable
static struct ntp_client ntp_state;

static void init() {
    set_sys_clock_khz(120000, true);
    stdio_init_all();
    gps_init();

    ethpio_parameters_t config;
    config.pio_num = ETH_PIO_NUM;
    config.rx_pin = ETH_RX_PIN;
    config.tx_neg_pin = ETH_TX_NEG_PIN;
    memcpy(config.mac_address, MAC_ADDRESS, 6);
    strncpy(config.hostname, "picoeth", 15);
    eth_pio_arch_init(&config);
    // Set default IP address
    ip4_addr_t ipaddr;
    ip4_addr_t netmask;
    ip4_addr_t gw;
    IP4_ADDR(&ipaddr, 192, 168, 1, 110);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 1, 1);
    netif_set_addr(&netif, &ipaddr, &netmask, &gw);
    dhcp_start(&netif);

    if (!ntp_client_init(&ntp_state))
        LOG_WARN1("Cannot init NTP client");
    ntp_server_open();
}

int main() {
    init();
    while (1) {
        eth_pio_arch_poll();
        ntp_client_check_run(&ntp_state);
        gps_parse_available();
    }
    return 0;
}
