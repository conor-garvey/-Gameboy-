#include "picoboy/sky_dodger_game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "picoboy/avatar_art.hpp"

namespace picoboy {

namespace {

struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

constexpr float pi = 3.14159265f;
constexpr float calibration_tilt_threshold_g = 0.05f;
constexpr uint32_t death_sequence_lock_ms = 3600;

float clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 1.0f) {
        return 1.0f;
    }

    return value;
}

Rgb blend(const Rgb& a, const Rgb& b, float amount) {
    const float t = clamp01(amount);
    return {
        static_cast<uint8_t>(a.r + static_cast<int16_t>((b.r - a.r) * t)),
        static_cast<uint8_t>(a.g + static_cast<int16_t>((b.g - a.g) * t)),
        static_cast<uint8_t>(a.b + static_cast<int16_t>((b.b - a.b) * t)),
    };
}

Display::Color toColor(const Rgb& color) {
    return Display::rgb565(color.r, color.g, color.b);
}

void drawSkyGradient(Display& display, int16_t screen_width, int16_t screen_height,
                     const Rgb& top, const Rgb& bottom) {
    constexpr int16_t band_count = 6;

    for (int16_t band = 0; band < band_count; ++band) {
        const int16_t y = static_cast<int16_t>(band * screen_height / band_count);
        const int16_t next_y = static_cast<int16_t>((band + 1) * screen_height / band_count);
        const float amount = static_cast<float>(band) / static_cast<float>(band_count - 1);
        const Rgb band_color = blend(top, bottom, amount);
        display.fillRect(0, y, screen_width, static_cast<int16_t>(next_y - y), toColor(band_color));
    }
}

void drawOrb(Display& display, int16_t x, int16_t y, Display::Color core, Display::Color glow) {
    display.fillRect(static_cast<int16_t>(x - 8), static_cast<int16_t>(y - 4), 16, 8, glow);
    display.fillRect(static_cast<int16_t>(x - 5), static_cast<int16_t>(y - 7), 10, 14, glow);
    display.fillRect(static_cast<int16_t>(x - 4), static_cast<int16_t>(y - 4), 8, 8, core);
    display.fillRect(static_cast<int16_t>(x - 2), static_cast<int16_t>(y - 6), 4, 12, core);
}

void drawCloud(Display& display, int16_t x, int16_t y, int16_t width, int16_t height, Display::Color color) {
    display.fillRect(x, static_cast<int16_t>(y + height / 3), width, static_cast<int16_t>(height / 2), color);
    display.fillRect(static_cast<int16_t>(x + width / 6), y, static_cast<int16_t>(width / 3), static_cast<int16_t>(height / 2), color);
    display.fillRect(static_cast<int16_t>(x + width / 2), static_cast<int16_t>(y + 2),
                     static_cast<int16_t>(width / 3), static_cast<int16_t>(height / 2), color);
}

void drawBalloon(Display& display, int16_t x, int16_t y, Display::Color shell,
                 Display::Color highlight, Display::Color basket, Display::Color face) {
    display.fillRect(static_cast<int16_t>(x + 2), y, 10, 13, shell);
    display.fillRect(x, static_cast<int16_t>(y + 3), 14, 8, shell);
    display.fillRect(static_cast<int16_t>(x + 3), static_cast<int16_t>(y + 2), 3, 3, highlight);
    display.fillRect(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 5), 2, 2, face);
    display.fillRect(static_cast<int16_t>(x + 8), static_cast<int16_t>(y + 5), 2, 2, face);
    display.drawLine(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 9),
                     static_cast<int16_t>(x + 9), static_cast<int16_t>(y + 9), face);
    display.drawLine(static_cast<int16_t>(x + 7), static_cast<int16_t>(y + 12),
                     static_cast<int16_t>(x + 5), static_cast<int16_t>(y + 18), basket);
    display.drawLine(static_cast<int16_t>(x + 7), static_cast<int16_t>(y + 12),
                     static_cast<int16_t>(x + 9), static_cast<int16_t>(y + 18), basket);
    display.fillRect(static_cast<int16_t>(x + 4), static_cast<int16_t>(y + 18), 6, 4, basket);
}

bool intersects(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                int16_t bx, int16_t by, int16_t bw, int16_t bh) {
    return ax < bx + bw &&
           ax + aw > bx &&
           ay < by + bh &&
           ay + ah > by;
}

bool timeReached(absolute_time_t deadline) {
    return absolute_time_diff_us(get_absolute_time(), deadline) <= 0;
}

}  // namespace

SkyDodgerGame::SkyDodgerGame(Lsm6dsl& imu, AudioEngine& audio) : imu_(imu), audio_(audio) {}

