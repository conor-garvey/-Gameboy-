#include "picoboy/sky_dodger_game.hpp"

#include <cmath>
#include <cstdio>

namespace picoboy {

namespace {

Display::Color skyTop() {
    return Display::rgb565(16, 26, 68);
}

Display::Color skyBottom() {
    return Display::rgb565(6, 10, 28);
}

Display::Color starColor() {
    return Display::rgb565(220, 220, 255);
}

Display::Color playerColor() {
    return Display::rgb565(90, 230, 255);
}

Display::Color hazardColor() {
    return Display::rgb565(255, 94, 94);
}

Display::Color hazardGlow() {
    return Display::rgb565(255, 210, 90);
}

}

SkyDodgerGame::SkyDodgerGame(Lsm6dsl& imu) : imu_(imu) {}

void SkyDodgerGame::enter(const Display& display) {
    exit_requested_ = false;
    game_over_ = false;
    calibration_warning_ = false;
    rng_state_ = 0x13579BDF;
    calibration_stage_ = CalibrationStage::HoldLevel;
    tilt_axis_ = TiltAxis::Y;
    steering_sign_ = 1.0f;
    neutral_x_g_ = 0.0f;
    neutral_y_g_ = 0.0f;
    resetRound(display);
}

void SkyDodgerGame::update(const Buttons& buttons, const Display& display) {
    if (buttons.pressed(ButtonId::Select) || buttons.pressed(ButtonId::B)) {
        exit_requested_ = true;
        return;
    }

    if (!imu_.ready()) {
        return;
    }

    if (calibration_stage_ == CalibrationStage::HoldLevel) {
        if (buttons.pressed(ButtonId::A) || buttons.pressed(ButtonId::Start)) {
            Lsm6dsl::Acceleration accel;
            if (captureAverageAcceleration(accel)) {
                neutral_x_g_ = accel.x_g;
                neutral_y_g_ = accel.y_g;
                calibration_stage_ = CalibrationStage::TiltRight;
                calibration_warning_ = false;
            }
        }
        return;
    }

    if (calibration_stage_ == CalibrationStage::TiltRight) {
        if (buttons.pressed(ButtonId::A) || buttons.pressed(ButtonId::Start)) {
            Lsm6dsl::Acceleration accel;
            if (!captureAverageAcceleration(accel)) {
                return;
            }

            const float delta_x = accel.x_g - neutral_x_g_;
            const float delta_y = accel.y_g - neutral_y_g_;

            tilt_axis_ = std::fabs(delta_x) >= std::fabs(delta_y) ? TiltAxis::X : TiltAxis::Y;
            const float selected_delta = tilt_axis_ == TiltAxis::X ? delta_x : delta_y;

            if (std::fabs(selected_delta) < 0.10f) {
                calibration_warning_ = true;
                return;
            }

            steering_sign_ = selected_delta >= 0.0f ? 1.0f : -1.0f;
            calibration_stage_ = CalibrationStage::Ready;
            calibration_warning_ = false;
            resetRound(display);
        }
        return;
    }

    if (game_over_) {
        if (buttons.pressed(ButtonId::Start) || buttons.pressed(ButtonId::A)) {
            resetRound(display);
        }
        return;
    }

    const float tilt_g = readSteeringTiltG();
    const int16_t movement = static_cast<int16_t>(tilt_g * 18.0f);
    player_x_ = static_cast<int16_t>(player_x_ + movement);

    const int16_t max_player_x = static_cast<int16_t>(display.width() - PlayerWidth);
    if (player_x_ < 0) {
        player_x_ = 0;
    }
    if (player_x_ > max_player_x) {
        player_x_ = max_player_x;
    }

    const int16_t player_y = static_cast<int16_t>(display.height() - 34);

    for (Obstacle& obstacle : obstacles_) {
        obstacle.y = static_cast<int16_t>(obstacle.y + obstacle.speed);

        if (obstacle.y > static_cast<int16_t>(display.height() + obstacle.size)) {
            ++score_;
            spawnObstacle(obstacle, display, false);
            continue;
        }

        const bool overlap_x = player_x_ < obstacle.x + obstacle.size &&
                               player_x_ + PlayerWidth > obstacle.x;
        const bool overlap_y = player_y < obstacle.y + obstacle.size &&
                               player_y + PlayerHeight > obstacle.y;

        if (overlap_x && overlap_y) {
            game_over_ = true;
        }
    }
}

void SkyDodgerGame::render(Display& display) const {
    const int16_t screen_width = static_cast<int16_t>(display.width());
    const int16_t screen_height = static_cast<int16_t>(display.height());
    const int16_t player_y = static_cast<int16_t>(screen_height - 34);

    display.beginFrame(skyBottom());
    display.fillRect(0, 0, screen_width, static_cast<int16_t>(screen_height / 2), skyTop());
    display.fillRect(0, static_cast<int16_t>(screen_height / 2), screen_width,
                     static_cast<int16_t>(screen_height - screen_height / 2), skyBottom());

    for (int16_t star = 0; star < 14; ++star) {
        const int16_t x = static_cast<int16_t>((star * 23 + 11) % screen_width);
        const int16_t y = static_cast<int16_t>((star * 17 + 7) % (screen_height - 24));
        display.drawPixel(x, y, starColor());
    }

    for (const Obstacle& obstacle : obstacles_) {
        display.fillRect(obstacle.x, obstacle.y, obstacle.size, obstacle.size, hazardColor());
        display.drawRect(obstacle.x, obstacle.y, obstacle.size, obstacle.size, hazardGlow());
    }

    display.fillRect(player_x_, player_y, PlayerWidth, PlayerHeight, playerColor());
    display.fillRect(static_cast<int16_t>(player_x_ + 4), static_cast<int16_t>(player_y - 4), 10, 5,
                     Display::rgb565(255, 255, 255));
    display.drawText(10, 8, "SKY DODGER", Display::rgb565(255, 255, 255), 1);

    char score_text[20];
    std::snprintf(score_text, sizeof(score_text), "SCORE %d", score_);
    const int16_t score_x = static_cast<int16_t>(screen_width - display.measureTextWidth(score_text, 1) - 10);
    display.drawText(score_x > 10 ? score_x : 10, 8, score_text, Display::rgb565(255, 220, 88), 1);

    if (!imu_.ready()) {
        display.fillRect(20, static_cast<int16_t>(screen_height / 2 - 28), static_cast<int16_t>(screen_width - 40), 56,
                         Display::rgb565(28, 36, 72));
        display.drawRect(20, static_cast<int16_t>(screen_height / 2 - 28), static_cast<int16_t>(screen_width - 40), 56,
                         Display::rgb565(255, 196, 48));
        display.drawText(54, static_cast<int16_t>(screen_height / 2 - 10), "IMU NOT READY", Display::rgb565(255, 255, 255), 2);
        display.drawText(40, static_cast<int16_t>(screen_height / 2 + 18), "CHECK LSM6DSL WIRING", Display::rgb565(180, 220, 255), 1);
    } else if (calibration_stage_ != CalibrationStage::Ready) {
        const int16_t box_x = 26;
        const int16_t box_y = static_cast<int16_t>(screen_height / 2 - 42);
        const int16_t box_width = static_cast<int16_t>(screen_width - 52);
        const int16_t box_height = 84;

        display.fillRect(box_x, box_y, box_width, box_height, Display::rgb565(28, 36, 72));
        display.drawRect(box_x, box_y, box_width, box_height, Display::rgb565(255, 196, 48));

        if (calibration_stage_ == CalibrationStage::HoldLevel) {
            display.drawText(64, static_cast<int16_t>(screen_height / 2 - 22), "HOLD IT LEVEL",
                             Display::rgb565(255, 255, 255), 2);
            display.drawText(56, static_cast<int16_t>(screen_height / 2 + 10), "PRESS A OR START",
                             Display::rgb565(180, 220, 255), 1);
        } else {
            display.drawText(68, static_cast<int16_t>(screen_height / 2 - 22), "TILT RIGHT",
                             Display::rgb565(255, 255, 255), 2);
            display.drawText(56, static_cast<int16_t>(screen_height / 2 + 10), "PRESS A OR START",
                             Display::rgb565(180, 220, 255), 1);

            if (calibration_warning_) {
                display.drawText(34, static_cast<int16_t>(screen_height / 2 + 24), "TILT MORE TO DETECT THE AXIS",
                                 Display::rgb565(255, 220, 88), 1);
            }
        }

        display.drawText(70, static_cast<int16_t>(screen_height - 16), "SELECT EXIT",
                         Display::rgb565(220, 220, 240), 1);
    } else if (game_over_) {
        display.fillRect(36, static_cast<int16_t>(screen_height / 2 - 34), static_cast<int16_t>(screen_width - 72), 68,
                         Display::rgb565(28, 36, 72));
        display.drawRect(36, static_cast<int16_t>(screen_height / 2 - 34), static_cast<int16_t>(screen_width - 72), 68,
                         Display::rgb565(255, 196, 48));
        display.drawText(74, static_cast<int16_t>(screen_height / 2 - 18), "YOU CRASHED", Display::rgb565(255, 255, 255), 2);
        display.drawText(62, static_cast<int16_t>(screen_height / 2 + 12), "START TO RETRY", Display::rgb565(180, 220, 255), 1);
    } else {
        display.drawText(10, static_cast<int16_t>(screen_height - 16), "TILT TO DODGE  SELECT EXIT",
                         Display::rgb565(220, 220, 240), 1);
    }

    display.present();
}

bool SkyDodgerGame::exitRequested() const {
    return exit_requested_;
}

void SkyDodgerGame::clearExitRequest() {
    exit_requested_ = false;
}

void SkyDodgerGame::resetRound(const Display& display) {
    const int16_t screen_width = static_cast<int16_t>(display.width());
    player_x_ = static_cast<int16_t>((screen_width - PlayerWidth) / 2);
    score_ = 0;
    game_over_ = false;
    exit_requested_ = false;

    for (size_t index = 0; index < obstacles_.size(); ++index) {
        spawnObstacle(obstacles_[index], display, true);
        obstacles_[index].y = static_cast<int16_t>(obstacles_[index].y - static_cast<int16_t>(index * 32));
    }
}

void SkyDodgerGame::spawnObstacle(Obstacle& obstacle, const Display& display, bool initial_spawn) {
    const int16_t screen_width = static_cast<int16_t>(display.width());
    const uint32_t random = nextRandom();

    obstacle.size = static_cast<int16_t>(12 + (random % 12));
    const int16_t max_x = static_cast<int16_t>(screen_width - obstacle.size);
    obstacle.x = max_x > 0 ? static_cast<int16_t>(nextRandom() % static_cast<uint32_t>(max_x + 1)) : 0;
    obstacle.speed = static_cast<int16_t>(2 + (nextRandom() % 3));

    if (initial_spawn) {
        obstacle.y = static_cast<int16_t>(-20 - static_cast<int16_t>(nextRandom() % 140));
    } else {
        obstacle.y = static_cast<int16_t>(-obstacle.size - static_cast<int16_t>(nextRandom() % 60));
    }
}

uint32_t SkyDodgerGame::nextRandom() {
    rng_state_ = rng_state_ * 1664525u + 1013904223u;
    return rng_state_;
}

bool SkyDodgerGame::captureAverageAcceleration(Lsm6dsl::Acceleration& out) const {
    constexpr int sample_count = 8;

    float x_total = 0.0f;
    float y_total = 0.0f;
    float z_total = 0.0f;
    int32_t raw_x_total = 0;
    int32_t raw_y_total = 0;
    int32_t raw_z_total = 0;

    for (int sample = 0; sample < sample_count; ++sample) {
        Lsm6dsl::Acceleration reading;
        if (!imu_.readAcceleration(reading)) {
            return false;
        }

        x_total += reading.x_g;
        y_total += reading.y_g;
        z_total += reading.z_g;
        raw_x_total += reading.raw_x;
        raw_y_total += reading.raw_y;
        raw_z_total += reading.raw_z;
    }

    out.x_g = x_total / static_cast<float>(sample_count);
    out.y_g = y_total / static_cast<float>(sample_count);
    out.z_g = z_total / static_cast<float>(sample_count);
    out.raw_x = static_cast<int16_t>(raw_x_total / sample_count);
    out.raw_y = static_cast<int16_t>(raw_y_total / sample_count);
    out.raw_z = static_cast<int16_t>(raw_z_total / sample_count);
    return true;
}

float SkyDodgerGame::readSteeringTiltG() const {
    Lsm6dsl::Acceleration accel;
    if (!imu_.readAcceleration(accel)) {
        return 0.0f;
    }

    const float axis_value = tilt_axis_ == TiltAxis::X
        ? (accel.x_g - neutral_x_g_)
        : (accel.y_g - neutral_y_g_);

    return axis_value * steering_sign_;
}

}  // namespace picoboy
