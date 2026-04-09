#include "picoboy/plumber_man_game.hpp"

#include <cstdio>

#include "picoboy/avatar_art.hpp"

namespace picoboy {

namespace {

struct Rect {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
};

struct Coin {
    int16_t x;
    int16_t y;
};

struct EnemySpawn {
    int16_t x;
    int16_t y;
    int16_t min_x;
    int16_t max_x;
};

constexpr int16_t world_width = 1024;
constexpr int16_t world_height = 240;
constexpr int16_t flag_x = 972;
constexpr Rect platforms[] = {
    {0, 216, 230, 24},
    {280, 216, 160, 24},
    {490, 216, 190, 24},
    {720, 216, 304, 24},
    {110, 170, 48, 10},
    {200, 142, 48, 10},
    {338, 166, 48, 10},
    {540, 170, 52, 10},
    {650, 138, 64, 10},
    {810, 166, 58, 10},
};
constexpr Coin coins[] = {
    {124, 154},
    {214, 126},
    {352, 150},
    {560, 154},
    {668, 122},
    {828, 150},
    {930, 186},
};
constexpr EnemySpawn enemy_spawns[] = {
    {320, 202, 296, 412},
    {522, 202, 506, 650},
    {340, 152, 338, 372},
    {810, 152, 810, 854},
    {760, 202, 736, 944},
};
constexpr Rect pits[] = {
    {230, 216, 50, static_cast<int16_t>(world_height - 216)},
    {440, 216, 50, static_cast<int16_t>(world_height - 216)},
    {680, 216, 40, static_cast<int16_t>(world_height - 216)},
};
constexpr int8_t gravity_per_frame = 3;
constexpr int8_t terminal_velocity = 11;
constexpr int16_t world_view_lift = -20;
constexpr uint32_t death_sequence_lock_ms = 3600;

bool intersects(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                int16_t bx, int16_t by, int16_t bw, int16_t bh) {
    return ax < bx + bw &&
           ax + aw > bx &&
           ay < by + bh &&
           ay + ah > by;
}

Display::Color skyColor() {
    return Display::rgb565(110, 188, 255);
}

Display::Color hillColor() {
    return Display::rgb565(92, 180, 92);
}

Display::Color groundColor() {
    return Display::rgb565(176, 104, 40);
}

Display::Color grassColor() {
    return Display::rgb565(92, 216, 92);
}

Display::Color groundShadowColor() {
    return Display::rgb565(98, 54, 18);
}

Display::Color grassShadowColor() {
    return Display::rgb565(40, 132, 52);
}

Display::Color pitDepthColor() {
    return Display::rgb565(8, 10, 18);
}

Display::Color pitGlowColor() {
    return Display::rgb565(168, 52, 28);
}

bool timeReached(absolute_time_t deadline) {
    return absolute_time_diff_us(get_absolute_time(), deadline) <= 0;
}

}

PlumberManGame::PlumberManGame(AudioEngine& audio) : audio_(audio) {}

void PlumberManGame::enter() {
    resetLevel();
}

void PlumberManGame::setAvatar(AvatarId avatar) {
    avatar_ = sanitizeAvatarId(static_cast<uint8_t>(avatar));
}

void PlumberManGame::update(const Buttons& buttons) {
    moving_horizontally_ = false;

    if (lost_) {
        updateLoseMenu(buttons);
        return;
    }

    if (!won_ && buttons.pressed(ButtonId::Select)) {
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

    if (buttons.pressed(ButtonId::Start)) {
        resetLevel();
        return;
    }

    if (won_) {
        return;
    }

    constexpr int16_t move_speed = 4;
    if (buttons.held(ButtonId::Left) && !buttons.held(ButtonId::Right)) {
        facing_left_ = true;
        moving_horizontally_ = true;
        movePlayerX(static_cast<int16_t>(-move_speed));
    } else if (buttons.held(ButtonId::Right) && !buttons.held(ButtonId::Left)) {
        facing_left_ = false;
        moving_horizontally_ = true;
        movePlayerX(move_speed);
    }

    if ((buttons.pressed(ButtonId::A) || buttons.pressed(ButtonId::Up)) && on_ground_) {
        velocity_y_ = JumpVelocity;
        on_ground_ = false;
    }

    if (velocity_y_ < terminal_velocity) {
        const int8_t gravity_step = velocity_y_ < 0 ? 1 : gravity_per_frame;
        velocity_y_ = static_cast<int8_t>(velocity_y_ + gravity_step);
        if (velocity_y_ > terminal_velocity) {
            velocity_y_ = terminal_velocity;
        }
    }

    movePlayerY(velocity_y_);
    collectCoins();

    if (updateEnemies()) {
        return;
    }

    if (player_y_ > world_height + 32) {
        triggerLoss();
        return;
    }

    if (player_x_ + PlayerWidth >= flag_x) {
        won_ = true;
        if (!completion_report_pending_) {
            pending_completion_time_ms_ = static_cast<uint32_t>(
                absolute_time_diff_us(run_started_, get_absolute_time()) / 1000);
            pending_completion_coins_ = coins_collected_;
            completion_report_pending_ = true;
        }
    }
}

void PlumberManGame::render(Display& display) const {
    const int16_t screen_width = static_cast<int16_t>(display.width());
    const int16_t screen_height = static_cast<int16_t>(display.height());
    const int16_t camera_x = cameraX(screen_width);
    const int16_t centered_offset = screen_height > world_height
        ? static_cast<int16_t>((screen_height - world_height) / 2)
        : 0;
    const int16_t world_y_offset = static_cast<int16_t>(centered_offset + world_view_lift);
    display.beginFrame(skyColor());

    display.fillRect(0, 0, screen_width, screen_height, skyColor());
    display.fillRect(0, static_cast<int16_t>(170 + world_y_offset), screen_width,
                     static_cast<int16_t>(screen_height - (170 + world_y_offset)),
                     Display::rgb565(170, 220, 255));

    for (int hill = 0; hill < 5; ++hill) {
        const int16_t hill_x = static_cast<int16_t>(hill * 220 - (camera_x / 2));
        display.fillRect(hill_x, static_cast<int16_t>(170 + world_y_offset), 110, 46, hillColor());
        display.fillRect(static_cast<int16_t>(hill_x + 20), static_cast<int16_t>(150 + world_y_offset), 70, 20, hillColor());
    }

    for (const Rect& pit : pits) {
        const int16_t screen_x = static_cast<int16_t>(pit.x - camera_x);
        if (screen_x + pit.width < 0 || screen_x >= screen_width) {
            continue;
        }

        const int16_t screen_y = static_cast<int16_t>(pit.y + world_y_offset);
        display.fillRect(screen_x, screen_y, pit.width, pit.height, pitDepthColor());
        display.fillRect(screen_x, screen_y, pit.width, 4, pitGlowColor());
        display.fillRect(screen_x, static_cast<int16_t>(screen_y + 4), pit.width, 4, Display::rgb565(24, 16, 26));
        display.fillRect(screen_x, screen_y, 3, pit.height, groundShadowColor());
        display.fillRect(static_cast<int16_t>(screen_x + pit.width - 3), screen_y, 3, pit.height, groundShadowColor());
    }

    for (const Rect& platform : platforms) {
        const int16_t screen_x = static_cast<int16_t>(platform.x - camera_x);
        if (screen_x + platform.width < 0 || screen_x >= screen_width) {
            continue;
        }

        const int16_t screen_y = static_cast<int16_t>(platform.y + world_y_offset);
        display.fillRect(screen_x, screen_y, platform.width, platform.height, groundColor());
        display.fillRect(screen_x, screen_y, platform.width, 5, grassColor());
        display.fillRect(screen_x, static_cast<int16_t>(screen_y + 5), platform.width, 2, grassShadowColor());
        display.drawRect(screen_x, screen_y, platform.width, platform.height, groundShadowColor());

        if (platform.height > 12) {
            for (int16_t stripe_x = 8; stripe_x < platform.width - 4; stripe_x = static_cast<int16_t>(stripe_x + 16)) {
                display.fillRect(static_cast<int16_t>(screen_x + stripe_x), static_cast<int16_t>(screen_y + 9), 3,
                                 static_cast<int16_t>(platform.height - 11), groundShadowColor());
            }
        }
    }

    for (size_t index = 0; index < coin_collected_.size(); ++index) {
        if (coin_collected_[index]) {
            continue;
        }

        const int16_t screen_x = static_cast<int16_t>(coins[index].x - camera_x);
        const int16_t screen_y = static_cast<int16_t>(coins[index].y + world_y_offset);

        if (screen_x < -6 || screen_x > static_cast<int16_t>(screen_width + 6)) {
            continue;
        }

        display.fillRect(screen_x, screen_y, 6, 10, Display::rgb565(255, 216, 0));
        display.drawRect(screen_x, screen_y, 6, 10, Display::rgb565(255, 248, 120));
    }

    for (const Enemy& enemy : enemies_) {
        if (!enemy.alive) {
            continue;
        }

        const int16_t screen_x = static_cast<int16_t>(enemy.x - camera_x);
        const int16_t screen_y = static_cast<int16_t>(enemy.y + world_y_offset);

        if (screen_x + EnemyWidth < 0 || screen_x >= screen_width) {
            continue;
        }

        display.fillRect(screen_x, screen_y, EnemyWidth, EnemyHeight, Display::rgb565(170, 112, 54));
        display.fillRect(screen_x, static_cast<int16_t>(screen_y + EnemyHeight - 4), EnemyWidth, 4,
                         Display::rgb565(112, 62, 22));
        display.fillRect(static_cast<int16_t>(screen_x + 3), static_cast<int16_t>(screen_y + 4), 2, 2,
                         Display::rgb565(255, 255, 255));
        display.fillRect(static_cast<int16_t>(screen_x + 9), static_cast<int16_t>(screen_y + 4), 2, 2,
                         Display::rgb565(255, 255, 255));
    }

    const int16_t flag_screen_x = static_cast<int16_t>(flag_x - camera_x);
    display.fillRect(flag_screen_x, static_cast<int16_t>(90 + world_y_offset), 4, 126, Display::rgb565(230, 230, 230));
    display.fillRect(static_cast<int16_t>(flag_screen_x + 4), static_cast<int16_t>(94 + world_y_offset), 26, 18,
                     Display::rgb565(220, 48, 48));

    const int16_t player_screen_x = static_cast<int16_t>(player_x_ - camera_x);
    const int16_t player_screen_y = static_cast<int16_t>(player_y_ + world_y_offset);
    AvatarPose avatar_pose = AvatarPose::Idle;
    if (lost_) {
        avatar_pose = AvatarPose::Death;
    } else if (!won_) {
        if (!on_ground_) {
            avatar_pose = AvatarPose::Jump;
        } else if (moving_horizontally_) {
            avatar_pose = ((player_x_ / 4) & 0x1) == 0 ? AvatarPose::WalkA : AvatarPose::WalkB;
        }
    }
    drawPlayerAvatar(display, player_screen_x, player_screen_y, avatar_, avatar_pose, facing_left_);

    display.fillRect(0, 0, screen_width, 24, Display::rgb565(10, 22, 56));
    display.drawText(10, 8, "PLUMBER MAN", Display::rgb565(255, 255, 255), 1);

    char hud_text[20];
    std::snprintf(hud_text, sizeof(hud_text), "COINS %u/%u", coins_collected_,
                  static_cast<unsigned>(coin_collected_.size()));
    const int16_t hud_x = static_cast<int16_t>(screen_width - display.measureTextWidth(hud_text, 1) - 10);
    display.drawText(hud_x > 10 ? hud_x : 10, 8, hud_text, Display::rgb565(255, 232, 92), 1);

    if (won_) {
        const int16_t box_width = 216;
        const int16_t box_height = 62;
        const int16_t box_x = static_cast<int16_t>((screen_width - box_width) / 2);
        const int16_t box_y = static_cast<int16_t>((screen_height - box_height) / 2);
        display.fillRect(box_x, box_y, box_width, box_height, Display::rgb565(18, 28, 58));
        display.drawRect(box_x, box_y, box_width, box_height, Display::rgb565(255, 196, 48));
        display.drawText(static_cast<int16_t>(box_x + 32), static_cast<int16_t>(box_y + 14), "LEVEL CLEAR",
                         Display::rgb565(255, 255, 255), 2);
        display.drawText(static_cast<int16_t>(box_x + 26), static_cast<int16_t>(box_y + 40), "START = RESTART",
                         Display::rgb565(180, 220, 255), 1);
    } else if (lost_) {
        renderLoseOverlay(display);
    } else {
        display.drawText(10, static_cast<int16_t>(screen_height - 16), "A JUMP  SELECT PAUSE",
                         Display::rgb565(240, 240, 240), 1);
    }

    if (paused_ && !lost_) {
        renderPauseOverlay(display);
    }

    display.present();
}

bool PlumberManGame::exitRequested() const {
    return exit_requested_;
}

void PlumberManGame::clearExitRequest() {
    exit_requested_ = false;
}

bool PlumberManGame::consumeCompletedRun(uint32_t& completion_time_ms, uint8_t& coins_collected) {
    if (!completion_report_pending_) {
        return false;
    }

    completion_time_ms = pending_completion_time_ms_;
    coins_collected = pending_completion_coins_;
    completion_report_pending_ = false;
    return true;
}

void PlumberManGame::resetLevel() {
    player_x_ = 28;
    player_y_ = 196;
    velocity_y_ = 0;
    on_ground_ = false;
    lost_ = false;
    won_ = false;
    exit_requested_ = false;
    paused_ = false;
    editing_volume_ = false;
    facing_left_ = false;
    moving_horizontally_ = false;
    pause_selection_ = PauseSelection::ReturnToMenu;
    lose_selection_ = LoseSelection::Retry;
    coins_collected_ = 0;
    run_started_ = get_absolute_time();
    loss_input_unlock_at_ = get_absolute_time();

    for (bool& collected : coin_collected_) {
        collected = false;
    }

    resetEnemies();
}

void PlumberManGame::triggerLoss() {
    if (lost_ || won_) {
        return;
    }

    lost_ = true;
    paused_ = false;
    editing_volume_ = false;
    on_ground_ = false;
    velocity_y_ = 0;
    moving_horizontally_ = false;
    lose_selection_ = LoseSelection::Retry;
    loss_input_unlock_at_ = delayed_by_ms(get_absolute_time(), death_sequence_lock_ms);
    audio_.stopBackgroundMusic();
    audio_.playEffect(SoundEffect::Death);
}

void PlumberManGame::resetEnemies() {
    for (size_t index = 0; index < enemies_.size(); ++index) {
        enemies_[index].x = enemy_spawns[index].x;
        enemies_[index].y = enemy_spawns[index].y;
        enemies_[index].min_x = enemy_spawns[index].min_x;
        enemies_[index].max_x = enemy_spawns[index].max_x;
        enemies_[index].direction = index % 2 == 0 ? 1 : -1;
        enemies_[index].alive = true;
    }
}

void PlumberManGame::collectCoins() {
    for (size_t index = 0; index < coin_collected_.size(); ++index) {
        if (coin_collected_[index]) {
            continue;
        }

        if (intersects(player_x_, player_y_, PlayerWidth, PlayerHeight,
                       coins[index].x, coins[index].y, 6, 10)) {
            coin_collected_[index] = true;
            ++coins_collected_;
            audio_.playEffect(SoundEffect::Coin);
        }
    }
}

bool PlumberManGame::updateEnemies() {
    for (Enemy& enemy : enemies_) {
        if (!enemy.alive) {
            continue;
        }

        enemy.x = static_cast<int16_t>(enemy.x + enemy.direction);
        if (enemy.x <= enemy.min_x) {
            enemy.x = enemy.min_x;
            enemy.direction = 1;
        } else if (enemy.x >= enemy.max_x) {
            enemy.x = enemy.max_x;
            enemy.direction = -1;
        }

        if (handleEnemyCollision(enemy)) {
            return true;
        }
    }

    return false;
}

bool PlumberManGame::handleEnemyCollision(Enemy& enemy) {
    if (!intersects(player_x_, player_y_, PlayerWidth, PlayerHeight,
                    enemy.x, enemy.y, EnemyWidth, EnemyHeight)) {
        return false;
    }

    const int16_t player_bottom = static_cast<int16_t>(player_y_ + PlayerHeight);
    const int16_t downward_speed = velocity_y_ > 0 ? velocity_y_ : 0;
    const int16_t previous_bottom = static_cast<int16_t>(player_bottom - downward_speed);
    const bool stomping = velocity_y_ > 0 &&
                          previous_bottom <= static_cast<int16_t>(enemy.y + 6) &&
                          player_y_ < static_cast<int16_t>(enemy.y + EnemyHeight);

    if (stomping) {
        enemy.alive = false;
        velocity_y_ = -6;
        on_ground_ = false;
        audio_.playEffect(SoundEffect::EnemyStomp);
        return false;
    }

    triggerLoss();
    return true;
}

void PlumberManGame::updateLoseMenu(const Buttons& buttons) {
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
            resetLevel();
            audio_.startBackgroundMusic(MusicTrack::PlumberMan);
        } else {
            exit_requested_ = true;
        }
        audio_.playEffect(SoundEffect::MenuMove);
    }
}

