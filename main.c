#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "ili9341_mono.h"
#include "lsm6ds3.h"

#define LCD_SPI_PORT spi0
#define LCD_PIN_SCK 18
#define LCD_PIN_MOSI 19
#define LCD_PIN_CS 17
#define LCD_PIN_DC 20
#define LCD_PIN_RST 21
#define LCD_PIN_BL 22

#define IMU_I2C_PORT i2c1
#define IMU_PIN_SDA 27
#define IMU_PIN_SCL 26
#define IMU_I2C_ADDR 0x6A

#define BUTTON_UP_PIN 2
#define BUTTON_DOWN_PIN 3
#define BUTTON_LEFT_PIN 4
#define BUTTON_RIGHT_PIN 5
#define BUTTON_A_PIN 7
#define BUTTON_B_PIN 6
#define BUTTON_X_PIN 8
#define BUTTON_Y_PIN 9

#define SCREEN_W ((int16_t)ILI9341_MONO_WIDTH)
#define SCREEN_H ((int16_t)ILI9341_MONO_HEIGHT)

#define MAX_PLATFORMS 18
#define MAX_COINS 18
#define MAX_ENEMIES 8
#define MAX_DROP_WALLS 6

typedef enum {
    BUTTON_UP = 0,
    BUTTON_DOWN,
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_A,
    BUTTON_B,
    BUTTON_X,
    BUTTON_Y,
    BUTTON_COUNT
} button_id_t;

typedef struct {
    uint pin[BUTTON_COUNT];
    bool raw[BUTTON_COUNT];
    bool down[BUTTON_COUNT];
    bool pressed[BUTTON_COUNT];
    bool released[BUTTON_COUNT];
    uint32_t changed_at_ms[BUTTON_COUNT];
} buttons_t;

typedef enum {
    APP_MENU = 0,
    APP_PLATFORMER,
    APP_DROP
} app_state_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} rect_i16_t;

typedef struct {
    rect_i16_t rect;
} platform_t;

typedef struct {
    int16_t x;
    int16_t y;
    bool active;
} coin_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t min_x;
    int16_t max_x;
    int8_t vx;
    bool active;
} enemy_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t vx;
    int16_t vy;
    int16_t w;
    int16_t h;
    bool on_ground;
    bool won;
    bool game_over;
    uint16_t score;
    int16_t camera_x;
    int16_t goal_x;
    platform_t platforms[MAX_PLATFORMS];
    coin_t coins[MAX_COINS];
    enemy_t enemies[MAX_ENEMIES];
    size_t platform_count;
    size_t coin_count;
    size_t enemy_count;
} platformer_t;

typedef struct {
    int16_t y;
    int16_t gap_x;
    int16_t gap_width;
    bool scored;
} drop_wall_t;

typedef struct {
    int16_t player_x;
    int16_t player_y;
    int16_t player_vx;
    int16_t tilt_x;
    int16_t speed;
    int16_t gap_width;
    uint16_t score;
    bool game_over;
    bool imu_ok;
    drop_wall_t walls[MAX_DROP_WALLS];
} drop_game_t;

static uint32_t rng_state = 0x13579BDFu;

