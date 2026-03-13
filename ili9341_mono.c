#include "ili9341_mono.h"

#include <stdlib.h>
#include <string.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define ILI9341_SWRESET 0x01u
#define ILI9341_SLPOUT 0x11u
#define ILI9341_DISPON 0x29u
#define ILI9341_CASET 0x2Au
#define ILI9341_PASET 0x2Bu
#define ILI9341_RAMWR 0x2Cu
#define ILI9341_MADCTL 0x36u
#define ILI9341_PIXFMT 0x3Au

typedef struct {
    uint8_t command;
    uint8_t length;
    uint16_t delay_ms;
    uint8_t data[16];
} ili9341_init_cmd_t;

static const ili9341_init_cmd_t ILI9341_INIT_SEQUENCE[] = {
    {0xEFu, 3u, 0u, {0x03u, 0x80u, 0x02u}},
    {0xCFu, 3u, 0u, {0x00u, 0xC1u, 0x30u}},
    {0xEDu, 4u, 0u, {0x64u, 0x03u, 0x12u, 0x81u}},
    {0xE8u, 3u, 0u, {0x85u, 0x00u, 0x78u}},
    {0xCBu, 5u, 0u, {0x39u, 0x2Cu, 0x00u, 0x34u, 0x02u}},
    {0xF7u, 1u, 0u, {0x20u}},
    {0xEAu, 2u, 0u, {0x00u, 0x00u}},
    {0xC0u, 1u, 0u, {0x23u}},
    {0xC1u, 1u, 0u, {0x10u}},
    {0xC5u, 2u, 0u, {0x3Eu, 0x28u}},
    {0xC7u, 1u, 0u, {0x86u}},
    {ILI9341_MADCTL, 1u, 0u, {0x48u}},
    {ILI9341_PIXFMT, 1u, 0u, {0x55u}},
    {0xB1u, 2u, 0u, {0x00u, 0x18u}},
    {0xB6u, 3u, 0u, {0x08u, 0x82u, 0x27u}},
    {0xF2u, 1u, 0u, {0x00u}},
    {0x26u, 1u, 0u, {0x01u}},
    {0xE0u, 15u, 0u, {0x0Fu, 0x31u, 0x2Bu, 0x0Cu, 0x0Eu, 0x08u, 0x4Eu, 0xF1u, 0x37u, 0x07u, 0x10u, 0x03u, 0x0Eu, 0x09u, 0x00u}},
    {0xE1u, 15u, 0u, {0x00u, 0x0Eu, 0x14u, 0x03u, 0x11u, 0x07u, 0x31u, 0xC1u, 0x48u, 0x08u, 0x0Fu, 0x0Cu, 0x31u, 0x36u, 0x0Fu}},
    {ILI9341_SLPOUT, 0u, 120u, {0u}},
    {ILI9341_DISPON, 0u, 20u, {0u}},
};

static inline bool ili9341_mono_in_bounds(int16_t x, int16_t y) {
    return x >= 0 && x < (int16_t)ILI9341_MONO_WIDTH && y >= 0 && y < (int16_t)ILI9341_MONO_HEIGHT;
}

static inline void ili9341_mono_select(const ili9341_mono_t *display) {
    if (display->pin_cs >= 0) {
        gpio_put((uint)display->pin_cs, 0);
    }
}

static inline void ili9341_mono_deselect(const ili9341_mono_t *display) {
    if (display->pin_cs >= 0) {
        gpio_put((uint)display->pin_cs, 1);
    }
}

static void ili9341_mono_write_command(const ili9341_mono_t *display, uint8_t command) {
    ili9341_mono_select(display);
    gpio_put((uint)display->pin_dc, 0);
    spi_write_blocking(display->spi, &command, 1);
    ili9341_mono_deselect(display);
}

static void ili9341_mono_write_data(const ili9341_mono_t *display, const uint8_t *data, size_t length) {
    if (length == 0u) {
        return;
    }

    ili9341_mono_select(display);
    gpio_put((uint)display->pin_dc, 1);
    spi_write_blocking(display->spi, data, length);
    ili9341_mono_deselect(display);
}

