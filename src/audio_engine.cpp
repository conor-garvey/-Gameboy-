#include "picoboy/audio_engine.hpp"

#include <algorithm>

#include "audio.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"

namespace picoboy {

namespace {

constexpr uint32_t sample_rate_hz = 22050;
// Match hello_pico.cpp: 125 MHz / 256 / 2 = about 244 kHz carrier,
// still comfortably above a ~7 kHz RC low-pass cutoff.
constexpr uint16_t pwm_wrap = 255;
constexpr uint8_t pwm_divider_int = 2;
constexpr uint8_t pwm_divider_frac = 0;
constexpr uint16_t pwm_silence = 0;
constexpr uint8_t pwm_midpoint = 128;
constexpr uint32_t oscillator_phase_scale = 1u << 24;
constexpr uint32_t fixed_point_scale = 1u << 16;
// Play the imported PCM a bit slower than its original export rate so the
// background theme feels less rushed on hardware.
constexpr uint32_t background_music_sample_rate_hz = 9000;
constexpr size_t background_music_sample_count = sizeof(audio) / sizeof(audio[0]);

using Waveform = AudioEngine::Waveform;
using ToneStep = AudioEngine::ToneStep;
using SoundSequence = AudioEngine::SoundSequence;

const ToneStep power_on_steps[] = {
    {523, 659, 34, 0, 96, Waveform::Pulse25},
    {784, 988, 40, 96, 84, Waveform::Pulse25},
    {1047, 1568, 76, 84, 0, Waveform::Pulse50},
};

const ToneStep menu_move_steps[] = {
    {1319, 1047, 24, 84, 0, Waveform::Pulse25},
};

const ToneStep coin_steps[] = {
    {988, 1319, 20, 36, 108, Waveform::Pulse25},
    {1568, 2093, 46, 128, 0, Waveform::Pulse12},
};

const ToneStep enemy_stomp_steps[] = {
    {960, 220, 18, 128, 52, Waveform::Noise},
    {247, 165, 44, 96, 0, Waveform::Pulse50},
};

uint32_t phaseStepForFrequency(uint16_t frequency_hz) {
    if (frequency_hz == 0) {
        return 0;
    }

    return (static_cast<uint32_t>(frequency_hz) << 24) / sample_rate_hz;
}

uint32_t samplesForDurationMs(uint16_t duration_ms) {
    const uint32_t samples = static_cast<uint32_t>(duration_ms) * sample_rate_hz / 1000u;
    return samples == 0 ? 1u : samples;
}

uint32_t interpolateU32(uint32_t start, uint32_t end, uint32_t progress, uint32_t progress_max) {
    if (progress_max == 0 || start == end) {
        return end;
    }

    const int64_t delta = static_cast<int64_t>(end) - static_cast<int64_t>(start);
    return static_cast<uint32_t>(static_cast<int64_t>(start) + delta * progress / progress_max);
}

uint8_t interpolateU8(uint8_t start, uint8_t end, uint32_t progress, uint32_t progress_max) {
    if (progress_max == 0 || start == end) {
        return end;
    }

    const int32_t delta = static_cast<int32_t>(end) - static_cast<int32_t>(start);
    return static_cast<uint8_t>(static_cast<int32_t>(start) + delta * static_cast<int32_t>(progress) /
                                                               static_cast<int32_t>(progress_max));
}

int32_t pulseValue(uint8_t phase_byte, uint8_t duty_threshold) {
    if (duty_threshold == 0 || duty_threshold >= 255) {
        return 0;
    }

    if (duty_threshold == 128) {
        return phase_byte < duty_threshold ? 127 : -127;
    }

    const int32_t high_level = 127;
    const int32_t low_level = -static_cast<int32_t>(
        static_cast<int32_t>(high_level) * static_cast<int32_t>(duty_threshold) /
        static_cast<int32_t>(256 - duty_threshold));
    return phase_byte < duty_threshold ? high_level : low_level;
}

}  // namespace

AudioEngine::AudioEngine() = default;

void AudioEngine::init(uint8_t pwm_pin) {
    pwm_pin_ = pwm_pin;
    gpio_set_function(pwm_pin_, GPIO_FUNC_PWM);
    pwm_slice_ = pwm_gpio_to_slice_num(pwm_pin_);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv_int_frac4(&config, pwm_divider_int, pwm_divider_frac);
    pwm_config_set_wrap(&config, pwm_wrap);
    pwm_init(pwm_slice_, &config, false);
    pwm_set_gpio_level(pwm_pin_, pwm_silence);

    if (!timer_running_) {
        add_repeating_timer_us(static_cast<int64_t>(-1000000 / static_cast<int32_t>(sample_rate_hz)),
                               &AudioEngine::timerCallback, this, &timer_);
        timer_running_ = true;
    }

    initialized_ = true;
    disableOutput();
}

void AudioEngine::startBackgroundMusic() {
    if (!initialized_ || !enabled_ || background_music_sample_count == 0) {
        return;
    }

    const uint32_t interrupts = save_and_disable_interrupts();
    music_data_ = audio;
    music_sample_count_ = background_music_sample_count;
    music_index_ = 0;
    music_fraction_ = 0;
    music_step_fp_ = static_cast<uint32_t>(
        (static_cast<uint64_t>(background_music_sample_rate_hz) * fixed_point_scale) / sample_rate_hz);
    if (music_step_fp_ == 0) {
        music_step_fp_ = 1;
    }
    music_enabled_ = music_sample_count_ > 0;
    restore_interrupts(interrupts);

    if (music_enabled_ && volume_ > 0) {
        enableOutput();
    }
}

void AudioEngine::stopBackgroundMusic() {
    const uint32_t interrupts = save_and_disable_interrupts();
    music_data_ = nullptr;
    music_sample_count_ = 0;
    music_index_ = 0;
    music_fraction_ = 0;
    music_step_fp_ = 0;
    music_enabled_ = false;
    restore_interrupts(interrupts);

    if (initialized_ && !hasActivePlayback()) {
        disableOutput();
    }
}

void AudioEngine::playEffect(SoundEffect effect) {
    if (!initialized_ || !enabled_) {
        return;
    }

    if (volume_ == 0) {
        return;
    }

    const SoundSequence sequence = sequenceForEffect(effect);
    const uint32_t interrupts = save_and_disable_interrupts();
    active_steps_ = sequence.steps;
    active_step_count_ = sequence.count;
    active_step_index_ = 0;
    phase_accumulator_ = 0;
    noise_phase_accumulator_ = 0;
    noise_lfsr_ = 0x5A5Au;
    advanceStep();
    restore_interrupts(interrupts);
    enableOutput();
}

void AudioEngine::stop() {
    const uint32_t interrupts = save_and_disable_interrupts();
    active_steps_ = nullptr;
    active_step_count_ = 0;
    active_step_index_ = 0;
    current_start_amplitude_ = 0;
    current_end_amplitude_ = 0;
    current_start_phase_step_ = 0;
    current_end_phase_step_ = 0;
    phase_accumulator_ = 0;
    noise_phase_accumulator_ = 0;
    samples_in_step_ = 0;
    samples_remaining_in_step_ = 0;
    smoothed_level_ = 0;
    noise_lfsr_ = 0x5A5Au;
    music_data_ = nullptr;
    music_sample_count_ = 0;
    music_index_ = 0;
    music_fraction_ = 0;
    music_step_fp_ = 0;
    music_enabled_ = false;
    restore_interrupts(interrupts);

    if (initialized_) {
        disableOutput();
    }
}

void AudioEngine::setEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_) {
        stop();
        return;
    }

    if (initialized_ && volume_ > 0 && hasActivePlayback()) {
        enableOutput();
    }
}

