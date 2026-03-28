#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace picoboy {

enum class ButtonId : uint8_t {
    Up = 0,
    Down,
    Left,
    Right,
    A,
    B,
    Select,
    Start,
    Count,
};

class Buttons {
public:
    void init();
    void update();

    bool pressed(ButtonId button) const;
    bool held(ButtonId button) const;

private:
    static constexpr size_t ButtonCount = static_cast<size_t>(ButtonId::Count);

    std::array<bool, ButtonCount> current_{};
    std::array<bool, ButtonCount> previous_{};
};

}  // namespace picoboy
