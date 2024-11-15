#ifndef _CONFIG_H
#define _CONFIG_H

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define ENABLE_GPS 1
#define ENABLE_NTP 1

static const char HOSTNAME[] = "picoeth";

// Ethernet-related
static const uint ETH_PIO_NUM = 0;
static const uint ETH_RX_PIN = 26;
    // TX positive pin is the next pin after TX negative pin (21)
static const uint ETH_TX_NEG_PIN = 20;
static const uint8_t MAC_ADDRESS[] = {0xe8, 0x6b, 0xea, 0x24, 0x3b, 0xf0};

// Time-related
static const char NTP_SERVER[] = "time-b-g.nist.gov";
static const uint16_t NTP_PORT = 123;
// 2 minutes between syncs
static const uint64_t NTP_INTERVAL_US = 120 * 1000 * 1000;
// Time to wait in case UDP requests are lost
static const uint32_t NTP_UDP_TIMEOUT_TIME_MS = 5 * 1000;
// Timezone for the alarms (RTC is in localtime);
static const int TZ_DIFF_SEC = -7 * 3600;

// GPS-related
#define GPS_UART uart0
// TX is not actually used
static const uint GPS_TX_PIN = 12;
static const uint GPS_RX_PIN = 13;
static const uint GPS_EN_PIN = 11;
static const uint GPS_PPS_PIN = 14;
static const uint GPS_BAUD = 115200;
#define PPS_EDGE_TYPE GPIO_IRQ_EDGE_RISE

#endif
