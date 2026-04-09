#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "picoboy/audio_engine.hpp"
#include "picoboy/buttons.hpp"
#include "picoboy/display.hpp"
#include "picoboy/lsm6dsl.hpp"
#include "picoboy/noise_diagnostic_app.hpp"
#include "picoboy/plumber_man_game.hpp"
#include "picoboy/profile_store.hpp"
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
    static constexpr size_t NoticeTextSize = 32;

    enum class Screen : uint8_t {
        Boot,
        ProfileSelect,
        GameSelect,
        Settings,
        NoiseDiagnostic,
        PlumberMan,
        SkyDodger,
    };

    enum class DisplayMode : uint8_t {
        Quiet,
        Original,
    };

    void updateBoot();
    void updateProfileSelect();
    void updateGameSelect();
    void updateSettings();
    void updatePlumberStats();
    void updateSkyDodgerStats();

    void renderBoot();
    void renderProfileSelect();
    void renderGameSelect();
    void renderSettings();

    void loadProfiles();
    void saveProfiles();
    void createProfile();
    void deleteProfile(int profile_index);
    void setNotice(const char* text, uint32_t duration_ms);
    void cycleSelectedAvatar(int delta);
    uint8_t nextReusableProfileNumber() const;
    AvatarId selectedEntryAvatar() const;
    StoredProfile* selectedProfile();
    const StoredProfile* selectedProfile() const;
    StoredProfile* activeProfile();
    const StoredProfile* activeProfile() const;
    bool hasFreeProfileSlot() const;
    size_t profileCount() const;
    int createEntryIndex() const;
    bool selectedEntryIsCreateNew() const;

    void drawCenteredText(int16_t y, const char* text, Display::Color color, uint8_t scale = 1);
    void drawMenuItem(int16_t y, const char* text, bool selected, Display::Color text_color);
    void drawProfileItem(int16_t y, const StoredProfile* profile, bool selected, bool create_new);
    void drawAvatar(int16_t x, int16_t y, AvatarId avatar, uint8_t scale, Display::Color background);
    void drawAvatarPanel(int16_t x, int16_t y, AvatarId avatar, bool selected);
    void formatBestTime(char* buffer, size_t buffer_size, uint32_t time_ms) const;
    const char* selectedProfileName() const;
    void applyDisplayMode(DisplayMode mode);
    void setAudioEnabled(bool enabled);
    const char* displayModeName() const;
    const char* audioSettingName() const;

    Display& display_;
    Lsm6dsl& imu_;
    AudioEngine audio_;
    Buttons buttons_;
    Screen screen_;
    absolute_time_t boot_started_;
    ProfileStore profile_store_;
    std::array<StoredProfile, ProfileStore::MaxProfiles> profiles_;
    uint8_t next_profile_number_;
    AvatarId create_profile_avatar_;
    int selected_profile_entry_;
    int active_profile_index_;
    int selected_game_index_;
    int selected_settings_item_;
    DisplayMode display_mode_;
    bool audio_enabled_;
    bool boot_sound_played_ = false;
    int pending_delete_index_;
    absolute_time_t notice_until_;
    std::array<char, NoticeTextSize> notice_text_;
    PlumberManGame plumber_man_;
    SkyDodgerGame sky_dodger_;
    NoiseDiagnosticApp noise_diagnostic_;
};

}  // namespace picoboy
