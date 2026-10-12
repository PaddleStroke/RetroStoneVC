/*
 * What four logic grids share on the screen (Futoshiki, Rectangles, Star Battle, Dominosa): the board's plank in its
 * walnut frame at any width and height, wooden tiles in several tones, round walnut buttons with a cream picture
 * (the lamp of a hint, the eye of the clashes, the pencil of the notes, the eraser), the win's wave of gold light,
 * the bar's lamp, and the difficulty's ebony stones under a picker's card. The rules' side is logicgrid2.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only, so a shared .c file would
 * be left out of one game's build or another's.
 */
#ifndef LOGICGRID2_UI_H
#define LOGICGRID2_UI_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "games/race.h"
#include "shell.h"

#define LG2_WAVE_STEP 55                        /* the win's wave: ms a cell further on */
#define LG2_WAVE_LEN 560
#define LG2_HINT_MS 1500                        /* a hint's gold light */

static const uint8_t LG2_WALNUT[3] = {104, 70, 46}, LG2_DARK[3] = {52, 34, 22}, LG2_RED[3] = {196, 64, 48},
                     LG2_GOLD[3] = {232, 190, 92}, LG2_BLUE[3] = {30, 64, 140}, LG2_CLASH[3] = {184, 44, 34};

/* ---- the wood ---------------------------------------------------------------------------------------------------- */
/* the house plank in its walnut frame (fr pixels), w x h: a square board's picture with its middle rows or columns
 * left out (the grain runs across, so the seam does not show) */
static inline void lg2_plank(uint8_t *o, int w, int h, int fr)
{
    int s = w > h ? w : h;
    uint8_t *sq = malloc((size_t)s * s * 4);
    if (!sq) return;
    art_board_n(1, 1, s - 2 * fr, fr, 0, sq);
    for (int y = 0; y < h; y++) {
        int sy = y < h / 2 ? y : s - h + y;
        for (int x = 0; x < w; x++) {
            int sx = x < w / 2 ? x : s - w + x;
            memcpy(o + 4 * ((size_t)y * w + x), sq + 4 * ((size_t)sy * s + sx), 4);
        }
    }
    free(sq);
}

/* the woods' tones, factors on the house plank (the grain stays): tiles, regions */
#define LG2_TONES 6
static const float LG2_TONE[LG2_TONES][3] = {
    {1.13f, 1.11f, 1.07f},                      /* maple */
    {1.06f, 0.93f, 0.70f},                      /* honey */
    {1.04f, 0.80f, 0.68f},                      /* cherry */
    {1.10f, 1.00f, 0.87f},                      /* beech */
    {0.99f, 1.02f, 0.86f},                      /* olive ash */
    {0.93f, 0.74f, 0.64f},                      /* padauk, faded */
};

/* a wooden tile, w x h: the plank tinted (tone: a factor per channel, the grain stays), rounded (rad) and bevelled */
static inline void lg2_tile(uint8_t *o, int w, int h, float rad, const float tone[3])
{
    int s = w > h ? w : h;
    uint8_t *plank = malloc((size_t)s * s * 4);
    if (!plank) return;
    art_board_n(1, 1, s, 0, 0, plank);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * ((size_t)y * w + x), *q = plank + 4 * ((size_t)y * s + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(q[i] * tone[i], 0, 255);
            p[3] = 255;
        }
    free(plank);
    puz_bevel(o, w, h, rad, fmaxf(2, 0.1f * (w < h ? w : h)), 0.34f);
}

/* ---- round walnut buttons with a cream picture ------------------------------------------------------------------ */
enum { LG2_LAMP, LG2_EYE, LG2_PENCIL, LG2_ERASER, LG2_PICS };
enum { LG2_DIM, LG2_READY, LG2_ON };             /* dimmed (not now), ready, held down (a mode that is on) */

/* a figure painted flat over a picture in one colour, its outline `line` in ink; then freed */
static inline void lg2_paint(uint8_t *o, int size, art_fig *f, const uint8_t rgb[3], const uint8_t ink[3], float line)
{
    art_style st = race_style(rgb, ink, line, 0.1f);
    uint8_t *b = calloc((size_t)size * size, 4);
    if (b) {
        art_render(f, &st, size, size, (float)size, 0, 0, b);
        art_blend(o, size, size, b, size, size, 0, 0);
        free(b);
    }
    art_fig_free(f);
}

/* a stroke from (x0, y0) to (x1, y1), wd wide */
static inline void lg2_band(art_fig *f, float x0, float y0, float x1, float y1, float wd)
{
    float dx = x1 - x0, dy = y1 - y0, l = hypotf(dx, dy) + 1e-6f, nx = -dy / l * wd / 2, ny = dx / l * wd / 2;
    const float q[] = {x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny};
    art_poly(f, ART_PART, q, 4);
}

