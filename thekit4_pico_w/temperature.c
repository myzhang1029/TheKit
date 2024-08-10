/*
 *  temperature.c
 *  Incorporated code from BSD-3-Clause bmp280_i2c.c
 *  Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
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

#include <math.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"

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

#if ENABLE_TEMPERATURE_SENSOR

// Marker: static variable
static struct bmp280_calib_data {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
} bmp280_calib_data;

#if 0
void ntc_temperature_init(void) {
    adc_gpio_init(ADC_TEMP_PIN);
}

/// Take a single temperature measurement
float ntc_temperature_measure(void) {
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

#define REG_CONFIG 0xF5
#define REG_CTRL_MEAS 0xF4

#define REG_TEMP_XLSB 0xFC
#define REG_TEMP_LSB 0xFB
#define REG_TEMP_MSB 0xFA

#define REG_PRESSURE_XLSB 0xF9
#define REG_PRESSURE_LSB 0xF8
#define REG_PRESSURE_MSB 0xF7

// calibration registers
// raw temp and pressure values need to be calibrated according to
// parameters generated during the manufacturing of the sensor
// there are 3 temperature params, and 9 pressure params, each with a LSB
// and MSB register, so we read from 24 registers
#define REG_DIG_T1_LSB 0x88
#define REG_DIG_T1_MSB 0x89
#define REG_DIG_T2_LSB 0x8A
#define REG_DIG_T2_MSB 0x8B
#define REG_DIG_T3_LSB 0x8C
#define REG_DIG_T3_MSB 0x8D
#define REG_DIG_P1_LSB 0x8E
#define REG_DIG_P1_MSB 0x8F
#define REG_DIG_P2_LSB 0x90
#define REG_DIG_P2_MSB 0x91
#define REG_DIG_P3_LSB 0x92
#define REG_DIG_P3_MSB 0x93
#define REG_DIG_P4_LSB 0x94
#define REG_DIG_P4_MSB 0x95
#define REG_DIG_P5_LSB 0x96
#define REG_DIG_P5_MSB 0x97
#define REG_DIG_P6_LSB 0x98
#define REG_DIG_P6_MSB 0x99
#define REG_DIG_P7_LSB 0x9A
#define REG_DIG_P7_MSB 0x9B
#define REG_DIG_P8_LSB 0x9C
#define REG_DIG_P8_MSB 0x9D
#define REG_DIG_P9_LSB 0x9E
#define REG_DIG_P9_MSB 0x9F
#define REG_DIG_N 24

static void bmp280_read_calibration_data(void) {
    uint8_t buf[REG_DIG_N] = {0};
    uint8_t reg = REG_DIG_T1_LSB;
    i2c_write_blocking(BMP280_I2C, BMP280_ADDR, &reg, 1, true); // true to keep master control of bus
    // read in one go as register addresses auto-increment
    i2c_read_blocking(BMP280_I2C, BMP280_ADDR, buf, REG_DIG_N, false); // false to release master control of bus

    // store these in a struct for later use
    bmp280_calib_data.dig_T1 = (uint16_t)(buf[1] << 8 | buf[0]);
    bmp280_calib_data.dig_T2 = (int16_t)(buf[3] << 8 | buf[2]);
    bmp280_calib_data.dig_T3 = (int16_t)(buf[5] << 8 | buf[4]);
    bmp280_calib_data.dig_P1 = (uint16_t)(buf[7] << 8 | buf[6]);
    bmp280_calib_data.dig_P2 = (int16_t)(buf[9] << 8 | buf[8]);
    bmp280_calib_data.dig_P3 = (int16_t)(buf[11] << 8 | buf[10]);
    bmp280_calib_data.dig_P4 = (int16_t)(buf[13] << 8 | buf[12]);
    bmp280_calib_data.dig_P5 = (int16_t)(buf[15] << 8 | buf[14]);
    bmp280_calib_data.dig_P6 = (int16_t)(buf[17] << 8 | buf[16]);
    bmp280_calib_data.dig_P7 = (int16_t)(buf[19] << 8 | buf[18]);
    bmp280_calib_data.dig_P8 = (int16_t)(buf[21] << 8 | buf[20]);
    bmp280_calib_data.dig_P9 = (int16_t)(buf[23] << 8 | buf[22]);
}

void bmp280_temperature_init(void) {
    i2c_init(BMP280_I2C, 100 * 1000);
    gpio_set_function(BMP280_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(BMP280_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(BMP280_SDA_PIN);
    gpio_pull_up(BMP280_SCL_PIN);

    // use the "handheld device dynamic" optimal setting (see datasheet)
    uint8_t buf[2];
    buf[0] = REG_CONFIG;
    // 500ms sampling time, x16 filter
    buf[1] = ((0x04 << 5) | (0x05 << 2)) & 0xFC;
    i2c_write_blocking(BMP280_I2C, BMP280_ADDR, buf, 2, false);

    buf[0] = REG_CTRL_MEAS;
    // osrs_t x1, osrs_p x4, normal mode operation
    buf[1] = (0x01 << 5) | (0x03 << 2) | 0x03;
    i2c_write_blocking(BMP280_I2C, BMP280_ADDR, buf, 2, false);

    bmp280_read_calibration_data();
}

static void bmp280_read_raw_data(int32_t *temp, int32_t *pressure) {
    uint8_t buf[6];
    uint8_t reg = REG_PRESSURE_MSB;
    i2c_write_blocking(BMP280_I2C, BMP280_ADDR, &reg, 1, true);
    i2c_read_blocking(BMP280_I2C, BMP280_ADDR, buf, 6, false);

    // store the 20 bit read in a 32 bit signed integer for conversion
    *pressure = (buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4);
    *temp = (buf[3] << 12) | (buf[4] << 4) | (buf[5] >> 4);
}

static void bmp280_compensate(int32_t raw_temp, int32_t raw_press, int32_t *temp_001C, uint32_t *press_Pa) {
    // Datasheet 8.2 pp45-46
    // bmp280_compensate_T_int32
    int32_t var1, var2;
    var1 = ((((raw_temp >> 3) - ((int32_t)bmp280_calib_data.dig_T1 << 1))) * ((int32_t)bmp280_calib_data.dig_T2)) >> 11;
    var2 = (((((raw_temp >> 4) - ((int32_t)bmp280_calib_data.dig_T1)) * ((raw_temp >> 4) - ((int32_t)bmp280_calib_data.dig_T1))) >> 12) * ((int32_t)bmp280_calib_data.dig_T3)) >> 14;
    int32_t t_fine = var1 + var2;
    if (temp_001C)
        *temp_001C = (t_fine * 5 + 128) >> 8;

    // bmp280_compensate_P_int32
    int32_t var3, var4;
    uint32_t p;
    var3 = ((int32_t)t_fine >> 1) - UINT32_C(64000);
    var4 = (((var3 >> 2) * (var3 >> 2)) >> 11) * ((int32_t)bmp280_calib_data.dig_P6);
    var4 += ((var3 * ((int32_t)bmp280_calib_data.dig_P5)) << 1);
    var4 = (var4 >> 2) + (((int32_t)bmp280_calib_data.dig_P4) << 16);
    var3 = (((bmp280_calib_data.dig_P3 * (((var3 >> 2) * (var3 >> 2)) >> 13)) >> 3) + ((((int32_t)bmp280_calib_data.dig_P2) * var3) >> 1)) >> 18;
    var3 = ((((32768 + var3)) * ((int32_t)bmp280_calib_data.dig_P1)) >> 15);
    if (var3 == 0) {
        if (press_Pa)
            *press_Pa = 0;
        return; // avoid division by zero
    }
    p = (((uint32_t)(((int32_t)1048576) - raw_press) - (var4 >> 12)) * 3125);
    if (p < 0x80000000) {
        p = (p << 1) / ((uint32_t)var3);
    } else {
        p = (p / (uint32_t)var3) * 2;
    }
    var3 = (((int32_t)bmp280_calib_data.dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var4 = (((int32_t)(p >> 2)) * ((int32_t)bmp280_calib_data.dig_P8)) >> 13;
    if (press_Pa)
        *press_Pa = (uint32_t)((int32_t)p + ((var3 + var4 + bmp280_calib_data.dig_P7) >> 4));
}

void bmp280_measure(float *temperature, uint32_t *pressure) {
    int32_t raw_temp, raw_press;
    bmp280_read_raw_data(&raw_temp, &raw_press);
    int32_t temp_001C;
    bmp280_compensate(raw_temp, raw_press, &temp_001C, pressure);
    if (temperature)
        *temperature = temp_001C / 100.0;
}

#endif