void SkyDodgerGame::setAvatar(AvatarId avatar) {
    avatar_ = sanitizeAvatarId(static_cast<uint8_t>(avatar));
}

void SkyDodgerGame::enter(const Display& display) {
    exit_requested_ = false;
    game_over_ = false;
    calibration_warning_ = false;
    facing_left_ = false;
    run_report_pending_ = false;
    run_result_committed_ = false;
    pending_distance_ = 0;
    pending_coins_ = 0;
    coins_collected_ = 0;
    rng_state_ = 0x13579BDF;
    calibration_stage_ = CalibrationStage::HoldLevel;
    tilt_axis_ = TiltAxis::Y;
    steering_sign_ = 1.0f;
    neutral_x_g_ = 0.0f;
    neutral_y_g_ = 0.0f;
    lose_selection_ = LoseSelection::Retry;
    loss_input_unlock_at_ = get_absolute_time();
    resetRound(display);
}

void SkyDodgerGame::update(const Buttons& buttons, const Display& display) {
    if (game_over_) {
        updateLoseMenu(buttons, display);
        return;
    }

    if (buttons.pressed(ButtonId::Select)) {
        paused_ = !paused_;
        editing_volume_ = false;
        pause_selection_ = PauseSelection::ReturnToMenu;
        audio_.playEffect(SoundEffect::MenuMove);
        return;
    }

    if (paused_) {
        updatePauseMenu(buttons);
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
            const float lateral_tilt_g = std::sqrt(delta_x * delta_x + delta_y * delta_y);

            tilt_axis_ = std::fabs(delta_x) >= std::fabs(delta_y) ? TiltAxis::X : TiltAxis::Y;
            const float selected_delta = tilt_axis_ == TiltAxis::X ? delta_x : delta_y;

            if (lateral_tilt_g < calibration_tilt_threshold_g) {
                calibration_warning_ = true;
                return;
            }

            steering_sign_ = selected_delta >= 0.0f ? 1.0f : -1.0f;
            calibration_stage_ = CalibrationStage::DifficultySelect;
            calibration_warning_ = false;
        }
        return;
    }

    if (calibration_stage_ == CalibrationStage::DifficultySelect) {
        if (buttons.pressed(ButtonId::Up)) {
            difficulty_level_ = std::min(5, difficulty_level_ + 1);
        } else if (buttons.pressed(ButtonId::Down)) {
            difficulty_level_ = std::max(1, difficulty_level_ - 1);
        } else if (buttons.pressed(ButtonId::A) || buttons.pressed(ButtonId::Start)) {
            calibration_stage_ = CalibrationStage::Ready;
            resetRound(display);
        }
        return;
    }

    float tilt_g = readSteeringTiltG();
    if (std::fabs(tilt_g) < 0.04f) {
        tilt_g = 0.0f;
    }

    const int16_t movement = static_cast<int16_t>(tilt_g * 14.0f);
    if (movement < 0) {
        facing_left_ = true;
    } else if (movement > 0) {
        facing_left_ = false;
    }
    player_x_ = static_cast<int16_t>(player_x_ + movement);

    const int16_t max_player_x = static_cast<int16_t>(display.width() - PlayerWidth - 6);
    if (player_x_ < 6) {
        player_x_ = 6;
    }
    if (player_x_ > max_player_x) {
        player_x_ = max_player_x;
    }

    ++distance_;
    ++frame_count_;

    updateClouds(display);
    updateHazards(display);
    updateCoins(display);

    const int16_t player_y = static_cast<int16_t>(display.height() / 2 - PlayerHeight / 2);
    for (const Hazard& hazard : hazards_) {
        if (hazard.active && intersects(player_x_, player_y, PlayerWidth, PlayerHeight,
                       hazard.x, hazard.y, hazard.width, hazard.height)) {
            game_over_ = true;
            lose_selection_ = LoseSelection::Retry;
            loss_input_unlock_at_ = delayed_by_ms(get_absolute_time(), death_sequence_lock_ms);
            audio_.stopBackgroundMusic();
            audio_.playEffect(SoundEffect::Death);
            finalizeRun();
            break;
        }
    }
}

