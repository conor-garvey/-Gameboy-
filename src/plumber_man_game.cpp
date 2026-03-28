#include "picoboy/plumber_man_game.hpp"

#include <cstdio>

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

constexpr int16_t world_width = 1024;
constexpr int16_t world_height = 240;
constexpr int16_t ground_y = 216;
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
    return Display::rgb565(140, 90, 38);
}

Display::Color grassColor() {
    return Display::rgb565(64, 190, 80);
}

}

void PlumberManGame::enter() {
    resetLevel();
}

void PlumberManGame::update(const Buttons& buttons) {
    if (buttons.pressed(ButtonId::Select)) {
        exit_requested_ = true;
        return;
    }

    if (buttons.pressed(ButtonId::Start)) {
        resetLevel();
        return;
    }

    if (won_) {
        return;
    }

    int16_t move_speed = buttons.held(ButtonId::B) ? 4 : 3;
    if (buttons.held(ButtonId::Left) && !buttons.held(ButtonId::Right)) {
        movePlayerX(static_cast<int16_t>(-move_speed));
    } else if (buttons.held(ButtonId::Right) && !buttons.held(ButtonId::Left)) {
        movePlayerX(move_speed);
    }

    if ((buttons.pressed(ButtonId::A) || buttons.pressed(ButtonId::Up)) && on_ground_) {
        velocity_y_ = -10;
        on_ground_ = false;
    }

    if (velocity_y_ < 9) {
        ++velocity_y_;
    }

    movePlayerY(velocity_y_);
    collectCoins();

    if (player_y_ > world_height + 32) {
        resetLevel();
        return;
    }

    if (player_x_ + PlayerWidth >= flag_x) {
        won_ = true;
    }
}

void PlumberManGame::render(Display& display) const {
    const int16_t screen_width = static_cast<int16_t>(display.width());
    const int16_t screen_height = static_cast<int16_t>(display.height());
    const int16_t camera_x = cameraX(screen_width);
    const int16_t world_y_offset = screen_height > world_height
        ? static_cast<int16_t>((screen_height - world_height) / 2)
        : 0;
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

    for (const Rect& platform : platforms) {
        const int16_t screen_x = static_cast<int16_t>(platform.x - camera_x);
        if (screen_x + platform.width < 0 || screen_x >= screen_width) {
            continue;
        }

        const int16_t screen_y = static_cast<int16_t>(platform.y + world_y_offset);
        display.fillRect(screen_x, screen_y, platform.width, platform.height, groundColor());
        display.fillRect(screen_x, screen_y, platform.width, 4, grassColor());
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

    const int16_t flag_screen_x = static_cast<int16_t>(flag_x - camera_x);
    display.fillRect(flag_screen_x, static_cast<int16_t>(90 + world_y_offset), 4, 126, Display::rgb565(230, 230, 230));
    display.fillRect(static_cast<int16_t>(flag_screen_x + 4), static_cast<int16_t>(94 + world_y_offset), 26, 18,
                     Display::rgb565(220, 48, 48));

    const int16_t player_screen_x = static_cast<int16_t>(player_x_ - camera_x);
    const int16_t player_screen_y = static_cast<int16_t>(player_y_ + world_y_offset);
    display.fillRect(player_screen_x, player_screen_y, PlayerWidth, PlayerHeight, Display::rgb565(210, 48, 48));
    display.fillRect(player_screen_x, static_cast<int16_t>(player_screen_y + 10), PlayerWidth, 6,
                     Display::rgb565(30, 90, 210));
    display.fillRect(static_cast<int16_t>(player_screen_x + 3), static_cast<int16_t>(player_screen_y - 4), 6, 5,
                     Display::rgb565(230, 40, 40));

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
    } else {
        display.drawText(12, static_cast<int16_t>(screen_height - 16), "A JUMP  B RUN  SELECT EXIT",
                         Display::rgb565(240, 240, 240), 1);
    }

    display.present();
}

bool PlumberManGame::exitRequested() const {
    return exit_requested_;
}

void PlumberManGame::clearExitRequest() {
    exit_requested_ = false;
}

void PlumberManGame::resetLevel() {
    player_x_ = 28;
    player_y_ = 196;
    velocity_y_ = 0;
    on_ground_ = false;
    won_ = false;
    exit_requested_ = false;
    coins_collected_ = 0;

    for (bool& collected : coin_collected_) {
        collected = false;
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
        }
    }
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
