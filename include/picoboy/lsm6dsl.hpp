#pragma once

#include <cstddef>
#include <cstdint>

#include "hardware/i2c.h"

namespace picoboy {

class Lsm6dsl {
public:
    struct Config {
        i2c_inst_t* i2c = i2c0;
        uint8_t pin_sda = 0;
        uint8_t pin_scl = 1;
        uint32_t bus_hz = 400000;
        uint8_t address = 0;
    };

    struct Acceleration {
        int16_t raw_x = 0;
        int16_t raw_y = 0;
        int16_t raw_z = 0;
        float x_g = 0.0f;
        float y_g = 0.0f;
        float z_g = 0.0f;
    };

    bool init(const Config& config);
    bool ready() const;
    uint8_t address() const;
    bool readAcceleration(Acceleration& out) const;

private:
    static constexpr uint8_t AddressLow = 0x6A;
    static constexpr uint8_t AddressHigh = 0x6B;

    bool probeAddress(uint8_t address) const;
    bool readRegisters(uint8_t reg, uint8_t* data, size_t length) const;
    bool writeRegister(uint8_t reg, uint8_t value) const;

    Config config_{};
    bool ready_ = false;
    uint8_t address_ = 0;
};

}  // namespace picoboy
