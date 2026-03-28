#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "picoboy/buttons.hpp"
#include "picoboy/display.hpp"
#include "picoboy/lsm6dsl.hpp"
#include "picoboy/plumber_man_game.hpp"
#include "picoboy/sky_dodger_game.hpp"
#include "pico/time.h"

namespace picoboy {

class MenuApp {
public:
    MenuApp(Display& display, Lsm6dsl& imu);

    void init();
    void update();
    void render();

private:
    static constexpr size_t MaxProfiles = 3;
    static constexpr size_t NoticeTextSize = 24;

    enum class Screen : uint8_t {
        Boot,
        ProfileSelect,
        GameSelect,
        PlumberMan,
        SkyDodger,
    };

    struct Profile {
        bool in_use;
        char name[16];
    };

    void updateBoot();
    void updateProfileSelect();
    void updateGameSelect();

    void renderBoot();
    void renderProfileSelect();
    void renderGameSelect();

    void createProfile();
    void setNotice(const char* text, uint32_t duration_ms);
    bool hasFreeProfileSlot() const;
    size_t profileCount() const;
    int createEntryIndex() const;

    void drawCenteredText(int16_t y, const char* text, Display::Color color, uint8_t scale = 1);
    void drawMenuItem(int16_t y, const char* text, bool selected, Display::Color text_color);
    const char* selectedProfileName() const;

    Display& display_;
    Lsm6dsl& imu_;
    Buttons buttons_;
    Screen screen_;
    absolute_time_t boot_started_;
    std::array<Profile, MaxProfiles> profiles_;
    uint8_t next_profile_number_;
    int selected_profile_entry_;
    int active_profile_index_;
    int selected_game_index_;
    absolute_time_t notice_until_;
    std::array<char, NoticeTextSize> notice_text_;
    PlumberManGame plumber_man_;
    SkyDodgerGame sky_dodger_;
};

}  // namespace picoboy
