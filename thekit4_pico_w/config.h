#ifndef _CONFIG_H
#define _CONFIG_H

#include "thekit4_pico_w.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include "lwip/ip_addr.h"

// Define WOLFRAM_DATABIN_ID, DDNS_HOSTNAME, DDNS_KEY, wifi_config, HOSTNAME
#include "private_config.h"

#ifndef ENABLE_WATCHDOG
#define ENABLE_WATCHDOG 1
#endif
#ifndef ENABLE_TEMPERATURE_SENSOR
#define ENABLE_TEMPERATURE_SENSOR 1
#endif
#ifndef ENABLE_LIGHT
#define ENABLE_LIGHT 1
#endif
#ifndef ENABLE_DDNS
#define ENABLE_DDNS 1
#endif
#ifndef ENABLE_NTP
#define ENABLE_NTP 1
#endif
#ifndef ENABLE_GPS
#define ENABLE_GPS 1
#endif

// Zeroing pin for all ADC measurements
static const uint ADC_ZERO_PIN = 28;
// LM2020
static const float VAref = 3.0; // Volts

// Light-related
#if ENABLE_LIGHT
// Definitions
static const uint LIGHT_PIN = 3;
static const uint BUTTON1_PIN = 18;
static const uint ADC_SMPS_FB_PIN = 27;
static const float LIGHT_SMPS_FB_RATIO = 11.0;
#define BUTTON1_EDGE_TYPE GPIO_IRQ_EDGE_FALL
// Frequency = 125MHz / clockdiv / WRAP, so we are at 125kHz
static const float clockdiv = 1.;
// Max duty
static const uint16_t WRAP = 1000;

// Light-based alarms
// Sort chronologically
static const struct light_sched_entry light_sched[] = {
    {6, 0, true},
    {8, 0, false},
    {20, 0, true},
    {22, 0, false},
};
#endif

// Light sensor
// Photocell to ground giving high when dark
static const uint LIGHT_SENSOR_PIN = 22;

// Temperature-related
#if ENABLE_TEMPERATURE_SENSOR
// BMP280 Information
#define BMP280_I2C i2c0
static const uint BMP280_SDA_PIN = 20;
static const uint BMP280_SCL_PIN = 21;
static const uint BMP280_ADDR = 0x76;
#endif

// Tasks-related
// 5 minutes
static const int32_t TASKS_INTERVAL_MS = (5 * 60 * 1000);
#if ENABLE_TEMPERATURE_SENSOR
static const char WOLFRAM_HOST[] = "datadrop.wolframcloud.com";
static const char WOLFRAM_URI[] = "/api/v1.0/Add?bin=%s&temperature=%.4f";
static const size_t WOLFRAM_URI_BUFSIZE = sizeof(WOLFRAM_URI) + sizeof(WOLFRAM_HOST) + sizeof(WOLFRAM_DATABIN_ID) - 6 + 8;
/* Access data as:
 * ```mma
 * data := TimeSeries[
 *   MapAt[ToExpression, #, 2] & /@
 *    Normal[TimeSeries[Databin["ID"]]["temperature"]]
 * ]
 * ```
 * because we are uploading the data as strings
 */
#endif
#if ENABLE_DDNS
static const char DDNS_HOST[] = "dyn.dns.he.net";
static const char DDNS_URI[] = "/nic/update?hostname=%s&password=%s&myip=%s";
static const size_t DDNS_URI_BUFSIZE = sizeof(DDNS_URI) + sizeof(DDNS_HOST) + sizeof(DDNS_KEY) + IPADDR_STRLEN_MAX - 6 + 8;
#endif

// Time-related
#if ENABLE_NTP
static const char NTP_SERVER[] = "time-b-g.nist.gov";
static const uint16_t NTP_PORT = 123;
// 2 minutes between syncs
static const uint64_t NTP_INTERVAL_US = 120 * 1000 * 1000;
// Time to wait in case UDP requests are lost
static const uint32_t NTP_UDP_TIMEOUT_TIME_MS = 5 * 1000;
#endif
// Timezone for the alarms (RTC is in localtime);
static const int TZ_DIFF_SEC = -7 * 3600;

// GPS-related
#if ENABLE_GPS
#define GPS_UART uart0
// TX is not actually used
static const uint GPS_TX_PIN = 12;
static const uint GPS_RX_PIN = 13;
static const uint GPS_EN_PIN = 11;
static const uint GPS_PPS_PIN = 14;
static const uint GPS_BAUD = 115200;
#define PPS_EDGE_TYPE GPIO_IRQ_EDGE_RISE
#endif

// Networking-related
static const char DEFAULT_DNS[] = "1.1.1.1";
static const bool FORCE_DEFAULT_DNS = false;

#endif