void SkyDodgerGame::render(Display& display) const {
    const int16_t screen_width = static_cast<int16_t>(display.width());
    const int16_t screen_height = static_cast<int16_t>(display.height());
    const int16_t player_y = static_cast<int16_t>(screen_height / 2 - PlayerHeight / 2);

    const float cycle_t = static_cast<float>(frame_count_ % SkyCycleFrames) / static_cast<float>(SkyCycleFrames);
    const float sun_angle = cycle_t * 2.0f * pi + pi / 2.0f;
    const float sun_height = std::sin(sun_angle);
    const float sun_x_float = static_cast<float>(screen_width / 2) + std::cos(sun_angle) * static_cast<float>(screen_width) / 2.3f;
    const float sun_y_float = static_cast<float>(screen_height) * 0.78f - sun_height * static_cast<float>(screen_height) * 0.48f;

    const float daylight = clamp01((sun_height + 0.18f) / 1.18f);
    const float horizon_glow = clamp01(1.0f - std::fabs(sun_height) * 2.8f);

    const Rgb day_top = {104, 184, 255};
    const Rgb day_bottom = {212, 240, 255};
    const Rgb sunset_top = {96, 58, 138};
    const Rgb sunset_bottom = {255, 150, 86};
    const Rgb night_top = {8, 12, 28};
    const Rgb night_bottom = {20, 32, 70};

    Rgb sky_top = blend(night_top, day_top, daylight);
    Rgb sky_bottom = blend(night_bottom, day_bottom, daylight);
    sky_top = blend(sky_top, sunset_top, horizon_glow * 0.65f);
    sky_bottom = blend(sky_bottom, sunset_bottom, horizon_glow);

    const Rgb cloud_night = {54, 64, 92};
    const Rgb cloud_day = {248, 248, 255};
    const Rgb cloud_rgb = blend(blend(cloud_night, cloud_day, daylight), sunset_bottom, horizon_glow * 0.18f);

    display.beginFrame(toColor(night_top));
    drawSkyGradient(display, screen_width, screen_height, sky_top, sky_bottom);

    if (daylight < 0.35f) {
        for (int16_t star = 0; star < 10; ++star) {
            const int16_t x = static_cast<int16_t>((star * 29 + 17) % screen_width);
            const int16_t y = static_cast<int16_t>((star * 19 + 11) % (screen_height / 2));
            if (((frame_count_ / 12) + static_cast<uint32_t>(star)) % 2 == 0) {
                display.drawPixel(x, y, Display::rgb565(220, 230, 255));
            }
        }
    }

    if (sun_height > -0.12f) {
        const Display::Color sun_glow = toColor(blend({255, 180, 110}, {255, 234, 160}, daylight));
        const Display::Color sun_core = toColor(blend({255, 214, 102}, {255, 246, 190}, daylight));
        drawOrb(display, static_cast<int16_t>(sun_x_float), static_cast<int16_t>(sun_y_float), sun_core, sun_glow);
    }

    for (const Cloud& cloud : clouds_) {
        drawCloud(display, cloud.x, cloud.y, cloud.width, cloud.height, toColor(cloud_rgb));
    }

    const Display::Color bird_wing = Display::rgb565(236, 240, 248);
    const Display::Color bird_body = Display::rgb565(88, 98, 118);
    const Display::Color bird_beak = Display::rgb565(255, 196, 82);
    const Display::Color balloon_basket = Display::rgb565(158, 96, 38);
    const Display::Color balloon_face = Display::rgb565(42, 34, 50);
    for (const Hazard& hazard : hazards_) {
        if (!hazard.active || hazard.x + hazard.width < 0 || hazard.x >= screen_width ||
            hazard.y + hazard.height < 0 || hazard.y >= screen_height) {
            continue;
        }

        if (hazard.kind == HazardKind::Birds) {
            const int16_t flock_count = static_cast<int16_t>(3 + hazard.variant);
            for (int16_t bird = 0; bird < flock_count; ++bird) {
                const int16_t bird_x = static_cast<int16_t>(hazard.x + bird * 10);
                const int16_t bird_y = static_cast<int16_t>(hazard.y + (bird % 2) * 2);

                display.drawLine(bird_x, bird_y, static_cast<int16_t>(bird_x + 5), static_cast<int16_t>(bird_y + 5), bird_wing);
                display.drawLine(bird_x, static_cast<int16_t>(bird_y + 1), static_cast<int16_t>(bird_x + 5), static_cast<int16_t>(bird_y + 6), bird_wing);
                display.drawLine(static_cast<int16_t>(bird_x + 5), static_cast<int16_t>(bird_y + 5), static_cast<int16_t>(bird_x + 10), bird_y, bird_wing);
                display.drawLine(static_cast<int16_t>(bird_x + 5), static_cast<int16_t>(bird_y + 6), static_cast<int16_t>(bird_x + 10), static_cast<int16_t>(bird_y + 1), bird_wing);
                display.fillRect(static_cast<int16_t>(bird_x + 4), static_cast<int16_t>(bird_y + 4), 3, 2, bird_body);
                display.drawPixel(static_cast<int16_t>(bird_x + 6), static_cast<int16_t>(bird_y + 6), bird_beak);
            }
        } else {
            Display::Color balloon_shell = Display::rgb565(255, 120, 124);
            Display::Color balloon_highlight = Display::rgb565(255, 196, 200);

            switch (hazard.variant % 4u) {
            case 0:
                balloon_shell = Display::rgb565(255, 120, 124);
                balloon_highlight = Display::rgb565(255, 196, 200);
                break;

            case 1:
                balloon_shell = Display::rgb565(255, 194, 72);
                balloon_highlight = Display::rgb565(255, 232, 164);
                break;

            case 2:
                balloon_shell = Display::rgb565(86, 216, 160);
                balloon_highlight = Display::rgb565(176, 244, 212);
                break;

            default:
                balloon_shell = Display::rgb565(120, 168, 255);
                balloon_highlight = Display::rgb565(202, 224, 255);
                break;
            }

            drawBalloon(display, hazard.x, hazard.y, balloon_shell, balloon_highlight,
                        balloon_basket, balloon_face);
        }
    }

    // Render coins
    for (const Coin& coin : coins_) {
        if (coin.active && !coin.collected) {
            // Draw larger, more obvious coin (8x8 instead of 4x4)
            display.fillRect(coin.x + 2, coin.y + 2, 8, 8, Display::rgb565(255, 215, 0));
            display.drawRect(coin.x + 2, coin.y + 2, 8, 8, Display::rgb565(200, 150, 0));
            // Add coin details - small inner highlight
            display.fillRect(coin.x + 4, coin.y + 4, 4, 4, Display::rgb565(255, 235, 100));
            // Add a small "C" or star pattern in the center
            display.drawPixel(coin.x + 5, coin.y + 5, Display::rgb565(180, 120, 0));
            display.drawPixel(coin.x + 6, coin.y + 5, Display::rgb565(180, 120, 0));
            display.drawPixel(coin.x + 5, coin.y + 6, Display::rgb565(180, 120, 0));
        }
    }

    AvatarPose avatar_pose = AvatarPose::Idle;
    if (game_over_) {
        avatar_pose = AvatarPose::Death;
    } else if (calibration_stage_ == CalibrationStage::Ready && !paused_) {
        avatar_pose = AvatarPose::Jump;
    }
    drawPlayerAvatar(display, player_x_, player_y, avatar_, avatar_pose, facing_left_);

    display.drawText(10, 8, "SKY DODGER", Display::rgb565(255, 255, 255), 1);

    char distance_text[24];
    std::snprintf(distance_text, sizeof(distance_text), "DIST %ld", static_cast<long>(distance_));
    const int16_t distance_x = static_cast<int16_t>(screen_width - display.measureTextWidth(distance_text, 1) - 10);
    display.drawText(distance_x > 10 ? distance_x : 10, 8, distance_text, Display::rgb565(255, 228, 120), 1);

    char coin_text[16];
    std::snprintf(coin_text, sizeof(coin_text), "COINS %lu", static_cast<unsigned long>(coins_collected_));
    const int16_t coin_x = static_cast<int16_t>(screen_width - display.measureTextWidth(coin_text, 1) - 10);
    display.drawText(coin_x > 10 ? coin_x : 10, 20, coin_text, Display::rgb565(255, 215, 0), 1);

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
            display.drawText(52, static_cast<int16_t>(screen_height / 2 + 10), "PRESS A OR START",
                             Display::rgb565(180, 220, 255), 1);
        } else if (calibration_stage_ == CalibrationStage::DifficultySelect) {
            display.drawText(46, static_cast<int16_t>(screen_height / 2 - 28), "SELECT DIFFICULTY",
                             Display::rgb565(255, 255, 255), 2);
            
            char difficulty_text[16];
            std::snprintf(difficulty_text, sizeof(difficulty_text), "LEVEL %d", difficulty_level_);
            display.drawText(70, static_cast<int16_t>(screen_height / 2 - 4), difficulty_text,
                             Display::rgb565(255, 196, 48), 2);
            
            display.drawText(34, static_cast<int16_t>(screen_height / 2 + 18), "UP/DOWN: CHANGE LEVEL",
                             Display::rgb565(180, 220, 255), 1);
            display.drawText(46, static_cast<int16_t>(screen_height / 2 + 30), "A/START: BEGIN GAME",
                             Display::rgb565(180, 220, 255), 1);
        } else {
            display.drawText(40, static_cast<int16_t>(screen_height / 2 - 22), "TILT TO ONE SIDE",
                             Display::rgb565(255, 255, 255), 2);
            display.drawText(52, static_cast<int16_t>(screen_height / 2 + 10), "PRESS A OR START",
                             Display::rgb565(180, 220, 255), 1);

            if (calibration_warning_) {
                display.drawText(34, static_cast<int16_t>(screen_height / 2 + 24), "TILT MORE TO DETECT THE AXIS",
                                 Display::rgb565(255, 220, 88), 1);
            }
        }

        const int16_t preview_x = static_cast<int16_t>((screen_width - PlayerWidth) / 2);
        const int16_t preview_y = static_cast<int16_t>(screen_height - 42);
        drawPlayerAvatar(display, preview_x, preview_y, avatar_, AvatarPose::Idle, false);

        display.drawText(64, static_cast<int16_t>(screen_height - 16), "SELECT PAUSE",
                         Display::rgb565(220, 220, 240), 1);
    } else if (game_over_) {
        renderLoseOverlay(display);
    } else {
        display.drawText(10, static_cast<int16_t>(screen_height - 16), "TILT TO DRIFT  SELECT PAUSE",
                         Display::rgb565(220, 220, 240), 1);
    }

    if (paused_ && !game_over_) {
        renderPauseOverlay(display);
    }

    display.present();
}

