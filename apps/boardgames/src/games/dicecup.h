/*
 * What the dice games played on the felt share (yacht, farkle, fourtwoone): the green felt in its walnut frame, the
 * dice in a player's colour drawn anywhere on it (turned, tumbling, sliding), the throws remembered so that undo never
 * throws again, the round wooden buttons on the felt (throw, bank, stop), a bone chip, where the players sit.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="yacht" builds
 * yacht.c and yacht_*.c), so a shared .c file would be left out of one game's build or the other's (as race.h).
 * The shell's dice (ui_dice_throw, ui_dice_draw) are four at most, side by side in one row; these games throw three,
 * five or six, each die at its own place (thrown, kept, set aside), so they draw them here with the shell's art_die
 * and race.h's tumble (the shell's feel).
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
static inline float cup_noise(int x, int y)
{
    uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (h & 255) / 255.0f - 0.5f;
}

/* the green felt in a rectangle of a picture (w x h): fine fibres, darker towards its edges (under the walls) */
static inline void cup_felt(uint8_t *o, int w, int h, int x0, int y0, int x1, int y1)
{
    static const float green[3] = {40, 116, 72};
    float soft = 0.018f * (w > h ? w : h);
    for (int y = y0 < 0 ? 0 : y0; y < y1 && y < h; y++)
        for (int x = x0 < 0 ? 0 : x0; x < x1 && x < w; x++) {
            float e = fminf(fminf((float)(x - x0), (float)(x1 - 1 - x)), fminf((float)(y - y0), (float)(y1 - 1 - y)));
            float k = (1 - 0.5f * expf(-e / soft)) * (1 + 0.07f * cup_noise(x, y) + 0.03f * cup_noise(x / 3, y / 2));
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)fminf(255, green[c] * k);
            p[3] = 255;
        }
}

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

/* a picture (sw x sh) painted over another (w x h), centred on (cx, cy) and turned by deg: bilinear, alpha blended */
static inline void cup_blend_rot(uint8_t *o, int w, int h, const uint8_t *src, int sw, int sh, float cx, float cy,
                                 float deg)
{
    float a = deg * RACE_PI / 180, c = cosf(a), s = sinf(a), r = 0.5f * hypotf((float)sw, (float)sh) + 1;
    for (int y = (int)(cy - r); y <= (int)(cy + r); y++)
        for (int x = (int)(cx - r); x <= (int)(cx + r); x++) {
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float u = c * dx + s * dy + sw / 2.0f - 0.5f, v = -s * dx + c * dy + sh / 2.0f - 0.5f;
            if (u < -1 || v < -1 || u > sw || v > sh) continue;
            int u0 = (int)floorf(u), v0 = (int)floorf(v);
            float fu = u - u0, fv = v - v0, px[4] = {0, 0, 0, 0};
            for (int k = 0; k < 4; k++) {
                int su = u0 + (k & 1), sv = v0 + (k >> 1);
                if (su < 0 || sv < 0 || su >= sw || sv >= sh) continue;
                float wk = (k & 1 ? fu : 1 - fu) * (k >> 1 ? fv : 1 - fv);
                const uint8_t *q = src + 4 * ((size_t)sv * sw + su);
                float al = q[3] / 255.0f * wk;
                for (int ch = 0; ch < 3; ch++) px[ch] += q[ch] * al;
                px[3] += al;
            }
            if (px[3] <= 0.001f) continue;
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            float al = clampf(px[3], 0, 1), da = p[3] / 255.0f, oa = al + da * (1 - al);
            for (int ch = 0; ch < 3; ch++) {
                float sc = px[ch] / px[3];
                p[ch] = (uint8_t)clampf((sc * al + p[ch] * da * (1 - al)) / (oa > 0 ? oa : 1), 0, 255);
            }
            p[3] = (uint8_t)clampf(oa * 255, 0, 255);
        }
}