static void ili9341_mono_write_command_data(const ili9341_mono_t *display, uint8_t command, const uint8_t *data, size_t length) {
    ili9341_mono_write_command(display, command);
    ili9341_mono_write_data(display, data, length);
}

static void ili9341_mono_reset_panel(const ili9341_mono_t *display) {
    if (display->pin_rst >= 0) {
        gpio_put((uint)display->pin_rst, 1);
        sleep_ms(5);
        gpio_put((uint)display->pin_rst, 0);
        sleep_ms(20);
        gpio_put((uint)display->pin_rst, 1);
        sleep_ms(150);
        return;
    }

    ili9341_mono_write_command(display, ILI9341_SWRESET);
    sleep_ms(150);
}

static void ili9341_mono_set_address_window(const ili9341_mono_t *display, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t window[4];

    window[0] = (uint8_t)(x0 >> 8u);
    window[1] = (uint8_t)(x0 & 0xFFu);
    window[2] = (uint8_t)(x1 >> 8u);
    window[3] = (uint8_t)(x1 & 0xFFu);
    ili9341_mono_write_command_data(display, ILI9341_CASET, window, sizeof(window));

    window[0] = (uint8_t)(y0 >> 8u);
    window[1] = (uint8_t)(y0 & 0xFFu);
    window[2] = (uint8_t)(y1 >> 8u);
    window[3] = (uint8_t)(y1 & 0xFFu);
    ili9341_mono_write_command_data(display, ILI9341_PASET, window, sizeof(window));

    ili9341_mono_write_command(display, ILI9341_RAMWR);
}

