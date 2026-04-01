#pragma once

#include <cstdint>

#include "picoboy/display.hpp"
#include "picoboy/profile_store.hpp"

namespace picoboy {

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

inline void scaledLine(Display& display, int16_t x, int16_t y, int16_t x0, int16_t y0,
                       int16_t x1, int16_t y1, Display::Color color, uint8_t scale) {
    const int16_t s = scale == 0 ? 1 : static_cast<int16_t>(scale);

    for (int16_t offset = 0; offset < s; ++offset) {
        display.drawLine(static_cast<int16_t>(x + x0 * s), static_cast<int16_t>(y + y0 * s + offset),
                         static_cast<int16_t>(x + x1 * s), static_cast<int16_t>(y + y1 * s + offset), color);
    }
}

}  // namespace avatar_art_detail

inline void drawAvatarBadge(Display& display, int16_t x, int16_t y, AvatarId avatar,
                            uint8_t scale, Display::Color background) {
    using namespace avatar_art_detail;

    switch (sanitizeAvatarId(static_cast<uint8_t>(avatar))) {
    case AvatarId::Hero:
        scaledRect(display, x, y, 3, 0, 6, 2, Display::rgb565(220, 44, 44), scale);
        scaledRect(display, x, y, 2, 2, 8, 3, Display::rgb565(220, 44, 44), scale);
        scaledRect(display, x, y, 2, 5, 8, 4, Display::rgb565(244, 196, 152), scale);
        scaledPixel(display, x, y, 4, 6, Display::rgb565(40, 40, 40), scale);
        scaledPixel(display, x, y, 7, 6, Display::rgb565(40, 40, 40), scale);
        scaledRect(display, x, y, 2, 9, 8, 4, Display::rgb565(210, 48, 48), scale);
        scaledRect(display, x, y, 2, 13, 8, 5, Display::rgb565(30, 90, 210), scale);
        scaledRect(display, x, y, 2, 18, 3, 4, Display::rgb565(122, 70, 30), scale);
        scaledRect(display, x, y, 7, 18, 3, 4, Display::rgb565(122, 70, 30), scale);
        break;

    case AvatarId::Pac: {
        const Display::Color outline = Display::rgb565(218, 152, 0);
        const Display::Color body = Display::rgb565(255, 228, 48);
        const Display::Color highlight = Display::rgb565(255, 244, 156);
        const Display::Color eye = Display::rgb565(20, 20, 20);

        scaledRect(display, x, y, 4, 0, 4, 1, outline, scale);
        scaledRect(display, x, y, 2, 1, 7, 1, outline, scale);
        scaledRect(display, x, y, 1, 2, 8, 1, outline, scale);
        scaledRect(display, x, y, 0, 3, 8, 1, outline, scale);
        scaledRect(display, x, y, 0, 4, 7, 2, outline, scale);
        scaledRect(display, x, y, 0, 6, 8, 1, outline, scale);
        scaledRect(display, x, y, 1, 7, 8, 1, outline, scale);
        scaledRect(display, x, y, 2, 8, 7, 1, outline, scale);
        scaledRect(display, x, y, 4, 9, 4, 1, outline, scale);

        scaledRect(display, x, y, 4, 1, 4, 1, body, scale);
        scaledRect(display, x, y, 2, 2, 6, 1, body, scale);
        scaledRect(display, x, y, 1, 3, 6, 1, body, scale);
        scaledRect(display, x, y, 1, 4, 5, 2, body, scale);
        scaledRect(display, x, y, 1, 6, 6, 1, body, scale);
        scaledRect(display, x, y, 2, 7, 6, 1, body, scale);
        scaledRect(display, x, y, 4, 8, 4, 1, body, scale);

        scaledRect(display, x, y, 3, 1, 2, 1, highlight, scale);
        scaledRect(display, x, y, 2, 2, 2, 1, highlight, scale);
        scaledPixel(display, x, y, 3, 3, eye, scale);
        scaledLine(display, x, y, 6, 4, 10, 2, outline, scale);
        scaledLine(display, x, y, 6, 5, 10, 7, outline, scale);
        scaledRect(display, x, y, 7, 4, 4, 2, background, scale);
        break;
    }

    case AvatarId::Stick:
        scaledRect(display, x, y, 3, 0, 4, 4, Display::rgb565(255, 255, 255), scale);
        scaledRect(display, x, y, 4, 1, 2, 2, background, scale);
        scaledLine(display, x, y, 5, 4, 5, 11, Display::rgb565(255, 255, 255), scale);
        scaledLine(display, x, y, 5, 6, 2, 8, Display::rgb565(255, 255, 255), scale);
        scaledLine(display, x, y, 5, 6, 8, 8, Display::rgb565(255, 255, 255), scale);
        scaledLine(display, x, y, 5, 11, 2, 15, Display::rgb565(255, 255, 255), scale);
        scaledLine(display, x, y, 5, 11, 8, 15, Display::rgb565(255, 255, 255), scale);
        break;

    case AvatarId::Count:
        break;
    }
}

