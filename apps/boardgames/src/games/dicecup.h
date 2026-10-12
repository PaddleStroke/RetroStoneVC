/*
 * What the dice games played on the felt share (yacht, farkle, fourtwoone): the green felt in its walnut frame, a die's
 * rest and slide on it, the throws remembered so that undo never throws again, the pictures of the felt's buttons
 * (throw, bank; the buttons are the shell's), a bone chip, where the players sit. The dice themselves are the shell's,
 * each at its own place (ui_die, ui_die_thrown).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="yacht" builds
 * yacht.c and yacht_*.c), so a shared .c file would be left out of one game's build or the other's (as race.h).
 */
#ifndef DICECUP_H
#define DICECUP_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "games/race.h"
#include "shell.h"

#define CUP_FR 0.035f                           /* the walnut frame, of the board's side */
#define CUP_SLIDE_MS 200                        /* a die kept or set aside sliding to its place */
#define CUP_KEYS 16384                          /* throws remembered (a game's own keys: turn, throw...) */

static const uint8_t CUP_INK[3] = {84, 56, 36}, CUP_CREAM[3] = {246, 238, 220};

/* ---- pictures ---------------------------------------------------------------------------------------------------- */
/* a rectangle tinted over a picture (a line, a well): rgb at alpha a, clipped */
static inline void cup_rect(uint8_t *o, int w, int h, float x0, float y0, float x1, float y1, const uint8_t rgb[3],
                            float a)
{
    int ix0 = (int)floorf(x0), iy0 = (int)floorf(y0), ix1 = (int)ceilf(x1), iy1 = (int)ceilf(y1);
    for (int y = iy0 < 0 ? 0 : iy0; y < iy1 && y < h; y++)
        for (int x = ix0 < 0 ? 0 : ix0; x < ix1 && x < w; x++) {
            float cx = fminf(x + 1.0f, x1) - fmaxf((float)x, x0), cy = fminf(y + 1.0f, y1) - fmaxf((float)y, y0);
            float k = a * clampf(cx, 0, 1) * clampf(cy, 0, 1);
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)(p[c] + (rgb[c] - p[c]) * k);
            if (p[3] < 255) p[3] = (uint8_t)(p[3] + (255 - p[3]) * k);
        }
}

/* the board: a plank of the house wood in its walnut frame (n x n) */
static inline void cup_board(uint8_t *o, int n)
{
    int fr = (int)(CUP_FR * n);
    art_board_n(1, 1, n - 2 * fr, fr, 0, o);
}

/* a sunken well of felt in the board (fractions of its side): the felt, the walls' dark edges */
static inline void cup_well(uint8_t *o, int n, float x0, float y0, float x1, float y1)
{
    int ix0 = (int)(x0 * n), iy0 = (int)(y0 * n), ix1 = (int)(x1 * n), iy1 = (int)(y1 * n);
    art_felt(o, n, n, ix0, iy0, ix1, iy1);
    float e = 0.004f * n;
    cup_rect(o, n, n, ix0 - e, iy0 - e, ix1 + e, iy0 + 0.5f * e, CUP_INK, 0.45f);
    cup_rect(o, n, n, ix0 - e, iy1 - 0.5f * e, ix1 + e, iy1 + e, CUP_INK, 0.45f);
    cup_rect(o, n, n, ix0 - e, iy0, ix0 + 0.5f * e, iy1, CUP_INK, 0.45f);
    cup_rect(o, n, n, ix1 - 0.5f * e, iy0, ix1 + e, iy1, CUP_INK, 0.45f);
}

/* a die's rest on the felt after throw `key`: a small steady tilt (degrees) and offset (-1..1) for die i */
static inline float cup_tilt(int key, int i) { return (race_hash(key, i) - 0.5f) * 24; }
static inline float cup_jitter(int key, int i, int axis) { return 2 * race_hash(key * 7 + axis, i + 11) - 1; }

/* a slide from a to b, f 0..1 of the way (eased) */
static inline float cup_mix(float a, float b, float f) { return a + (b - a) * race_ease(clampf(f, 0, 1)); }

/* ---- the throws, remembered: undo never throws again (the same faces come back) -------------------------------- */
static inline uint8_t (*cup_mem(void))[6]
{
    static uint8_t mem[CUP_KEYS][6];            /* 0: not thrown yet */
    return mem;
}
static inline void cup_mem_reset(void) { memset(cup_mem(), 0, sizeof(uint8_t[CUP_KEYS][6])); }

/* the faces of throw `key` (n dice): thrown once, the same ever after; keys out of range: thrown afresh */
static inline void cup_roll(int key, int n, int *out)
{
    uint8_t *m = key >= 0 && key < CUP_KEYS ? cup_mem()[key] : NULL;
    for (int i = 0; i < n && i < 6; i++) {
        if (m && m[i]) { out[i] = m[i]; continue; }
        out[i] = 1 + ui_random(6);
        if (m) m[i] = (uint8_t)out[i];
    }
}

/* a throw made otherwise (the script): remembered as it fell (0: a die not thrown, left as it was) */
static inline void cup_remember(int key, int n, const int *v)
{
    if (key < 0 || key >= CUP_KEYS) return;
    for (int i = 0; i < n && i < 6; i++)
        if (v[i]) cup_mem()[key][i] = (uint8_t)v[i];
}

/* ---- where the players sit: the first half along the bottom (left to right), the others along the top (right to
 * left): the turn goes round the table. 0..1 of the board, not flipped. */