static void ili9341_mono_hw_init(ili9341_mono_t *display, const ili9341_mono_config_t *config) {
    spi_init(display->spi, display->baudrate_hz);
    spi_set_format(display->spi, 8u, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function((uint)config->pin_sck, GPIO_FUNC_SPI);
    gpio_set_function((uint)config->pin_mosi, GPIO_FUNC_SPI);

    if (display->pin_cs >= 0) {
        gpio_init((uint)display->pin_cs);
        gpio_set_dir((uint)display->pin_cs, GPIO_OUT);
        gpio_put((uint)display->pin_cs, 1);
    }

    gpio_init((uint)display->pin_dc);
    gpio_set_dir((uint)display->pin_dc, GPIO_OUT);
    gpio_put((uint)display->pin_dc, 1);

    if (display->pin_rst >= 0) {
        gpio_init((uint)display->pin_rst);
        gpio_set_dir((uint)display->pin_rst, GPIO_OUT);
        gpio_put((uint)display->pin_rst, 1);
    }

    if (display->pin_bl >= 0) {
        gpio_init((uint)display->pin_bl);
        gpio_set_dir((uint)display->pin_bl, GPIO_OUT);
        gpio_put((uint)display->pin_bl, 0);
    }

    ili9341_mono_reset_panel(display);

    for (size_t i = 0; i < sizeof(ILI9341_INIT_SEQUENCE) / sizeof(ILI9341_INIT_SEQUENCE[0]); ++i) {
        const ili9341_init_cmd_t *step = &ILI9341_INIT_SEQUENCE[i];
        ili9341_mono_write_command_data(display, step->command, step->data, step->length);
        if (step->delay_ms != 0u) {
            sleep_ms(step->delay_ms);
        }
    }

    if (display->pin_bl >= 0) {
        gpio_put((uint)display->pin_bl, 1);
    }
}

static void ili9341_mono_write_back_buffer_bit(ili9341_mono_t *display, int16_t x, int16_t y, bool color) {
    uint8_t *byte = &display->back_buffer[(size_t)y * ILI9341_MONO_STRIDE_BYTES + ((uint16_t)x >> 3u)];
    const uint8_t mask = (uint8_t)(0x80u >> (x & 7));

    if (color) {
        *byte |= mask;
    } else {
        *byte &= (uint8_t)~mask;
    }
}

bool ili9341_mono_init(ili9341_mono_t *display, const ili9341_mono_config_t *config) {
    if (display == NULL || config == NULL || config->spi == NULL) {
        return false;
    }
    if (config->pin_sck < 0 || config->pin_mosi < 0 || config->pin_dc < 0) {
        return false;
    }

    memset(display, 0, sizeof(*display));
    display->spi = config->spi;
    display->baudrate_hz = config->baudrate_hz != 0u ? config->baudrate_hz : 40000000u;
    display->pin_cs = config->pin_cs;
    display->pin_dc = config->pin_dc;
    display->pin_rst = config->pin_rst;
    display->pin_bl = config->pin_bl;
    display->fg_color = config->fg_color;
    display->bg_color = config->bg_color;
    display->buffer_a = config->buffer_a;
    display->buffer_b = config->buffer_b;

    if (display->buffer_a != NULL && display->buffer_a == display->buffer_b) {
        return false;
    }

    if (display->buffer_a == NULL) {
        display->buffer_a = (uint8_t *)calloc(1u, ILI9341_MONO_BUFFER_SIZE);
        if (display->buffer_a == NULL) {
            return false;
        }
        display->owns_buffer_a = true;
    }

    if (display->buffer_b == NULL) {
        display->buffer_b = (uint8_t *)calloc(1u, ILI9341_MONO_BUFFER_SIZE);
        if (display->buffer_b == NULL) {
            ili9341_mono_deinit(display);
            return false;
        }
        display->owns_buffer_b = true;
    }

    display->front_buffer = display->buffer_a;
    display->back_buffer = display->buffer_b;

    memset(display->buffer_a, 0, ILI9341_MONO_BUFFER_SIZE);
    memset(display->buffer_b, 0, ILI9341_MONO_BUFFER_SIZE);

    ili9341_mono_hw_init(display, config);
    ili9341_mono_present(display);
    return true;
}

void ili9341_mono_deinit(ili9341_mono_t *display) {
    if (display == NULL) {
        return;
    }

    if (display->owns_buffer_a && display->buffer_a != NULL) {
        free(display->buffer_a);
    }
    if (display->owns_buffer_b && display->buffer_b != NULL) {
        free(display->buffer_b);
    }

    memset(display, 0, sizeof(*display));
}

void ili9341_mono_set_palette(ili9341_mono_t *display, uint16_t fg_color, uint16_t bg_color) {
    if (display == NULL) {
        return;
    }

    display->fg_color = fg_color;
    display->bg_color = bg_color;
}

void ili9341_mono_clear(ili9341_mono_t *display, bool color) {
    if (display == NULL || display->back_buffer == NULL) {
        return;
    }

    memset(display->back_buffer, color ? 0xFF : 0x00, ILI9341_MONO_BUFFER_SIZE);
}

void ili9341_mono_copy_front_to_back(ili9341_mono_t *display) {
    if (display == NULL || display->front_buffer == NULL || display->back_buffer == NULL) {
        return;
    }

    memcpy(display->back_buffer, display->front_buffer, ILI9341_MONO_BUFFER_SIZE);
}

void ili9341_mono_present(ili9341_mono_t *display) {
    if (display == NULL || display->front_buffer == NULL || display->back_buffer == NULL) {
        return;
    }

    uint8_t *swap = display->front_buffer;
    display->front_buffer = display->back_buffer;
    display->back_buffer = swap;

    ili9341_mono_set_address_window(display, 0u, 0u, ILI9341_MONO_WIDTH - 1u, ILI9341_MONO_HEIGHT - 1u);

    ili9341_mono_select(display);
    gpio_put((uint)display->pin_dc, 1);

    for (uint16_t y = 0; y < ILI9341_MONO_HEIGHT; ++y) {
        uint8_t scanline[ILI9341_MONO_WIDTH * 2u];
        const uint8_t *row = &display->front_buffer[(size_t)y * ILI9341_MONO_STRIDE_BYTES];
        size_t out = 0u;

        for (uint16_t byte_index = 0; byte_index < ILI9341_MONO_STRIDE_BYTES; ++byte_index) {
            uint8_t bits = row[byte_index];
            for (uint8_t bit = 0; bit < 8u; ++bit) {
                const uint16_t color = (bits & (uint8_t)(0x80u >> bit)) ? display->fg_color : display->bg_color;
                scanline[out++] = (uint8_t)(color >> 8u);
                scanline[out++] = (uint8_t)(color & 0xFFu);
            }
        }

        spi_write_blocking(display->spi, scanline, sizeof(scanline));
    }

    ili9341_mono_deselect(display);
}

uint8_t *ili9341_mono_get_draw_buffer(ili9341_mono_t *display) {
    return display != NULL ? display->back_buffer : NULL;
}

const uint8_t *ili9341_mono_get_front_buffer(const ili9341_mono_t *display) {
    return display != NULL ? display->front_buffer : NULL;
}

void ili9341_mono_draw_pixel(ili9341_mono_t *display, int16_t x, int16_t y, bool color) {
    if (display == NULL || display->back_buffer == NULL || !ili9341_mono_in_bounds(x, y)) {
        return;
    }

    ili9341_mono_write_back_buffer_bit(display, x, y, color);
}

bool ili9341_mono_get_pixel(const ili9341_mono_t *display, int16_t x, int16_t y) {
    if (display == NULL || display->back_buffer == NULL || !ili9341_mono_in_bounds(x, y)) {
        return false;
    }

    const uint8_t byte = display->back_buffer[(size_t)y * ILI9341_MONO_STRIDE_BYTES + ((uint16_t)x >> 3u)];
    return (byte & (uint8_t)(0x80u >> (x & 7))) != 0u;
}

void ili9341_mono_draw_hline(ili9341_mono_t *display, int16_t x, int16_t y, int16_t width, bool color) {
    if (display == NULL || display->back_buffer == NULL || width <= 0 || y < 0 || y >= (int16_t)ILI9341_MONO_HEIGHT) {
        return;
    }

    int16_t x0 = x;
    int16_t x1 = x + width - 1;
    if (x1 < 0 || x0 >= (int16_t)ILI9341_MONO_WIDTH) {
        return;
    }

    if (x0 < 0) {
        x0 = 0;
    }
    if (x1 >= (int16_t)ILI9341_MONO_WIDTH) {
        x1 = (int16_t)ILI9341_MONO_WIDTH - 1;
    }

    for (int16_t px = x0; px <= x1; ++px) {
        ili9341_mono_write_back_buffer_bit(display, px, y, color);
    }
}

void ili9341_mono_draw_vline(ili9341_mono_t *display, int16_t x, int16_t y, int16_t height, bool color) {
    if (display == NULL || display->back_buffer == NULL || height <= 0 || x < 0 || x >= (int16_t)ILI9341_MONO_WIDTH) {
        return;
    }

    int16_t y0 = y;
    int16_t y1 = y + height - 1;
    if (y1 < 0 || y0 >= (int16_t)ILI9341_MONO_HEIGHT) {
        return;
    }

    if (y0 < 0) {
        y0 = 0;
    }
    if (y1 >= (int16_t)ILI9341_MONO_HEIGHT) {
        y1 = (int16_t)ILI9341_MONO_HEIGHT - 1;
    }

    for (int16_t py = y0; py <= y1; ++py) {
        ili9341_mono_write_back_buffer_bit(display, x, py, color);
    }
}

void ili9341_mono_draw_line(ili9341_mono_t *display, int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool color) {
    if (display == NULL) {
        return;
    }

    int16_t dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = y1 >= y0 ? y0 - y1 : y1 - y0;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;

    while (true) {
        ili9341_mono_draw_pixel(display, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }

        int16_t e2 = (int16_t)(2 * err);
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void ili9341_mono_draw_rect(ili9341_mono_t *display, int16_t x, int16_t y, int16_t width, int16_t height, bool color) {
    if (display == NULL || display->back_buffer == NULL || width <= 0 || height <= 0) {
        return;
    }

    ili9341_mono_draw_hline(display, x, y, width, color);
    ili9341_mono_draw_hline(display, x, (int16_t)(y + height - 1), width, color);
    ili9341_mono_draw_vline(display, x, y, height, color);
    ili9341_mono_draw_vline(display, (int16_t)(x + width - 1), y, height, color);
}

void ili9341_mono_fill_rect(ili9341_mono_t *display, int16_t x, int16_t y, int16_t width, int16_t height, bool color) {
    if (display == NULL || display->back_buffer == NULL || width <= 0 || height <= 0) {
        return;
    }

    int16_t x0 = x;
    int16_t y0 = y;
    int16_t x1 = x + width - 1;
    int16_t y1 = y + height - 1;

    if (x1 < 0 || y1 < 0 || x0 >= (int16_t)ILI9341_MONO_WIDTH || y0 >= (int16_t)ILI9341_MONO_HEIGHT) {
        return;
    }

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 >= (int16_t)ILI9341_MONO_WIDTH) {
        x1 = (int16_t)ILI9341_MONO_WIDTH - 1;
    }
    if (y1 >= (int16_t)ILI9341_MONO_HEIGHT) {
        y1 = (int16_t)ILI9341_MONO_HEIGHT - 1;
    }

    for (int16_t py = y0; py <= y1; ++py) {
        for (int16_t px = x0; px <= x1; ++px) {
            ili9341_mono_write_back_buffer_bit(display, px, py, color);
        }
    }
}

void ili9341_mono_draw_circle(ili9341_mono_t *display, int16_t x0, int16_t y0, int16_t radius, bool color) {
    if (display == NULL || radius < 0) {
        return;
    }

    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 1 - x;

    while (x >= y) {
        ili9341_mono_draw_pixel(display, (int16_t)(x0 + x), (int16_t)(y0 + y), color);
        ili9341_mono_draw_pixel(display, (int16_t)(x0 + y), (int16_t)(y0 + x), color);
        ili9341_mono_draw_pixel(display, (int16_t)(x0 - y), (int16_t)(y0 + x), color);
        ili9341_mono_draw_pixel(display, (int16_t)(x0 - x), (int16_t)(y0 + y), color);
        ili9341_mono_draw_pixel(display, (int16_t)(x0 - x), (int16_t)(y0 - y), color);
        ili9341_mono_draw_pixel(display, (int16_t)(x0 - y), (int16_t)(y0 - x), color);
        ili9341_mono_draw_pixel(display, (int16_t)(x0 + y), (int16_t)(y0 - x), color);
        ili9341_mono_draw_pixel(display, (int16_t)(x0 + x), (int16_t)(y0 - y), color);

        ++y;
        if (err < 0) {
            err += (int16_t)(2 * y + 1);
        } else {
            --x;
            err += (int16_t)(2 * (y - x) + 1);
        }
    }
}

void ili9341_mono_fill_circle(ili9341_mono_t *display, int16_t x0, int16_t y0, int16_t radius, bool color) {
    if (display == NULL || radius < 0) {
        return;
    }

    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 1 - x;

    while (x >= y) {
        ili9341_mono_draw_hline(display, (int16_t)(x0 - x), (int16_t)(y0 + y), (int16_t)(2 * x + 1), color);
        ili9341_mono_draw_hline(display, (int16_t)(x0 - x), (int16_t)(y0 - y), (int16_t)(2 * x + 1), color);
        ili9341_mono_draw_hline(display, (int16_t)(x0 - y), (int16_t)(y0 + x), (int16_t)(2 * y + 1), color);
        ili9341_mono_draw_hline(display, (int16_t)(x0 - y), (int16_t)(y0 - x), (int16_t)(2 * y + 1), color);

        ++y;
        if (err < 0) {
            err += (int16_t)(2 * y + 1);
        } else {
            --x;
            err += (int16_t)(2 * (y - x) + 1);
        }
    }
}

void ili9341_mono_draw_bitmap(
    ili9341_mono_t *display,
    int16_t x,
    int16_t y,
    uint16_t width,
    uint16_t height,
    const uint8_t *bitmap,
    bool color,
    bool transparent
) {
    if (display == NULL || bitmap == NULL || width == 0u || height == 0u) {
        return;
    }

    const size_t stride = (width + 7u) / 8u;
    for (uint16_t row = 0; row < height; ++row) {
        for (uint16_t col = 0; col < width; ++col) {
            const uint8_t byte = bitmap[(size_t)row * stride + (col >> 3u)];
            const bool bit = (byte & (uint8_t)(0x80u >> (col & 7u))) != 0u;
            if (bit) {
                ili9341_mono_draw_pixel(display, (int16_t)(x + (int16_t)col), (int16_t)(y + (int16_t)row), color);
            } else if (!transparent) {
                ili9341_mono_draw_pixel(display, (int16_t)(x + (int16_t)col), (int16_t)(y + (int16_t)row), !color);
            }
        }
    }
}