bool SkyDodgerGame::exitRequested() const {
    return exit_requested_;
}

void SkyDodgerGame::clearExitRequest() {
    exit_requested_ = false;
}

bool SkyDodgerGame::consumeFinishedRun(uint32_t& distance, uint32_t& coins) {
    if (!run_report_pending_) {
        return false;
    }

    distance = pending_distance_;
    coins = pending_coins_;
    run_report_pending_ = false;
    return true;
}

void SkyDodgerGame::resetRound(const Display& display) {
    const int16_t screen_width = static_cast<int16_t>(display.width());
    player_x_ = static_cast<int16_t>((screen_width - PlayerWidth) / 2);
    facing_left_ = false;
    distance_ = 0;
    coins_collected_ = 0;
    frame_count_ = 0;
    game_over_ = false;
    exit_requested_ = false;
    paused_ = false;
    editing_volume_ = false;
    pause_selection_ = PauseSelection::ReturnToMenu;
    lose_selection_ = LoseSelection::Retry;
    run_result_committed_ = false;
    loss_input_unlock_at_ = get_absolute_time();

    seedClouds(display);

    for (size_t index = 0; index < hazards_.size(); ++index) {
        if (index < static_cast<size_t>(difficulty_level_ + 2)) { // Level 1: 3, Level 2: 4, Level 3: 5, Level 4: 6, Level 5: 7 hazards
            spawnHazard(hazards_[index], display, true);
            hazards_[index].active = true;
        } else {
            hazards_[index].active = false;
            hazards_[index].x = -100; // Position off-screen
            hazards_[index].y = -100;
        }
    }

    for (size_t index = 0; index < coins_.size(); ++index) {
        spawnCoin(coins_[index], display, true);
    }
}

