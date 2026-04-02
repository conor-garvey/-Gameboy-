#pragma once

#include <cstddef>
#include <cstdint>

#include "pico/time.h"

namespace picoboy {

enum class SoundEffect : uint8_t {
    PowerOn,
    MenuMove,
    Coin,
    EnemyStomp,
};

class AudioEngine {
public:
    static constexpr uint8_t DefaultPwmPin = 16;
    static constexpr uint8_t MaxVolume = 10;

    enum class Waveform : uint8_t {
        Pulse12,
        Pulse25,
        Pulse50,
        Noise,
    };

    struct ToneStep {
        uint16_t start_frequency_hz;
        uint16_t end_frequency_hz;
        uint16_t duration_ms;
        uint8_t start_amplitude;
        uint8_t end_amplitude;
        Waveform waveform;
    };

    struct SoundSequence {
        const ToneStep* steps = nullptr;
        size_t count = 0;
    };

    AudioEngine();

    void init(uint8_t pwm_pin = DefaultPwmPin);
    void startBackgroundMusic();
    void playEffect(SoundEffect effect);
    void stop();

    void setVolume(uint8_t volume);
    void changeVolume(int delta);
    uint8_t volume() const;
    uint8_t pwmPin() const;

private:
    static bool timerCallback(repeating_timer_t* timer);
    void advanceStep();
    SoundSequence sequenceForEffect(SoundEffect effect) const;
    void enableOutput();
    void disableOutput();
    bool hasActivePlayback() const;
    int16_t nextEffectSample();
    int16_t nextMusicSample();
    uint8_t nextPwmLevel();

    bool initialized_ = false;
    uint8_t pwm_pin_ = DefaultPwmPin;
    uint8_t pwm_slice_ = 0;
    uint8_t volume_ = 8;
    bool output_enabled_ = false;
    bool timer_running_ = false;
    repeating_timer_t timer_{};
    const ToneStep* active_steps_ = nullptr;
    size_t active_step_count_ = 0;
    size_t active_step_index_ = 0;
    uint8_t current_start_amplitude_ = 0;
    uint8_t current_end_amplitude_ = 0;
    Waveform current_waveform_ = Waveform::Pulse25;
    uint32_t phase_accumulator_ = 0;
    uint32_t noise_phase_accumulator_ = 0;
    uint32_t current_start_phase_step_ = 0;
    uint32_t current_end_phase_step_ = 0;
    uint32_t samples_in_step_ = 0;
    uint32_t samples_remaining_in_step_ = 0;
    int32_t smoothed_level_ = 0;
    uint16_t noise_lfsr_ = 0x5A5Au;
    const uint8_t* music_data_ = nullptr;
    size_t music_sample_count_ = 0;
    uint32_t music_index_ = 0;
    uint32_t music_fraction_ = 0;
    uint32_t music_step_fp_ = 0;
    bool music_enabled_ = false;
};

}  // namespace picoboy