bool AudioEngine::enabled() const {
    return enabled_;
}

void AudioEngine::setVolume(uint8_t volume) {
    volume_ = std::min<uint8_t>(volume, MaxVolume);
    if (volume_ == 0) {
        if (initialized_) {
            disableOutput();
        }
        return;
    }

    if (!enabled_) {
        if (initialized_) {
            disableOutput();
        }
        return;
    }

    if (initialized_ && hasActivePlayback()) {
        enableOutput();
    }
}

void AudioEngine::changeVolume(int delta) {
    int next_volume = static_cast<int>(volume_) + delta;
    if (next_volume < 0) {
        next_volume = 0;
    }
    if (next_volume > static_cast<int>(MaxVolume)) {
        next_volume = MaxVolume;
    }

    volume_ = static_cast<uint8_t>(next_volume);
    if (volume_ == 0) {
        if (initialized_) {
            disableOutput();
        }
        return;
    }

    if (!enabled_) {
        if (initialized_) {
            disableOutput();
        }
        return;
    }

    if (initialized_ && hasActivePlayback()) {
        enableOutput();
    }
}

uint8_t AudioEngine::volume() const {
    return volume_;
}

uint8_t AudioEngine::pwmPin() const {
    return pwm_pin_;
}

bool AudioEngine::timerCallback(repeating_timer_t* timer) {
    AudioEngine* self = static_cast<AudioEngine*>(timer->user_data);
    if (self == nullptr || !self->initialized_) {
        return true;
    }

    if (!self->output_enabled_) {
        return true;
    }

    pwm_set_gpio_level(self->pwm_pin_, self->nextPwmLevel());
    if (!self->hasActivePlayback()) {
        self->disableOutput();
    }
    return true;
}

