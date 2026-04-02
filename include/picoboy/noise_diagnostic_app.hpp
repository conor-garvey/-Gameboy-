#pragma once

#include <cstdint>

#include "picoboy/audio_engine.hpp"
#include "picoboy/buttons.hpp"
#include "picoboy/display.hpp"
#include "picoboy/lsm6dsl.hpp"

namespace picoboy {

class NoiseDiagnosticApp {
public:
    NoiseDiagnosticApp(Display& display, Lsm6dsl& imu, AudioEngine& audio);

    void init();
    void update();
    void render();

    bool shouldRender() const;
    bool exitRequested() const;

private:
    enum class Item : uint8_t {
        ScreenLoad,
        Backlight,
        ImuPoll,
        Count,
    };

    void toggleSelectedItem();
    void setBacklightEnabled(bool enabled);
    void sampleImu();
    bool anyButtonHeld() const;
    void drawCenteredText(int16_t y, const char* text, Display::Color color, uint8_t scale = 1);
    const char* onOffText(bool enabled) const;

    Display& display_;
    Lsm6dsl& imu_;
    AudioEngine& audio_;
    Buttons buttons_;
    bool render_load_enabled_ = true;
    bool backlight_enabled_ = true;
    bool imu_poll_enabled_ = true;
    bool imu_sample_valid_ = false;
    bool needs_render_ = true;
    bool exit_requested_ = false;
    bool awaiting_button_release_ = true;
    uint8_t selected_item_ = 0;
    uint32_t frame_counter_ = 0;
    int16_t imu_raw_x_ = 0;
    int16_t imu_raw_y_ = 0;
    int16_t imu_raw_z_ = 0;
};

}  // namespace picoboy
