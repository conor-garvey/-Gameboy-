#pragma once

#include <array>
#include <cstdint>

#include "picoboy/audio_engine.hpp"
#include "picoboy/buttons.hpp"
#include "picoboy/display.hpp"
#include "picoboy/profile_store.hpp"
#include "pico/time.h"

namespace picoboy {

class PlumberManGame {
public:
    explicit PlumberManGame(AudioEngine& audio);

    void enter();
    void update(const Buttons& buttons);
    void render(Display& display) const;
    void setAvatar(AvatarId avatar);

    bool exitRequested() const;
    void clearExitRequest();
    bool consumeCompletedRun(uint32_t& completion_time_ms, uint8_t& coins_collected);

private:
    enum class PauseSelection : uint8_t {
        ReturnToMenu,
        Volume,
    };

    enum class LoseSelection : uint8_t {
        Retry,
        ReturnToMenu,
    };

    struct Enemy {
        int16_t x = 0;
        int16_t y = 0;
        int16_t min_x = 0;
        int16_t max_x = 0;
        int8_t direction = 1;
        bool alive = true;
    };

    static constexpr int16_t PlayerWidth = 12;
    static constexpr int16_t PlayerHeight = 16;
    static constexpr size_t CoinCount = 7;
    static constexpr int16_t EnemyWidth = 14;
    static constexpr int16_t EnemyHeight = 14;
    static constexpr size_t EnemyCount = 5;
    static constexpr int8_t JumpVelocity = -12;

    void resetLevel();
    void triggerLoss();
    void resetEnemies();
    void collectCoins();
    bool updateEnemies();
    bool handleEnemyCollision(Enemy& enemy);
    void updateLoseMenu(const Buttons& buttons);
    void updatePauseMenu(const Buttons& buttons);
    void adjustVolume(int delta);
    void renderLoseOverlay(Display& display) const;
    void renderPauseOverlay(Display& display) const;
    void movePlayerX(int16_t delta);
    void movePlayerY(int16_t delta);
    bool collidesAt(int16_t x, int16_t y) const;
    int16_t cameraX(int16_t viewport_width) const;

    AudioEngine& audio_;
    int16_t player_x_ = 24;
    int16_t player_y_ = 0;
    int8_t velocity_y_ = 0;
    bool on_ground_ = false;
    bool lost_ = false;
    bool won_ = false;
    bool exit_requested_ = false;
    bool paused_ = false;
    bool editing_volume_ = false;
    bool facing_left_ = false;
    bool moving_horizontally_ = false;
    uint8_t coins_collected_ = 0;
    AvatarId avatar_ = AvatarId::Hero;
    PauseSelection pause_selection_ = PauseSelection::ReturnToMenu;
    LoseSelection lose_selection_ = LoseSelection::Retry;
    absolute_time_t run_started_{};
    absolute_time_t loss_input_unlock_at_{};
    bool completion_report_pending_ = false;
    uint32_t pending_completion_time_ms_ = 0;
    uint8_t pending_completion_coins_ = 0;
    std::array<bool, CoinCount> coin_collected_{};
    std::array<Enemy, EnemyCount> enemies_{};
};

}  // namespace picoboy