void PlumberManGame::updatePauseMenu(const Buttons& buttons) {
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
            paused_ = false;
            exit_requested_ = true;
        } else {
            editing_volume_ = true;
        }
        audio_.playEffect(SoundEffect::MenuMove);
    }
}

void PlumberManGame::adjustVolume(int delta) {
    audio_.changeVolume(delta);
}

void PlumberManGame::renderLoseOverlay(Display& display) const {
    const int16_t screen_width = static_cast<int16_t>(display.width());
    const int16_t screen_height = static_cast<int16_t>(display.height());
    const int16_t box_width = 224;
    const int16_t box_height = 96;
    const int16_t box_x = static_cast<int16_t>((screen_width - box_width) / 2);
    const int16_t box_y = static_cast<int16_t>((screen_height - box_height) / 2);
    const bool input_ready = timeReached(loss_input_unlock_at_);
    const Display::Color highlight = Display::rgb565(54, 120, 200);

    display.fillRect(box_x, box_y, box_width, box_height, Display::rgb565(18, 28, 58));
    display.drawRect(box_x, box_y, box_width, box_height, Display::rgb565(255, 196, 48));
    display.drawText(static_cast<int16_t>(box_x + 50), static_cast<int16_t>(box_y + 10), "YOU LOSE",
                     Display::rgb565(255, 255, 255), 2);

    const bool retry_selected = lose_selection_ == LoseSelection::Retry && input_ready;
    const bool menu_selected = lose_selection_ == LoseSelection::ReturnToMenu && input_ready;
    if (retry_selected) {
        display.fillRect(static_cast<int16_t>(box_x + 18), static_cast<int16_t>(box_y + 38), 188, 14, highlight);
    }
    if (menu_selected) {
        display.fillRect(static_cast<int16_t>(box_x + 18), static_cast<int16_t>(box_y + 58), 188, 14, highlight);
    }

    display.drawText(static_cast<int16_t>(box_x + 26), static_cast<int16_t>(box_y + 41), "PLAY AGAIN",
                     Display::rgb565(255, 255, 255), 1);
    display.drawText(static_cast<int16_t>(box_x + 26), static_cast<int16_t>(box_y + 61), "RETURN TO MENU",
                     Display::rgb565(255, 255, 255), 1);

    display.drawText(static_cast<int16_t>(box_x + 18), static_cast<int16_t>(box_y + 80),
                     input_ready ? "UP/DOWN CHOOSE  A/START OK" : "PLEASE WAIT...",
                     Display::rgb565(200, 200, 220), 1);
}

