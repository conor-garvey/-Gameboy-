#include "lsm6ds3.h"

#include <string.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define LSM6DS3_DEFAULT_ADDR 0x6Au
#define LSM6DS3_WHO_AM_I 0x0Fu
#define LSM6DS3_CTRL1_XL 0x10u
#define LSM6DS3_CTRL3_C 0x12u
#define LSM6DS3_OUTX_L_XL 0x28u
#define LSM6DS3_WHO_AM_I_VALUE 0x69u

static bool lsm6ds3_write_reg(lsm6ds3_t *imu, uint8_t reg, uint8_t value) {
    uint8_t buffer[2] = {reg, value};
    return i2c_write_blocking(imu->i2c, imu->address, buffer, 2, false) == 2;
}

static bool lsm6ds3_read_regs(lsm6ds3_t *imu, uint8_t reg, uint8_t *buffer, size_t length) {
    if (i2c_write_blocking(imu->i2c, imu->address, &reg, 1, true) != 1) {
        return false;
    }
    return i2c_read_blocking(imu->i2c, imu->address, buffer, length, false) == (int)length;
}

bool lsm6ds3_init(lsm6ds3_t *imu, const lsm6ds3_config_t *config) {
    if (imu == NULL || config == NULL || config->i2c == NULL) {
        return false;
    }
    if (config->pin_sda < 0 || config->pin_scl < 0) {
        return false;
    }

    memset(imu, 0, sizeof(*imu));
    imu->i2c = config->i2c;
    imu->address = config->address != 0u ? config->address : LSM6DS3_DEFAULT_ADDR;

    i2c_init(imu->i2c, config->baudrate_hz != 0u ? config->baudrate_hz : 400000u);
    gpio_set_function((uint)config->pin_sda, GPIO_FUNC_I2C);
    gpio_set_function((uint)config->pin_scl, GPIO_FUNC_I2C);
    gpio_pull_up((uint)config->pin_sda);
    gpio_pull_up((uint)config->pin_scl);
    sleep_ms(10);

    uint8_t who_am_i = 0u;
    if (!lsm6ds3_read_regs(imu, LSM6DS3_WHO_AM_I, &who_am_i, 1u) || who_am_i != LSM6DS3_WHO_AM_I_VALUE) {
        return false;
    }

    if (!lsm6ds3_write_reg(imu, LSM6DS3_CTRL3_C, 0x44u)) {
        return false;
    }
    if (!lsm6ds3_write_reg(imu, LSM6DS3_CTRL1_XL, 0x40u)) {
        return false;
    }

    sleep_ms(20);
    imu->ready = true;
    return lsm6ds3_calibrate(imu, 32u);
}

bool lsm6ds3_calibrate(lsm6ds3_t *imu, uint sample_count) {
    if (imu == NULL || !imu->ready || sample_count == 0u) {
        return false;
    }

    int32_t sum_x = 0;
    int32_t sum_y = 0;
    int32_t sum_z = 0;

    for (uint i = 0; i < sample_count; ++i) {
        int16_t x = 0;
        int16_t y = 0;
        int16_t z = 0;
        if (!lsm6ds3_read_accel_raw(imu, &x, &y, &z)) {
            return false;
        }
        sum_x += x;
        sum_y += y;
        sum_z += z;
        sleep_ms(4);
    }

    imu->accel_bias_x = (int16_t)(sum_x / (int32_t)sample_count);
    imu->accel_bias_y = (int16_t)(sum_y / (int32_t)sample_count);
    imu->accel_bias_z = (int16_t)(sum_z / (int32_t)sample_count);
    return true;
}

bool lsm6ds3_read_accel_raw(lsm6ds3_t *imu, int16_t *x, int16_t *y, int16_t *z) {
    if (imu == NULL || !imu->ready || x == NULL || y == NULL || z == NULL) {
        return false;
    }

    uint8_t buffer[6];
    if (!lsm6ds3_read_regs(imu, LSM6DS3_OUTX_L_XL, buffer, sizeof(buffer))) {
        return false;
    }

    *x = (int16_t)((buffer[1] << 8) | buffer[0]);
    *y = (int16_t)((buffer[3] << 8) | buffer[2]);
    *z = (int16_t)((buffer[5] << 8) | buffer[4]);
    return true;
}

bool lsm6ds3_read_accel(lsm6ds3_t *imu, int16_t *x, int16_t *y, int16_t *z) {
    int16_t raw_x = 0;
    int16_t raw_y = 0;
    int16_t raw_z = 0;

    if (!lsm6ds3_read_accel_raw(imu, &raw_x, &raw_y, &raw_z)) {
        return false;
    }

    *x = (int16_t)(raw_x - imu->accel_bias_x);
    *y = (int16_t)(raw_y - imu->accel_bias_y);
    *z = (int16_t)(raw_z - imu->accel_bias_z);
    return true;
}
