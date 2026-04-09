#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "picoboy/audio_engine.hpp"
#include "picoboy/buttons.hpp"
#include "picoboy/display.hpp"
#include "picoboy/lsm6dsl.hpp"
#include "picoboy/profile_store.hpp"
#include "pico/time.h"

namespace picoboy {

class SkyDodgerGame {
public:
    SkyDodgerGame(Lsm6dsl& imu, AudioEngine& audio);

    void enter(const Display& display);
    void update(const Buttons& buttons, const Display& display);
    void render(Display& display) const;
    void setAvatar(AvatarId avatar);

    bool exitRequested() const;
    void clearExitRequest();
    bool consumeFinishedRun(uint32_t& distance, uint32_t& coins);

private:
    enum class PauseSelection : uint8_t {
        ReturnToMenu,
        Volume,
    };

    enum class LoseSelection : uint8_t {
        Retry,
        ReturnToMenu,
    };

    enum class CalibrationStage : uint8_t {
        HoldLevel,
        TiltRight,
        DifficultySelect,
        Ready,
    };

    enum class TiltAxis : uint8_t {
        X,
        Y,
    };

    enum class HazardKind : uint8_t {
        Birds,
        Balloon,
    };

    struct Cloud {
        int16_t x = 0;
        int16_t y = 0;
        int16_t width = 0;
        int16_t height = 0;
        int8_t drift = 0;
        uint8_t speed = 0;
    };

    struct Hazard {
        int16_t x = 0;
        int16_t y = 0;
        int16_t width = 0;
        int16_t height = 0;
        int16_t dx = 0;
        int16_t dy = 0;
        uint8_t variant = 0;
        HazardKind kind = HazardKind::Birds;
        bool active = false;
    };

    struct Coin {
        int16_t x = 0;
        int16_t y = 0;
        bool collected = false;
        bool active = false;
    };

    static constexpr size_t CloudCount = 7;
    static constexpr size_t HazardCount = 8;
    static constexpr size_t CoinCount = 8;
    static constexpr int16_t PlayerWidth = 12;
    static constexpr int16_t PlayerHeight = 16;
    static constexpr uint16_t SkyCycleFrames = 2400;

    void resetRound(const Display& display);
    void seedClouds(const Display& display);
    void spawnCloud(Cloud& cloud, const Display& display, bool initial_spawn);
    void updateClouds(const Display& display);
    void spawnHazard(Hazard& hazard, const Display& display, bool initial_spawn);
    void updateHazards(const Display& display);
    void spawnCoin(Coin& coin, const Display& display, bool initial_spawn);
    void updateCoins(const Display& display);
    void finalizeRun();
    void updateLoseMenu(const Buttons& buttons, const Display& display);
    void updatePauseMenu(const Buttons& buttons);
    void adjustVolume(int delta);
    void renderLoseOverlay(Display& display) const;
    void renderPauseOverlay(Display& display) const;
    uint32_t nextRandom();
    bool captureAverageAcceleration(Lsm6dsl::Acceleration& out) const;
    float readSteeringTiltG() const;

    Lsm6dsl& imu_;
    AudioEngine& audio_;
    bool exit_requested_ = false;
    bool game_over_ = false;
    bool calibration_warning_ = false;
    bool paused_ = false;
    bool editing_volume_ = false;
    bool facing_left_ = false;
    int16_t player_x_ = 0;
    int32_t distance_ = 0;
    uint32_t coins_collected_ = 0;
    uint32_t frame_count_ = 0;
    uint8_t difficulty_level_ = 3;
    AvatarId avatar_ = AvatarId::Hero;
    PauseSelection pause_selection_ = PauseSelection::ReturnToMenu;
    LoseSelection lose_selection_ = LoseSelection::Retry;
    bool run_report_pending_ = false;
    bool run_result_committed_ = false;
    uint32_t pending_distance_ = 0;
    uint32_t pending_coins_ = 0;
    absolute_time_t loss_input_unlock_at_{};
    float neutral_x_g_ = 0.0f;
    float neutral_y_g_ = 0.0f;
    float steering_sign_ = 1.0f;
    uint32_t rng_state_ = 0x12345678;
    CalibrationStage calibration_stage_ = CalibrationStage::HoldLevel;
    TiltAxis tilt_axis_ = TiltAxis::Y;
    std::array<Cloud, CloudCount> clouds_{};
    std::array<Hazard, HazardCount> hazards_{};
    std::array<Coin, CoinCount> coins_{};
};

}  // namespace picoboy