void SkyDodgerGame::seedClouds(const Display& display) {
    for (Cloud& cloud : clouds_) {
        spawnCloud(cloud, display, true);
    }
}

void SkyDodgerGame::spawnCloud(Cloud& cloud, const Display& display, bool initial_spawn) {
    const int16_t screen_width = static_cast<int16_t>(display.width());
    const int16_t screen_height = static_cast<int16_t>(display.height());

    cloud.width = static_cast<int16_t>(26 + (nextRandom() % 32));
    cloud.height = static_cast<int16_t>(12 + (nextRandom() % 10));
    cloud.speed = static_cast<uint8_t>(2 + (nextRandom() % 3));
    cloud.drift = static_cast<int8_t>(static_cast<int32_t>(nextRandom() % 3) - 1);
    cloud.x = static_cast<int16_t>(static_cast<int32_t>(nextRandom() % static_cast<uint32_t>(screen_width + 40)) - 20);

    if (initial_spawn) {
        cloud.y = static_cast<int16_t>(static_cast<int32_t>(nextRandom() % static_cast<uint32_t>(screen_height + 30)) - 10);
    } else {
        cloud.y = static_cast<int16_t>(screen_height + cloud.height + static_cast<int16_t>(nextRandom() % 18));
    }
}

void SkyDodgerGame::updateClouds(const Display& display) {
    const int16_t screen_width = static_cast<int16_t>(display.width());

    for (Cloud& cloud : clouds_) {
        cloud.y = static_cast<int16_t>(cloud.y - cloud.speed);
        cloud.x = static_cast<int16_t>(cloud.x + cloud.drift);

        if (cloud.x + cloud.width < -20) {
            cloud.x = static_cast<int16_t>(screen_width + 16);
        } else if (cloud.x > screen_width + 20) {
            cloud.x = static_cast<int16_t>(-cloud.width - 16);
        }

        if (cloud.y + cloud.height < -10) {
            spawnCloud(cloud, display, false);
        }
    }
}

