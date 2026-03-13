#ifndef LSM6DS3_H
#define LSM6DS3_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_inst_t *i2c;
    uint baudrate_hz;
    uint8_t address;
    int pin_sda;
    int pin_scl;
} lsm6ds3_config_t;

typedef struct {
    i2c_inst_t *i2c;
    uint8_t address;
    int16_t accel_bias_x;
    int16_t accel_bias_y;
    int16_t accel_bias_z;
    bool ready;
} lsm6ds3_t;

bool lsm6ds3_init(lsm6ds3_t *imu, const lsm6ds3_config_t *config);
bool lsm6ds3_calibrate(lsm6ds3_t *imu, uint sample_count);
bool lsm6ds3_read_accel_raw(lsm6ds3_t *imu, int16_t *x, int16_t *y, int16_t *z);
bool lsm6ds3_read_accel(lsm6ds3_t *imu, int16_t *x, int16_t *y, int16_t *z);

#ifdef __cplusplus
}
#endif

#endif