static inline void cup_home(int player, int players, float *fx, float *fy)
{
    int bottom = (players + 1) / 2, top = players - bottom;
    if (player < bottom) {
        *fx = (player + 0.5f) / bottom;
        *fy = 0.92f;
    } else {
        *fx = 1 - (player - bottom + 0.5f) / (top ? top : 1);
        *fy = 0.08f;
    }
}

/* ---- the felt's buttons: the shell's (ui_button), their pictures: throw, bank; stop is the shell's tick ---------- */
enum { CUP_THROW, CUP_BANK, CUP_STOP, CUP_BUTTONS };

static inline void cup_button_pic(int kind, int size, uint8_t *o)
{
    if (kind == CUP_THROW) {                    /* a die in mid-air, turning, two marks of its flight */
        art_die_blend(o, size, size, 0.56f * size, 0.47f * size, (int)(0.58f * size), 0, 5, -18);
        art_fig *f = art_fig_new();
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.33f, 0.37f, 150, 200);
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.24f, 0.28f, 155, 195);
        art_style st = art_flat(CUP_CREAM, CUP_CREAM, 0, 0);
        uint8_t *b = calloc((size_t)size * size, 4);
        if (b) {
            art_render(f, &st, size, size, (float)size, -0.06f * size, 0.02f * size, b);
            art_blend(o, size, size, b, size, size, 0, 0);
            free(b);
        }
        art_fig_free(f);
    } else if (kind == CUP_BANK) {              /* a stack of gold coins */
        static const uint8_t gold[3] = {232, 190, 92}, ink[3] = {120, 84, 30};
        art_fig *f = art_fig_new();
        for (int k = 0; k < 4; k++) {
            float y = 0.68f - 0.11f * k;
            art_rect(f, ART_PART, 0.29f, y - 0.05f, 0.71f, y + 0.02f);
            art_ellipse(f, ART_PART, 0.5f, y + 0.02f, 0.21f, 0.07f);
            art_ellipse(f, ART_PART, 0.5f, y - 0.05f, 0.21f, 0.07f);
        }
        art_fig_paint(o, size, f, gold, ink, 0.025f, 0.3f);
    }
}

/* a button on the felt: lit (it can be pressed: a gold light round it) or dimmed, upside down for the top player */
static inline void cup_button_at(int kind, float cx, float cy, float d, int lit, float deg)
{
    int how = (lit ? UI_LIT : UI_DIM) | (deg != 0 ? UI_UPSIDE : 0);
    if (kind == CUP_STOP) ui_button(cx, cy, d, ICON_TICK, how);
    else ui_button_paint(cx, cy, d, cup_button_pic, kind, 1, how);
}

/* ---- a chip (421's jetons): a bone token, its rim turned, a ring inlaid ------------------------------------------ */
static inline void cup_chip_pic(int size, uint8_t *o)
{
    static const uint8_t bone[3] = {236, 224, 196}, rim[3] = {150, 118, 80}, inlay[3] = {168, 64, 48};
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.46f, 0.46f);
    art_style st = art_flat(bone, rim, 0.05f, 0.4f);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
    f = art_fig_new();
    art_arc(f, ART_PART, 0.5f, 0.5f, 0.27f, 0.33f, 0, 360);
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.1f, 0.1f);
    st = art_flat(inlay, inlay, 0, 0.1f);
    uint8_t *b = calloc((size_t)size * size, 4);
    if (b) {
        art_render(f, &st, size, size, (float)size, 0, 0, b);
        art_blend(o, size, size, b, size, size, 0, 0);
        free(b);
    }
    art_fig_free(f);
}

static inline SDL_Texture *cup_chip(int size)
{
    static SDL_Texture *t;
    static int made;
    if (size < 4) return NULL;
    if (made != size || !t) {
        ui_free(&t);
        made = size;
        cup_chip_pic(size, ui_buf(size, size));
        t = ui_tex(size, size, ui_buf(size, size));
    }
    return t;
}

/* a token for the shell (the bars, the end card): a die of the player's colour showing five */
static inline void cup_token(int player, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    int d = (int)(0.8f * size);
    uint8_t *b = malloc((size_t)d * d * 4);
    if (!b) return;
    art_die(5, player, d, b);
    art_blend(rgba, size, size, b, d, d, (size - d) / 2, (size - d) / 2);
    free(b);
}

/* ---- kept for the games written with them (new code: ui_die, ui_die_thrown, art_die_blend, art_felt...) -------- */
static inline float cup_noise(int x, int y) { return art_noise(x, y); }
static inline void cup_felt(uint8_t *o, int w, int h, int x0, int y0, int x1, int y1)
{
    art_felt(o, w, h, x0, y0, x1, y1);
}
static inline void cup_blend_rot(uint8_t *o, int w, int h, const uint8_t *src, int sw, int sh, float cx, float cy,
                                 float deg)
{
    art_blend_rot(o, w, h, src, sw, sh, cx, cy, deg);
}
static inline void cup_paint_die(uint8_t *o, int w, int h, float cx, float cy, int size, int player, int value,
                                 float deg)
{
    art_die_blend(o, w, h, cx, cy, size, player, value, deg);
}
static inline void cup_die_ex(float cx, float cy, float size, int player, int value, float deg, float lift,
                              float alpha, float shade)
{
    ui_die(cx, cy, size, player, value, deg, lift, alpha, shade);
}
static inline void cup_die(float cx, float cy, float size, int player, int value, float deg, float lift, float alpha)
{
    ui_die(cx, cy, size, player, value, deg, lift, alpha, 1);
}
static inline int cup_die_thrown(float cx, float cy, float size, int player, int value, float deg, int i, int t,
                                 float shade)
{
    return ui_die_thrown(cx, cy, size, player, value, deg, i, t, shade);
}

#endif