inline void drawPlayerAvatar(Display& display, int16_t x, int16_t y, AvatarId avatar) {
    switch (sanitizeAvatarId(static_cast<uint8_t>(avatar))) {
    case AvatarId::Hero:
        display.fillRect(x, y, 12, 16, Display::rgb565(210, 48, 48));
        display.fillRect(x, static_cast<int16_t>(y + 10), 12, 6, Display::rgb565(30, 90, 210));
        display.fillRect(static_cast<int16_t>(x + 3), static_cast<int16_t>(y - 4), 6, 5, Display::rgb565(230, 40, 40));
        break;

    case AvatarId::Pac: {
        const int16_t body_y = static_cast<int16_t>(y + 2);
        const Display::Color outline = Display::rgb565(218, 152, 0);
        const Display::Color body = Display::rgb565(255, 228, 48);
        const Display::Color highlight = Display::rgb565(255, 244, 156);
        const Display::Color eye = Display::rgb565(20, 20, 20);

        display.fillRect(static_cast<int16_t>(x + 3), static_cast<int16_t>(body_y + 0), 5, 1, outline);
        display.fillRect(static_cast<int16_t>(x + 1), static_cast<int16_t>(body_y + 1), 8, 1, outline);
        display.fillRect(x, static_cast<int16_t>(body_y + 2), 8, 1, outline);
        display.fillRect(x, static_cast<int16_t>(body_y + 3), 7, 3, outline);
        display.fillRect(x, static_cast<int16_t>(body_y + 6), 8, 1, outline);
        display.fillRect(static_cast<int16_t>(x + 1), static_cast<int16_t>(body_y + 7), 8, 1, outline);
        display.fillRect(static_cast<int16_t>(x + 3), static_cast<int16_t>(body_y + 8), 5, 1, outline);

        display.fillRect(static_cast<int16_t>(x + 3), static_cast<int16_t>(body_y + 1), 4, 1, body);
        display.fillRect(static_cast<int16_t>(x + 1), static_cast<int16_t>(body_y + 2), 6, 1, body);
        display.fillRect(static_cast<int16_t>(x + 1), static_cast<int16_t>(body_y + 3), 5, 3, body);
        display.fillRect(static_cast<int16_t>(x + 1), static_cast<int16_t>(body_y + 6), 6, 1, body);
        display.fillRect(static_cast<int16_t>(x + 3), static_cast<int16_t>(body_y + 7), 4, 1, body);

        display.fillRect(static_cast<int16_t>(x + 2), static_cast<int16_t>(body_y + 2), 2, 1, highlight);
        display.drawPixel(static_cast<int16_t>(x + 3), static_cast<int16_t>(body_y + 3), eye);
        display.drawLine(static_cast<int16_t>(x + 6), static_cast<int16_t>(body_y + 4),
                         static_cast<int16_t>(x + 10), static_cast<int16_t>(body_y + 2), outline);
        display.drawLine(static_cast<int16_t>(x + 6), static_cast<int16_t>(body_y + 5),
                         static_cast<int16_t>(x + 10), static_cast<int16_t>(body_y + 7), outline);
        break;
    }

    case AvatarId::Stick:
        display.drawRect(static_cast<int16_t>(x + 4), y, 4, 4, Display::rgb565(255, 255, 255));
        display.fillRect(static_cast<int16_t>(x + 5), static_cast<int16_t>(y + 1), 2, 2, Display::rgb565(4, 8, 20));
        display.drawLine(static_cast<int16_t>(x + 6), static_cast<int16_t>(y + 4),
                         static_cast<int16_t>(x + 6), static_cast<int16_t>(y + 11), Display::rgb565(255, 255, 255));
        display.drawLine(static_cast<int16_t>(x + 6), static_cast<int16_t>(y + 6),
                         static_cast<int16_t>(x + 2), static_cast<int16_t>(y + 8), Display::rgb565(255, 255, 255));
        display.drawLine(static_cast<int16_t>(x + 6), static_cast<int16_t>(y + 6),
                         static_cast<int16_t>(x + 10), static_cast<int16_t>(y + 8), Display::rgb565(255, 255, 255));
        display.drawLine(static_cast<int16_t>(x + 6), static_cast<int16_t>(y + 11),
                         static_cast<int16_t>(x + 3), static_cast<int16_t>(y + 15), Display::rgb565(255, 255, 255));
        display.drawLine(static_cast<int16_t>(x + 6), static_cast<int16_t>(y + 11),
                         static_cast<int16_t>(x + 9), static_cast<int16_t>(y + 15), Display::rgb565(255, 255, 255));
        break;

    case AvatarId::Count:
        break;
    }
}

}  // namespace picoboy