static uint32_t rng_next(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static int16_t clamp_i16(int16_t value, int16_t min_value, int16_t max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static bool rects_overlap(rect_i16_t a, rect_i16_t b) {
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

static void buttons_init(buttons_t *buttons) {
    static const uint pins[BUTTON_COUNT] = {
        BUTTON_UP_PIN,
        BUTTON_DOWN_PIN,
        BUTTON_LEFT_PIN,
        BUTTON_RIGHT_PIN,
        BUTTON_A_PIN,
        BUTTON_B_PIN,
        BUTTON_X_PIN,
        BUTTON_Y_PIN,
    };

    memset(buttons, 0, sizeof(*buttons));
    for (size_t i = 0; i < BUTTON_COUNT; ++i) {
        buttons->pin[i] = pins[i];
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
        buttons->raw[i] = gpio_get(pins[i]) == 0;
        buttons->down[i] = buttons->raw[i];
        buttons->changed_at_ms[i] = to_ms_since_boot(get_absolute_time());
    }
}

static void buttons_update(buttons_t *buttons) {
    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    for (size_t i = 0; i < BUTTON_COUNT; ++i) {
        buttons->pressed[i] = false;
        buttons->released[i] = false;

        const bool raw = gpio_get(buttons->pin[i]) == 0;
        if (raw != buttons->raw[i]) {
            buttons->raw[i] = raw;
            buttons->changed_at_ms[i] = now_ms;
        }

        if (buttons->down[i] != buttons->raw[i] && now_ms - buttons->changed_at_ms[i] >= 18u) {
            buttons->down[i] = buttons->raw[i];
            buttons->pressed[i] = buttons->down[i];
            buttons->released[i] = !buttons->down[i];
        }
    }
}

static const uint8_t *glyph_for_char(char c) {
    static const uint8_t GLYPH_SPACE[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t GLYPH_DASH[7] = {0, 0, 0, 31, 0, 0, 0};
    static const uint8_t GLYPH_COLON[7] = {0, 4, 4, 0, 4, 4, 0};
    static const uint8_t GLYPH_EXCL[7] = {4, 4, 4, 4, 4, 0, 4};
    static const uint8_t GLYPH_0[7] = {14, 17, 19, 21, 25, 17, 14};
    static const uint8_t GLYPH_1[7] = {4, 12, 4, 4, 4, 4, 14};
    static const uint8_t GLYPH_2[7] = {14, 17, 1, 2, 4, 8, 31};
    static const uint8_t GLYPH_3[7] = {30, 1, 1, 14, 1, 1, 30};
    static const uint8_t GLYPH_4[7] = {2, 6, 10, 18, 31, 2, 2};
    static const uint8_t GLYPH_5[7] = {31, 16, 16, 30, 1, 1, 30};
    static const uint8_t GLYPH_6[7] = {14, 16, 16, 30, 17, 17, 14};
    static const uint8_t GLYPH_7[7] = {31, 1, 2, 4, 8, 8, 8};
    static const uint8_t GLYPH_8[7] = {14, 17, 17, 14, 17, 17, 14};
    static const uint8_t GLYPH_9[7] = {14, 17, 17, 15, 1, 1, 14};
    static const uint8_t GLYPH_A[7] = {14, 17, 17, 31, 17, 17, 17};
    static const uint8_t GLYPH_B[7] = {30, 17, 17, 30, 17, 17, 30};
    static const uint8_t GLYPH_C[7] = {14, 17, 16, 16, 16, 17, 14};
    static const uint8_t GLYPH_D[7] = {30, 17, 17, 17, 17, 17, 30};
    static const uint8_t GLYPH_E[7] = {31, 16, 16, 30, 16, 16, 31};
    static const uint8_t GLYPH_F[7] = {31, 16, 16, 30, 16, 16, 16};
    static const uint8_t GLYPH_G[7] = {14, 17, 16, 23, 17, 17, 15};
    static const uint8_t GLYPH_H[7] = {17, 17, 17, 31, 17, 17, 17};
    static const uint8_t GLYPH_I[7] = {14, 4, 4, 4, 4, 4, 14};
    static const uint8_t GLYPH_J[7] = {1, 1, 1, 1, 17, 17, 14};
    static const uint8_t GLYPH_K[7] = {17, 18, 20, 24, 20, 18, 17};
    static const uint8_t GLYPH_L[7] = {16, 16, 16, 16, 16, 16, 31};
    static const uint8_t GLYPH_M[7] = {17, 27, 21, 21, 17, 17, 17};
    static const uint8_t GLYPH_N[7] = {17, 17, 25, 21, 19, 17, 17};
    static const uint8_t GLYPH_O[7] = {14, 17, 17, 17, 17, 17, 14};
    static const uint8_t GLYPH_P[7] = {30, 17, 17, 30, 16, 16, 16};
    static const uint8_t GLYPH_Q[7] = {14, 17, 17, 17, 21, 18, 13};
    static const uint8_t GLYPH_R[7] = {30, 17, 17, 30, 20, 18, 17};
    static const uint8_t GLYPH_S[7] = {15, 16, 16, 14, 1, 1, 30};
    static const uint8_t GLYPH_T[7] = {31, 4, 4, 4, 4, 4, 4};
    static const uint8_t GLYPH_U[7] = {17, 17, 17, 17, 17, 17, 14};
    static const uint8_t GLYPH_V[7] = {17, 17, 17, 17, 17, 10, 4};
    static const uint8_t GLYPH_W[7] = {17, 17, 17, 21, 21, 21, 10};
    static const uint8_t GLYPH_X[7] = {17, 17, 10, 4, 10, 17, 17};
    static const uint8_t GLYPH_Y[7] = {17, 17, 10, 4, 4, 4, 4};
    static const uint8_t GLYPH_Z[7] = {31, 1, 2, 4, 8, 16, 31};

    switch (c) {
        case '-': return GLYPH_DASH;
        case ':': return GLYPH_COLON;
        case '!': return GLYPH_EXCL;
        case '0': return GLYPH_0;
        case '1': return GLYPH_1;
        case '2': return GLYPH_2;
        case '3': return GLYPH_3;
        case '4': return GLYPH_4;
        case '5': return GLYPH_5;
        case '6': return GLYPH_6;
        case '7': return GLYPH_7;
        case '8': return GLYPH_8;
        case '9': return GLYPH_9;
        case 'A': return GLYPH_A;
        case 'B': return GLYPH_B;
        case 'C': return GLYPH_C;
        case 'D': return GLYPH_D;
        case 'E': return GLYPH_E;
        case 'F': return GLYPH_F;
        case 'G': return GLYPH_G;
        case 'H': return GLYPH_H;
        case 'I': return GLYPH_I;
        case 'J': return GLYPH_J;
        case 'K': return GLYPH_K;
        case 'L': return GLYPH_L;
        case 'M': return GLYPH_M;
        case 'N': return GLYPH_N;
        case 'O': return GLYPH_O;
        case 'P': return GLYPH_P;
        case 'Q': return GLYPH_Q;
        case 'R': return GLYPH_R;
        case 'S': return GLYPH_S;
        case 'T': return GLYPH_T;
        case 'U': return GLYPH_U;
        case 'V': return GLYPH_V;
        case 'W': return GLYPH_W;
        case 'X': return GLYPH_X;
        case 'Y': return GLYPH_Y;
        case 'Z': return GLYPH_Z;
        default: return GLYPH_SPACE;
    }
}

static void draw_char(ili9341_mono_t *display, int16_t x, int16_t y, char c, uint8_t scale) {
    const uint8_t *glyph = glyph_for_char(c);
    for (uint8_t row = 0; row < 7u; ++row) {
        for (uint8_t col = 0; col < 5u; ++col) {
            if ((glyph[row] & (uint8_t)(1u << (4u - col))) != 0u) {
                ili9341_mono_fill_rect(
                    display,
                    (int16_t)(x + (int16_t)(col * scale)),
                    (int16_t)(y + (int16_t)(row * scale)),
                    scale,
                    scale,
                    true
                );
            }
        }
    }
}

static void draw_text(ili9341_mono_t *display, int16_t x, int16_t y, const char *text, uint8_t scale) {
    int16_t cursor_x = x;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        draw_char(display, cursor_x, y, text[i], scale);
        cursor_x = (int16_t)(cursor_x + (int16_t)(6 * scale));
    }
}

static void draw_text_centered(ili9341_mono_t *display, int16_t center_x, int16_t y, const char *text, uint8_t scale) {
    const int16_t width = (int16_t)(strlen(text) * (size_t)(6 * scale));
    draw_text(display, (int16_t)(center_x - width / 2), y, text, scale);
}

static void draw_number(ili9341_mono_t *display, int16_t x, int16_t y, uint16_t value, uint8_t scale) {
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "%u", value);
    draw_text(display, x, y, buffer, scale);
}

static void draw_player_sprite(ili9341_mono_t *display, int16_t x, int16_t y) {
    ili9341_mono_fill_rect(display, x + 3, y, 8, 3, true);
    ili9341_mono_fill_rect(display, x + 2, y + 3, 10, 2, true);
    ili9341_mono_fill_rect(display, x + 3, y + 5, 8, 5, true);
    ili9341_mono_fill_rect(display, x + 2, y + 10, 10, 3, true);
    ili9341_mono_fill_rect(display, x + 1, y + 13, 4, 3, true);
    ili9341_mono_fill_rect(display, x + 8, y + 13, 4, 3, true);
}

static void draw_enemy_sprite(ili9341_mono_t *display, int16_t x, int16_t y) {
    ili9341_mono_fill_circle(display, x + 6, y + 5, 5, true);
    ili9341_mono_fill_rect(display, x + 2, y + 8, 8, 4, true);
    ili9341_mono_draw_pixel(display, x + 4, y + 4, false);
    ili9341_mono_draw_pixel(display, x + 8, y + 4, false);
}

static void draw_drop_player(ili9341_mono_t *display, int16_t x, int16_t y) {
    ili9341_mono_fill_circle(display, x + 6, y + 4, 4, true);
    ili9341_mono_draw_line(display, x + 6, y + 8, x + 6, y + 16, true);
    ili9341_mono_draw_line(display, x + 6, y + 11, x + 1, y + 7, true);
    ili9341_mono_draw_line(display, x + 6, y + 11, x + 11, y + 7, true);
    ili9341_mono_draw_line(display, x + 6, y + 16, x + 2, y + 22, true);
    ili9341_mono_draw_line(display, x + 6, y + 16, x + 10, y + 22, true);
}

static void platformer_add_platform(platformer_t *game, int16_t x, int16_t y, int16_t w, int16_t h) {
    if (game->platform_count >= MAX_PLATFORMS) {
        return;
    }
    game->platforms[game->platform_count++].rect = (rect_i16_t){x, y, w, h};
}

static void platformer_add_coin(platformer_t *game, int16_t x, int16_t y) {
    if (game->coin_count >= MAX_COINS) {
        return;
    }
    game->coins[game->coin_count++] = (coin_t){x, y, true};
}

static void platformer_add_enemy(platformer_t *game, int16_t x, int16_t y, int16_t min_x, int16_t max_x) {
    if (game->enemy_count >= MAX_ENEMIES) {
        return;
    }
    game->enemies[game->enemy_count++] = (enemy_t){x, y, min_x, max_x, 1, true};
}

static void platformer_reset(platformer_t *game) {
    memset(game, 0, sizeof(*game));
    game->x = 24;
    game->y = 240;
    game->w = 12;
    game->h = 16;
    game->goal_x = 1320;

    platformer_add_platform(game, 0, 286, 220, 34);
    platformer_add_platform(game, 290, 286, 190, 34);
    platformer_add_platform(game, 540, 286, 230, 34);
    platformer_add_platform(game, 840, 286, 170, 34);
    platformer_add_platform(game, 1090, 286, 280, 34);
    platformer_add_platform(game, 180, 240, 60, 12);
    platformer_add_platform(game, 330, 220, 54, 12);
    platformer_add_platform(game, 430, 198, 70, 12);
    platformer_add_platform(game, 620, 232, 74, 12);
    platformer_add_platform(game, 760, 200, 64, 12);
    platformer_add_platform(game, 930, 228, 80, 12);
    platformer_add_platform(game, 1140, 214, 70, 12);

    platformer_add_coin(game, 202, 220);
    platformer_add_coin(game, 350, 198);
    platformer_add_coin(game, 460, 176);
    platformer_add_coin(game, 650, 210);
    platformer_add_coin(game, 790, 178);
    platformer_add_coin(game, 955, 206);
    platformer_add_coin(game, 1170, 192);
    platformer_add_coin(game, 1260, 250);

    platformer_add_enemy(game, 116, 274, 40, 180);
    platformer_add_enemy(game, 365, 274, 310, 454);
    platformer_add_enemy(game, 676, 274, 570, 742);
    platformer_add_enemy(game, 964, 216, 940, 998);
    platformer_add_enemy(game, 1210, 274, 1120, 1338);
}

static bool platformer_collides(const platformer_t *game, rect_i16_t actor) {
    for (size_t i = 0; i < game->platform_count; ++i) {
        if (rects_overlap(actor, game->platforms[i].rect)) {
            return true;
        }
    }
    return false;
}

static void platformer_step(platformer_t *game, const buttons_t *buttons) {
    if (game->game_over || game->won) {
        return;
    }

    const bool move_left = buttons->down[BUTTON_LEFT];
    const bool move_right = buttons->down[BUTTON_RIGHT];
    const int16_t move_speed = buttons->down[BUTTON_X] ? 4 : 3;

    game->vx = 0;
    if (move_left) {
        game->vx = (int16_t)-move_speed;
    }
    if (move_right) {
        game->vx = move_speed;
    }
    if (buttons->pressed[BUTTON_A] && game->on_ground) {
        game->vy = -11;
        game->on_ground = false;
    }

    for (int16_t step = 0; step < (game->vx >= 0 ? game->vx : -game->vx); ++step) {
        rect_i16_t next = {game->x + (game->vx > 0 ? 1 : -1), game->y, game->w, game->h};
        if (!platformer_collides(game, next)) {
            game->x = next.x;
        } else {
            break;
        }
    }

    game->vy = clamp_i16((int16_t)(game->vy + 1), -12, 10);
    game->on_ground = false;

    for (int16_t step = 0; step < (game->vy >= 0 ? game->vy : -game->vy); ++step) {
        rect_i16_t next = {game->x, game->y + (game->vy > 0 ? 1 : -1), game->w, game->h};
        if (!platformer_collides(game, next)) {
            game->y = next.y;
        } else {
            if (game->vy > 0) {
                game->on_ground = true;
            }
            game->vy = 0;
            break;
        }
    }

    for (size_t i = 0; i < game->coin_count; ++i) {
        coin_t *coin = &game->coins[i];
        rect_i16_t coin_box = {coin->x - 5, coin->y - 5, 10, 10};
        if (coin->active && rects_overlap((rect_i16_t){game->x, game->y, game->w, game->h}, coin_box)) {
            coin->active = false;
            game->score = (uint16_t)(game->score + 10);
        }
    }

    for (size_t i = 0; i < game->enemy_count; ++i) {
        enemy_t *enemy = &game->enemies[i];
        if (!enemy->active) {
            continue;
        }

        enemy->x = (int16_t)(enemy->x + enemy->vx);
        if (enemy->x <= enemy->min_x || enemy->x >= enemy->max_x) {
            enemy->vx = (int8_t)-enemy->vx;
        }

        rect_i16_t enemy_box = {enemy->x, enemy->y, 12, 12};
        rect_i16_t player_box = {game->x, game->y, game->w, game->h};
        if (rects_overlap(player_box, enemy_box)) {
            if (game->vy > 0 && game->y + game->h <= enemy->y + 6) {
                enemy->active = false;
                game->vy = -7;
                game->score = (uint16_t)(game->score + 25);
            } else {
                game->game_over = true;
            }
        }
    }

    if (game->y > 320) {
        game->game_over = true;
    }
    if (game->x >= game->goal_x) {
        game->won = true;
        game->score = (uint16_t)(game->score + 100);
    }

    game->camera_x = clamp_i16((int16_t)(game->x - 76), 0, (int16_t)(game->goal_x - SCREEN_W + 30));
}

static void draw_platformer(ili9341_mono_t *display, const platformer_t *game) {
    ili9341_mono_clear(display, false);

    for (int16_t y = 0; y < SCREEN_H; y += 24) {
        const int16_t offset = (int16_t)(((game->camera_x / 3) + y) % SCREEN_W);
        ili9341_mono_draw_pixel(display, offset, y, true);
        ili9341_mono_draw_pixel(display, (int16_t)((offset + 80) % SCREEN_W), (int16_t)(y + 8 < SCREEN_H ? y + 8 : y), true);
    }

    for (size_t i = 0; i < game->platform_count; ++i) {
        const rect_i16_t r = game->platforms[i].rect;
        const int16_t draw_x = (int16_t)(r.x - game->camera_x);
        if (draw_x + r.w < 0 || draw_x >= SCREEN_W) {
            continue;
        }
        ili9341_mono_fill_rect(display, draw_x, r.y, r.w, r.h, true);
        ili9341_mono_draw_hline(display, draw_x, r.y, r.w, false);
        ili9341_mono_draw_hline(display, draw_x, (int16_t)(r.y + 2), r.w, false);
    }

    for (size_t i = 0; i < game->coin_count; ++i) {
        if (!game->coins[i].active) {
            continue;
        }
        const int16_t draw_x = (int16_t)(game->coins[i].x - game->camera_x);
        ili9341_mono_draw_circle(display, draw_x, game->coins[i].y, 5, true);
        ili9341_mono_draw_vline(display, draw_x, (int16_t)(game->coins[i].y - 3), 7, true);
    }

    for (size_t i = 0; i < game->enemy_count; ++i) {
        if (!game->enemies[i].active) {
            continue;
        }
        draw_enemy_sprite(display, (int16_t)(game->enemies[i].x - game->camera_x), game->enemies[i].y);
    }

    const int16_t flag_x = (int16_t)(game->goal_x - game->camera_x);
    ili9341_mono_draw_vline(display, flag_x, 170, 116, true);
    ili9341_mono_fill_rect(display, (int16_t)(flag_x + 2), 176, 24, 12, true);
    ili9341_mono_fill_rect(display, (int16_t)(flag_x + 8), 188, 10, 16, true);

    draw_player_sprite(display, (int16_t)(game->x - game->camera_x), game->y);
    draw_text(display, 8, 8, "PLUMBER", 1);
    draw_text(display, 8, 20, "SCORE", 1);
    draw_number(display, 44, 20, game->score, 1);

    if (game->won) {
        ili9341_mono_fill_rect(display, 34, 118, 172, 54, false);
        ili9341_mono_draw_rect(display, 34, 118, 172, 54, true);
        draw_text_centered(display, 120, 132, "YOU WIN", 2);
        draw_text_centered(display, 120, 154, "A AGAIN B MENU", 1);
    } else if (game->game_over) {
        ili9341_mono_fill_rect(display, 30, 118, 180, 54, false);
        ili9341_mono_draw_rect(display, 30, 118, 180, 54, true);
        draw_text_centered(display, 120, 132, "GAME OVER", 2);
        draw_text_centered(display, 120, 154, "A AGAIN B MENU", 1);
    }
}

static void drop_reset(drop_game_t *game, bool imu_ok) {
    memset(game, 0, sizeof(*game));
    game->player_x = 114;
    game->player_y = 74;
    game->speed = 3;
    game->gap_width = 62;
    game->imu_ok = imu_ok;

    for (size_t i = 0; i < MAX_DROP_WALLS; ++i) {
        game->walls[i].y = (int16_t)(SCREEN_H + (int16_t)(i * 56));
        game->walls[i].gap_width = game->gap_width;
        game->walls[i].gap_x = (int16_t)(20 + (rng_next() % (SCREEN_W - game->gap_width - 40)));
    }
}

static void drop_step(drop_game_t *game, const buttons_t *buttons, lsm6ds3_t *imu) {
    if (game->game_over) {
        return;
    }

    int16_t accel_x = 0;
    int16_t accel_y = 0;
    int16_t accel_z = 0;
    if (imu != NULL && imu->ready && lsm6ds3_read_accel(imu, &accel_x, &accel_y, &accel_z)) {
        game->tilt_x = accel_x;
    } else {
        game->tilt_x = 0;
        game->imu_ok = false;
    }

    int16_t target_vx = clamp_i16((int16_t)(game->tilt_x / 2800), -4, 4);
    if (buttons->down[BUTTON_LEFT]) {
        target_vx = (int16_t)(target_vx - 2);
    }
    if (buttons->down[BUTTON_RIGHT]) {
        target_vx = (int16_t)(target_vx + 2);
    }

    game->player_vx = clamp_i16((int16_t)((game->player_vx * 3 + target_vx) / 4), -5, 5);
    game->player_x = clamp_i16((int16_t)(game->player_x + game->player_vx), 4, SCREEN_W - 16);

    const int16_t wall_height = 14;
    for (size_t i = 0; i < MAX_DROP_WALLS; ++i) {
        drop_wall_t *wall = &game->walls[i];
        wall->y = (int16_t)(wall->y - game->speed);

        if (!wall->scored && wall->y + wall_height < game->player_y) {
            wall->scored = true;
            game->score++;
            if ((game->score % 5u) == 0u && game->gap_width > 42) {
                game->gap_width -= 2;
                if (game->speed < 6) {
                    game->speed++;
                }
            }
        }

        if (wall->y < -wall_height) {
            int16_t max_y = SCREEN_H;
            for (size_t j = 0; j < MAX_DROP_WALLS; ++j) {
                if (game->walls[j].y > max_y) {
                    max_y = game->walls[j].y;
                }
            }
            wall->y = (int16_t)(max_y + 58);
            wall->gap_width = game->gap_width;
            wall->gap_x = (int16_t)(12 + (rng_next() % (SCREEN_W - wall->gap_width - 24)));
            wall->scored = false;
        }

        rect_i16_t player_box = {game->player_x, game->player_y, 12, 22};
        rect_i16_t left_bar = {0, wall->y, wall->gap_x, wall_height};
        rect_i16_t right_bar = {(int16_t)(wall->gap_x + wall->gap_width), wall->y, (int16_t)(SCREEN_W - (wall->gap_x + wall->gap_width)), wall_height};
        if (rects_overlap(player_box, left_bar) || rects_overlap(player_box, right_bar)) {
            game->game_over = true;
        }
    }
}

static void draw_drop_game(ili9341_mono_t *display, const drop_game_t *game) {
    ili9341_mono_clear(display, false);

    for (int16_t x = 8; x < SCREEN_W; x += 24) {
        const int16_t star_y = (int16_t)((x * 11 + game->score * 9) % SCREEN_H);
        ili9341_mono_draw_pixel(display, x, star_y, true);
        ili9341_mono_draw_pixel(display, (int16_t)(x + 7 < SCREEN_W ? x + 7 : x), (int16_t)((star_y + 36) % SCREEN_H), true);
    }

    ili9341_mono_draw_rect(display, 0, 0, SCREEN_W, SCREEN_H, true);
    for (size_t i = 0; i < MAX_DROP_WALLS; ++i) {
        const drop_wall_t *wall = &game->walls[i];
        ili9341_mono_fill_rect(display, 0, wall->y, wall->gap_x, 14, true);
        ili9341_mono_fill_rect(display, (int16_t)(wall->gap_x + wall->gap_width), wall->y, (int16_t)(SCREEN_W - (wall->gap_x + wall->gap_width)), 14, true);
    }

    draw_drop_player(display, game->player_x, game->player_y);
    draw_text(display, 8, 8, "DROP", 1);
    draw_text(display, 8, 20, "SCORE", 1);
    draw_number(display, 44, 20, game->score, 1);

    if (game->imu_ok) {
        draw_text(display, 150, 8, "IMU", 1);
    } else {
        draw_text(display, 140, 8, "BUTTON", 1);
    }

    if (game->game_over) {
        ili9341_mono_fill_rect(display, 28, 116, 184, 62, false);
        ili9341_mono_draw_rect(display, 28, 116, 184, 62, true);
        draw_text_centered(display, 120, 130, "TOO SLOW", 2);
        draw_text_centered(display, 120, 154, "A AGAIN B MENU", 1);
    }
}

static void draw_menu(ili9341_mono_t *display, uint8_t selected, bool imu_ok) {
    ili9341_mono_clear(display, false);
    ili9341_mono_draw_rect(display, 0, 0, SCREEN_W, SCREEN_H, true);
    ili9341_mono_draw_rect(display, 6, 6, SCREEN_W - 12, SCREEN_H - 12, true);

    for (int16_t x = 18; x < SCREEN_W; x += 26) {
        ili9341_mono_draw_line(display, x, 0, (int16_t)(x - 12), 40, true);
        ili9341_mono_draw_line(display, x, SCREEN_H - 1, (int16_t)(x - 12), SCREEN_H - 41, true);
    }

    draw_text_centered(display, 120, 28, "PICO ARCADE", 2);
    draw_text_centered(display, 120, 56, "UP DOWN TO CHOOSE", 1);
    draw_text_centered(display, 120, 68, "A START B BACK", 1);

    ili9341_mono_fill_rect(display, 24, 104, 192, 72, false);
    ili9341_mono_fill_rect(display, 24, 196, 192, 72, false);
    ili9341_mono_draw_rect(display, 24, 104, 192, 72, true);
    ili9341_mono_draw_rect(display, 24, 196, 192, 72, true);

    if (selected == 0u) {
        ili9341_mono_draw_rect(display, 20, 100, 200, 80, true);
    } else {
        ili9341_mono_draw_rect(display, 20, 192, 200, 80, true);
    }

    ili9341_mono_fill_rect(display, 40, 126, 34, 10, true);
    ili9341_mono_fill_rect(display, 50, 110, 12, 16, true);
    ili9341_mono_fill_rect(display, 92, 140, 42, 10, true);
    ili9341_mono_draw_line(display, 134, 140, 160, 122, true);
    ili9341_mono_draw_line(display, 160, 122, 180, 140, true);
    draw_text(display, 94, 116, "PLUMBER", 1);
    draw_text(display, 94, 128, "RUN JUMP GOAL", 1);

    draw_drop_player(display, 48, 210);
    ili9341_mono_fill_rect(display, 20, 238, 54, 10, true);
    ili9341_mono_fill_rect(display, 120, 222, 96, 10, true);
    draw_text(display, 94, 208, "DROP", 1);
    draw_text(display, 94, 220, "TILT LEFT RIGHT", 1);

    if (imu_ok) {
        draw_text_centered(display, 120, 286, "Y RECALIBRATE IMU READY", 1);
    } else {
        draw_text_centered(display, 120, 286, "NO IMU LEFT RIGHT FALLBACK", 1);
    }
}

int main(void) {
    stdio_init_all();
    sleep_ms(120);

    ili9341_mono_t display;
    const ili9341_mono_config_t display_config = {
        .spi = LCD_SPI_PORT,
        .baudrate_hz = 40000000u,
        .pin_sck = LCD_PIN_SCK,
        .pin_mosi = LCD_PIN_MOSI,
        .pin_cs = LCD_PIN_CS,
        .pin_dc = LCD_PIN_DC,
        .pin_rst = LCD_PIN_RST,
        .pin_bl = LCD_PIN_BL,
        .fg_color = ILI9341_MONO_COLOR_WHITE,
        .bg_color = ILI9341_MONO_COLOR_BLACK,
        .buffer_a = NULL,
        .buffer_b = NULL,
    };

    if (!ili9341_mono_init(&display, &display_config)) {
        while (true) {
            sleep_ms(1000);
        }
    }

    buttons_t buttons;
    buttons_init(&buttons);

    lsm6ds3_t imu;
    const lsm6ds3_config_t imu_config = {
        .i2c = IMU_I2C_PORT,
        .baudrate_hz = 400000u,
        .address = IMU_I2C_ADDR,
        .pin_sda = IMU_PIN_SDA,
        .pin_scl = IMU_PIN_SCL,
    };
    lsm6ds3_init(&imu, &imu_config);

    platformer_t platformer;
    drop_game_t drop_game;
    platformer_reset(&platformer);
    drop_reset(&drop_game, imu.ready);

    app_state_t state = APP_MENU;
    uint8_t menu_index = 0u;
    absolute_time_t last_frame = get_absolute_time();

    while (true) {
        buttons_update(&buttons);

        if (buttons.pressed[BUTTON_Y] && imu.ready) {
            lsm6ds3_calibrate(&imu, 24u);
            drop_game.imu_ok = true;
        }

        if (state == APP_MENU) {
            if (buttons.pressed[BUTTON_UP] || buttons.pressed[BUTTON_LEFT]) {
                menu_index = menu_index == 0u ? 1u : 0u;
            }
            if (buttons.pressed[BUTTON_DOWN] || buttons.pressed[BUTTON_RIGHT]) {
                menu_index = menu_index == 0u ? 1u : 0u;
            }
            if (buttons.pressed[BUTTON_A]) {
                if (menu_index == 0u) {
                    platformer_reset(&platformer);
                    state = APP_PLATFORMER;
                } else {
                    drop_reset(&drop_game, imu.ready);
                    state = APP_DROP;
                }
            }
            draw_menu(&display, menu_index, imu.ready);
        } else if (state == APP_PLATFORMER) {
            if (platformer.game_over || platformer.won) {
                if (buttons.pressed[BUTTON_A]) {
                    platformer_reset(&platformer);
                }
                if (buttons.pressed[BUTTON_B]) {
                    state = APP_MENU;
                }
            } else if (buttons.pressed[BUTTON_B]) {
                state = APP_MENU;
            } else {
                platformer_step(&platformer, &buttons);
            }
            draw_platformer(&display, &platformer);
        } else {
            if (drop_game.game_over) {
                if (buttons.pressed[BUTTON_A]) {
                    drop_reset(&drop_game, imu.ready);
                }
                if (buttons.pressed[BUTTON_B]) {
                    state = APP_MENU;
                }
            } else if (buttons.pressed[BUTTON_B]) {
                state = APP_MENU;
            } else {
                drop_step(&drop_game, &buttons, imu.ready ? &imu : NULL);
            }
            draw_drop_game(&display, &drop_game);
        }

        ili9341_mono_present(&display);

        const absolute_time_t frame_start = get_absolute_time();
        const int64_t elapsed_us = absolute_time_diff_us(last_frame, frame_start);
        if (elapsed_us < 16600) {
            sleep_us((uint32_t)(16600 - elapsed_us));
        }
        last_frame = get_absolute_time();
    }
}
