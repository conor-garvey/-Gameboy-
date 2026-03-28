#pragma once

#include <array>
#include <cstdint>

#include "picoboy/buttons.hpp"
#include "picoboy/display.hpp"

namespace picoboy {

class PlumberManGame {
public:
    void enter();
    void update(const Buttons& buttons);
    void render(Display& display) const;

    bool exitRequested() const;
    void clearExitRequest();

private:
    static constexpr int16_t PlayerWidth = 12;
    static constexpr int16_t PlayerHeight = 16;
    static constexpr size_t CoinCount = 7;

    void resetLevel();
    void collectCoins();
    void movePlayerX(int16_t delta);
    void movePlayerY(int16_t delta);
    bool collidesAt(int16_t x, int16_t y) const;
    int16_t cameraX(int16_t viewport_width) const;

    int16_t player_x_ = 24;
    int16_t player_y_ = 0;
    int8_t velocity_y_ = 0;
    bool on_ground_ = false;
    bool won_ = false;
    bool exit_requested_ = false;
    uint8_t coins_collected_ = 0;
    std::array<bool, CoinCount> coin_collected_{};
};

}  // namespace picoboy