void SkyDodgerGame::spawnHazard(Hazard& hazard, const Display& display, bool initial_spawn) {
    const int16_t screen_width = static_cast<int16_t>(display.width());
    const int16_t screen_height = static_cast<int16_t>(display.height());
    // Map difficulty level (1-5) to hazard difficulty, with 3 as baseline
    const int16_t difficulty = static_cast<int16_t>(difficulty_level_ - 3 + 3); // Level 3 = 3, Level 1 = 1, Level 5 = 7

    if ((nextRandom() % 3u) == 0u) {
        hazard.kind = HazardKind::Balloon;
        hazard.variant = static_cast<uint8_t>(nextRandom() % 4u);
        hazard.width = 16;
        hazard.height = 24;
        hazard.dx = static_cast<int16_t>(static_cast<int32_t>(nextRandom() % 5u) - 2);
        hazard.dy = static_cast<int16_t>(-2 - difficulty / 2 - static_cast<int16_t>(nextRandom() % 2u));

        const uint32_t spawn_span = static_cast<uint32_t>(std::max<int16_t>(1, static_cast<int16_t>(screen_width - hazard.width - 12)));
        hazard.x = static_cast<int16_t>(6 + nextRandom() % spawn_span);

        const int16_t entry_gap = initial_spawn
            ? static_cast<int16_t>(16 + nextRandom() % static_cast<uint32_t>(screen_height + 20))
            : static_cast<int16_t>(12 + nextRandom() % 24u);
        hazard.y = static_cast<int16_t>(screen_height + entry_gap);
        return;
    }

    hazard.kind = HazardKind::Birds;
    hazard.variant = static_cast<uint8_t>(nextRandom() % 3);
    hazard.width = static_cast<int16_t>(28 + hazard.variant * 10);
    hazard.height = 12;

    const bool from_left = (nextRandom() & 1u) == 0;
    const int16_t speed = static_cast<int16_t>(4 + difficulty + static_cast<int16_t>(nextRandom() % 3));
    hazard.dx = from_left ? speed : static_cast<int16_t>(-speed);
    hazard.dy = static_cast<int16_t>(-2 - difficulty / 2);
    hazard.y = static_cast<int16_t>(16 + nextRandom() % static_cast<uint32_t>(screen_height - 32));
    const int16_t entry_gap = initial_spawn
        ? static_cast<int16_t>(28 + nextRandom() % static_cast<uint32_t>(screen_width / 2 + 30))
        : 10;

    if (from_left) {
        hazard.x = static_cast<int16_t>(-hazard.width - entry_gap);
    } else {
        hazard.x = static_cast<int16_t>(screen_width + entry_gap);
    }
}

void SkyDodgerGame::updateHazards(const Display& display) {
    const int16_t screen_width = static_cast<int16_t>(display.width());

    for (Hazard& hazard : hazards_) {
        if (!hazard.active) {
            continue;
        }
        
        hazard.x = static_cast<int16_t>(hazard.x + hazard.dx);
        hazard.y = static_cast<int16_t>(hazard.y + hazard.dy);

        const bool off_left = hazard.x + hazard.width < -20;
        const bool off_right = hazard.x > screen_width + 20;
        const bool off_top = hazard.y + hazard.height < -16;

        if (off_left || off_right || off_top) {
            spawnHazard(hazard, display, false);
        }
    }
}

void SkyDodgerGame::spawnCoin(Coin& coin, const Display& display, bool initial_spawn) {
    const int16_t screen_width = static_cast<int16_t>(display.width());
    const int16_t screen_height = static_cast<int16_t>(display.height());

    coin.x = static_cast<int16_t>(nextRandom() % static_cast<uint32_t>(screen_width - 12));
    coin.y = initial_spawn
        ? static_cast<int16_t>(nextRandom() % static_cast<uint32_t>(screen_height - 12))
        : static_cast<int16_t>(screen_height + 20 + (nextRandom() % 80));
    coin.collected = false;
    coin.active = true;
}

