/*
 * What Pipes and Paths share on the screen: the hint button at the right end of the bar (a walnut disc with a brass
 * rim and a lit lamp on it, drawn by code), the tiles of the board cut in the house plank (each square bevelled, a
 * groove between them), and the gold light of the win running over the board.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="pipes" builds
 * pipes.c and pipes_*.c), so a shared .c file would be left out of one game's build or the other's.
 */
#ifndef ROUTES_H
#define ROUTES_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "games/race.h"
#include "shell.h"

static const uint8_t RTE_WALNUT[3] = {98, 64, 42}, RTE_BRASS[3] = {214, 170, 84}, RTE_DARK[3] = {50, 33, 22},
                     RTE_GLASS[3] = {252, 226, 140};

/* a figure painted over a picture (size x size) in one colour, its outline in ink; the figure freed */
static inline void rte_paint(uint8_t *o, int size, art_fig *f, const uint8_t rgb[3], const uint8_t ink[3], float line,
                             float light)
{
    art_style st = race_style(rgb, ink, line, light);
    uint8_t *b = calloc((size_t)size * size, 4);
    if (b) {
        art_render(f, &st, size, size, (float)size, 0, 0, b);
        art_blend(o, size, size, b, size, size, 0, 0);
        free(b);
    }
    art_fig_free(f);
}

/* the hint button's picture: the walnut disc in its brass rim, a lamp lit on it (a glass bulb, its screw base, rays) */
static inline void rte_lamp_paint(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.48f, 0.48f);
    rte_paint(o, size, f, RTE_BRASS, RTE_DARK, 0.03f, 0.5f);
    f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.41f, 0.41f);
    rte_paint(o, size, f, RTE_WALNUT, RTE_DARK, 0.02f, 0.35f);
    f = art_fig_new();                          /* the rays */
    for (int i = 0; i < 7; i++) {
        float a = (180 + 30 * i) * RACE_PI / 180, c = cosf(a), s = sinf(a), w = 0.022f;
        float x0 = 0.5f + c * 0.25f, y0 = 0.43f + s * 0.25f, x1 = 0.5f + c * 0.33f, y1 = 0.43f + s * 0.33f;
        const float q[] = {x0 - s * w, y0 + c * w, x1 - s * w, y1 + c * w,
                           x1 + s * w, y1 - c * w, x0 + s * w, y0 - c * w};
        art_poly(f, ART_PART, q, 4);
    }
    rte_paint(o, size, f, RTE_GLASS, RTE_GLASS, 0, 0);
    f = art_fig_new();                          /* the bulb and its neck */
    art_ellipse(f, ART_PART, 0.5f, 0.43f, 0.16f, 0.16f);
    const float neck[] = {0.41f, 0.52f, 0.59f, 0.52f, 0.565f, 0.62f, 0.435f, 0.62f};
    art_poly(f, ART_PART, neck, 4);
    rte_paint(o, size, f, RTE_GLASS, RTE_DARK, 0.018f, 0.4f);
    f = art_fig_new();                          /* the screw base */
    art_rect(f, ART_PART, 0.43f, 0.625f, 0.57f, 0.665f);
    art_rect(f, ART_PART, 0.44f, 0.675f, 0.56f, 0.715f);
    art_ellipse(f, ART_PART, 0.5f, 0.74f, 0.035f, 0.025f);
    rte_paint(o, size, f, RTE_BRASS, RTE_DARK, 0.012f, 0.3f);
}

/* the hint button at the right end of the bar */
typedef struct { SDL_Texture *t; int size; float x, y, d; box bar; } rte_lamp;

static inline void rte_lamp_place(rte_lamp *l, box b)
{
    l->bar = b;
    l->d = 0.86f * ui.unit;
    l->x = b.x + b.w - l->d / 2 - 0.06f * ui.unit;
    l->y = b.y + b.h / 2;
}

/* drawn lit (a gold light round it: a hint can be had) or dimmed */
static inline void rte_lamp_draw(rte_lamp *l, box b, int lit)
{
    rte_lamp_place(l, b);
    int size = (int)(l->d + 0.5f);
    if (l->size != size || !l->t) {
        l->size = size;
        ui_free(&l->t);
        uint8_t *p = ui_buf(size, size);
        rte_lamp_paint(size, p);
        l->t = ui_tex(size, size, p);
    }
    if (lit) ui_mark_gold(l->x, l->y, 1.3f * l->d);
    ui_shadow_flat(l->x, l->y, l->d, l->d, lit ? 0.6f : 0.3f);
    SDL_SetTextureAlphaMod(l->t, lit ? 255 : 110);
    ui_put_c(l->t, l->x, l->y, l->d, l->d);
    SDL_SetTextureAlphaMod(l->t, 255);
}

static inline int rte_lamp_hit(const rte_lamp *l, float x, float y)
{
    return l->bar.w > 0 && hypotf(x - l->x, y - l->y) <= 0.62f * l->d;
}

/* the board: the plank in its walnut frame (side S, frame f), n x n squares of q pixels from (ox, oy) cut in it: each
 * one bevelled (lit at the top left), a dark groove between them */
static inline void rte_tiles(uint8_t *o, int S, int f, int n, int q, int ox, int oy)
{
    art_board_n(1, 1, S - 2 * f, f, 0, o);
    float b = fmaxf(1.5f, 0.07f * q), groove = fmaxf(1, 0.035f * q);
    for (int y = oy; y < oy + n * q && y < S; y++)
        for (int x = ox; x < ox + n * q && x < S; x++) {
            float px = (float)((x - ox) % q) + 0.5f, py = (float)((y - oy) % q) + 0.5f;
            float dl = px, dr = q - px, dt = py, db = q - py, d = fminf(fminf(dl, dr), fminf(dt, db));
            uint8_t *p = o + 4 * ((size_t)y * S + x);
            float k = 1;
            if (d < groove) k = 0.55f + 0.2f * d / groove;
            else if (d < b) {
                float s = d == dl || d == dt ? 1 : -1;   /* the top and left lit, the bottom and right shaded */
                k = 1 + s * 0.16f * (1 - (d - groove) / (b - groove));
            }
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * k, 0, 255);
        }
}

#endif
