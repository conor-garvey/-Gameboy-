#include "picoboy/buttons.hpp"

#include "hardware/gpio.h"

namespace picoboy {

namespace {

constexpr uint8_t button_pins[] = {2, 3, 4, 5, 6, 7, 8, 9};

size_t buttonIndex(ButtonId button) {
    return static_cast<size_t>(button);
}

}

void Buttons::init() {
    for (size_t index = 0; index < ButtonCount; ++index) {
        const uint8_t pin = button_pins[index];
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
        current_[index] = false;
        previous_[index] = false;
    }
}

void Buttons::update() {
    previous_ = current_;

    for (size_t index = 0; index < ButtonCount; ++index) {
        current_[index] = gpio_get(button_pins[index]) == 0;
    }
}

bool Buttons::pressed(ButtonId button) const {
    const size_t index = buttonIndex(button);
    return current_[index] && !previous_[index];
}

bool Buttons::held(ButtonId button) const {
    return current_[buttonIndex(button)];
}

}  // namespace picoboy
