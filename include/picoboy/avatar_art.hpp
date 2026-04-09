#pragma once

#include <cstdint>

#include "picoboy/display.hpp"
#include "picoboy/profile_store.hpp"

namespace picoboy {

enum class AvatarPose : uint8_t {
    Idle,
    WalkA,
    WalkB,
    Jump,
    Death,
};

inline AvatarId sanitizeAvatarId(uint8_t avatar) {
    if (avatar >= static_cast<uint8_t>(AvatarId::Count)) {
        return AvatarId::Hero;
    }

    return static_cast<AvatarId>(avatar);
}

namespace avatar_art_detail {

inline void scaledRect(Display& display, int16_t x, int16_t y, int16_t ox, int16_t oy,
                       int16_t width, int16_t height, Display::Color color, uint8_t scale) {
    const int16_t s = scale == 0 ? 1 : static_cast<int16_t>(scale);
    display.fillRect(static_cast<int16_t>(x + ox * s), static_cast<int16_t>(y + oy * s),
                     static_cast<int16_t>(width * s), static_cast<int16_t>(height * s), color);
}

inline void scaledPixel(Display& display, int16_t x, int16_t y, int16_t ox, int16_t oy,
                        Display::Color color, uint8_t scale) {
    const int16_t s = scale == 0 ? 1 : static_cast<int16_t>(scale);
    if (s == 1) {
        display.drawPixel(static_cast<int16_t>(x + ox), static_cast<int16_t>(y + oy), color);
        return;
    }

    display.fillRect(static_cast<int16_t>(x + ox * s), static_cast<int16_t>(y + oy * s), s, s, color);
}

inline constexpr int16_t player_sprite_width = 16;
inline constexpr int16_t player_sprite_height = 16;

inline constexpr Display::Color mario_palette[] = {
    static_cast<Display::Color>(0),
    Display::rgb565(107, 109, 0),
    Display::rgb565(181, 49, 32),
    Display::rgb565(234, 158, 34),
};

inline constexpr Display::Color luigi_palette[] = {
    static_cast<Display::Color>(0),
    Display::rgb565(56, 135, 0),
    Display::rgb565(255, 254, 255),
    Display::rgb565(234, 158, 34),
};

inline constexpr uint8_t plumber_frame_idle[] = {
    0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 3, 3, 1, 3, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 3, 1, 3, 3, 3, 1, 3, 3, 3, 0, 0, 0,
    0, 0, 0, 1, 3, 1, 1, 3, 3, 3, 1, 3, 3, 3, 0, 0,
    0, 0, 0, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 2, 2, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 2, 2, 3, 2, 2, 3, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 0, 0, 0, 0,
    0, 0, 0, 2, 1, 1, 3, 3, 3, 2, 2, 2, 0, 0, 0, 0,
    0, 0, 0, 0, 2, 1, 3, 3, 2, 2, 2, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 2, 2, 2, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
};

inline constexpr uint8_t plumber_frame_walk_a[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0,
    0, 0, 0, 0, 0, 1, 1, 1, 3, 3, 1, 3, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 3, 1, 3, 3, 3, 1, 3, 3, 3, 0, 0,
    0, 0, 0, 0, 1, 3, 1, 1, 3, 3, 3, 1, 3, 3, 3, 0,
    0, 0, 0, 0, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 1, 0, 3, 0, 0, 0,
    0, 0, 0, 0, 3, 1, 1, 1, 1, 1, 1, 3, 3, 3, 0, 0,
    0, 0, 0, 3, 3, 2, 1, 1, 1, 1, 1, 3, 3, 0, 0, 0,
    0, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0,
    0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0,
    0, 0, 1, 1, 2, 2, 2, 0, 2, 2, 2, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0,
};

inline constexpr uint8_t plumber_frame_walk_b[] = {
    0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 3, 1, 3, 0, 0, 0, 0, 0,
    0, 3, 3, 1, 3, 3, 1, 3, 3, 3, 3, 3, 3, 0, 0, 0,
    0, 3, 3, 1, 3, 3, 1, 1, 3, 3, 1, 1, 3, 3, 0, 0,
    0, 0, 3, 3, 1, 3, 3, 3, 3, 3, 3, 1, 1, 0, 0, 0,
    0, 0, 0, 2, 2, 2, 1, 1, 1, 2, 3, 3, 0, 0, 0, 0,
    0, 0, 2, 2, 3, 3, 3, 1, 2, 2, 1, 1, 1, 0, 0, 0,
    0, 0, 2, 1, 3, 3, 3, 1, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 2, 2, 2, 3, 3, 1, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 0, 2, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 2, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 0, 0, 0, 0, 0,
    0, 1, 0, 1, 2, 2, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

inline constexpr uint8_t plumber_frame_jump[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3,
    0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 0, 3, 3, 3,
    0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3,
    0, 0, 0, 0, 0, 1, 1, 1, 3, 3, 1, 3, 0, 1, 1, 1,
    0, 0, 0, 0, 1, 3, 1, 3, 3, 3, 1, 3, 3, 1, 1, 1,
    0, 0, 0, 0, 1, 3, 1, 1, 3, 3, 3, 1, 3, 3, 3, 1,
    0, 0, 0, 0, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 1, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 1, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 0, 0, 1,
    3, 3, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 0, 0, 1,
    3, 3, 3, 0, 2, 2, 1, 2, 2, 3, 2, 2, 3, 2, 1, 1,
    0, 3, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1,
    0, 0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1,
    0, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0,
};

inline constexpr uint8_t plumber_frame_death[] = {
    0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 3, 3, 1, 3, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 3, 1, 3, 3, 3, 1, 3, 3, 3, 0, 0, 0,
    0, 0, 0, 1, 3, 1, 1, 3, 3, 3, 1, 3, 3, 3, 0, 0,
    0, 0, 0, 1, 1, 3, 3, 3, 3, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 2, 2, 2, 1, 1, 1, 1, 1, 1, 3, 3, 0,
    0, 0, 0, 0, 2, 2, 2, 2, 1, 1, 1, 1, 1, 3, 3, 3,
    0, 0, 0, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 3, 3, 3,
    0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1,
    0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0,
    0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0,
};

inline const uint8_t* frameForPose(AvatarPose pose) {
    switch (pose) {
    case AvatarPose::Idle:
        return plumber_frame_idle;

    case AvatarPose::WalkA:
        return plumber_frame_walk_a;

    case AvatarPose::WalkB:
        return plumber_frame_walk_b;

    case AvatarPose::Jump:
        return plumber_frame_jump;

    case AvatarPose::Death:
        return plumber_frame_death;
    }

    return plumber_frame_idle;
}

inline const Display::Color* paletteForAvatar(AvatarId avatar) {
    switch (sanitizeAvatarId(static_cast<uint8_t>(avatar))) {
    case AvatarId::Hero:
        return mario_palette;

    case AvatarId::Pac:
        return luigi_palette;

    case AvatarId::Count:
        break;
    }

    return mario_palette;
}

inline void drawSpriteFrame(Display& display, int16_t x, int16_t y, const uint8_t* frame,
                            const Display::Color* palette, bool facing_left) {
    if (frame == nullptr || palette == nullptr) {
        return;
    }

    for (int16_t row = 0; row < player_sprite_height; ++row) {
        int16_t run_start = 0;
        uint8_t run_color_index = 0;

        for (int16_t column = 0; column <= player_sprite_width; ++column) {
            uint8_t color_index = 0;
            if (column < player_sprite_width) {
                const int16_t sample_column = facing_left
                    ? static_cast<int16_t>(player_sprite_width - 1 - column)
                    : column;
                color_index = frame[row * player_sprite_width + sample_column];
            }

            if (column == 0) {
                run_color_index = color_index;
                run_start = 0;
                continue;
            }

            if (color_index == run_color_index) {
                continue;
            }

            if (run_color_index != 0) {
                display.fillRect(static_cast<int16_t>(x + run_start), static_cast<int16_t>(y + row),
                                 static_cast<int16_t>(column - run_start), 1, palette[run_color_index]);
            }

            run_start = column;
            run_color_index = color_index;
        }
    }
}

}  // namespace avatar_art_detail

inline void drawAvatarBadge(Display& display, int16_t x, int16_t y, AvatarId avatar,
                            uint8_t scale, Display::Color background) {
    using namespace avatar_art_detail;

    const char* letter = "M";
    Display::Color color = mario_palette[2];
    Display::Color plate_fill = Display::rgb565(86, 26, 22);
    Display::Color plate_border = Display::rgb565(255, 198, 150);
    Display::Color letter_highlight = Display::rgb565(255, 214, 176);

    switch (sanitizeAvatarId(static_cast<uint8_t>(avatar))) {
    case AvatarId::Hero:
        letter = "M";
        color = mario_palette[2];
        plate_fill = Display::rgb565(86, 26, 22);
        plate_border = Display::rgb565(255, 198, 150);
        letter_highlight = Display::rgb565(255, 214, 176);
        break;

    case AvatarId::Pac:
        letter = "L";
        color = luigi_palette[1];
        plate_fill = Display::rgb565(18, 78, 22);
        plate_border = Display::rgb565(188, 255, 188);
        letter_highlight = Display::rgb565(232, 255, 232);
        break;

    case AvatarId::Count:
        break;
    }

    const uint8_t badge_scale = scale == 0 ? 1 : scale;
    const uint8_t text_scale = static_cast<uint8_t>(badge_scale + 1);
    const int16_t text_width = display.measureTextWidth(letter, text_scale);
    const int16_t text_height = static_cast<int16_t>(Display::FontHeight * text_scale);
    const int16_t badge_width = static_cast<int16_t>(text_width + 4);
    const int16_t badge_height = static_cast<int16_t>(text_height + 4);

    display.fillRect(static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1), badge_width, badge_height,
                     Display::rgb565(12, 18, 32));
    display.fillRect(x, y, badge_width, badge_height, plate_fill);
    display.drawRect(x, y, badge_width, badge_height, plate_border);
    display.fillRect(static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1),
                     static_cast<int16_t>(badge_width - 2), 1, letter_highlight);
    display.fillRect(static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1),
                     1, static_cast<int16_t>(badge_height - 2), letter_highlight);

    const int16_t text_x = static_cast<int16_t>(x + (badge_width - text_width) / 2);
    const int16_t text_y = static_cast<int16_t>(y + (badge_height - text_height) / 2 - 1);
    display.drawText(static_cast<int16_t>(text_x + 1), static_cast<int16_t>(text_y + 1), letter, background, text_scale);
    display.drawText(text_x, text_y, letter, color, text_scale);
}

inline void drawPlayerAvatar(Display& display, int16_t x, int16_t y, AvatarId avatar,
                             AvatarPose pose = AvatarPose::Idle, bool facing_left = false) {
    using namespace avatar_art_detail;

    const int16_t draw_x = static_cast<int16_t>(x - 2);
    drawSpriteFrame(display, draw_x, y, frameForPose(pose), paletteForAvatar(avatar), facing_left);
}

}  // namespace picoboy
