/*
 *  light.c
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

/* This module is designed to be used around a 8-20V LED light.
 * There are two designs for the surrounding circuit: a buck arrangement and
 * a boost arrangement. Since Pico runs on USB/5V, the former requires a 20V
 * supply in front of this part of the circuit.
 *
 * Buck: (https://maiyun.me/blog/2022/11/11/Buck-Converter). This design is
 * easier to implement and almost works with any diode or FET.
 * N-MOSFET:
 *   - Connect the gate to LIGHT_PIN
 *   - Connect the source to GND
 *   - Connect the drain to the LED, a capacitor, and the anode of a diode
 *   - Connect the cathode of the diode to the 20V supply and one end of an
 *     inductor of about 40uH, whose other end is connected to the light and
 *     the capacitor
 *
 * Boost: (https://maiyun.me/blog/2024/03/07/Boost-Converter).
 * I got best results with an IRL520N and a 30uH inductor from an
 * old power supply.
 *
 * Now connect the drain to the 5V rail via the inductor. The drain also
 * goes to the light via a diode (in this direction).
 * To form a closed loop (software TODO), divide the output: 68k and 6.8k.
 * This is calculated at max output = 33V so that the divided voltage is
 * within ADC range with a 3.00V reference (LM4040).
 */

#include "config.h"
#include "thekit4_pico_w.h"
#include "log.h"

#include <math.h>

#include "pico/platform.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"

#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/rtc.h"

#if ENABLE_LIGHT
// Marker: static variable
static volatile uint16_t __uninitialized_ram(current_pwm_level);
// This is always the bit complement of `current_pwm_level` when
// `current_pwm_level` is initialized. Used as a form of checksum
// Marker: static variable
static volatile uint16_t __uninitialized_ram(current_pwm_level_complement);
// Set the light intensity as a percentage
#define SET_INTENSITY(intensity) do { \
    uint16_t __level = intensity_to_dcycle((intensity)); \
    current_pwm_level = __level; \
    current_pwm_level_complement = ~__level; \
    pwm_set_gpio_level(LIGHT_PIN, __level); \
} while (0)

/// Convert a desired light intensity to the actual PWM level
/* constexpr */
static uint16_t intensity_to_dcycle(float intensity) {
    float real_intensity = exp(intensity * log(101.) / 100.) - 1.;
#if LIGHT_IS_BUCK
    float voltage = real_intensity * (19.2 - 7.845) / 100. + 7.845;
    if (7.845 < voltage && voltage <= 9.275)
        return (uint16_t)((-7.664 + voltage) * 0.281970 * WRAP);
    if (9.275 < voltage && voltage <= 13.75)
        return (uint16_t)((6.959 + voltage) * 0.026520 * WRAP);
    if (13.75 < voltage && voltage <= 16.88)
        return (uint16_t)((-2.529 + voltage) * 0.049485 * WRAP);
    if (16.88 < voltage)
    {
        uint16_t r = (uint16_t)((26.90 + voltage) * 0.021692 * WRAP);
        return r > WRAP ? WRAP : r;
    }
#else
    float voltage = real_intensity * (25.0 - 7.936) / 100. + 7.936;
    if (7.936 < voltage && voltage <= 9.122)
        return (uint16_t)((-7.900 + voltage) * 0.298954 * WRAP);
    if (9.122 < voltage && voltage <= 14.874)
        return (uint16_t)((10.369 + voltage) * 0.018742 * WRAP);
    if (14.874 < voltage && voltage <= 20.305)
        return (uint16_t)((32.852 + voltage) * 0.009913 * WRAP);
    if (20.305 < voltage)
    {
        uint16_t r = (uint16_t)((86.950 + voltage) * 0.004913 * WRAP);
        // Limit max duty so that the inductor doesn't complain
        // Found by experiment yielding 29V, which is the max these
        // piecewise functions are fitted to
        const uint16_t max_duty = 0.576 * WRAP;
        return r > max_duty ? max_duty : r;
    }
#endif
    return 0;
}

/// Retrieve the current PWM level
uint16_t light_get_pwm_level(void) {
    return current_pwm_level;
}

