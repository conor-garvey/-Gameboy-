#include "picoboy/menu_app.hpp"

#include <algorithm>
#include <cstdio>

#include "picoboy/avatar_art.hpp"

namespace picoboy {

namespace {

constexpr uint32_t boot_duration_ms = 2200;
constexpr uint32_t quiet_target_fps = 30;
constexpr uint32_t original_target_fps = 60;

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

Display::Color dangerColor() {
    return Display::rgb565(220, 82, 82);
}

bool timeReached(absolute_time_t deadline) {
    return absolute_time_diff_us(get_absolute_time(), deadline) <= 0;
}

AvatarId sanitizeAvatar(uint8_t avatar) {
    if (avatar >= static_cast<uint8_t>(AvatarId::Count)) {
        return AvatarId::Hero;
    }

    return static_cast<AvatarId>(avatar);
}

AvatarId cycleAvatar(AvatarId avatar, int delta) {
    constexpr int avatar_count = static_cast<int>(AvatarId::Count);
    int avatar_index = static_cast<int>(avatar);
    avatar_index = (avatar_index + delta) % avatar_count;

    if (avatar_index < 0) {
        avatar_index += avatar_count;
    }

    return static_cast<AvatarId>(avatar_index);
}

}  // namespace

MenuApp::MenuApp(Display& display, Lsm6dsl& imu)
    : display_(display),
      imu_(imu),
      audio_(),
      buttons_(),
      screen_(Screen::Boot),
      boot_started_(get_absolute_time()),
      profile_store_(),
      profiles_{},
      next_profile_number_(1),
      create_profile_avatar_(AvatarId::Hero),
      selected_profile_entry_(0),
      active_profile_index_(-1),
      selected_game_index_(0),
      selected_settings_item_(0),
      display_mode_(DisplayMode::Quiet),
      pending_delete_index_(-1),
      notice_until_(get_absolute_time()),
      notice_text_{},
      plumber_man_(audio_),
      sky_dodger_(imu_, audio_),
      noise_diagnostic_(display_, imu_, audio_) {}

void MenuApp::init() {
    audio_.init();
    applyDisplayMode(display_mode_);
    audio_.playEffect(SoundEffect::PowerOn);
    audio_.startBackgroundMusic();
    buttons_.init();
    boot_started_ = get_absolute_time();
    notice_text_[0] = '\0';
    pending_delete_index_ = -1;
    active_profile_index_ = -1;
    create_profile_avatar_ = AvatarId::Hero;
    loadProfiles();
    next_profile_number_ = nextReusableProfileNumber();
    selected_profile_entry_ = hasFreeProfileSlot() ? createEntryIndex() : 0;

    if (profileCount() > 0) {
        selected_profile_entry_ = 0;
    }
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

    case Screen::Settings:
        updateSettings();
        break;

    case Screen::NoiseDiagnostic:
        noise_diagnostic_.update();
        if (noise_diagnostic_.exitRequested()) {
            audio_.startBackgroundMusic();
            display_.backlightOn();
            screen_ = Screen::Settings;
            setNotice("BACK TO SETTINGS", 900);
        }
        break;

    case Screen::PlumberMan:
        plumber_man_.update(buttons_);
        updatePlumberStats();
        if (plumber_man_.exitRequested()) {
            plumber_man_.clearExitRequest();
            screen_ = Screen::GameSelect;
            setNotice("BACK TO GAMES", 900);
        }
        break;

    case Screen::SkyDodger:
        sky_dodger_.update(buttons_, display_);
        updateSkyDodgerStats();
        if (sky_dodger_.exitRequested()) {
            sky_dodger_.clearExitRequest();
            screen_ = Screen::GameSelect;
            setNotice("BACK TO GAMES", 900);
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

    case Screen::Settings:
        renderSettings();
        break;

    case Screen::NoiseDiagnostic:
        noise_diagnostic_.render();
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
        selected_profile_entry_ = profileCount() > 0 ? 0 : createEntryIndex();
        setNotice("CHOOSE A PROFILE", 1200);
    }
}

void MenuApp::updateProfileSelect() {
    const int last_entry = hasFreeProfileSlot()
        ? createEntryIndex()
        : static_cast<int>(profileCount()) - 1;
    const int clamped_last_entry = std::max(0, last_entry);

    selected_profile_entry_ = std::clamp(selected_profile_entry_, 0, clamped_last_entry);

    if (pending_delete_index_ >= 0) {
        if (buttons_.pressed(ButtonId::A) || buttons_.pressed(ButtonId::Start)) {
            deleteProfile(pending_delete_index_);
            pending_delete_index_ = -1;
            audio_.playEffect(SoundEffect::MenuMove);
            setNotice("PROFILE DELETED", 1200);
        } else if (buttons_.pressed(ButtonId::B) || buttons_.pressed(ButtonId::Select)) {
            pending_delete_index_ = -1;
            audio_.playEffect(SoundEffect::MenuMove);
            setNotice("DELETE CANCELED", 900);
        }

        return;
    }

    if (buttons_.pressed(ButtonId::Up) && selected_profile_entry_ > 0) {
        --selected_profile_entry_;
        audio_.playEffect(SoundEffect::MenuMove);
    }

    if (buttons_.pressed(ButtonId::Down) && selected_profile_entry_ < clamped_last_entry) {
        ++selected_profile_entry_;
        audio_.playEffect(SoundEffect::MenuMove);
    }

    if (buttons_.pressed(ButtonId::Left)) {
        cycleSelectedAvatar(-1);
        audio_.playEffect(SoundEffect::MenuMove);
    }

    if (buttons_.pressed(ButtonId::Right)) {
        cycleSelectedAvatar(1);
        audio_.playEffect(SoundEffect::MenuMove);
    }

    if (buttons_.pressed(ButtonId::B) && !selectedEntryIsCreateNew() && profileCount() > 0) {
        pending_delete_index_ = selected_profile_entry_;
        audio_.playEffect(SoundEffect::MenuMove);
        return;
    }

    if (buttons_.pressed(ButtonId::A) || buttons_.pressed(ButtonId::Start)) {
        if (selectedEntryIsCreateNew()) {
            createProfile();
            audio_.playEffect(SoundEffect::MenuMove);
            selected_game_index_ = 0;
            screen_ = Screen::GameSelect;
            setNotice("PROFILE CREATED", 1200);
            return;
        }

        if (selected_profile_entry_ < static_cast<int>(profileCount())) {
            active_profile_index_ = selected_profile_entry_;
            audio_.playEffect(SoundEffect::MenuMove);
            selected_game_index_ = 0;
            screen_ = Screen::GameSelect;
            setNotice(selectedProfileName(), 1200);
        }
    }
}

void MenuApp::updateGameSelect() {
    if (activeProfile() == nullptr) {
        screen_ = Screen::ProfileSelect;
        selected_profile_entry_ = profileCount() > 0 ? 0 : createEntryIndex();
        setNotice("CHOOSE A PROFILE", 1200);
        return;
    }

    if (buttons_.pressed(ButtonId::Up) && selected_game_index_ > 0) {
        --selected_game_index_;
        audio_.playEffect(SoundEffect::MenuMove);
    }

    if (buttons_.pressed(ButtonId::Down) && selected_game_index_ < 2) {
        ++selected_game_index_;
        audio_.playEffect(SoundEffect::MenuMove);
    }

    if (buttons_.pressed(ButtonId::B) || buttons_.pressed(ButtonId::Select)) {
        audio_.playEffect(SoundEffect::MenuMove);
        screen_ = Screen::ProfileSelect;
        selected_profile_entry_ = active_profile_index_ >= 0 ? active_profile_index_ : createEntryIndex();
        pending_delete_index_ = -1;
        setNotice("BACK TO PROFILES", 900);
        return;
    }

    if (buttons_.pressed(ButtonId::A) || buttons_.pressed(ButtonId::Start)) {
        const StoredProfile* profile = activeProfile();
        if (profile == nullptr) {
            return;
        }

        if (selected_game_index_ == 0) {
            plumber_man_.setAvatar(sanitizeAvatar(profile->avatar));
            audio_.playEffect(SoundEffect::MenuMove);
            plumber_man_.enter();
            screen_ = Screen::PlumberMan;
            return;
        }

        if (selected_game_index_ == 1) {
            if (!imu_.ready()) {
                setNotice("IMU NOT FOUND", 1200);
                return;
            }

            sky_dodger_.setAvatar(sanitizeAvatar(profile->avatar));
            audio_.playEffect(SoundEffect::MenuMove);
            sky_dodger_.enter(display_);
            screen_ = Screen::SkyDodger;
            return;
        }

        audio_.playEffect(SoundEffect::MenuMove);
        selected_settings_item_ = 0;
        screen_ = Screen::Settings;
        setNotice("SYSTEM SETTINGS", 1200);
    }
}

void MenuApp::updateSettings() {
    constexpr int settings_item_count = 2;

    if (buttons_.pressed(ButtonId::Up) && selected_settings_item_ > 0) {
        --selected_settings_item_;
        audio_.playEffect(SoundEffect::MenuMove);
    }

    if (buttons_.pressed(ButtonId::Down) && selected_settings_item_ < settings_item_count - 1) {
        ++selected_settings_item_;
        audio_.playEffect(SoundEffect::MenuMove);
    }

    if (buttons_.pressed(ButtonId::B) || buttons_.pressed(ButtonId::Select)) {
        audio_.playEffect(SoundEffect::MenuMove);
        screen_ = Screen::GameSelect;
        setNotice("BACK TO GAMES", 900);
        return;
    }

    if (selected_settings_item_ == 0) {
        if (buttons_.pressed(ButtonId::Left) || buttons_.pressed(ButtonId::Right) ||
            buttons_.pressed(ButtonId::A) || buttons_.pressed(ButtonId::Start)) {
            display_mode_ = display_mode_ == DisplayMode::Quiet
                ? DisplayMode::Original
                : DisplayMode::Quiet;
            applyDisplayMode(display_mode_);
            audio_.playEffect(SoundEffect::MenuMove);
            setNotice(display_mode_ == DisplayMode::Quiet ? "QUIET DISPLAY MODE" : "ORIGINAL DISPLAY MODE", 1200);
        }
        return;
    }

    if (buttons_.pressed(ButtonId::A) || buttons_.pressed(ButtonId::Start)) {
        audio_.playEffect(SoundEffect::MenuMove);
        noise_diagnostic_.init();
        screen_ = Screen::NoiseDiagnostic;
    }
}

void MenuApp::updatePlumberStats() {
    StoredProfile* profile = activeProfile();
    if (profile == nullptr) {
        return;
    }

    uint32_t completion_time_ms = 0;
    uint8_t coins_collected = 0;
    if (!plumber_man_.consumeCompletedRun(completion_time_ms, coins_collected)) {
        return;
    }

    profile->stats.plumber_total_coins += coins_collected;
    if (profile->stats.plumber_best_time_ms == 0 || completion_time_ms < profile->stats.plumber_best_time_ms) {
        profile->stats.plumber_best_time_ms = completion_time_ms;
    }

    saveProfiles();
}

void MenuApp::updateSkyDodgerStats() {
    StoredProfile* profile = activeProfile();
    if (profile == nullptr) {
        return;
    }

    uint32_t distance = 0;
    if (!sky_dodger_.consumeFinishedRun(distance)) {
        return;
    }

    if (distance > profile->stats.sky_best_distance) {
        profile->stats.sky_best_distance = distance;
        saveProfiles();
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

    display_.fillRect(0, 0, screen_width, 30, backgroundTop());
    drawCenteredText(8, "SELECT PROFILE", Display::rgb565(255, 255, 255), 2);

    int16_t y = 38;
    for (size_t index = 0; index < profileCount(); ++index) {
        drawProfileItem(y, &profiles_[index], selected_profile_entry_ == static_cast<int>(index), false);
        y = static_cast<int16_t>(y + 26);
    }

    if (hasFreeProfileSlot()) {
        drawProfileItem(y, nullptr, selectedEntryIsCreateNew(), true);
    }

    if (notice_text_[0] != '\0' && !timeReached(notice_until_)) {
        drawCenteredText(170, notice_text_.data(), Display::rgb565(160, 220, 255), 1);
    }

    if (pending_delete_index_ >= 0 && pending_delete_index_ < static_cast<int>(profileCount())) {
        const int16_t box_x = 28;
        const int16_t box_y = 172;
        const int16_t box_width = static_cast<int16_t>(screen_width - 56);
        const int16_t box_height = 40;

        display_.fillRect(box_x, box_y, box_width, box_height, panelColor());
        display_.drawRect(box_x, box_y, box_width, box_height, dangerColor());
        drawCenteredText(178, "DELETE PROFILE?", Display::rgb565(255, 255, 255), 1);
        drawCenteredText(190, profiles_[pending_delete_index_].name, accentColor(), 1);
        drawCenteredText(202, "A YES   B OR SELECT NO", Display::rgb565(220, 220, 220), 1);
    } else if (selectedEntryIsCreateNew()) {
        drawCenteredText(182, "NEW PROFILE", Display::rgb565(255, 255, 255), 1);
        drawCenteredText(194, "LEFT RIGHT = AVATAR", Display::rgb565(180, 180, 180), 1);
        drawCenteredText(206, "A START = CREATE", Display::rgb565(180, 180, 180), 1);
    } else {
        const StoredProfile* profile = selectedProfile();
        if (profile != nullptr) {
            char coins_text[20];
            char time_text[20];
            char sky_text[24];
            std::snprintf(coins_text, sizeof(coins_text), "COINS %lu",
                          static_cast<unsigned long>(profile->stats.plumber_total_coins));
            formatBestTime(time_text, sizeof(time_text), profile->stats.plumber_best_time_ms);
            std::snprintf(sky_text, sizeof(sky_text), "SKY BEST %lu",
                          static_cast<unsigned long>(profile->stats.sky_best_distance));

            display_.drawText(12, 180, coins_text, Display::rgb565(255, 226, 112), 1);
            display_.drawText(120, 180, time_text, Display::rgb565(160, 220, 255), 1);
            display_.drawText(12, 194, sky_text, Display::rgb565(140, 220, 180), 1);
            drawCenteredText(206, "A PLAY  L/R AVATAR  B DELETE", Display::rgb565(180, 180, 180), 1);
        }
    }

    display_.present();
}

void MenuApp::renderGameSelect() {
    const int16_t screen_width = static_cast<int16_t>(display_.width());
    const int16_t screen_height = static_cast<int16_t>(display_.height());
    const StoredProfile* profile = activeProfile();

    display_.beginFrame(backgroundBottom());

    display_.fillRect(0, 0, screen_width, 30, backgroundTop());
    drawCenteredText(8, "GAME SELECT", Display::rgb565(255, 255, 255), 2);

    if (profile != nullptr) {
        drawAvatarPanel(18, 46, sanitizeAvatar(profile->avatar), true);
        display_.drawText(62, 50, profile->name, accentColor(), 2);

        char line_one[24];
        char line_two[24];
        if (selected_game_index_ == 0) {
            std::snprintf(line_one, sizeof(line_one), "COINS %lu",
                          static_cast<unsigned long>(profile->stats.plumber_total_coins));
            formatBestTime(line_two, sizeof(line_two), profile->stats.plumber_best_time_ms);
        } else if (selected_game_index_ == 1) {
            std::snprintf(line_one, sizeof(line_one), "SKY BEST %lu",
                          static_cast<unsigned long>(profile->stats.sky_best_distance));
            std::snprintf(line_two, sizeof(line_two), "%s",
                          imu_.ready() ? "TILT TO DODGE" : "IMU REQUIRED");
        } else {
            std::snprintf(line_one, sizeof(line_one), "DISPLAY %s", displayModeName());
            std::snprintf(line_two, sizeof(line_two), "SPI %luM  FPS %lu",
                          static_cast<unsigned long>(display_.spiClockHz() / 1000000u),
                          static_cast<unsigned long>(display_.targetFps()));
        }

        display_.drawText(62, 74, line_one, Display::rgb565(255, 255, 255), 1);
        display_.drawText(62, 88, line_two, Display::rgb565(170, 220, 255), 1);
    }

    if (notice_text_[0] != '\0' && !timeReached(notice_until_)) {
        drawCenteredText(96, notice_text_.data(), Display::rgb565(255, 220, 140), 1);
    } else if (selected_game_index_ == 0) {
        drawCenteredText(96, "COLLECT COINS, CHASE A FAST TIME", Display::rgb565(140, 220, 180), 1);
    } else if (selected_game_index_ == 1 && imu_.ready()) {
        drawCenteredText(96, "DODGE BIRDS AND SILLY BALLOONS", Display::rgb565(140, 220, 180), 1);
    } else if (selected_game_index_ == 1) {
        drawCenteredText(96, "IMU REQUIRED FOR SKY DODGER", Display::rgb565(140, 180, 220), 1);
    } else {
        drawCenteredText(96, "SET DISPLAY SPEED AND RUN NOISE TESTS", Display::rgb565(140, 220, 180), 1);
    }

    drawMenuItem(110, "PLUMBER MAN", selected_game_index_ == 0, Display::rgb565(255, 255, 255));
    drawMenuItem(140, "SKY DODGER", selected_game_index_ == 1, Display::rgb565(255, 255, 255));
    drawMenuItem(170, "SETTINGS", selected_game_index_ == 2, Display::rgb565(255, 255, 255));

    display_.drawText(18, static_cast<int16_t>(screen_height - 24),
                      selected_game_index_ == 2 ? "A START = OPEN" : "A START = PLAY",
                      Display::rgb565(200, 200, 200), 1);
    display_.drawText(18, static_cast<int16_t>(screen_height - 12), "B SELECT = PROFILES", Display::rgb565(200, 200, 200), 1);

    display_.present();
}

void MenuApp::renderSettings() {
    const int16_t screen_width = static_cast<int16_t>(display_.width());

    display_.beginFrame(backgroundBottom());
    display_.fillRect(0, 0, screen_width, 30, backgroundTop());
    drawCenteredText(8, "SETTINGS", Display::rgb565(255, 255, 255), 2);

    const int16_t item_x = 18;
    const int16_t item_width = static_cast<int16_t>(screen_width - 36);
    const int16_t first_y = 52;
    const int16_t gap = 38;

    for (int item = 0; item < 2; ++item) {
        const int16_t y = static_cast<int16_t>(first_y + item * gap);
        const bool selected = selected_settings_item_ == item;
        const Display::Color fill = selected ? selectedColor() : panelColor();
        const Display::Color border = selected ? accentColor() : Display::rgb565(70, 90, 130);
        display_.fillRect(item_x, y, item_width, 28, fill);
        display_.drawRect(item_x, y, item_width, 28, border);

        if (item == 0) {
            display_.drawText(static_cast<int16_t>(item_x + 12), static_cast<int16_t>(y + 9),
                              "DISPLAY MODE", Display::rgb565(255, 255, 255), 1);
            const char* mode_text = displayModeName();
            const int16_t mode_x = static_cast<int16_t>(screen_width - display_.measureTextWidth(mode_text, 1) - 30);
            display_.drawText(mode_x, static_cast<int16_t>(y + 9), mode_text, accentColor(), 1);
        } else {
            display_.drawText(static_cast<int16_t>(item_x + 12), static_cast<int16_t>(y + 9),
                              "NOISE DIAGNOSTIC", Display::rgb565(255, 255, 255), 1);
            display_.drawText(static_cast<int16_t>(screen_width - 54), static_cast<int16_t>(y + 9),
                              "RUN", accentColor(), 1);
        }
    }

    char config_text[40];
    std::snprintf(config_text, sizeof(config_text), "SPI %lu MHz   FPS %lu",
                  static_cast<unsigned long>(display_.spiClockHz() / 1000000u),
                  static_cast<unsigned long>(display_.targetFps()));
    drawCenteredText(138, config_text, Display::rgb565(160, 220, 255), 1);
    drawCenteredText(154, "QUIET = 10MHZ / 30FPS", Display::rgb565(220, 220, 220), 1);
    drawCenteredText(166, "ORIGINAL = 20MHZ / 60FPS", Display::rgb565(220, 220, 220), 1);

    if (notice_text_[0] != '\0' && !timeReached(notice_until_)) {
        drawCenteredText(184, notice_text_.data(), Display::rgb565(255, 220, 140), 1);
    } else if (selected_settings_item_ == 0) {
        drawCenteredText(184, "LEFT/RIGHT/A TO SWITCH DISPLAY MODE", Display::rgb565(255, 220, 140), 1);
    } else {
        drawCenteredText(184, "RUN THE DIAGNOSTIC LISTEN TEST", Display::rgb565(255, 220, 140), 1);
    }

    drawCenteredText(204, "B OR SELECT = BACK", Display::rgb565(200, 200, 200), 1);
    display_.present();
}

void MenuApp::loadProfiles() {
    profile_store_.load(profiles_, next_profile_number_);
}

void MenuApp::saveProfiles() {
    profile_store_.save(profiles_, next_profile_number_);
}

void MenuApp::createProfile() {
    const size_t count = profileCount();
    if (count >= profiles_.size()) {
        return;
    }

    StoredProfile& profile = profiles_[count];
    profile = StoredProfile{};
    profile.in_use = 1;
    profile.avatar = static_cast<uint8_t>(create_profile_avatar_);
    const uint8_t profile_number = nextReusableProfileNumber();
    std::snprintf(profile.name, sizeof(profile.name), "PLAYER %u", profile_number);
    next_profile_number_ = nextReusableProfileNumber();

    active_profile_index_ = static_cast<int>(count);
    selected_profile_entry_ = active_profile_index_;
    saveProfiles();
}

void MenuApp::deleteProfile(int profile_index) {
    if (profile_index < 0 || profile_index >= static_cast<int>(profileCount())) {
        return;
    }

    for (size_t index = static_cast<size_t>(profile_index); index + 1 < profiles_.size(); ++index) {
        profiles_[index] = profiles_[index + 1];
    }
    profiles_.back() = StoredProfile{};

    if (active_profile_index_ == profile_index) {
        active_profile_index_ = -1;
    } else if (active_profile_index_ > profile_index) {
        --active_profile_index_;
    }

    const int last_entry = hasFreeProfileSlot()
        ? createEntryIndex()
        : static_cast<int>(profileCount()) - 1;
    selected_profile_entry_ = std::clamp(selected_profile_entry_, 0, std::max(0, last_entry));
    next_profile_number_ = nextReusableProfileNumber();
    saveProfiles();
}

void MenuApp::setNotice(const char* text, uint32_t duration_ms) {
    if (text == nullptr) {
        notice_text_[0] = '\0';
    } else {
        std::snprintf(notice_text_.data(), notice_text_.size(), "%s", text);
    }

    notice_until_ = delayed_by_ms(get_absolute_time(), duration_ms);
}

void MenuApp::cycleSelectedAvatar(int delta) {
    if (delta == 0) {
        return;
    }

    if (selectedEntryIsCreateNew()) {
        create_profile_avatar_ = cycleAvatar(create_profile_avatar_, delta);
        return;
    }

    StoredProfile* profile = selectedProfile();
    if (profile == nullptr) {
        return;
    }

    profile->avatar = static_cast<uint8_t>(cycleAvatar(sanitizeAvatar(profile->avatar), delta));
    saveProfiles();
}

uint8_t MenuApp::nextReusableProfileNumber() const {
    std::array<bool, ProfileStore::MaxProfiles + 1> used_numbers{};

    for (const StoredProfile& profile : profiles_) {
        if (profile.in_use == 0) {
            continue;
        }

        unsigned number = 0;
        if (std::sscanf(profile.name, "PLAYER %u", &number) == 1 &&
            number > 0 && number <= ProfileStore::MaxProfiles) {
            used_numbers[number] = true;
        }
    }

    for (uint8_t number = 1; number <= ProfileStore::MaxProfiles; ++number) {
        if (!used_numbers[number]) {
            return number;
        }
    }

    return static_cast<uint8_t>(ProfileStore::MaxProfiles);
}

AvatarId MenuApp::selectedEntryAvatar() const {
    if (selectedEntryIsCreateNew()) {
        return create_profile_avatar_;
    }

    const StoredProfile* profile = selectedProfile();
    return profile == nullptr ? AvatarId::Hero : sanitizeAvatar(profile->avatar);
}

StoredProfile* MenuApp::selectedProfile() {
    if (selected_profile_entry_ < 0 || selected_profile_entry_ >= static_cast<int>(profileCount())) {
        return nullptr;
    }

    return &profiles_[selected_profile_entry_];
}

const StoredProfile* MenuApp::selectedProfile() const {
    if (selected_profile_entry_ < 0 || selected_profile_entry_ >= static_cast<int>(profileCount())) {
        return nullptr;
    }

    return &profiles_[selected_profile_entry_];
}

StoredProfile* MenuApp::activeProfile() {
    if (active_profile_index_ < 0 || active_profile_index_ >= static_cast<int>(profileCount())) {
        return nullptr;
    }

    return &profiles_[active_profile_index_];
}

const StoredProfile* MenuApp::activeProfile() const {
    if (active_profile_index_ < 0 || active_profile_index_ >= static_cast<int>(profileCount())) {
        return nullptr;
    }

    return &profiles_[active_profile_index_];
}

bool MenuApp::hasFreeProfileSlot() const {
    return profileCount() < profiles_.size();
}

size_t MenuApp::profileCount() const {
    size_t count = 0;

    for (const StoredProfile& profile : profiles_) {
        if (profile.in_use != 0) {
            ++count;
        }
    }

    return count;
}

int MenuApp::createEntryIndex() const {
    return static_cast<int>(profileCount());
}

bool MenuApp::selectedEntryIsCreateNew() const {
    return hasFreeProfileSlot() && selected_profile_entry_ == createEntryIndex();
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

void MenuApp::drawProfileItem(int16_t y, const StoredProfile* profile, bool selected, bool create_new) {
    const int16_t box_x = 16;
    const int16_t box_width = static_cast<int16_t>(display_.width() - 32);
    const int16_t box_height = 24;
    const Display::Color fill = selected ? selectedColor() : panelColor();
    const Display::Color border = selected ? accentColor() : Display::rgb565(70, 90, 130);
    const Display::Color text_color = selected ? Display::rgb565(255, 255, 255)
                                               : (create_new ? accentColor() : Display::rgb565(240, 240, 240));

    display_.fillRect(box_x, y - 4, box_width, box_height, fill);
    display_.drawRect(box_x, y - 4, box_width, box_height, border);
    drawAvatar(static_cast<int16_t>(box_x + 8), static_cast<int16_t>(y - 2), create_new
                   ? create_profile_avatar_
                   : (profile == nullptr ? AvatarId::Hero : sanitizeAvatar(profile->avatar)),
               1, fill);

    if (create_new) {
        display_.drawText(static_cast<int16_t>(box_x + 36), y + 1, "CREATE NEW", text_color, 1);
        return;
    }

    if (profile != nullptr) {
        display_.drawText(static_cast<int16_t>(box_x + 36), y + 1, profile->name, text_color, 1);
    }
}

void MenuApp::drawAvatar(int16_t x, int16_t y, AvatarId avatar, uint8_t scale, Display::Color background) {
    drawAvatarBadge(display_, x, y, avatar, scale, background);
}

void MenuApp::drawAvatarPanel(int16_t x, int16_t y, AvatarId avatar, bool selected) {
    const int16_t panel_size = 34;
    const Display::Color fill = selected ? Display::rgb565(28, 50, 94) : panelColor();
    const Display::Color border = selected ? accentColor() : Display::rgb565(70, 90, 130);

    display_.fillRect(x, y, panel_size, panel_size, fill);
    display_.drawRect(x, y, panel_size, panel_size, border);
    drawAvatar(static_cast<int16_t>(x + 8), static_cast<int16_t>(y + 6), avatar, 2, fill);
}

void MenuApp::formatBestTime(char* buffer, size_t buffer_size, uint32_t time_ms) const {
    if (buffer == nullptr || buffer_size == 0) {
        return;
    }

    if (time_ms == 0) {
        std::snprintf(buffer, buffer_size, "BEST --");
        return;
    }

    const uint32_t total_seconds = time_ms / 1000;
    const uint32_t minutes = total_seconds / 60;
    const uint32_t seconds = total_seconds % 60;
    const uint32_t tenths = (time_ms % 1000) / 100;

    if (minutes > 0) {
        std::snprintf(buffer, buffer_size, "BEST %lu:%02lu.%1lu",
                      static_cast<unsigned long>(minutes),
                      static_cast<unsigned long>(seconds),
                      static_cast<unsigned long>(tenths));
    } else {
        std::snprintf(buffer, buffer_size, "BEST %lu.%1lus",
                      static_cast<unsigned long>(seconds),
                      static_cast<unsigned long>(tenths));
    }
}

const char* MenuApp::selectedProfileName() const {
    const StoredProfile* profile = activeProfile();
    if (profile == nullptr) {
        return "NO PROFILE";
    }

    return profile->name;
}

void MenuApp::applyDisplayMode(DisplayMode mode) {
    display_mode_ = mode;
    if (display_mode_ == DisplayMode::Quiet) {
        display_.setSpiClockHz(Display::SpiClockHz);
        display_.setTargetFps(quiet_target_fps);
    } else {
        display_.setSpiClockHz(Display::OriginalSpiClockHz);
        display_.setTargetFps(original_target_fps);
    }
}

const char* MenuApp::displayModeName() const {
    return display_mode_ == DisplayMode::Quiet ? "QUIET" : "ORIGINAL";
}

}  // namespace picoboy