void AudioEngine::advanceStep() {
    if (active_steps_ == nullptr || active_step_index_ >= active_step_count_) {
        active_steps_ = nullptr;
        active_step_count_ = 0;
        active_step_index_ = 0;
        current_start_amplitude_ = 0;
        current_end_amplitude_ = 0;
        current_start_phase_step_ = 0;
        current_end_phase_step_ = 0;
        samples_in_step_ = 0;
        samples_remaining_in_step_ = 0;
        return;
    }

    const ToneStep& step = active_steps_[active_step_index_++];
    current_start_amplitude_ = step.start_amplitude;
    current_end_amplitude_ = step.end_amplitude;
    current_waveform_ = step.waveform;
    current_start_phase_step_ = phaseStepForFrequency(step.start_frequency_hz);
    current_end_phase_step_ = phaseStepForFrequency(step.end_frequency_hz);
    phase_accumulator_ = 0;
    noise_phase_accumulator_ = 0;
    samples_in_step_ = samplesForDurationMs(step.duration_ms);
    samples_remaining_in_step_ = samples_in_step_;
}

AudioEngine::SoundSequence AudioEngine::sequenceForEffect(SoundEffect effect) const {
    switch (effect) {
    case SoundEffect::PowerOn:
        return SoundSequence{power_on_steps, sizeof(power_on_steps) / sizeof(power_on_steps[0])};

    case SoundEffect::MenuMove:
        return SoundSequence{menu_move_steps, sizeof(menu_move_steps) / sizeof(menu_move_steps[0])};

    case SoundEffect::Coin:
        return SoundSequence{coin_steps, sizeof(coin_steps) / sizeof(coin_steps[0])};

    case SoundEffect::EnemyStomp:
        return SoundSequence{enemy_stomp_steps, sizeof(enemy_stomp_steps) / sizeof(enemy_stomp_steps[0])};
    }

    return SoundSequence{};
}

void AudioEngine::enableOutput() {
    const uint32_t interrupts = save_and_disable_interrupts();
    if (output_enabled_) {
        restore_interrupts(interrupts);
        return;
    }

    gpio_set_function(pwm_pin_, GPIO_FUNC_PWM);
    pwm_set_gpio_level(pwm_pin_, pwm_silence);
    pwm_set_enabled(pwm_slice_, true);
    output_enabled_ = true;
    restore_interrupts(interrupts);
}

void AudioEngine::disableOutput() {
    const uint32_t interrupts = save_and_disable_interrupts();
    output_enabled_ = false;
    pwm_set_gpio_level(pwm_pin_, pwm_silence);
    pwm_set_enabled(pwm_slice_, false);
    gpio_set_function(pwm_pin_, GPIO_FUNC_SIO);
    gpio_set_dir(pwm_pin_, GPIO_OUT);
    gpio_put(pwm_pin_, 0);
    restore_interrupts(interrupts);
}

bool AudioEngine::hasActivePlayback() const {
    return (music_enabled_ && music_data_ != nullptr && music_sample_count_ > 0) ||
           active_steps_ != nullptr ||
           samples_remaining_in_step_ > 0 ||
           smoothed_level_ != 0;
}