static inline void lg2_pic(int kind, int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();                 /* the disc: turned walnut */
    art_style st = race_style(LG2_WALNUT, LG2_DARK, 0.035f, 0.35f);
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.47f, 0.47f);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
    f = art_fig_new();
    switch (kind) {
    case LG2_LAMP: {                            /* a bulb, lit, its rays */
        art_ellipse(f, ART_PART, 0.5f, 0.43f, 0.16f, 0.16f);
        const float neck[] = {0.41f, 0.53f, 0.59f, 0.53f, 0.57f, 0.64f, 0.43f, 0.64f};
        art_poly(f, ART_PART, neck, 4);
        lg2_paint(o, size, f, LG2_GOLD, LG2_DARK, 0.02f);
        f = art_fig_new();
        art_rect(f, ART_PART, 0.43f, 0.665f, 0.57f, 0.705f);
        art_rect(f, ART_PART, 0.45f, 0.725f, 0.55f, 0.765f);
        for (int i = -2; i <= 2; i++) {
            float a = (90 + 38 * i) * PUZ_PI / 180, c = cosf(a), s = -sinf(a);
            lg2_band(f, 0.5f + c * 0.22f, 0.43f + s * 0.22f, 0.5f + c * 0.3f, 0.43f + s * 0.3f, 0.045f);
        }
        lg2_paint(o, size, f, PUZ_CREAM, PUZ_CREAM, 0);
        return;
    }
    case LG2_EYE: {                             /* an eye, its red iris */
        float lens[2 * 26];
        for (int i = 0; i <= 12; i++) {
            float t = i / 12.0f;
            lens[2 * i] = 0.21f + 0.58f * t;
            lens[2 * i + 1] = 0.5f - 0.19f * sinf(PUZ_PI * t);
            lens[2 * (13 + i)] = 0.79f - 0.58f * t;
            lens[2 * (13 + i) + 1] = 0.5f + 0.19f * sinf(PUZ_PI * t);
        }
        art_poly(f, ART_PART, lens, 26);
        lg2_paint(o, size, f, PUZ_CREAM, LG2_DARK, 0.02f);
        f = art_fig_new();
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.125f, 0.125f);
        lg2_paint(o, size, f, LG2_RED, LG2_DARK, 0.015f);
        f = art_fig_new();
        art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.05f, 0.05f);
        lg2_paint(o, size, f, LG2_DARK, LG2_DARK, 0);
        return;
    }
    case LG2_PENCIL: {                          /* a pencil, its point down to the left */
        lg2_band(f, 0.41f, 0.59f, 0.72f, 0.28f, 0.15f);
        lg2_paint(o, size, f, PUZ_CREAM, LG2_DARK, 0.02f);
        f = art_fig_new();
        const float tip[] = {0.357f, 0.537f, 0.463f, 0.643f, 0.27f, 0.73f};
        art_poly(f, ART_PART, tip, 3);
        lg2_paint(o, size, f, LG2_GOLD, LG2_DARK, 0.02f);
        f = art_fig_new();
        const float lead[] = {0.305f, 0.655f, 0.345f, 0.695f, 0.27f, 0.73f};
        art_poly(f, ART_PART, lead, 3);
        lg2_paint(o, size, f, LG2_DARK, LG2_DARK, 0);
        return;
    }
    default:                                    /* an eraser, half red, half cream */
        lg2_band(f, 0.29f, 0.63f, 0.5f, 0.46f, 0.25f);
        lg2_paint(o, size, f, LG2_RED, LG2_DARK, 0.02f);
        f = art_fig_new();
        lg2_band(f, 0.5f, 0.46f, 0.71f, 0.29f, 0.25f);
        lg2_paint(o, size, f, PUZ_CREAM, LG2_DARK, 0.02f);
        return;
    }
}

typedef struct { int size; SDL_Texture *t[LG2_PICS]; } lg2_buttons;

static inline SDL_Texture *lg2_button_tex(lg2_buttons *k, int kind, int size)
{
    if (size < 4 || kind < 0 || kind >= LG2_PICS) return NULL;
    if (k->size != size) {
        for (int i = 0; i < LG2_PICS; i++) ui_free(&k->t[i]);
        k->size = size;
    }
    if (!k->t[kind]) {
        uint8_t *o = ui_buf(size, size);
        lg2_pic(kind, size, o);
        k->t[kind] = ui_tex(size, size, o);
    }
    return k->t[kind];
}

