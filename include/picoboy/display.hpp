#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "hardware/spi.h"
#include "pico/time.h"

namespace picoboy {

class Display {
public:
    using Color = uint16_t;
    enum class Rotation : uint8_t {
        Portrait0,
        Landscape90,
        Portrait180,
        Landscape270,
    };

    static constexpr uint16_t NativeWidth = 240;
    static constexpr uint16_t NativeHeight = 320;
    static constexpr uint16_t DefaultWidth = 320;
    static constexpr uint16_t DefaultHeight = 240;
    static constexpr uint16_t MaxWidth = 320;
    static constexpr uint16_t MaxHeight = 320;
    static constexpr int16_t StripHeight = 16;
    static constexpr size_t MaxCommands = 256;
    static constexpr size_t MaxTextBytes = 512;
    static constexpr int16_t FontWidth = 5;
    static constexpr int16_t FontHeight = 7;
    static constexpr int16_t FontSpacing = 1;
    static constexpr int16_t LineSpacing = 1;

    static constexpr uint8_t PinCs = 17;
    static constexpr uint8_t PinSck = 18;
    static constexpr uint8_t PinMosi = 19;
    static constexpr uint8_t PinDc = 20;
    static constexpr uint8_t PinRst = 21;
    static constexpr uint8_t PinLed = 22;

    static constexpr uint32_t SpiClockHz = 10 * 1000 * 1000;
    static constexpr uint32_t OriginalSpiClockHz = 20 * 1000 * 1000;

    explicit Display(spi_inst_t* spi = spi0);

    static constexpr Color rgb565(uint8_t red, uint8_t green, uint8_t blue) {
        return static_cast<Color>(((red & 0xF8) << 8) |
                                  ((green & 0xFC) << 3) |
                                  (blue >> 3));
    }

    void init();
    void hardwareReset();
    void backlightOn();
    void backlightOff();
    void initializePanel();
    void setRotation(Rotation rotation);
    void setViewport(uint16_t width, uint16_t height);
    Rotation rotation() const;
    uint16_t width() const;
    uint16_t height() const;

    void setSpiClockHz(uint32_t spi_clock_hz);
    uint32_t spiClockHz() const;
    void setTargetFps(uint32_t target_fps);
    uint32_t targetFps() const;
    uint32_t targetFrameUs() const;
    void beginFrameTiming();
    void waitForNextFrame();

    void beginFrame(Color background_color);
    void clear(Color background_color);
    bool drawPixel(int16_t x, int16_t y, Color color);
    bool drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color);
    bool fillRect(int16_t x, int16_t y, int16_t width, int16_t height, Color color);
    bool drawRect(int16_t x, int16_t y, int16_t width, int16_t height, Color color);
    bool drawChar(int16_t x, int16_t y, char character, Color color, uint8_t scale = 1);
    bool drawText(int16_t x, int16_t y, const char* text, Color color, uint8_t scale = 1);
    int16_t measureTextWidth(const char* text, uint8_t scale = 1) const;
    void present();
    size_t commandCount() const;

    void writeCommand(uint8_t command);
    void writeData(const uint8_t* data, size_t length);
    void writeDataByte(uint8_t value);
    void setAddressWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    void writePixels(const uint8_t* data, size_t length);

private:
    enum class CommandType : uint8_t {
        Pixel,
        Line,
        FillRect,
        Rect,
        Text,
    };

    struct Command {
        CommandType type;
        int16_t x0;
        int16_t y0;
        int16_t x1;
        int16_t y1;
        uint16_t data;
        Color color;
    };

    bool pushCommand(CommandType type, int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color);
    bool pushTextCommand(int16_t x, int16_t y, const char* text, size_t length, Color color, uint8_t scale);
    void clearStrip(int16_t strip_height);
    void renderStrip(int16_t strip_y, int16_t strip_height);
    void renderCommandToStrip(const Command& command, int16_t strip_y, int16_t strip_height);
    void renderFillRectToStrip(int16_t x, int16_t y, int16_t width, int16_t height, Color color,
                               int16_t strip_y, int16_t strip_height);
    void renderLineToStrip(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color,
                           int16_t strip_y, int16_t strip_height);
    void renderTextToStrip(const Command& command, int16_t strip_y, int16_t strip_height);
    void renderGlyphToStrip(int16_t x, int16_t y, char character, Color color, uint8_t scale,
                            int16_t strip_y, int16_t strip_height);
    void plotInStrip(int16_t x, int16_t y, int16_t strip_y, int16_t strip_height, Color color);
    void writeColorToPixel(size_t pixel_index, Color color);
    void writeCommandWithData(uint8_t command, const uint8_t* data, size_t length);
    void updatePhysicalDimensions();
    void updateViewportOffsets();
    void setPhysicalAddressWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    void fillPhysicalRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, Color color);
    void clearPhysicalScreen(Color color);
    void select();
    void deselect();
    void setCommandMode();
    void setDataMode();

    spi_inst_t* spi_;
    bool initialized_;
    uint32_t spi_clock_hz_;
    uint32_t target_fps_;
    uint32_t target_frame_us_;
    absolute_time_t frame_start_;
    Rotation rotation_;
    uint16_t physical_width_;
    uint16_t physical_height_;
    uint16_t viewport_x_;
    uint16_t viewport_y_;
    uint16_t width_;
    uint16_t height_;
    std::array<Command, MaxCommands> commands_;
    size_t command_count_;
    std::array<char, MaxTextBytes> text_buffer_;
    size_t text_buffer_size_;
    Color background_color_;
    std::array<uint8_t, static_cast<size_t>(MaxWidth) * static_cast<size_t>(StripHeight) * 2> strip_buffer_;
};

}  // namespace picoboy
