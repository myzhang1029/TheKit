/*
 *  wifi.c
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

#include "pico/cyw43_arch.h"

#if ENABLE_WATCHDOG
#include "hardware/watchdog.h"
#endif

#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/apps/mdns.h"

static void register_mdns(void) {
    cyw43_arch_lwip_begin();
    mdns_resp_init();
    mdns_resp_add_netif(&WIFI_NETIF, HOSTNAME);
    cyw43_arch_lwip_end();
}

static void print_ip(void) {
    cyw43_arch_lwip_begin();
    LOG_INFO("IP Address: %s\n", ipaddr_ntoa(&WIFI_NETIF.ip_addr));
    LOG_INFO("IPv6 Address[0]: %s, state=%d\n", ipaddr_ntoa(&WIFI_NETIF.ip6_addr[0]), (int) WIFI_NETIF.ip6_addr_state[0]);
    LOG_INFO("IPv6 Address[1]: %s, state=%d\n", ipaddr_ntoa(&WIFI_NETIF.ip6_addr[1]), (int) WIFI_NETIF.ip6_addr_state[1]);
    LOG_INFO("IPv6 Address[2]: %s, state=%d\n", ipaddr_ntoa(&WIFI_NETIF.ip6_addr[2]), (int) WIFI_NETIF.ip6_addr_state[2]);
    cyw43_arch_lwip_end();
}

static void print_and_check_dns(void) {
    cyw43_arch_lwip_begin();
    const ip_addr_t *pdns = dns_getserver(0);
    LOG_INFO("DNS Server: %s\n", ipaddr_ntoa(pdns));
    if (FORCE_DEFAULT_DNS || ip_addr_eq(pdns, &ip_addr_any)) {
        ip_addr_t default_dns;
        LOG_INFO("Reconfiguing DNS server to %s\n", DEFAULT_DNS);
        ipaddr_aton(DEFAULT_DNS, &default_dns);
        dns_setserver(0, &default_dns);
    }
    cyw43_arch_lwip_end();
}

/// Connect to Wi-Fi
bool wifi_connect(void) {
    int n_configs = sizeof(wifi_config) / sizeof(struct wifi_config_entry);
    for (int i = 0; i < n_configs; ++i) {
        LOG_INFO("Attempting Wi-Fi %s\n", wifi_config[i].ssid);
#if ENABLE_WATCHDOG
        watchdog_update();
#endif
        int result = cyw43_arch_wifi_connect_timeout_ms(
            wifi_config[i].ssid,
            wifi_config[i].password,
            wifi_config[i].auth,
            5000
        );
#if ENABLE_WATCHDOG
        watchdog_update();
#endif
        if (result == 0) {
            print_ip();
            print_and_check_dns();
            register_mdns();
            return true;
        }
        LOG_ERR("Failed with status %d\n", result);
    }
    LOG_WARN1("Cannot connect to Wi-Fi");
    return false;
}
