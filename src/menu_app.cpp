#include "picoboy/menu_app.hpp"

#include <cstdio>

namespace picoboy {

namespace {

constexpr uint32_t boot_duration_ms = 2200;

Display::Color backgroundTop() {
    return Display::rgb565(10, 16, 40);
}

Display::Color backgroundBottom() {
    return Display::rgb565(4, 8, 20);
}

Display::Color accentColor() {
    return Display::rgb565(255, 196, 48);
}

Display::Color panelColor() {
    return Display::rgb565(18, 28, 58);
}

Display::Color selectedColor() {
    return Display::rgb565(54, 120, 200);
}

bool timeReached(absolute_time_t deadline) {
    return absolute_time_diff_us(get_absolute_time(), deadline) <= 0;
}

}

MenuApp::MenuApp(Display& display, Lsm6dsl& imu)
    : display_(display),
      imu_(imu),
      buttons_(),
      screen_(Screen::Boot),
      boot_started_(get_absolute_time()),
      profiles_{},
      next_profile_number_(1),
      selected_profile_entry_(0),
      active_profile_index_(-1),
      selected_game_index_(0),
      notice_until_(get_absolute_time()),
      notice_text_{},
      plumber_man_(),
      sky_dodger_(imu_) {}

void MenuApp::init() {
    buttons_.init();
    boot_started_ = get_absolute_time();

    for (Profile& profile : profiles_) {
        profile.in_use = false;
        profile.name[0] = '\0';
    }

    notice_text_[0] = '\0';
}

void MenuApp::update() {
    buttons_.update();

    switch (screen_) {
    case Screen::Boot:
        updateBoot();
        break;

    case Screen::ProfileSelect:
        updateProfileSelect();
        break;

    case Screen::GameSelect:
        updateGameSelect();
        break;

    case Screen::PlumberMan:
        plumber_man_.update(buttons_);
        if (plumber_man_.exitRequested()) {
            plumber_man_.clearExitRequest();
            screen_ = Screen::GameSelect;
            setNotice("BACK TO GAME MENU", 900);
        }
        break;

    case Screen::SkyDodger:
        sky_dodger_.update(buttons_, display_);
        if (sky_dodger_.exitRequested()) {
            sky_dodger_.clearExitRequest();
            screen_ = Screen::GameSelect;
            setNotice("BACK TO GAME MENU", 900);
        }
        break;
    }
}

void MenuApp::render() {
    switch (screen_) {
    case Screen::Boot:
        renderBoot();
        break;

    case Screen::ProfileSelect:
        renderProfileSelect();
        break;

    case Screen::GameSelect:
        renderGameSelect();
        break;

    case Screen::PlumberMan:
        plumber_man_.render(display_);
        break;

    case Screen::SkyDodger:
        sky_dodger_.render(display_);
        break;
    }
}

void MenuApp::updateBoot() {
    const int64_t elapsed_ms = absolute_time_diff_us(boot_started_, get_absolute_time()) / 1000;

    if (elapsed_ms >= static_cast<int64_t>(boot_duration_ms) ||
        buttons_.pressed(ButtonId::A) ||
        buttons_.pressed(ButtonId::Start)) {
        screen_ = Screen::ProfileSelect;
        selected_profile_entry_ = 0;
        setNotice("CHOOSE A PROFILE", 1200);
    }
}

void MenuApp::updateProfileSelect() {
    const int last_entry = hasFreeProfileSlot()
        ? createEntryIndex()
        : static_cast<int>(profileCount()) - 1;

    if (selected_profile_entry_ > last_entry) {
        selected_profile_entry_ = last_entry;
    }

    if (buttons_.pressed(ButtonId::Up) && selected_profile_entry_ > 0) {
        --selected_profile_entry_;
    }

    if (buttons_.pressed(ButtonId::Down) && selected_profile_entry_ < last_entry) {
        ++selected_profile_entry_;
    }

    if (buttons_.pressed(ButtonId::A) || buttons_.pressed(ButtonId::Start)) {
        if (hasFreeProfileSlot() && selected_profile_entry_ == createEntryIndex()) {
            createProfile();
            selected_game_index_ = 0;
            screen_ = Screen::GameSelect;
            setNotice("PROFILE CREATED", 1200);
            return;
        }

        if (selected_profile_entry_ < static_cast<int>(profileCount())) {
            active_profile_index_ = selected_profile_entry_;
            selected_game_index_ = 0;
            screen_ = Screen::GameSelect;
            setNotice(selectedProfileName(), 1200);
        }
    }
}

void MenuApp::updateGameSelect() {
    if (buttons_.pressed(ButtonId::Up) && selected_game_index_ > 0) {
        --selected_game_index_;
    }

    if (buttons_.pressed(ButtonId::Down) && selected_game_index_ < 1) {
        ++selected_game_index_;
    }

    if (buttons_.pressed(ButtonId::B) || buttons_.pressed(ButtonId::Select)) {
        screen_ = Screen::ProfileSelect;
        selected_profile_entry_ = active_profile_index_ >= 0 ? active_profile_index_ : 0;
        setNotice("BACK TO PROFILES", 900);
        return;
    }

    if (buttons_.pressed(ButtonId::A) || buttons_.pressed(ButtonId::Start)) {
        if (selected_game_index_ == 0) {
            plumber_man_.enter();
            screen_ = Screen::PlumberMan;
            return;
        }

        if (!imu_.ready()) {
            setNotice("IMU NOT FOUND", 1200);
            return;
        }

        sky_dodger_.enter(display_);
        screen_ = Screen::SkyDodger;
    }
}

void MenuApp::renderBoot() {
    const int16_t screen_width = static_cast<int16_t>(display_.width());
    const int16_t screen_height = static_cast<int16_t>(display_.height());
    const int16_t top_height = static_cast<int16_t>(screen_height / 3);
    const int16_t middle_height = static_cast<int16_t>(screen_height / 3);
    const int16_t bottom_y = static_cast<int16_t>(top_height + middle_height);

    display_.beginFrame(backgroundBottom());

    display_.fillRect(0, 0, screen_width, top_height, backgroundTop());
    display_.fillRect(0, top_height, screen_width, middle_height, panelColor());
    display_.fillRect(0, bottom_y, screen_width, static_cast<int16_t>(screen_height - bottom_y), backgroundBottom());

    const int16_t frame_x = 22;
    const int16_t frame_y = 34;
    const int16_t frame_width = static_cast<int16_t>(screen_width - 44);
    const int16_t frame_height = static_cast<int16_t>(screen_height - 68);

    display_.drawRect(frame_x, frame_y, frame_width, frame_height, accentColor());
    drawCenteredText(static_cast<int16_t>(screen_height / 2 - 50), "PICOBOY", Display::rgb565(255, 255, 255), 4);
    drawCenteredText(static_cast<int16_t>(screen_height / 2 + 5), "POWERED BY PICO W", Display::rgb565(160, 220, 255), 1);
    drawCenteredText(static_cast<int16_t>(screen_height / 2 + 30), "HANDHELD FUN IN THE MAKING", Display::rgb565(210, 210, 210), 1);
    drawCenteredText(static_cast<int16_t>(screen_height - 52), "PRESS START", accentColor(), 1);

    display_.present();
}

void MenuApp::renderProfileSelect() {
    const int16_t screen_width = static_cast<int16_t>(display_.width());
    const int16_t screen_height = static_cast<int16_t>(display_.height());

    display_.beginFrame(backgroundBottom());

    display_.fillRect(0, 0, screen_width, 32, backgroundTop());
    drawCenteredText(10, "SELECT PROFILE", Display::rgb565(255, 255, 255), 2);

    int item_index = 0;
    int16_t y = 54;
    for (const Profile& profile : profiles_) {
        if (!profile.in_use) {
            continue;
        }

        drawMenuItem(y, profile.name, selected_profile_entry_ == item_index, Display::rgb565(255, 255, 255));
        y = static_cast<int16_t>(y + 38);
        ++item_index;
    }

    if (hasFreeProfileSlot()) {
        drawMenuItem(y, "CREATE NEW", selected_profile_entry_ == item_index, accentColor());
    }

    drawCenteredText(static_cast<int16_t>(screen_height - 30), "UP DOWN = MOVE", Display::rgb565(180, 180, 180), 1);
    drawCenteredText(static_cast<int16_t>(screen_height - 16), "A OR START = CHOOSE", Display::rgb565(180, 180, 180), 1);

    if (notice_text_[0] != '\0' && !timeReached(notice_until_)) {
        drawCenteredText(static_cast<int16_t>(screen_height - 46), notice_text_.data(), Display::rgb565(160, 220, 255), 1);
    }

    display_.present();
}

void MenuApp::renderGameSelect() {
    const int16_t screen_width = static_cast<int16_t>(display_.width());
    const int16_t screen_height = static_cast<int16_t>(display_.height());

    display_.beginFrame(backgroundBottom());

    display_.fillRect(0, 0, screen_width, 30, backgroundTop());
    drawCenteredText(8, "GAME SELECT", Display::rgb565(255, 255, 255), 2);

    display_.drawText(16, 40, "PROFILE", Display::rgb565(180, 180, 180), 1);
    display_.drawText(16, 54, selectedProfileName(), accentColor(), 2);

    drawMenuItem(100, "PLUMBER MAN", selected_game_index_ == 0, Display::rgb565(255, 255, 255));
    drawMenuItem(144, "SKY DODGER", selected_game_index_ == 1, Display::rgb565(255, 255, 255));

    display_.drawText(20, static_cast<int16_t>(screen_height - 50), "A START = SELECT", Display::rgb565(200, 200, 200), 1);
    display_.drawText(20, static_cast<int16_t>(screen_height - 34), "B SELECT = BACK", Display::rgb565(200, 200, 200), 1);

    if (notice_text_[0] != '\0' && !timeReached(notice_until_)) {
        drawCenteredText(static_cast<int16_t>(screen_height - 16), notice_text_.data(), Display::rgb565(255, 220, 140), 1);
    } else if (selected_game_index_ == 0) {
        drawCenteredText(static_cast<int16_t>(screen_height - 16), "PLUMBER MAN READY TO PLAY", Display::rgb565(140, 220, 180), 1);
    } else if (imu_.ready()) {
        drawCenteredText(static_cast<int16_t>(screen_height - 16), "CALIBRATES, THEN TILT TO PLAY", Display::rgb565(140, 220, 180), 1);
    } else {
        drawCenteredText(static_cast<int16_t>(screen_height - 16), "IMU REQUIRED FOR SKY DODGER", Display::rgb565(140, 180, 220), 1);
    }

    display_.present();
}

void MenuApp::createProfile() {
    for (size_t index = 0; index < profiles_.size(); ++index) {
        if (profiles_[index].in_use) {
            continue;
        }

        profiles_[index].in_use = true;
        std::snprintf(profiles_[index].name, sizeof(profiles_[index].name), "PLAYER %u", next_profile_number_);
        ++next_profile_number_;
        active_profile_index_ = static_cast<int>(index);
        selected_profile_entry_ = active_profile_index_;
        return;
    }
}

void MenuApp::setNotice(const char* text, uint32_t duration_ms) {
    if (text == nullptr) {
        notice_text_[0] = '\0';
    } else {
        std::snprintf(notice_text_.data(), notice_text_.size(), "%s", text);
    }

    notice_until_ = delayed_by_ms(get_absolute_time(), duration_ms);
}

bool MenuApp::hasFreeProfileSlot() const {
    return profileCount() < profiles_.size();
}

size_t MenuApp::profileCount() const {
    size_t count = 0;

    for (const Profile& profile : profiles_) {
        if (profile.in_use) {
            ++count;
        }
    }

    return count;
}

int MenuApp::createEntryIndex() const {
    return static_cast<int>(profileCount());
}

void MenuApp::drawCenteredText(int16_t y, const char* text, Display::Color color, uint8_t scale) {
    const int16_t text_width = display_.measureTextWidth(text, scale);
    const int16_t x = static_cast<int16_t>((static_cast<int16_t>(display_.width()) - text_width) / 2);
    display_.drawText(x < 0 ? 0 : x, y, text, color, scale);
}

void MenuApp::drawMenuItem(int16_t y, const char* text, bool selected, Display::Color text_color) {
    const int16_t box_x = 26;
    const int16_t box_width = static_cast<int16_t>(display_.width() - 52);
    const int16_t box_height = 28;

    if (selected) {
        display_.fillRect(box_x, y - 6, box_width, box_height, selectedColor());
        display_.drawRect(box_x, y - 6, box_width, box_height, accentColor());
    } else {
        display_.fillRect(box_x, y - 6, box_width, box_height, panelColor());
        display_.drawRect(box_x, y - 6, box_width, box_height, Display::rgb565(70, 90, 130));
    }

    const int16_t text_width = display_.measureTextWidth(text, 1);
    const int16_t text_x = static_cast<int16_t>((static_cast<int16_t>(display_.width()) - text_width) / 2);
    display_.drawText(text_x < 0 ? 0 : text_x, y + 2, text,
                      selected ? Display::rgb565(255, 255, 255) : text_color, 1);
}

const char* MenuApp::selectedProfileName() const {
    if (active_profile_index_ < 0 || active_profile_index_ >= static_cast<int>(profiles_.size())) {
        return "NO PROFILE";
    }

    return profiles_[active_profile_index_].name;
}

}  // namespace picoboy
