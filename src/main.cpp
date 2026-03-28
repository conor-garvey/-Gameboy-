#include <stdio.h>

#include "picoboy/display.hpp"
#include "picoboy/lsm6dsl.hpp"
#include "picoboy/menu_app.hpp"
#include "pico/stdlib.h"

int main()
{
    stdio_init_all();

    static picoboy::Display display;
    static picoboy::Lsm6dsl imu;
    static picoboy::MenuApp menu(display, imu);

    display.setRotation(picoboy::Display::Rotation::Landscape90);
    display.setViewport(260, 218);
    display.init();
    display.setTargetFps(60);

    // Change these fields if your LSM6DSL is wired to different I2C pins.
    picoboy::Lsm6dsl::Config imu_config;
    imu_config.i2c = i2c0;
    imu_config.pin_sda = 0;
    imu_config.pin_scl = 1;
    imu_config.bus_hz = 400000;
    // Leave address at 0 to auto-detect either 0x6A or 0x6B.
    imu.init(imu_config);

    menu.init();
    printf("Display initialized.\n");

    while (true) {
        display.beginFrameTiming();
        menu.update();
        menu.render();
        display.waitForNextFrame();
    }
}
