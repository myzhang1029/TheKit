/*
 *  gps_util.h
 *  Copyright (C) 2023 Zhang Maiyun <me@maiyun.me>
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

//! Yet another ad-hoc GPS NMEA-0183 parser.
//! Also slightly faster than TinyGPS++ for my use case.

#ifndef _GPS_UTIL_H
#define _GPS_UTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#if defined(ARDUINO)
#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
typedef unsigned long timestamp_t;
#define timestamp_micros() micros()
#elif defined(RPI_PICO)
#include "hardware/timer.h"
typedef uint64_t timestamp_t;
#define timestamp_micros() time_us_64()
#elif defined(unix) || defined(__unix__) || defined(__unix) || defined(__APPLE__)
typedef uint64_t timestamp_t;
static inline timestamp_t timestamp_micros()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
#define HAVE_TIMESTAMP 1
#else
#warning "GPS timestamping is not implemented for this platform"
#define HAVE_TIMESTAMP 0
typedef uint64_t timestamp_t;
#define timestamp_micros() 0
#endif

struct gps_status {
    // `true` if RMC gives 'A'
    bool gps_valid;
    // `true` if GGA, RMC, or ZDA gives a valid time
    bool gps_time_valid;
    // North is positive, South is negative
    float gps_lat;
    // East is positive, West is negative
    float gps_lon;
    // Altitude in meters
    float gps_alt;
    // Number of satellites used in position fix
    uint8_t gps_sat_num;
    uint8_t utc_hour;
    uint8_t utc_min;
    float utc_sec;
    uint16_t utc_year;
    uint8_t utc_month;
    uint8_t utc_day;
    // Maximum length of a sentence that we care about plus some headroom
    // '$GNGGA,000000.000000,00000.000000,N,00000.000000,W,1,99,1.5,00000.000,M,00000.000,M,00.0,0000,*4D'
    // '$' is never included; the first character is the first character of the sentence type
    // The parser is immediately called once a newline is received.
    char buffer[128];
    // Current position in buffer (also length used)
    uint8_t buffer_pos;
    // Whether we are currently in a sentence
    bool in_sentence;
    // Timestamp of the previous update to the position
    timestamp_t last_position_update;
    // Timestamp of the previous update to the time
    timestamp_t last_time_update;
};

#define GPS_STATUS_INIT { \
    .gps_valid = false, \
    .gps_time_valid = false, \
    .gps_lat = 0, \
    .gps_lon = 0, \
    .gps_alt = 0, \
    .gps_sat_num = 0, \
    .utc_hour = 0, \
    .utc_min = 0, \
    .utc_sec = 0, \
    .utc_year = 0, \
    .utc_month = 0, \
    .utc_day = 0, \
    .buffer = {0}, \
    .buffer_pos = 0, \
    .in_sentence = false, \
    .last_position_update = 0, \
    .last_time_update = 0, \
}

/// Feed a character to the parser, returns true if a sentence is parsed successfully
bool gpsutil_feed(struct gps_status *gps_status, int c);

/// Get the current time in UTC
bool gpsutil_get_time(const struct gps_status *gps_status, time_t *t, timestamp_t *timestamp);

/// Get the current GPS position
bool gpsutil_get_location(const struct gps_status *gps_status, float *lat, float *lon, float *alt, timestamp_t *timestamp);

#endif
