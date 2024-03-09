/*
 *  temperature.c
 *  Copyright (C) 2022-2023 Zhang Maiyun <me@maiyun.me>
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

#include <math.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"

#if ENABLE_TEMPERATURE_SENSOR

void temperature_init(void) {
    adc_gpio_init(ADC_TEMP_PIN);
}

/// Take a single temperature measurement
float temperature_measure(void) {
    adc_select_input(ADC_ZERO_PIN - 26);
    uint16_t bias = adc_read();
    adc_select_input(ADC_TEMP_PIN - 26);
    uint16_t place = adc_read();
    uint16_t sensed = place - bias;
    float VR = (VAref / 4096.00) * sensed;
    float NTC = R * (VAref - VR) / VR;
    // R = R0 * exp(beta/T - beta/T0)
    // ln(R/R0) + beta/T0 = beta/T
    float T = BETA / (log(NTC / R0) + BETA / T0);
    return T - 273.15;
}

#endif

/// Measure VSYS voltage. See datasheet for details.
// Maybe I should put this in a separate file
// This however, does not work on Pico W:
// Pin 29 is also used for SPICLK to CYW43. We can force a reading by
// setting Pin 25 high, but that kills the WiFi.
float vsys_measure(void) {
    adc_select_input(ADC_ZERO_PIN - 26);
    uint16_t bias = adc_read();
    adc_select_input(29 - 26);
    uint16_t place = adc_read();
    uint16_t sensed = place - bias;
    float voltage = (VAref / 4096.00) * sensed;
    return voltage * 3.0;
}

/// Measure core temperature. See datasheet for details.
float temperature_core(void) {
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
    uint16_t sensed = adc_read();
    adc_set_temp_sensor_enabled(false);
    float voltage = (VAref / 4096.00) * sensed;
    return 27 - (voltage - 0.706) / 0.001721;
}