void SkyDodgerGame::updateCoins(const Display& display) {
    const int16_t screen_height = static_cast<int16_t>(display.height());

    for (Coin& coin : coins_) {
        if (!coin.active || coin.collected) {
            continue;
        }

        // Coins move upwards faster with the background
        coin.y -= 5;

        // Respawn coin if it goes off screen
        if (coin.y < -24) {
            spawnCoin(coin, display, false);
        }

        // Check collision with player (player is at center of screen) - updated for larger coin
        const int16_t player_y = static_cast<int16_t>(display.height() / 2 - PlayerHeight / 2);
        const bool collision = player_x_ < coin.x + 12 && player_x_ + PlayerWidth > coin.x &&
                              player_y < coin.y + 12 && player_y + PlayerHeight > coin.y;
        if (collision && !coin.collected) {
            coin.collected = true;
            coin.active = false;
            coins_collected_++;
            audio_.playEffect(SoundEffect::Coin);
        }
    }
}

void SkyDodgerGame::finalizeRun() {
    if (calibration_stage_ != CalibrationStage::Ready || run_result_committed_) {
        return;
    }

    pending_distance_ = distance_ > 0 ? static_cast<uint32_t>(distance_) : 0;
    pending_coins_ = coins_collected_;
    run_report_pending_ = true;
    run_result_committed_ = true;
}

void SkyDodgerGame::updateLoseMenu(const Buttons& buttons, const Display& display) {
    if (!timeReached(loss_input_unlock_at_)) {
        return;
    }

    if (buttons.pressed(ButtonId::Up) || buttons.pressed(ButtonId::Down) ||
        buttons.pressed(ButtonId::Left) || buttons.pressed(ButtonId::Right)) {
        lose_selection_ = lose_selection_ == LoseSelection::Retry
            ? LoseSelection::ReturnToMenu
            : LoseSelection::Retry;
        audio_.playEffect(SoundEffect::MenuMove);
    }

    if (buttons.pressed(ButtonId::B) || buttons.pressed(ButtonId::Select)) {
        exit_requested_ = true;
        audio_.playEffect(SoundEffect::MenuMove);
        return;
    }

    if (buttons.pressed(ButtonId::A) || buttons.pressed(ButtonId::Start)) {
        if (lose_selection_ == LoseSelection::Retry) {
            resetRound(display);
            audio_.startBackgroundMusic(MusicTrack::SkyDodger);
        } else {
            exit_requested_ = true;
        }
        audio_.playEffect(SoundEffect::MenuMove);
    }
}

void SkyDodgerGame::updatePauseMenu(const Buttons& buttons) {
    if (editing_volume_) {
        bool changed = false;
        if (buttons.pressed(ButtonId::Right) || buttons.pressed(ButtonId::Up)) {
            adjustVolume(1);
            changed = true;
        }
        if (buttons.pressed(ButtonId::Left) || buttons.pressed(ButtonId::Down)) {
            adjustVolume(-1);
            changed = true;
        }
        if (changed) {
            audio_.playEffect(SoundEffect::MenuMove);
        }

        if (buttons.pressed(ButtonId::A) || buttons.pressed(ButtonId::Start) ||
            buttons.pressed(ButtonId::B) || buttons.pressed(ButtonId::Select)) {
            editing_volume_ = false;
            audio_.playEffect(SoundEffect::MenuMove);
        }
        return;
    }

    if (buttons.pressed(ButtonId::Up) || buttons.pressed(ButtonId::Down)) {
        pause_selection_ = pause_selection_ == PauseSelection::ReturnToMenu
            ? PauseSelection::Volume
            : PauseSelection::ReturnToMenu;
        audio_.playEffect(SoundEffect::MenuMove);
    }

    if (buttons.pressed(ButtonId::B) || buttons.pressed(ButtonId::Select)) {
        paused_ = false;
        audio_.playEffect(SoundEffect::MenuMove);
        return;
    }

    if (buttons.pressed(ButtonId::A) || buttons.pressed(ButtonId::Start)) {
        if (pause_selection_ == PauseSelection::ReturnToMenu) {
            finalizeRun();
            paused_ = false;
            exit_requested_ = true;
        } else {
            editing_volume_ = true;
        }
        audio_.playEffect(SoundEffect::MenuMove);
    }
}

void SkyDodgerGame::adjustVolume(int delta) {
    audio_.changeVolume(delta);
}

