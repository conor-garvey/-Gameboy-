#include "picoboy/lsm6dsl.hpp"

#include "hardware/gpio.h"

namespace picoboy {

namespace {

constexpr uint8_t reg_who_am_i = 0x0F;
constexpr uint8_t who_am_i_value = 0x6A;
constexpr uint8_t reg_ctrl1_xl = 0x10;
constexpr uint8_t reg_ctrl2_g = 0x11;
constexpr uint8_t reg_ctrl3_c = 0x12;
constexpr uint8_t reg_out_xl = 0x28;

constexpr float accel_sensitivity_g_per_lsb = 0.000061f;

}

bool Lsm6dsl::init(const Config& config) {
    config_ = config;
    ready_ = false;
    address_ = 0;

    i2c_init(config_.i2c, config_.bus_hz);

    gpio_set_function(config_.pin_sda, GPIO_FUNC_I2C);
    gpio_set_function(config_.pin_scl, GPIO_FUNC_I2C);
    gpio_pull_up(config_.pin_sda);
    gpio_pull_up(config_.pin_scl);

    if (config_.address != 0) {
        if (!probeAddress(config_.address)) {
            return false;
        }

        address_ = config_.address;
    } else if (probeAddress(AddressLow)) {
        address_ = AddressLow;
    } else if (probeAddress(AddressHigh)) {
        address_ = AddressHigh;
    } else {
        return false;
    }

    if (!writeRegister(reg_ctrl3_c, 0x44)) {
        return false;
    }

    if (!writeRegister(reg_ctrl2_g, 0x00)) {
        return false;
    }

    if (!writeRegister(reg_ctrl1_xl, 0x40)) {
        return false;
    }

    ready_ = true;
    return true;
}

bool Lsm6dsl::ready() const {
    return ready_;
}

uint8_t Lsm6dsl::address() const {
    return address_;
}

bool Lsm6dsl::readAcceleration(Acceleration& out) const {
    if (!ready_) {
        return false;
    }

    uint8_t buffer[6] = {};
    if (!readRegisters(reg_out_xl, buffer, sizeof(buffer))) {
        return false;
    }

    out.raw_x = static_cast<int16_t>((static_cast<uint16_t>(buffer[1]) << 8) | buffer[0]);
    out.raw_y = static_cast<int16_t>((static_cast<uint16_t>(buffer[3]) << 8) | buffer[2]);
    out.raw_z = static_cast<int16_t>((static_cast<uint16_t>(buffer[5]) << 8) | buffer[4]);

    out.x_g = static_cast<float>(out.raw_x) * accel_sensitivity_g_per_lsb;
    out.y_g = static_cast<float>(out.raw_y) * accel_sensitivity_g_per_lsb;
    out.z_g = static_cast<float>(out.raw_z) * accel_sensitivity_g_per_lsb;
    return true;
}

bool Lsm6dsl::probeAddress(uint8_t address) const {
    uint8_t who_am_i = 0;

    const int write_result = i2c_write_blocking(config_.i2c, address, &reg_who_am_i, 1, true);
    if (write_result != 1) {
        return false;
    }

    const int read_result = i2c_read_blocking(config_.i2c, address, &who_am_i, 1, false);
    return read_result == 1 && who_am_i == who_am_i_value;
}

bool Lsm6dsl::readRegisters(uint8_t reg, uint8_t* data, size_t length) const {
    if (data == nullptr || length == 0) {
        return false;
    }

    const int write_result = i2c_write_blocking(config_.i2c, address_, &reg, 1, true);
    if (write_result != 1) {
        return false;
    }

    const int read_result = i2c_read_blocking(config_.i2c, address_, data, length, false);
    return read_result == static_cast<int>(length);
}

bool Lsm6dsl::writeRegister(uint8_t reg, uint8_t value) const {
    uint8_t buffer[2] = {reg, value};
    const int result = i2c_write_blocking(config_.i2c, address_, buffer, 2, false);
    return result == 2;
}

}  // namespace picoboy