void PlumberManGame::renderPauseOverlay(Display& display) const {
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

void PlumberManGame::movePlayerX(int16_t delta) {
    const int16_t step = delta > 0 ? 1 : -1;

    for (int16_t moved = 0; moved != delta; moved = static_cast<int16_t>(moved + step)) {
        const int16_t next_x = static_cast<int16_t>(player_x_ + step);
        if (collidesAt(next_x, player_y_)) {
            return;
        }

        player_x_ = next_x;
    }
}

void PlumberManGame::movePlayerY(int16_t delta) {
    on_ground_ = false;

    const int16_t step = delta > 0 ? 1 : -1;
    for (int16_t moved = 0; moved != delta; moved = static_cast<int16_t>(moved + step)) {
        const int16_t next_y = static_cast<int16_t>(player_y_ + step);
        if (collidesAt(player_x_, next_y)) {
            if (step > 0) {
                on_ground_ = true;
            }

            velocity_y_ = 0;
            return;
        }

        player_y_ = next_y;
    }
}

bool PlumberManGame::collidesAt(int16_t x, int16_t y) const {
    if (x < 0 || x + PlayerWidth > world_width) {
        return true;
    }

    if (y < 0) {
        return true;
    }

    for (const Rect& platform : platforms) {
        if (intersects(x, y, PlayerWidth, PlayerHeight,
                       platform.x, platform.y, platform.width, platform.height)) {
            return true;
        }
    }

    return false;
}

int16_t PlumberManGame::cameraX(int16_t viewport_width) const {
    int16_t camera_x = static_cast<int16_t>(player_x_ - static_cast<int16_t>(viewport_width / 2));

    if (camera_x < 0) {
        camera_x = 0;
    }

    const int16_t max_camera = static_cast<int16_t>(world_width - viewport_width);
    if (camera_x > max_camera) {
        camera_x = max_camera;
    }

    return camera_x;
}

}  // namespace picoboy
