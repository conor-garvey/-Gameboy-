#ifndef ILI9341_MONO_H
#define ILI9341_MONO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hardware/spi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ILI9341_MONO_WIDTH 240u
#define ILI9341_MONO_HEIGHT 320u
#define ILI9341_MONO_STRIDE_BYTES (ILI9341_MONO_WIDTH / 8u)
#define ILI9341_MONO_BUFFER_SIZE (ILI9341_MONO_STRIDE_BYTES * ILI9341_MONO_HEIGHT)

#define ILI9341_MONO_COLOR_BLACK 0x0000u
#define ILI9341_MONO_COLOR_WHITE 0xFFFFu
#define ILI9341_MONO_RGB565(r, g, b) \
    ((((uint16_t)(r) & 0xF8u) << 8u) | (((uint16_t)(g) & 0xFCu) << 3u) | ((uint16_t)(b) >> 3u))

typedef struct {
    spi_inst_t *spi;
    uint baudrate_hz;
    int pin_sck;
    int pin_mosi;
    int pin_cs;
    int pin_dc;
    int pin_rst;
    int pin_bl;
    uint16_t fg_color;
    uint16_t bg_color;
    uint8_t *buffer_a;
    uint8_t *buffer_b;
} ili9341_mono_config_t;

typedef struct {
    spi_inst_t *spi;
    uint baudrate_hz;
    int pin_cs;
    int pin_dc;
    int pin_rst;
    int pin_bl;
    uint16_t fg_color;
    uint16_t bg_color;
    uint8_t *buffer_a;
    uint8_t *buffer_b;
    uint8_t *front_buffer;
    uint8_t *back_buffer;
    bool owns_buffer_a;
    bool owns_buffer_b;
} ili9341_mono_t;

bool ili9341_mono_init(ili9341_mono_t *display, const ili9341_mono_config_t *config);
void ili9341_mono_deinit(ili9341_mono_t *display);

void ili9341_mono_set_palette(ili9341_mono_t *display, uint16_t fg_color, uint16_t bg_color);
void ili9341_mono_clear(ili9341_mono_t *display, bool color);
void ili9341_mono_copy_front_to_back(ili9341_mono_t *display);
void ili9341_mono_present(ili9341_mono_t *display);

uint8_t *ili9341_mono_get_draw_buffer(ili9341_mono_t *display);
const uint8_t *ili9341_mono_get_front_buffer(const ili9341_mono_t *display);

void ili9341_mono_draw_pixel(ili9341_mono_t *display, int16_t x, int16_t y, bool color);
bool ili9341_mono_get_pixel(const ili9341_mono_t *display, int16_t x, int16_t y);
void ili9341_mono_draw_line(ili9341_mono_t *display, int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool color);
void ili9341_mono_draw_hline(ili9341_mono_t *display, int16_t x, int16_t y, int16_t width, bool color);
void ili9341_mono_draw_vline(ili9341_mono_t *display, int16_t x, int16_t y, int16_t height, bool color);
void ili9341_mono_draw_rect(ili9341_mono_t *display, int16_t x, int16_t y, int16_t width, int16_t height, bool color);
void ili9341_mono_fill_rect(ili9341_mono_t *display, int16_t x, int16_t y, int16_t width, int16_t height, bool color);
void ili9341_mono_draw_circle(ili9341_mono_t *display, int16_t x0, int16_t y0, int16_t radius, bool color);
void ili9341_mono_fill_circle(ili9341_mono_t *display, int16_t x0, int16_t y0, int16_t radius, bool color);
void ili9341_mono_draw_bitmap(
    ili9341_mono_t *display,
    int16_t x,
    int16_t y,
    uint16_t width,
    uint16_t height,
    const uint8_t *bitmap,
    bool color,
    bool transparent
);

#ifdef __cplusplus
}
#endif

#endif