/* a die (a player's colour, a face) painted into a picture with its soft shadow, centred, turned by deg */
static inline void cup_paint_die(uint8_t *o, int w, int h, float cx, float cy, int size, int player, int value,
                                 float deg)
{
    if (size < 3) return;
    uint8_t *b = malloc((size_t)size * size * 4);
    if (!b) return;
    art_flat_shadow(size, b);
    cup_blend_rot(o, w, h, b, size, size, cx + size / 14.0f, cy + size / 10.0f, 0);
    art_die(value, player, size, b);
    cup_blend_rot(o, w, h, b, size, size, cx, cy, deg);
    free(b);
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
    cup_felt(o, n, n, ix0, iy0, ix1, iy1);
    float e = 0.004f * n;
    cup_rect(o, n, n, ix0 - e, iy0 - e, ix1 + e, iy0 + 0.5f * e, CUP_INK, 0.45f);
    cup_rect(o, n, n, ix0 - e, iy1 - 0.5f * e, ix1 + e, iy1 + e, CUP_INK, 0.45f);
    cup_rect(o, n, n, ix0 - e, iy0, ix0 + 0.5f * e, iy1, CUP_INK, 0.45f);
    cup_rect(o, n, n, ix1 - 0.5f * e, iy0, ix1 + e, iy1, CUP_INK, 0.45f);
}

/* ---- the dice on the screen -------------------------------------------------------------------------------------- */
/* a die's picture, a player's, a face, at a size (made once for each, remade when the size changes) */
static inline SDL_Texture *cup_face(int player, int value, int size)
{
    static SDL_Texture *t[MAX_PLAYERS][7];
    static int made;
    if (size < 4) return NULL;
    if (made != size) {
        for (int p = 0; p < MAX_PLAYERS; p++)
            for (int v = 1; v <= 6; v++) ui_free(&t[p][v]);
        made = size;
    }
    player = player < 0 ? 0 : player >= MAX_PLAYERS ? MAX_PLAYERS - 1 : player;
    value = value < 1 ? 1 : value > 6 ? 6 : value;
    if (!t[player][value]) {
        art_die(value, player, size, ui_buf(size, size));
        t[player][value] = ui_tex(size, size, ui_buf(size, size));
    }
    return t[player][value];
}

/* one die at (cx, cy), turned by deg, lifted (pixels: its shadow stays on the felt), faded (alpha 0..1), darkened
 * (shade 0..1: 1 as it is; a die that cannot be used) */
static inline void cup_die_ex(float cx, float cy, float size, int player, int value, float deg, float lift,
                              float alpha, float shade)
{
    SDL_Texture *f = cup_face(player, value, (int)(size + 0.5f));
    if (!f) return;
    ui_shadow_flat(cx, cy, size * 1.1f, size * 1.1f, alpha * (lift > 0 ? 0.7f : 1));
    Uint8 k = (Uint8)(255 * clampf(shade, 0, 1) + 0.5f);
    SDL_SetTextureAlphaMod(f, (Uint8)(255 * clampf(alpha, 0, 1) + 0.5f));
    SDL_SetTextureColorMod(f, k, k, k);
    ui_put_rot(f, cx, cy - lift, size, size, deg);
    SDL_SetTextureAlphaMod(f, 255);
    SDL_SetTextureColorMod(f, 255, 255, 255);
}

static inline void cup_die(float cx, float cy, float size, int player, int value, float deg, float lift, float alpha)
{
    cup_die_ex(cx, cy, size, player, value, deg, lift, alpha, 1);
}

/* a die thrown t ms ago (from 0: tumbling, its faces flickering; then at rest on `value`, tilted by deg): i, which
 * die (each spins its own way); shade: cup_die_ex's. 1 while it still tumbles. */