/* a button d wide centred on (cx, cy): ready, dimmed, or on (pressed in, in gold light) */
static inline void lg2_button(lg2_buttons *k, int kind, float cx, float cy, float d, int state)
{
    SDL_Texture *t = lg2_button_tex(k, kind, (int)d);
    if (!t) return;
    if (state == LG2_ON) ui_mark_gold(cx, cy, 1.6f * d);
    ui_shadow_flat(cx, cy, d * 1.05f, d * 1.05f, state == LG2_DIM ? 0.3f : state == LG2_ON ? 0.35f : 0.7f);
    SDL_SetTextureAlphaMod(t, state == LG2_DIM ? 110 : 255);
    if (state == LG2_ON) SDL_SetTextureColorMod(t, 210, 200, 180);
    float s = state == LG2_ON ? 0.94f : 1;
    ui_put_c(t, cx, cy + (state == LG2_ON ? 0.03f * d : 0), d * s, d * s);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

static inline int lg2_hit(float cx, float cy, float d, float x, float y) { return hypotf(x - cx, y - cy) <= 0.6f * d; }

/* ---- the bar: the moves, and a lamp at its right end (a hint) ---------------------------------------------------- */
typedef struct { box b; float lx, ly, ld; } lg2_bar;

static inline void lg2_bar_draw(lg2_bar *k, lg2_buttons *btn, box b, SDL_Texture *icon, int moves, int lamp)
{
    k->b = b;
    k->ld = 0.84f * ui.unit;
    k->lx = b.x + b.w - k->ld / 2 - 0.05f * ui.unit;
    k->ly = b.y + b.h / 2;
    puz_bar_moves(b, icon, moves);
    if (lamp) lg2_button(btn, LG2_LAMP, k->lx, k->ly, k->ld, LG2_READY);
}

static inline int lg2_bar_lamp(const lg2_bar *k, float x, float y)
{
    return k->b.w > 0 && lg2_hit(k->lx, k->ly, k->ld, x, y);
}

/* ---- the win: a wave of gold light from a cell ------------------------------------------------------------------ */
typedef struct { int on; float fx, fy; Uint32 t0; } lg2_wave;

static inline void lg2_wave_start(lg2_wave *w, float fx, float fy)
{
    w->on = 1;
    w->fx = fx;
    w->fy = fy;
    w->t0 = ui_ms();
}

/* the wave's length (ms) over a grid nx x ny */
static inline Uint32 lg2_wave_end(int nx, int ny) { return (Uint32)(hypotf((float)nx, (float)ny) * LG2_WAVE_STEP + LG2_WAVE_LEN); }

static inline int lg2_wave_busy(const lg2_wave *w, int nx, int ny) { return w->on && ui_ms() - w->t0 < lg2_wave_end(nx, ny); }

/* the light over the cell (gx, gy) of the grid, centred on (cx, cy), the cell cs wide */
static inline void lg2_wave_cell(const lg2_wave *w, float gx, float gy, float cx, float cy, float cs)
{
    if (!w->on) return;
    float t = (float)(ui_ms() - w->t0), dist = hypotf(gx - w->fx, gy - w->fy);
    float p = puz_pulse(t, dist * LG2_WAVE_STEP, LG2_WAVE_LEN);
    if (p > 0) ui_mark_gold(cx, cy, 2.3f * cs * p);
}

/* after the cells: on while it runs */
static inline void lg2_wave_tick(lg2_wave *w, int nx, int ny)
{
    if (!w->on) return;
    if (ui_ms() - w->t0 < lg2_wave_end(nx, ny)) ui_animate();
    else w->on = 0;
}

/* a hint's cell: a gold light swelling and fading; 0 once it has faded */
static inline int lg2_hint_glow(Uint32 t0, float cx, float cy, float cs)
{
    float p = puz_pulse((float)(ui_ms() - t0), 0, LG2_HINT_MS);
    if (p <= 0) return 0;
    ui_mark_gold(cx, cy, 2.6f * cs * p);
    ui_animate();
    return 1;
}

/* ---- pictures for the shell ------------------------------------------------------------------------------------- */
/* the level's ebony stones (level + 1 of them) in a row centred at y (their top), in a picker's card */
static inline void lg2_stones(uint8_t *rgba, int size, int y, int level)
{
    int d = (int)(0.1f * size);
    uint8_t *st = calloc((size_t)d * d, 4);
    if (!st) return;
    art_counter(COUNTER_STONE, 1, d, st);
    for (int i = 0; i <= level; i++)
        art_blend(rgba, size, size, st, d, d, (int)(size / 2.0f + (i - level / 2.0f) * 1.25f * d - d / 2.0f), y);
    free(st);
}

/* the bar's small picture: the game's token at 0.6 unit, remade when the unit changes */
static inline void lg2_small(SDL_Texture **t, int *made, void (*token)(int player, int size, uint8_t *rgba))
{
    int small = (int)(0.6f * ui.unit);
    if (*made == small && *t) return;
    *made = small;
    ui_free(t);
    uint8_t *b = calloc((size_t)small * small, 4);
    if (!b) return;
    token(0, small, b);
    *t = ui_tex(small, small, b);
    free(b);
}

#endif
