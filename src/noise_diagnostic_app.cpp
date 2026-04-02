#include "picoboy/noise_diagnostic_app.hpp"

#include <cstdio>

namespace picoboy {

namespace {

Display::Color backgroundColor() {
    return Display::rgb565(8, 14, 28);
}

Display::Color panelColor() {
    return Display::rgb565(18, 28, 58);
}

Display::Color selectedColor() {
    return Display::rgb565(54, 120, 200);
}

Display::Color accentColor() {
    return Display::rgb565(255, 196, 48);
}

}  // namespace

NoiseDiagnosticApp::NoiseDiagnosticApp(Display& display, Lsm6dsl& imu, AudioEngine& audio)
    : display_(display), imu_(imu), audio_(audio), buttons_() {}

void NoiseDiagnosticApp::init(bool audio_test_enabled) {
    buttons_.init();
    audio_test_enabled_ = audio_test_enabled;
    if (audio_test_enabled_) {
        audio_.startBackgroundMusic();
    } else {
        audio_.stop();
    }
    render_load_enabled_ = true;
    backlight_enabled_ = true;
    imu_poll_enabled_ = true;
    imu_sample_valid_ = false;
    exit_requested_ = false;
    awaiting_button_release_ = true;
    selected_item_ = 0;
    frame_counter_ = 0;
    setBacklightEnabled(true);
    sampleImu();
    needs_render_ = true;
}

void NoiseDiagnosticApp::update() {
    buttons_.update();

    if (awaiting_button_release_) {
        awaiting_button_release_ = anyButtonHeld();
        if (awaiting_button_release_) {
            if (imu_poll_enabled_) {
                sampleImu();
            }
            if (render_load_enabled_) {
                ++frame_counter_;
                needs_render_ = true;
            }
            return;
        }
    }

    if (buttons_.pressed(ButtonId::Start)) {
        exit_requested_ = true;
        return;
    }

    if (buttons_.pressed(ButtonId::Up) && selected_item_ > 0) {
        --selected_item_;
        needs_render_ = true;
    }

    const uint8_t last_item = static_cast<uint8_t>(Item::Count) - 1;
    if (buttons_.pressed(ButtonId::Down) && selected_item_ < last_item) {
        ++selected_item_;
        needs_render_ = true;
    }

    if (buttons_.pressed(ButtonId::A) ||
        buttons_.pressed(ButtonId::Left) ||
        buttons_.pressed(ButtonId::Right)) {
        toggleSelectedItem();
        needs_render_ = true;
    }

    if (imu_poll_enabled_) {
        sampleImu();
    }

    if (render_load_enabled_) {
        ++frame_counter_;
        needs_render_ = true;
    }
}

void NoiseDiagnosticApp::render() {
    if (!render_load_enabled_ && !needs_render_) {
        return;
    }

    const int16_t screen_width = static_cast<int16_t>(display_.width());
    char audio_text[40];
    std::snprintf(audio_text, sizeof(audio_text), "AUDIO TEST %s", onOffText(audio_test_enabled_));

    display_.beginFrame(backgroundColor());
    display_.fillRect(0, 0, screen_width, 28, panelColor());
    drawCenteredText(7, "NOISE DIAGNOSTIC", Display::rgb565(255, 255, 255), 2);
    drawCenteredText(34, "LISTEN AFTER TOGGLING EACH ITEM", Display::rgb565(180, 220, 255), 1);

    const char* labels[] = {"SCREEN LOAD", "BACKLIGHT", "IMU POLL"};
    const bool values[] = {render_load_enabled_, backlight_enabled_, imu_poll_enabled_};

    for (uint8_t index = 0; index < static_cast<uint8_t>(Item::Count); ++index) {
        const int16_t y = static_cast<int16_t>(58 + index * 28);
        const bool selected = selected_item_ == index;
        const Display::Color fill = selected ? selectedColor() : panelColor();
        const Display::Color border = selected ? accentColor() : Display::rgb565(70, 90, 130);

        display_.fillRect(20, y, static_cast<int16_t>(screen_width - 40), 22, fill);
        display_.drawRect(20, y, static_cast<int16_t>(screen_width - 40), 22, border);
        display_.drawText(30, static_cast<int16_t>(y + 7), labels[index], Display::rgb565(255, 255, 255), 1);

        const char* state_text = onOffText(values[index]);
        const int16_t state_x = static_cast<int16_t>(screen_width -
            display_.measureTextWidth(state_text, 1) - 30);
        display_.drawText(state_x, static_cast<int16_t>(y + 7), state_text, accentColor(), 1);
    }

    display_.drawText(20, 140, "A/L/R TOGGLE   START EXIT", Display::rgb565(220, 220, 220), 1);
    display_.drawText(20, 154, audio_text,
                      audio_test_enabled_ ? Display::rgb565(160, 240, 180) : Display::rgb565(255, 220, 140), 1);
    display_.drawText(20, 168, "AUDIO IGNORES SCREEN/BACKLIGHT TESTS", Display::rgb565(255, 220, 140), 1);

    if (!imu_.ready()) {
        display_.drawText(20, 178, "IMU NOT READY", Display::rgb565(255, 120, 120), 1);
    } else if (imu_sample_valid_) {
        char imu_text[48];
        std::snprintf(imu_text, sizeof(imu_text), "IMU RAW X:%d Y:%d Z:%d",
                      static_cast<int>(imu_raw_x_),
                      static_cast<int>(imu_raw_y_),
                      static_cast<int>(imu_raw_z_));
        display_.drawText(20, 178, imu_text, Display::rgb565(160, 220, 255), 1);
    } else {
        display_.drawText(20, 178, "IMU POLL IS OFF", Display::rgb565(160, 220, 255), 1);
    }

    if (render_load_enabled_) {
        for (int16_t stripe = 0; stripe < screen_width; stripe += 16) {
            const uint8_t offset = static_cast<uint8_t>((stripe + static_cast<int16_t>(frame_counter_ * 3)) & 0x3F);
            const Display::Color color = Display::rgb565(static_cast<uint8_t>(32 + offset * 3),
                                                         static_cast<uint8_t>(100 + offset * 2),
                                                         static_cast<uint8_t>(140 + offset));
            display_.fillRect(stripe, 190, 12, 14, color);
        }
        drawCenteredText(208, "SCREEN LOAD ON = CONTINUOUS SPI", Display::rgb565(220, 220, 220), 1);
        drawCenteredText(218, "UPDATES", Display::rgb565(220, 220, 220), 1);
    } else {
        drawCenteredText(208, "SCREEN LOAD OFF = REDRAW ONLY", Display::rgb565(220, 220, 220), 1);
        drawCenteredText(218, "WHEN SOMETHING CHANGES", Display::rgb565(220, 220, 220), 1);
    }

    display_.present();
    needs_render_ = false;
}

bool NoiseDiagnosticApp::shouldRender() const {
    return render_load_enabled_ || needs_render_;
}

bool NoiseDiagnosticApp::exitRequested() const {
    return exit_requested_;
}

void NoiseDiagnosticApp::toggleSelectedItem() {
    switch (static_cast<Item>(selected_item_)) {
    case Item::ScreenLoad:
        render_load_enabled_ = !render_load_enabled_;
        break;

    case Item::Backlight:
        setBacklightEnabled(!backlight_enabled_);
        break;

    case Item::ImuPoll:
        imu_poll_enabled_ = !imu_poll_enabled_;
        if (!imu_poll_enabled_) {
            imu_sample_valid_ = false;
        } else {
            sampleImu();
        }
        break;

    case Item::Count:
        break;
    }
}

void NoiseDiagnosticApp::setBacklightEnabled(bool enabled) {
    backlight_enabled_ = enabled;
    if (backlight_enabled_) {
        display_.backlightOn();
    } else {
        display_.backlightOff();
    }
}

void NoiseDiagnosticApp::sampleImu() {
    if (!imu_.ready()) {
        imu_sample_valid_ = false;
        return;
    }

    Lsm6dsl::Acceleration reading;
    if (!imu_.readAcceleration(reading)) {
        imu_sample_valid_ = false;
        return;
    }

    imu_raw_x_ = reading.raw_x;
    imu_raw_y_ = reading.raw_y;
    imu_raw_z_ = reading.raw_z;
    imu_sample_valid_ = true;
}

bool NoiseDiagnosticApp::anyButtonHeld() const {
    return buttons_.held(ButtonId::Up) ||
           buttons_.held(ButtonId::Down) ||
           buttons_.held(ButtonId::Left) ||
           buttons_.held(ButtonId::Right) ||
           buttons_.held(ButtonId::A) ||
           buttons_.held(ButtonId::B) ||
           buttons_.held(ButtonId::Select) ||
           buttons_.held(ButtonId::Start);
}

void NoiseDiagnosticApp::drawCenteredText(int16_t y, const char* text, Display::Color color, uint8_t scale) {
    const int16_t text_width = display_.measureTextWidth(text, scale);
    const int16_t x = static_cast<int16_t>((static_cast<int16_t>(display_.width()) - text_width) / 2);
    display_.drawText(x < 0 ? 0 : x, y, text, color, scale);
}

const char* NoiseDiagnosticApp::onOffText(bool enabled) const {
    return enabled ? "ON" : "OFF";
}

}  // namespace picoboy