int16_t AudioEngine::nextEffectSample() {
    if (samples_remaining_in_step_ == 0) {
        advanceStep();
    }

    if (samples_remaining_in_step_ == 0) {
        if (smoothed_level_ == 0) {
            return 0;
        }

        smoothed_level_ = static_cast<int32_t>(smoothed_level_ * 3 / 4);
        if (smoothed_level_ > -2 && smoothed_level_ < 2) {
            smoothed_level_ = 0;
        }

        return static_cast<int16_t>(std::clamp<int32_t>(smoothed_level_, -127, 127));
    }

    const uint32_t progress = samples_in_step_ - samples_remaining_in_step_;
    const uint32_t progress_max = samples_in_step_ > 1 ? samples_in_step_ - 1 : 0;
    const uint32_t phase_step = interpolateU32(current_start_phase_step_, current_end_phase_step_, progress, progress_max);
    const uint8_t amplitude = interpolateU8(current_start_amplitude_, current_end_amplitude_, progress, progress_max);

    --samples_remaining_in_step_;

    if (amplitude == 0 || phase_step == 0) {
        smoothed_level_ = static_cast<int32_t>(smoothed_level_ * 3 / 4);
        if (smoothed_level_ > -2 && smoothed_level_ < 2) {
            smoothed_level_ = 0;
        }
        return static_cast<int16_t>(std::clamp<int32_t>(smoothed_level_, -127, 127));
    }

    phase_accumulator_ += phase_step;

    int32_t wave_value = 0;
    switch (current_waveform_) {
    case Waveform::Pulse12:
        wave_value = pulseValue(static_cast<uint8_t>(phase_accumulator_ >> 24), 32);
        break;

    case Waveform::Pulse25:
        wave_value = pulseValue(static_cast<uint8_t>(phase_accumulator_ >> 24), 64);
        break;

    case Waveform::Pulse50:
        wave_value = pulseValue(static_cast<uint8_t>(phase_accumulator_ >> 24), 128);
        break;

    case Waveform::Noise:
        noise_phase_accumulator_ += phase_step;
        while (noise_phase_accumulator_ >= oscillator_phase_scale) {
            noise_phase_accumulator_ -= oscillator_phase_scale;
            const uint16_t feedback = static_cast<uint16_t>((noise_lfsr_ ^ (noise_lfsr_ >> 1)) & 0x1u);
            noise_lfsr_ = static_cast<uint16_t>((noise_lfsr_ >> 1) | (feedback << 14));
            if (noise_lfsr_ == 0) {
                noise_lfsr_ = 0x5A5Au;
            }
        }
        wave_value = (noise_lfsr_ & 0x1u) == 0 ? 127 : -127;
        break;
    }

    const int32_t target_sample = wave_value * static_cast<int32_t>(amplitude) / 128;
    smoothed_level_ += (target_sample - smoothed_level_) / 3;
    return static_cast<int16_t>(std::clamp<int32_t>(smoothed_level_, -127, 127));
}

int16_t AudioEngine::nextMusicSample() {
    if (!music_enabled_ || music_data_ == nullptr || music_sample_count_ == 0) {
        return 0;
    }

    const uint8_t sample = music_data_[music_index_];

    music_fraction_ += music_step_fp_;
    music_index_ += (music_fraction_ >> 16);
    music_fraction_ &= (fixed_point_scale - 1);

    while (music_index_ >= music_sample_count_) {
        music_index_ -= static_cast<uint32_t>(music_sample_count_);
    }

    return static_cast<int16_t>(static_cast<int16_t>(sample) - static_cast<int16_t>(pwm_midpoint));
}

uint8_t AudioEngine::nextPwmLevel() {
    if (!enabled_ || volume_ == 0) {
        return pwm_silence;
    }

    const int16_t effect_sample = nextEffectSample();
    const int16_t music_sample = nextMusicSample();

    if (music_sample == 0 &&
        effect_sample == 0 &&
        !music_enabled_ &&
        active_steps_ == nullptr &&
        samples_remaining_in_step_ == 0 &&
        smoothed_level_ == 0) {
        return pwm_silence;
    }

    int32_t mixed_sample = static_cast<int32_t>(music_sample) + static_cast<int32_t>(effect_sample);
    mixed_sample = mixed_sample * static_cast<int32_t>(volume_) / static_cast<int32_t>(MaxVolume);
    mixed_sample = std::clamp<int32_t>(mixed_sample, -128, 127);
    return static_cast<uint8_t>(mixed_sample + static_cast<int32_t>(pwm_midpoint));
}

}  // namespace picoboy
