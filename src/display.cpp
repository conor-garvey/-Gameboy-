#include "picoboy/display.hpp"

#include <algorithm>
#include <cstdlib>

#include "hardware/gpio.h"
#include "pico/time.h"

namespace picoboy {

namespace {

constexpr uint32_t microseconds_per_second = 1000 * 1000;
constexpr uint8_t madctl_my = 0x80;
constexpr uint8_t madctl_mx = 0x40;
constexpr uint8_t madctl_mv = 0x20;
constexpr uint8_t madctl_bgr = 0x08;

const uint8_t* glyphForCharacter(char character) {
    static const uint8_t glyph_space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t glyph_exclamation[5] = {0x00, 0x00, 0x5F, 0x00, 0x00};
    static const uint8_t glyph_double_quote[5] = {0x00, 0x07, 0x00, 0x07, 0x00};
    static const uint8_t glyph_hash[5] = {0x14, 0x7F, 0x14, 0x7F, 0x14};
    static const uint8_t glyph_dollar[5] = {0x24, 0x2A, 0x7F, 0x2A, 0x12};
    static const uint8_t glyph_percent[5] = {0x23, 0x13, 0x08, 0x64, 0x62};
    static const uint8_t glyph_ampersand[5] = {0x36, 0x49, 0x55, 0x22, 0x50};
    static const uint8_t glyph_apostrophe[5] = {0x00, 0x05, 0x03, 0x00, 0x00};
    static const uint8_t glyph_left_paren[5] = {0x00, 0x1C, 0x22, 0x41, 0x00};
    static const uint8_t glyph_right_paren[5] = {0x00, 0x41, 0x22, 0x1C, 0x00};
    static const uint8_t glyph_asterisk[5] = {0x14, 0x08, 0x3E, 0x08, 0x14};
    static const uint8_t glyph_plus[5] = {0x08, 0x08, 0x3E, 0x08, 0x08};
    static const uint8_t glyph_comma[5] = {0x00, 0x40, 0x20, 0x00, 0x00};
    static const uint8_t glyph_minus[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t glyph_period[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
    static const uint8_t glyph_slash[5] = {0x20, 0x10, 0x08, 0x04, 0x02};
    static const uint8_t glyph_0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
    static const uint8_t glyph_1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
    static const uint8_t glyph_2[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
    static const uint8_t glyph_3[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
    static const uint8_t glyph_4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
    static const uint8_t glyph_5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
    static const uint8_t glyph_6[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
    static const uint8_t glyph_7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
    static const uint8_t glyph_8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
    static const uint8_t glyph_9[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};
    static const uint8_t glyph_colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
    static const uint8_t glyph_semicolon[5] = {0x00, 0x56, 0x36, 0x00, 0x00};
    static const uint8_t glyph_less_than[5] = {0x08, 0x14, 0x22, 0x41, 0x00};
    static const uint8_t glyph_equals[5] = {0x14, 0x14, 0x14, 0x14, 0x14};
    static const uint8_t glyph_greater_than[5] = {0x00, 0x41, 0x22, 0x14, 0x08};
    static const uint8_t glyph_question[5] = {0x02, 0x01, 0x51, 0x09, 0x06};
    static const uint8_t glyph_at[5] = {0x32, 0x49, 0x79, 0x41, 0x3E};
    static const uint8_t glyph_a[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
    static const uint8_t glyph_b[5] = {0x7F, 0x49, 0x49, 0x49, 0x36};
    static const uint8_t glyph_c[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
    static const uint8_t glyph_d[5] = {0x7F, 0x41, 0x41, 0x22, 0x1C};
    static const uint8_t glyph_e[5] = {0x7F, 0x49, 0x49, 0x49, 0x41};
    static const uint8_t glyph_f[5] = {0x7F, 0x09, 0x09, 0x09, 0x01};
    static const uint8_t glyph_g[5] = {0x3E, 0x41, 0x49, 0x49, 0x7A};
    static const uint8_t glyph_h[5] = {0x7F, 0x08, 0x08, 0x08, 0x7F};
    static const uint8_t glyph_i[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};
    static const uint8_t glyph_j[5] = {0x20, 0x40, 0x41, 0x3F, 0x01};
    static const uint8_t glyph_k[5] = {0x7F, 0x08, 0x14, 0x22, 0x41};
    static const uint8_t glyph_l[5] = {0x7F, 0x40, 0x40, 0x40, 0x40};
    static const uint8_t glyph_m[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
    static const uint8_t glyph_n[5] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
    static const uint8_t glyph_o[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
    static const uint8_t glyph_p[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
    static const uint8_t glyph_q[5] = {0x3E, 0x41, 0x51, 0x21, 0x5E};
    static const uint8_t glyph_r[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
    static const uint8_t glyph_s[5] = {0x46, 0x49, 0x49, 0x49, 0x31};
    static const uint8_t glyph_t[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
    static const uint8_t glyph_u[5] = {0x3F, 0x40, 0x40, 0x40, 0x3F};
    static const uint8_t glyph_v[5] = {0x1F, 0x20, 0x40, 0x20, 0x1F};
    static const uint8_t glyph_w[5] = {0x3F, 0x40, 0x38, 0x40, 0x3F};
    static const uint8_t glyph_x[5] = {0x63, 0x14, 0x08, 0x14, 0x63};
    static const uint8_t glyph_y[5] = {0x07, 0x08, 0x70, 0x08, 0x07};
    static const uint8_t glyph_z[5] = {0x61, 0x51, 0x49, 0x45, 0x43};
    static const uint8_t glyph_left_bracket[5] = {0x00, 0x7F, 0x41, 0x41, 0x00};
    static const uint8_t glyph_backslash[5] = {0x02, 0x04, 0x08, 0x10, 0x20};
    static const uint8_t glyph_right_bracket[5] = {0x00, 0x41, 0x41, 0x7F, 0x00};
    static const uint8_t glyph_caret[5] = {0x04, 0x02, 0x01, 0x02, 0x04};
    static const uint8_t glyph_underscore[5] = {0x40, 0x40, 0x40, 0x40, 0x40};

    if (character >= 'a' && character <= 'z') {
        character = static_cast<char>(character - ('a' - 'A'));
    }

    switch (character) {
    case ' ': return glyph_space;
    case '!': return glyph_exclamation;
    case '"': return glyph_double_quote;
    case '#': return glyph_hash;
    case '$': return glyph_dollar;
    case '%': return glyph_percent;
    case '&': return glyph_ampersand;
    case '\'': return glyph_apostrophe;
    case '(': return glyph_left_paren;
    case ')': return glyph_right_paren;
    case '*': return glyph_asterisk;
    case '+': return glyph_plus;
    case ',': return glyph_comma;
    case '-': return glyph_minus;
    case '.': return glyph_period;
    case '/': return glyph_slash;
    case '0': return glyph_0;
    case '1': return glyph_1;
    case '2': return glyph_2;
    case '3': return glyph_3;
    case '4': return glyph_4;
    case '5': return glyph_5;
    case '6': return glyph_6;
    case '7': return glyph_7;
    case '8': return glyph_8;
    case '9': return glyph_9;
    case ':': return glyph_colon;
    case ';': return glyph_semicolon;
    case '<': return glyph_less_than;
    case '=': return glyph_equals;
    case '>': return glyph_greater_than;
    case '?': return glyph_question;
    case '@': return glyph_at;
    case 'A': return glyph_a;
    case 'B': return glyph_b;
    case 'C': return glyph_c;
    case 'D': return glyph_d;
    case 'E': return glyph_e;
    case 'F': return glyph_f;
    case 'G': return glyph_g;
    case 'H': return glyph_h;
    case 'I': return glyph_i;
    case 'J': return glyph_j;
    case 'K': return glyph_k;
    case 'L': return glyph_l;
    case 'M': return glyph_m;
    case 'N': return glyph_n;
    case 'O': return glyph_o;
    case 'P': return glyph_p;
    case 'Q': return glyph_q;
    case 'R': return glyph_r;
    case 'S': return glyph_s;
    case 'T': return glyph_t;
    case 'U': return glyph_u;
    case 'V': return glyph_v;
    case 'W': return glyph_w;
    case 'X': return glyph_x;
    case 'Y': return glyph_y;
    case 'Z': return glyph_z;
    case '[': return glyph_left_bracket;
    case '\\': return glyph_backslash;
    case ']': return glyph_right_bracket;
    case '^': return glyph_caret;
    case '_': return glyph_underscore;
    default: return glyph_question;
    }
}

}

Display::Display(spi_inst_t* spi)
    : spi_(spi),
      initialized_(false),
      target_fps_(60),
      target_frame_us_(microseconds_per_second / 60),
      frame_start_(get_absolute_time()),
      rotation_(Rotation::Landscape90),
      physical_width_(DefaultWidth),
      physical_height_(DefaultHeight),
      viewport_x_(0),
      viewport_y_(0),
      width_(DefaultWidth),
      height_(DefaultHeight),
      commands_{},
      command_count_(0),
      text_buffer_{},
      text_buffer_size_(0),
      background_color_(0),
      strip_buffer_{} {}

void Display::init() {
    spi_init(spi_, SpiClockHz);
    spi_set_format(spi_, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(PinSck, GPIO_FUNC_SPI);
    gpio_set_function(PinMosi, GPIO_FUNC_SPI);

    gpio_init(PinCs);
    gpio_set_dir(PinCs, GPIO_OUT);
    gpio_put(PinCs, 1);

    gpio_init(PinDc);
    gpio_set_dir(PinDc, GPIO_OUT);
    gpio_put(PinDc, 1);

    gpio_init(PinRst);
    gpio_set_dir(PinRst, GPIO_OUT);
    gpio_put(PinRst, 1);

    gpio_init(PinLed);
    gpio_set_dir(PinLed, GPIO_OUT);
    gpio_put(PinLed, 0);

    initialized_ = true;
    hardwareReset();
    initializePanel();
    backlightOn();
}

void Display::hardwareReset() {
    gpio_put(PinRst, 1);
    sleep_ms(5);

    gpio_put(PinRst, 0);
    sleep_ms(20);

    gpio_put(PinRst, 1);
    sleep_ms(150);
}

void Display::backlightOn() {
    gpio_put(PinLed, 1);
}

void Display::backlightOff() {
    gpio_put(PinLed, 0);
}

void Display::setTargetFps(uint32_t target_fps) {
    if (target_fps == 0) {
        target_fps = 1;
    }

    target_fps_ = target_fps;
    target_frame_us_ = microseconds_per_second / target_fps_;
}

uint32_t Display::targetFps() const {
    return target_fps_;
}

uint32_t Display::targetFrameUs() const {
    return target_frame_us_;
}

void Display::beginFrameTiming() {
    frame_start_ = get_absolute_time();
}

void Display::waitForNextFrame() {
    const absolute_time_t now = get_absolute_time();
    const int64_t elapsed_us = absolute_time_diff_us(frame_start_, now);

    if (elapsed_us < static_cast<int64_t>(target_frame_us_)) {
        sleep_us(target_frame_us_ - static_cast<uint32_t>(elapsed_us));
    }
}

void Display::initializePanel() {
    static const uint8_t power_b[] = {0x00, 0xC1, 0x30};
    static const uint8_t power_seq[] = {0x64, 0x03, 0x12, 0x81};
    static const uint8_t driver_timing_a[] = {0x85, 0x00, 0x78};
    static const uint8_t power_a[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    static const uint8_t pump_ratio[] = {0x20};
    static const uint8_t driver_timing_b[] = {0x00, 0x00};
    static const uint8_t power_control_1[] = {0x23};
    static const uint8_t power_control_2[] = {0x10};
    static const uint8_t vcom_control_1[] = {0x3E, 0x28};
    static const uint8_t vcom_control_2[] = {0x86};
    static const uint8_t pixel_format[] = {0x55};
    static const uint8_t frame_rate[] = {0x00, 0x18};
    static const uint8_t display_function[] = {0x08, 0x82, 0x27};
    static const uint8_t gamma_disable[] = {0x00};
    static const uint8_t gamma_curve[] = {0x01};
    static const uint8_t positive_gamma[] = {
        0x0F, 0x31, 0x2B, 0x0C, 0x0E,
        0x08, 0x4E, 0xF1, 0x37, 0x07,
        0x10, 0x03, 0x0E, 0x09, 0x00
    };
    static const uint8_t negative_gamma[] = {
        0x00, 0x0E, 0x14, 0x03, 0x11,
        0x07, 0x31, 0xC1, 0x48, 0x08,
        0x0F, 0x0C, 0x31, 0x36, 0x0F
    };

    writeCommand(0x01);
    sleep_ms(150);

    writeCommandWithData(0xCF, power_b, sizeof(power_b));
    writeCommandWithData(0xED, power_seq, sizeof(power_seq));
    writeCommandWithData(0xE8, driver_timing_a, sizeof(driver_timing_a));
    writeCommandWithData(0xCB, power_a, sizeof(power_a));
    writeCommandWithData(0xF7, pump_ratio, sizeof(pump_ratio));
    writeCommandWithData(0xEA, driver_timing_b, sizeof(driver_timing_b));
    writeCommandWithData(0xC0, power_control_1, sizeof(power_control_1));
    writeCommandWithData(0xC1, power_control_2, sizeof(power_control_2));
    writeCommandWithData(0xC5, vcom_control_1, sizeof(vcom_control_1));
    writeCommandWithData(0xC7, vcom_control_2, sizeof(vcom_control_2));
    writeCommandWithData(0x3A, pixel_format, sizeof(pixel_format));
    writeCommandWithData(0xB1, frame_rate, sizeof(frame_rate));
    writeCommandWithData(0xB6, display_function, sizeof(display_function));
    writeCommandWithData(0xF2, gamma_disable, sizeof(gamma_disable));
    writeCommandWithData(0x26, gamma_curve, sizeof(gamma_curve));
    writeCommandWithData(0xE0, positive_gamma, sizeof(positive_gamma));
    writeCommandWithData(0xE1, negative_gamma, sizeof(negative_gamma));

    writeCommand(0x11);
    sleep_ms(120);

    writeCommand(0x29);
    sleep_ms(20);

    setRotation(rotation_);
    setViewport(width_, height_);
}

void Display::setRotation(Rotation rotation) {
    rotation_ = rotation;
    uint8_t madctl = madctl_bgr;
    updatePhysicalDimensions();

    switch (rotation_) {
    case Rotation::Portrait0:
        madctl = static_cast<uint8_t>(madctl_mx | madctl_bgr);
        break;

    case Rotation::Landscape90:
        madctl = static_cast<uint8_t>(madctl_mv | madctl_bgr);
        break;

    case Rotation::Portrait180:
        madctl = static_cast<uint8_t>(madctl_my | madctl_bgr);
        break;

    case Rotation::Landscape270:
        madctl = static_cast<uint8_t>(madctl_mx | madctl_my | madctl_mv | madctl_bgr);
        break;
    }

    if (width_ > physical_width_ || height_ > physical_height_) {
        width_ = physical_width_;
        height_ = physical_height_;
    }

    updateViewportOffsets();

    if (initialized_) {
        writeCommandWithData(0x36, &madctl, 1);
        clearPhysicalScreen(rgb565(0, 0, 0));
    }
}

void Display::setViewport(uint16_t width, uint16_t height) {
    if (width == 0 || width > physical_width_) {
        width = physical_width_;
    }

    if (height == 0 || height > physical_height_) {
        height = physical_height_;
    }

    width_ = width;
    height_ = height;
    updateViewportOffsets();

    if (initialized_) {
        clearPhysicalScreen(rgb565(0, 0, 0));
    }
}

Display::Rotation Display::rotation() const {
    return rotation_;
}

uint16_t Display::width() const {
    return width_;
}

uint16_t Display::height() const {
    return height_;
}

void Display::beginFrame(Color background_color) {
    command_count_ = 0;
    text_buffer_size_ = 0;
    background_color_ = background_color;
}

void Display::clear(Color background_color) {
    beginFrame(background_color);
}

bool Display::drawPixel(int16_t x, int16_t y, Color color) {
    return pushCommand(CommandType::Pixel, x, y, 0, 0, color);
}

bool Display::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color) {
    return pushCommand(CommandType::Line, x0, y0, x1, y1, color);
}

bool Display::fillRect(int16_t x, int16_t y, int16_t width, int16_t height, Color color) {
    if (width <= 0 || height <= 0) {
        return true;
    }

    return pushCommand(CommandType::FillRect, x, y, width, height, color);
}

bool Display::drawRect(int16_t x, int16_t y, int16_t width, int16_t height, Color color) {
    if (width <= 0 || height <= 0) {
        return true;
    }

    return pushCommand(CommandType::Rect, x, y, width, height, color);
}

bool Display::drawChar(int16_t x, int16_t y, char character, Color color, uint8_t scale) {
    char text[2] = {character, '\0'};
    return drawText(x, y, text, color, scale);
}

bool Display::drawText(int16_t x, int16_t y, const char* text, Color color, uint8_t scale) {
    if (text == nullptr) {
        return false;
    }

    if (scale == 0) {
        scale = 1;
    }

    size_t length = 0;
    while (text[length] != '\0') {
        ++length;
    }

    if (length == 0) {
        return true;
    }

    return pushTextCommand(x, y, text, length, color, scale);
}

int16_t Display::measureTextWidth(const char* text, uint8_t scale) const {
    if (text == nullptr) {
        return 0;
    }

    if (scale == 0) {
        scale = 1;
    }

    int16_t current_width = 0;
    int16_t max_width = 0;
    const int16_t advance = static_cast<int16_t>((FontWidth + FontSpacing) * scale);

    for (size_t index = 0; text[index] != '\0'; ++index) {
        if (text[index] == '\n') {
            max_width = std::max(max_width, current_width);
            current_width = 0;
            continue;
        }

        current_width = static_cast<int16_t>(current_width + advance);
    }

    return std::max(max_width, current_width);
}

void Display::present() {
    for (int16_t strip_y = 0; strip_y < static_cast<int16_t>(height_); strip_y += StripHeight) {
        const int16_t strip_height = std::min<int16_t>(
            StripHeight, static_cast<int16_t>(static_cast<int16_t>(height_) - strip_y));
        renderStrip(strip_y, strip_height);
    }
}

size_t Display::commandCount() const {
    return command_count_;
}

void Display::writeCommand(uint8_t command) {
    select();
    setCommandMode();
    spi_write_blocking(spi_, &command, 1);
    deselect();
}

void Display::writeData(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) {
        return;
    }

    select();
    setDataMode();
    spi_write_blocking(spi_, data, length);
    deselect();
}

void Display::writeDataByte(uint8_t value) {
    writeData(&value, 1);
}

void Display::setAddressWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    if (width == 0 || height == 0 || x >= width_ || y >= height_) {
        return;
    }

    if (x + width > width_) {
        width = static_cast<uint16_t>(width_ - x);
    }

    if (y + height > height_) {
        height = static_cast<uint16_t>(height_ - y);
    }

    setPhysicalAddressWindow(static_cast<uint16_t>(viewport_x_ + x),
                             static_cast<uint16_t>(viewport_y_ + y),
                             width,
                             height);
}

void Display::writePixels(const uint8_t* data, size_t length) {
    writeData(data, length);
}

bool Display::pushCommand(CommandType type, int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color) {
    if (command_count_ >= commands_.size()) {
        return false;
    }

    commands_[command_count_++] = Command{type, x0, y0, x1, y1, 0, color};
    return true;
}

void Display::clearStrip(int16_t strip_height) {
    const size_t pixel_count = static_cast<size_t>(width_) * static_cast<size_t>(strip_height);
    for (size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
        writeColorToPixel(pixel_index, background_color_);
    }
}

void Display::renderStrip(int16_t strip_y, int16_t strip_height) {
    clearStrip(strip_height);

    for (size_t index = 0; index < command_count_; ++index) {
        renderCommandToStrip(commands_[index], strip_y, strip_height);
    }

    setAddressWindow(0, static_cast<uint16_t>(strip_y), width_, static_cast<uint16_t>(strip_height));
    writePixels(strip_buffer_.data(), static_cast<size_t>(width_) * static_cast<size_t>(strip_height) * 2);
}

void Display::renderCommandToStrip(const Command& command, int16_t strip_y, int16_t strip_height) {
    switch (command.type) {
    case CommandType::Pixel:
        plotInStrip(command.x0, command.y0, strip_y, strip_height, command.color);
        break;

    case CommandType::Line:
        renderLineToStrip(command.x0, command.y0, command.x1, command.y1, command.color, strip_y, strip_height);
        break;

    case CommandType::FillRect:
        renderFillRectToStrip(command.x0, command.y0, command.x1, command.y1, command.color, strip_y, strip_height);
        break;

    case CommandType::Rect:
        renderFillRectToStrip(command.x0, command.y0, command.x1, 1, command.color, strip_y, strip_height);
        renderFillRectToStrip(command.x0, static_cast<int16_t>(command.y0 + command.y1 - 1), command.x1, 1,
                              command.color, strip_y, strip_height);
        renderFillRectToStrip(command.x0, command.y0, 1, command.y1, command.color, strip_y, strip_height);
        renderFillRectToStrip(static_cast<int16_t>(command.x0 + command.x1 - 1), command.y0, 1, command.y1,
                              command.color, strip_y, strip_height);
        break;

    case CommandType::Text:
        renderTextToStrip(command, strip_y, strip_height);
        break;
    }
}

void Display::renderFillRectToStrip(int16_t x, int16_t y, int16_t width, int16_t height, Color color,
                                    int16_t strip_y, int16_t strip_height) {
    if (width <= 0 || height <= 0) {
        return;
    }

    const int16_t x_start = std::max<int16_t>(0, x);
    const int16_t y_start = std::max<int16_t>(strip_y, y);
    const int16_t x_end = std::min<int16_t>(static_cast<int16_t>(width_) - 1, static_cast<int16_t>(x + width - 1));
    const int16_t y_end = std::min<int16_t>(static_cast<int16_t>(height_) - 1, static_cast<int16_t>(y + height - 1));
    const int16_t strip_end = static_cast<int16_t>(strip_y + strip_height - 1);

    if (x_start > x_end || y_start > y_end || y_start > strip_end || y_end < strip_y) {
        return;
    }

    const int16_t clipped_y_start = std::max<int16_t>(y_start, strip_y);
    const int16_t clipped_y_end = std::min<int16_t>(y_end, strip_end);

    for (int16_t py = clipped_y_start; py <= clipped_y_end; ++py) {
        const int16_t local_y = static_cast<int16_t>(py - strip_y);
        size_t pixel_index = static_cast<size_t>(local_y) * static_cast<size_t>(width_) + static_cast<size_t>(x_start);

        for (int16_t px = x_start; px <= x_end; ++px) {
            (void)px;
            writeColorToPixel(pixel_index++, color);
        }
    }
}

void Display::renderLineToStrip(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color,
                                int16_t strip_y, int16_t strip_height) {
    int16_t dx = static_cast<int16_t>(std::abs(x1 - x0));
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = static_cast<int16_t>(-std::abs(y1 - y0));
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t error = static_cast<int16_t>(dx + dy);

    while (true) {
        plotInStrip(x0, y0, strip_y, strip_height, color);

        if (x0 == x1 && y0 == y1) {
            break;
        }

        const int16_t doubled_error = static_cast<int16_t>(error * 2);
        if (doubled_error >= dy) {
            error = static_cast<int16_t>(error + dy);
            x0 = static_cast<int16_t>(x0 + sx);
        }

        if (doubled_error <= dx) {
            error = static_cast<int16_t>(error + dx);
            y0 = static_cast<int16_t>(y0 + sy);
        }
    }
}

void Display::plotInStrip(int16_t x, int16_t y, int16_t strip_y, int16_t strip_height, Color color) {
    if (x < 0 || x >= static_cast<int16_t>(width_) || y < 0 || y >= static_cast<int16_t>(height_)) {
        return;
    }

    if (y < strip_y || y >= strip_y + strip_height) {
        return;
    }

    const int16_t local_y = static_cast<int16_t>(y - strip_y);
    const size_t pixel_index = static_cast<size_t>(local_y) * static_cast<size_t>(width_) + static_cast<size_t>(x);
    writeColorToPixel(pixel_index, color);
}

void Display::writeColorToPixel(size_t pixel_index, Color color) {
    const size_t byte_index = pixel_index * 2;
    strip_buffer_[byte_index] = static_cast<uint8_t>(color >> 8);
    strip_buffer_[byte_index + 1] = static_cast<uint8_t>(color & 0xFF);
}

void Display::updatePhysicalDimensions() {
    switch (rotation_) {
    case Rotation::Portrait0:
    case Rotation::Portrait180:
        physical_width_ = NativeWidth;
        physical_height_ = NativeHeight;
        break;

    case Rotation::Landscape90:
    case Rotation::Landscape270:
        physical_width_ = NativeHeight;
        physical_height_ = NativeWidth;
        break;
    }
}

void Display::updateViewportOffsets() {
    viewport_x_ = static_cast<uint16_t>((physical_width_ - width_) / 2);
    viewport_y_ = static_cast<uint16_t>((physical_height_ - height_) / 2);
}

void Display::setPhysicalAddressWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    if (width == 0 || height == 0 || x >= physical_width_ || y >= physical_height_) {
        return;
    }

    if (x + width > physical_width_) {
        width = static_cast<uint16_t>(physical_width_ - x);
    }

    if (y + height > physical_height_) {
        height = static_cast<uint16_t>(physical_height_ - y);
    }

    const uint16_t x_end = static_cast<uint16_t>(x + width - 1);
    const uint16_t y_end = static_cast<uint16_t>(y + height - 1);

    const uint8_t column_data[] = {
        static_cast<uint8_t>(x >> 8),
        static_cast<uint8_t>(x & 0xFF),
        static_cast<uint8_t>(x_end >> 8),
        static_cast<uint8_t>(x_end & 0xFF),
    };

    const uint8_t row_data[] = {
        static_cast<uint8_t>(y >> 8),
        static_cast<uint8_t>(y & 0xFF),
        static_cast<uint8_t>(y_end >> 8),
        static_cast<uint8_t>(y_end & 0xFF),
    };

    writeCommandWithData(0x2A, column_data, sizeof(column_data));
    writeCommandWithData(0x2B, row_data, sizeof(row_data));
    writeCommand(0x2C);
}

void Display::fillPhysicalRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, Color color) {
    if (width == 0 || height == 0) {
        return;
    }

    const uint8_t hi = static_cast<uint8_t>(color >> 8);
    const uint8_t lo = static_cast<uint8_t>(color & 0xFF);

    for (uint16_t chunk_y = 0; chunk_y < height; chunk_y = static_cast<uint16_t>(chunk_y + StripHeight)) {
        const uint16_t chunk_height = static_cast<uint16_t>(
            std::min<int32_t>(StripHeight, static_cast<int32_t>(height - chunk_y)));
        const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(chunk_height);

        for (size_t index = 0; index < pixel_count; ++index) {
            const size_t byte_index = index * 2;
            strip_buffer_[byte_index] = hi;
            strip_buffer_[byte_index + 1] = lo;
        }

        setPhysicalAddressWindow(x, static_cast<uint16_t>(y + chunk_y), width, chunk_height);
        writePixels(strip_buffer_.data(), pixel_count * 2);
    }
}

void Display::clearPhysicalScreen(Color color) {
    fillPhysicalRect(0, 0, physical_width_, physical_height_, color);
}

bool Display::pushTextCommand(int16_t x, int16_t y, const char* text, size_t length, Color color, uint8_t scale) {
    if (command_count_ >= commands_.size()) {
        return false;
    }

    if (text_buffer_size_ + length > text_buffer_.size()) {
        return false;
    }

    const uint16_t text_offset = static_cast<uint16_t>(text_buffer_size_);
    for (size_t index = 0; index < length; ++index) {
        text_buffer_[text_buffer_size_++] = text[index];
    }

    commands_[command_count_++] = Command{
        CommandType::Text,
        x,
        y,
        static_cast<int16_t>(text_offset),
        static_cast<int16_t>(scale),
        static_cast<uint16_t>(length),
        color
    };

    return true;
}

void Display::renderTextToStrip(const Command& command, int16_t strip_y, int16_t strip_height) {
    const int16_t origin_x = command.x0;
    int16_t cursor_x = origin_x;
    int16_t cursor_y = command.y0;
    const uint16_t text_offset = static_cast<uint16_t>(command.x1);
    const uint16_t length = command.data;
    const uint8_t scale = static_cast<uint8_t>(command.y1 <= 0 ? 1 : command.y1);
    const int16_t advance_x = static_cast<int16_t>((FontWidth + FontSpacing) * scale);
    const int16_t advance_y = static_cast<int16_t>((FontHeight + LineSpacing) * scale);

    for (uint16_t index = 0; index < length; ++index) {
        const char character = text_buffer_[static_cast<size_t>(text_offset) + index];

        if (character == '\r') {
            continue;
        }

        if (character == '\n') {
            cursor_x = origin_x;
            cursor_y = static_cast<int16_t>(cursor_y + advance_y);
            continue;
        }

        renderGlyphToStrip(cursor_x, cursor_y, character, command.color, scale, strip_y, strip_height);
        cursor_x = static_cast<int16_t>(cursor_x + advance_x);
    }
}

void Display::renderGlyphToStrip(int16_t x, int16_t y, char character, Color color, uint8_t scale,
                                 int16_t strip_y, int16_t strip_height) {
    const uint8_t* glyph = glyphForCharacter(character);

    for (int16_t column = 0; column < FontWidth; ++column) {
        const uint8_t bits = glyph[column];

        for (int16_t row = 0; row < FontHeight; ++row) {
            if ((bits & (1u << row)) == 0) {
                continue;
            }

            if (scale == 1) {
                plotInStrip(static_cast<int16_t>(x + column), static_cast<int16_t>(y + row), strip_y, strip_height,
                            color);
                continue;
            }

            renderFillRectToStrip(static_cast<int16_t>(x + column * scale), static_cast<int16_t>(y + row * scale),
                                  scale, scale, color, strip_y, strip_height);
        }
    }
}

void Display::writeCommandWithData(uint8_t command, const uint8_t* data, size_t length) {
    writeCommand(command);
    writeData(data, length);
}

void Display::select() {
    gpio_put(PinCs, 0);
}

void Display::deselect() {
    gpio_put(PinCs, 1);
}

void Display::setCommandMode() {
    gpio_put(PinDc, 0);
}

void Display::setDataMode() {
    gpio_put(PinDc, 1);
}

}  // namespace picoboy