void SkyDodgerGame::renderLoseOverlay(Display& display) const {
    const int16_t screen_width = static_cast<int16_t>(display.width());
    const int16_t screen_height = static_cast<int16_t>(display.height());
    const int16_t box_width = 224;
    const int16_t box_height = 96;
    const int16_t box_x = static_cast<int16_t>((screen_width - box_width) / 2);
    const int16_t box_y = static_cast<int16_t>((screen_height - box_height) / 2);
    const bool input_ready = timeReached(loss_input_unlock_at_);
    const Display::Color highlight = Display::rgb565(54, 120, 200);

    display.fillRect(box_x, box_y, box_width, box_height, Display::rgb565(28, 36, 72));
    display.drawRect(box_x, box_y, box_width, box_height, Display::rgb565(255, 196, 48));
    display.drawText(static_cast<int16_t>(box_x + 40), static_cast<int16_t>(box_y + 10), "MID-AIR CRASH",
                     Display::rgb565(255, 255, 255), 2);

    const bool retry_selected = lose_selection_ == LoseSelection::Retry && input_ready;
    const bool menu_selected = lose_selection_ == LoseSelection::ReturnToMenu && input_ready;
    if (retry_selected) {
        display.fillRect(static_cast<int16_t>(box_x + 18), static_cast<int16_t>(box_y + 40), 188, 14, highlight);
    }
    if (menu_selected) {
        display.fillRect(static_cast<int16_t>(box_x + 18), static_cast<int16_t>(box_y + 60), 188, 14, highlight);
    }

    display.drawText(static_cast<int16_t>(box_x + 26), static_cast<int16_t>(box_y + 43), "FLY AGAIN",
                     Display::rgb565(255, 255, 255), 1);
    display.drawText(static_cast<int16_t>(box_x + 26), static_cast<int16_t>(box_y + 63), "RETURN TO MENU",
                     Display::rgb565(255, 255, 255), 1);
    display.drawText(static_cast<int16_t>(box_x + 18), static_cast<int16_t>(box_y + 80),
                     input_ready ? "UP/DOWN CHOOSE  A/START OK" : "PLEASE WAIT...",
                     Display::rgb565(200, 200, 220), 1);
}

void SkyDodgerGame::renderPauseOverlay(Display& display) const {
    const int16_t screen_width = static_cast<int16_t>(display.width());
    const int16_t screen_height = static_cast<int16_t>(display.height());
    const int16_t box_width = 210;
    const int16_t box_height = 92;
    const int16_t box_x = static_cast<int16_t>((screen_width - box_width) / 2);
    const int16_t box_y = static_cast<int16_t>((screen_height - box_height) / 2);

    display.fillRect(box_x, box_y, box_width, box_height, Display::rgb565(18, 28, 58));
    display.drawRect(box_x, box_y, box_width, box_height, Display::rgb565(255, 196, 48));
    display.drawText(static_cast<int16_t>(box_x + 68), static_cast<int16_t>(box_y + 10), "PAUSED",
                     Display::rgb565(255, 255, 255), 2);

    const bool return_selected = pause_selection_ == PauseSelection::ReturnToMenu && !editing_volume_;
    const bool volume_selected = pause_selection_ == PauseSelection::Volume;
    const Display::Color highlight = Display::rgb565(54, 120, 200);

    if (return_selected) {
        display.fillRect(static_cast<int16_t>(box_x + 16), static_cast<int16_t>(box_y + 36), 178, 14, highlight);
    }
    display.drawText(static_cast<int16_t>(box_x + 22), static_cast<int16_t>(box_y + 39), "RETURN TO MENU",
                     Display::rgb565(255, 255, 255), 1);

    if (volume_selected && !editing_volume_) {
        display.fillRect(static_cast<int16_t>(box_x + 16), static_cast<int16_t>(box_y + 56), 178, 24, highlight);
    }
    display.drawText(static_cast<int16_t>(box_x + 22), static_cast<int16_t>(box_y + 59), "VOLUME",
                     Display::rgb565(255, 255, 255), 1);

    const int16_t bar_x = static_cast<int16_t>(box_x + 84);
    const int16_t bar_y = static_cast<int16_t>(box_y + 60);
    const int16_t bar_width = 96;
    const int16_t bar_height = 10;
    display.drawRect(bar_x, bar_y, bar_width, bar_height, Display::rgb565(220, 220, 240));
    const int16_t fill_width = static_cast<int16_t>((bar_width - 2) * audio_.volume() / AudioEngine::MaxVolume);
    if (fill_width > 0) {
        display.fillRect(static_cast<int16_t>(bar_x + 1), static_cast<int16_t>(bar_y + 1), fill_width,
                         static_cast<int16_t>(bar_height - 2), Display::rgb565(120, 220, 140));
    }

    display.drawText(static_cast<int16_t>(box_x + 16), static_cast<int16_t>(box_y + 78),
                     editing_volume_ ? "UP/RIGHT +  LEFT/DOWN -" : "A SELECT  B/SELECT RESUME",
                     Display::rgb565(200, 200, 220), 1);
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
