#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "picoboy/buttons.hpp"
#include "picoboy/display.hpp"
#include "picoboy/lsm6dsl.hpp"

namespace picoboy {

class SkyDodgerGame {
public:
    explicit SkyDodgerGame(Lsm6dsl& imu);

    void enter(const Display& display);
    void update(const Buttons& buttons, const Display& display);
    void render(Display& display) const;

    bool exitRequested() const;
    void clearExitRequest();

private:
    enum class CalibrationStage : uint8_t {
        HoldLevel,
        TiltRight,
        Ready,
    };

    enum class TiltAxis : uint8_t {
        X,
        Y,
    };

    struct Obstacle {
        int16_t x = 0;
        int16_t y = 0;
        int16_t size = 0;
        int16_t speed = 0;
    };

    static constexpr size_t ObstacleCount = 6;
    static constexpr int16_t PlayerWidth = 18;
    static constexpr int16_t PlayerHeight = 12;

    void resetRound(const Display& display);
    void spawnObstacle(Obstacle& obstacle, const Display& display, bool initial_spawn);
    uint32_t nextRandom();
    bool captureAverageAcceleration(Lsm6dsl::Acceleration& out) const;
    float readSteeringTiltG() const;

    Lsm6dsl& imu_;
    bool exit_requested_ = false;
    bool game_over_ = false;
    bool calibration_warning_ = false;
    int16_t player_x_ = 0;
    int16_t score_ = 0;
    float neutral_x_g_ = 0.0f;
    float neutral_y_g_ = 0.0f;
    float steering_sign_ = 1.0f;
    uint32_t rng_state_ = 0x12345678;
    CalibrationStage calibration_stage_ = CalibrationStage::HoldLevel;
    TiltAxis tilt_axis_ = TiltAxis::Y;
    std::array<Obstacle, ObstacleCount> obstacles_{};
};

}  // namespace picoboy