static inline int cup_die_thrown(float cx, float cy, float size, int player, int value, float deg, int i, int t,
                                 float shade)
{
    if (t >= RACE_TUMBLE_MS) {
        cup_die_ex(cx, cy, size, player, value, deg, 0, 1, shade);
        return 0;
    }
    float turn, lift;
    race_tumble(t < 0 ? 0 : t, i, &turn, &lift);
    int v = 1 + (int)((unsigned)(ui_ms() / 70 + i * 3 + value) % 6);
    cup_die(cx, cy, size, player, v, deg + turn, lift * size, 1);
    ui_animate();
    return 1;
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

/* ---- the felt's buttons: round, walnut, a cream picture on it ---------------------------------------------------- */
enum { CUP_THROW, CUP_BANK, CUP_STOP, CUP_BUTTONS };

static inline void cup_button_pic(int kind, int size, uint8_t *o)
{
    static const uint8_t walnut[3] = {104, 70, 46}, dark[3] = {52, 34, 22}, gold[3] = {232, 190, 92};
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    art_style st = race_style(walnut, dark, 0.035f, 0.35f);
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.47f, 0.47f);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
    int d = (int)(0.58f * size);
    if (kind == CUP_THROW) {                    /* a die in mid-air, turning, two marks of its flight */
        cup_paint_die(o, size, size, 0.56f * size, 0.47f * size, d, 0, 5, -18);
        f = art_fig_new();
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.33f, 0.37f, 150, 200);
        art_arc(f, ART_PART, 0.5f, 0.5f, 0.24f, 0.28f, 155, 195);
        st = race_style(CUP_CREAM, CUP_CREAM, 0, 0);
        uint8_t *b = calloc((size_t)size * size, 4);
        if (b) {
            art_render(f, &st, size, size, (float)size, -0.06f * size, 0.02f * size, b);
            art_blend(o, size, size, b, size, size, 0, 0);
            free(b);
        }
        art_fig_free(f);
    } else if (kind == CUP_BANK) {              /* a stack of gold coins */
        f = art_fig_new();
        for (int k = 0; k < 4; k++) {
            float y = 0.68f - 0.11f * k;
            art_rect(f, ART_PART, 0.29f, y - 0.05f, 0.71f, y + 0.02f);
            art_ellipse(f, ART_PART, 0.5f, y + 0.02f, 0.21f, 0.07f);
            art_ellipse(f, ART_PART, 0.5f, y - 0.05f, 0.21f, 0.07f);
        }
        st = race_style(gold, (const uint8_t[3]){120, 84, 30}, 0.025f, 0.3f);
        uint8_t *b = calloc((size_t)size * size, 4);
        if (b) {
            art_render(f, &st, size, size, (float)size, 0, 0, b);
            art_blend(o, size, size, b, size, size, 0, 0);
            free(b);
        }
        art_fig_free(f);
    } else {                                    /* a check mark: done */
        f = art_fig_new();
        static const float tick[] = {0.25f, 0.52f, 0.33f, 0.44f, 0.45f, 0.56f, 0.70f, 0.29f, 0.78f, 0.37f, 0.45f,
                                     0.72f};
        art_poly(f, ART_PART, tick, 6);
        st = race_style(CUP_CREAM, CUP_CREAM, 0, 0.1f);
        uint8_t *b = calloc((size_t)size * size, 4);
        if (b) {
            art_render(f, &st, size, size, (float)size, 0, 0, b);
            art_blend(o, size, size, b, size, size, 0, 0);
            free(b);
        }
        art_fig_free(f);
    }
}

/* a button's picture, at a size (made once, remade when the size changes) */
static inline SDL_Texture *cup_button(int kind, int size)
{
    static SDL_Texture *t[CUP_BUTTONS];
    static int made;
    if (size < 4 || kind < 0 || kind >= CUP_BUTTONS) return NULL;
    if (made != size) {
        for (int k = 0; k < CUP_BUTTONS; k++) ui_free(&t[k]);
        made = size;
    }
    if (!t[kind]) {
        cup_button_pic(kind, size, ui_buf(size, size));
        t[kind] = ui_tex(size, size, ui_buf(size, size));
    }
    return t[kind];
}

/* a button on the screen: lit (it can be pressed: a gold light round it) or dimmed, turned by deg */
static inline void cup_button_at(int kind, float cx, float cy, float d, int lit, float deg)
{
    SDL_Texture *t = cup_button(kind, (int)(d + 0.5f));
    if (!t) return;
    if (lit) ui_mark_gold(cx, cy, 1.6f * d);
    ui_shadow_flat(cx, cy, d * 1.05f, d * 1.05f, lit ? 0.7f : 0.35f);
    SDL_SetTextureAlphaMod(t, lit ? 255 : 120);
    ui_put_rot(t, cx, cy, d, d, deg);
    SDL_SetTextureAlphaMod(t, 255);
}

/* ---- a chip (421's jetons): a bone token, its rim turned, a ring inlaid ------------------------------------------ */
static inline void cup_chip_pic(int size, uint8_t *o)
{
    static const uint8_t bone[3] = {236, 224, 196}, rim[3] = {150, 118, 80}, inlay[3] = {168, 64, 48};
    memset(o, 0, (size_t)size * size * 4);
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.46f, 0.46f);
    art_style st = race_style(bone, rim, 0.05f, 0.4f);
    art_render(f, &st, size, size, (float)size, 0, 0, o);
    art_fig_free(f);
    f = art_fig_new();
    art_arc(f, ART_PART, 0.5f, 0.5f, 0.27f, 0.33f, 0, 360);
    art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.1f, 0.1f);
    st = race_style(inlay, inlay, 0, 0.1f);
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

#endif