// For rtc alarm
static void light_on(void) {
    SET_INTENSITY(100);
}

// For rtc alarm
static void light_off(void) {
    SET_INTENSITY(0);
}

// For gpio irq
void light_toggle(void) {
    // Marker: static variable
    // Debounce
    static volatile uint32_t last_button1_irq_timestamp = 0;
    uint32_t irq_timestamp = time_us_32();
    if (irq_timestamp - last_button1_irq_timestamp < 8000)
        return;
    last_button1_irq_timestamp = irq_timestamp;
    uint16_t new_level = current_pwm_level ? 0 : 100;
    SET_INTENSITY(new_level);
    LOG_INFO1("Toggling");
}

void light_init(void) {
    // IO
    gpio_set_function(LIGHT_PIN, GPIO_FUNC_PWM);

    // Button is set up in irq.c

    // Check if the state is valid
    if (current_pwm_level & current_pwm_level_complement)
        // Invalid state, reset to default
        SET_INTENSITY(0);

    // PWM
    uint light_slice_num = pwm_gpio_to_slice_num(LIGHT_PIN);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clockdiv);
    pwm_config_set_wrap(&config, WRAP - 1);
    pwm_init(light_slice_num, &config, true);
    pwm_set_gpio_level(LIGHT_PIN, current_pwm_level);
    pwm_set_enabled(light_slice_num, true);

    // SMPS feedback ADC
    adc_gpio_init(ADC_SMPS_FB_PIN);
}

/// Takes a percentage perceived intensity and dim the light
void light_dim(float intensity) {
    SET_INTENSITY(intensity);
    LOG_INFO("Dimming to %d\n", (int)current_pwm_level);
}

/// Take a single feedback voltage reading
float light_smps_measure(void) {
    adc_select_input(ADC_ZERO_PIN - 26);
    uint16_t bias = adc_read();
    adc_select_input(ADC_SMPS_FB_PIN - 26);
    uint16_t place = adc_read();
    uint16_t sensed = place - bias;
    float voltage = (VAref / 4096.00) * sensed;
    return voltage * LIGHT_SMPS_FB_RATIO;
}

static void do_register_alarm(const datetime_t *current, int index) {
    datetime_t alarm = *current;
    alarm.hour = light_sched[index].hour;
    alarm.min = light_sched[index].min;
    alarm.sec = 0;
    rtc_enable_alarm();
    if (light_sched[index].on)
        rtc_set_alarm(&alarm, light_on);
    else
        rtc_set_alarm(&alarm, light_off);
    LOG_INFO("Registered alarm to turn %s the light at %04d-%02d-%02d %d:%02d\n",
           light_sched[index].on ? "on" : "off", alarm.year, alarm.month,
           alarm.day, alarm.hour, alarm.min);
}

/// Move a datetime to the next day
static void next_day(datetime_t *dt) {
    if (dt->day < 28) {
        dt->day += 1;
        return;
    }
    switch (dt->month) {
        case 1:
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
            if (dt->day == 31) {
                dt->day = 1;
                dt->month += 1;
                return;
            }
            dt->day += 1;
            return;
        case 2:
            // We already know it is day 28
            dt->day = 1;
            dt->month += 1;
            return;
        case 4:
        case 6:
        case 9:
        case 11:
            if (dt->day == 30) {
                dt->day = 1;
                dt->month += 1;
                return;
            }
            dt->day += 1;
            return;
        default:
            // ???
            return;
    }
}

void light_register_next_alarm(datetime_t *current) {
    int n_alarms = sizeof(light_sched) / sizeof(struct light_sched_entry);
    // Find the next alarm
    for (int i = 0; i < n_alarms; ++i) {
        if (light_sched[i].hour > current->hour) {
            do_register_alarm(current, i);
            return;
        }
        if (light_sched[i].hour == current->hour
                && light_sched[i].min > current->min) {
            do_register_alarm(current, i);
            return;
        }
        // Go to the next one
    }
    // We past the last one. Register the first alarm after incrementing `day`
    next_day(current);
    do_register_alarm(current, 0);
}

#endif
