/*
 * Round walnut buttons on the table for a game's own actions (Word Tiles' play, shuffle, recall, exchange and
 * challenge; Liar's Dice's bid and call): a turned walnut disc, a cream picture on it (drawn by code, no words), lit
 * (a gold light round it: it can be pressed now), dimmed, or held down (on: a mode that is on).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only, so a shared .c file would
 * be left out of one game's build or the other's (as race.h). The pictures are figures in a unit square (art.h).
 */
#ifndef KNOBS_H
#define KNOBS_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "games/race.h"
#include "shell.h"

static const uint8_t KNOB_WALNUT[3] = {104, 70, 46}, KNOB_DARK[3] = {52, 34, 22}, KNOB_CREAM[3] = {246, 238, 220},
                     KNOB_RED[3] = {196, 64, 48}, KNOB_GOLD[3] = {232, 190, 92};

/* the walnut disc (size x size, cleared first) */
static inline void knob_disc(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    art_style st = race_style(KNOB_WALNUT, KNOB_DARK, 0.035f, 0.35f);
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.47f, 0.47f);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
}

/* a figure painted flat over a picture in one colour (its outline `line` in `ink`), then freed */
static inline void knob_paint(uint8_t *o, int size, art_fig *f, const uint8_t rgb[3], const uint8_t ink[3], float line)
{
    art_style st = race_style(rgb, ink ? ink : rgb, line, 0.1f);
    uint8_t *b = calloc((size_t)size * size, 4);
    if (b) {
        art_render(f, &st, size, size, (float)size, 0, 0, b);
        art_blend(o, size, size, b, size, size, 0, 0);
        free(b);
    }
    art_fig_free(f);
}

/* a band from (x0, y0) to (x1, y1), wd wide (a stroke of a picture) */
static inline void knob_band(art_fig *f, float x0, float y0, float x1, float y1, float wd)
{
    float dx = x1 - x0, dy = y1 - y0, l = hypotf(dx, dy) + 1e-6f, nx = -dy / l * wd / 2, ny = dx / l * wd / 2;
    const float q[] = {x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny};
    art_poly(f, ART_PART, q, 4);
}

/* an arrow head at (x, y) pointing at angle deg (0 right, 90 down), long l */
static inline void knob_head(art_fig *f, float x, float y, float deg, float l)
{
    float a = deg * RACE_PI / 180, c = cosf(a), s = sinf(a), bx = x - c * l * 0.45f, by = y - s * l * 0.45f;
    const float q[] = {x + c * l * 0.55f, y + s * l * 0.55f, bx - s * l * 0.5f, by + c * l * 0.5f, bx + s * l * 0.5f,
                       by - c * l * 0.5f};
    art_poly(f, ART_PART, q, 3);
}

/* a check mark (done, play, bid) */
static inline void knob_check(art_fig *f)
{
    static const float tick[] = {0.25f, 0.52f, 0.33f, 0.44f, 0.45f, 0.56f, 0.70f, 0.29f, 0.78f, 0.37f, 0.45f, 0.72f};
    art_poly(f, ART_PART, tick, 6);
}

/* a red pennant on a pole (a challenge, a call) */
static inline void knob_flag(uint8_t *o, int size)
{
    art_fig *f = art_fig_new();
    art_rect(f, ART_PART, 0.33f, 0.2f, 0.38f, 0.8f);
    art_ellipse(f, ART_PART, 0.355f, 0.2f, 0.04f, 0.04f);
    knob_paint(o, size, f, KNOB_CREAM, KNOB_DARK, 0.01f);
    f = art_fig_new();
    static const float flag[] = {0.38f, 0.23f, 0.74f, 0.33f, 0.38f, 0.5f};
    art_poly(f, ART_PART, flag, 3);
    knob_paint(o, size, f, KNOB_RED, KNOB_DARK, 0.015f);
}

/* a set of button pictures for one game, made at one size (remade when it changes): paint(kind, size, rgba) draws
 * kind's picture over the walnut disc */
typedef struct {
    int size, n;
    SDL_Texture *t[16];
} knob_set;

static inline SDL_Texture *knob_tex(knob_set *k, int kind, int size, void (*paint)(int kind, int size, uint8_t *o))
{
    if (size < 4 || kind < 0 || kind >= 16) return NULL;
    if (k->size != size) {
        for (int i = 0; i < 16; i++) ui_free(&k->t[i]);
        k->size = size;
    }
    if (!k->t[kind]) {
        uint8_t *o = ui_buf(size, size);
        knob_disc(size, o);
        paint(kind, size, o);
        k->t[kind] = ui_tex(size, size, o);
    }
    return k->t[kind];
}

/* a button on the screen: lit (a gold light: it can be pressed), dimmed (not now), on (pressed in: a mode) */
enum { KNOB_DIM, KNOB_READY, KNOB_LIT, KNOB_ON };
static inline void knob_at(SDL_Texture *t, float cx, float cy, float d, int state)
{
    if (!t) return;
    if (state == KNOB_LIT || state == KNOB_ON) ui_mark_gold(cx, cy, 1.6f * d);
    ui_shadow_flat(cx, cy, d * 1.05f, d * 1.05f, state == KNOB_DIM ? 0.3f : state == KNOB_ON ? 0.35f : 0.7f);
    SDL_SetTextureAlphaMod(t, state == KNOB_DIM ? 110 : 255);
    if (state == KNOB_ON) SDL_SetTextureColorMod(t, 210, 200, 180);
    float s = state == KNOB_ON ? 0.94f : 1;
    ui_put_c(t, cx, cy + (state == KNOB_ON ? 0.03f * d : 0), d * s, d * s);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

static inline int knob_hit(float cx, float cy, float d, float x, float y)
{
    return hypotf(x - cx, y - cy) <= 0.55f * d;
}

#endif
