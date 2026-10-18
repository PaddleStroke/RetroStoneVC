/*
 * What Water Sort, Ball Sort and Same Game share on the screen: their twelve colours (the house's player colours
 * first: red lacquer, jade, amber, lapis, ivory, ebony; then plum, orange, turquoise, rose, leaf and walnut), a mark
 * for each (for the colour-blind: a dot, a triangle, a square, a diamond, a ring, a cross, a star, a plus, two bars, a
 * crescent, a chevron, a hexagon), lacquered wood in a colour, and a glossy marble in a colour.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="samegame" builds
 * samegame.c and samegame_*.c), so a shared .c file would be left out of one game's build or another's.
 */
#ifndef HUES_H
#define HUES_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "games/puzzles.h"
#include "shell.h"

#define HUE_N 12

/* the art_player colour each of the first six is (red lacquer, jade, amber, lapis, ivory, ebony); -1: an extra */
static const int HUE_PLAYER[HUE_N + 1] = {-1, 2, 3, 4, 5, 0, 1, -1, -1, -1, -1, -1, -1};
static const uint8_t HUE_EXTRA[HUE_N + 1][3] = {
    {0, 0, 0},    {0, 0, 0},     {0, 0, 0},      {0, 0, 0},      {0, 0, 0},     {0, 0, 0}, {0, 0, 0},
    {124, 60, 138}, {226, 112, 36}, {58, 170, 186}, {226, 126, 164}, {142, 184, 54}, {120, 76, 44}};

/* colour c (1..12) */
static inline void hue_rgb(int c, uint8_t rgb[3])
{
    c = c < 1 ? 1 : c > HUE_N ? HUE_N : c;
    if (HUE_PLAYER[c] >= 0)
        for (int i = 0; i < 3; i++) rgb[i] = (uint8_t)art_player[HUE_PLAYER[c]].fill[i];
    else memcpy(rgb, HUE_EXTRA[c], 3);
}

static inline int hue_dark(int c)               /* 1: a dark colour (its mark light) */
{
    uint8_t q[3];
    hue_rgb(c, q);
    return 0.3f * q[0] + 0.59f * q[1] + 0.11f * q[2] < 150;
}

/* colour c's mark in a unit square, as shapes */
static inline void hue_mark_fig(art_fig *f, int c)
{
    switch (c) {
    case 1: art_ellipse(f, ART_PART, 0.5f, 0.5f, 0.24f, 0.24f); break;
    case 2: { const float t[] = {0.5f, 0.2f, 0.82f, 0.76f, 0.18f, 0.76f}; art_poly(f, ART_PART, t, 3); break; }
    case 3: art_rect(f, ART_PART, 0.25f, 0.25f, 0.75f, 0.75f); break;
    case 4: { const float d[] = {0.5f, 0.14f, 0.84f, 0.5f, 0.5f, 0.86f, 0.16f, 0.5f}; art_poly(f, ART_PART, d, 4); break; }
    case 5: art_arc(f, ART_PART, 0.5f, 0.5f, 0.17f, 0.31f, 0, 360); break;
    case 6:
        for (int k = 0; k < 2; k++) {           /* two bars crossing */
            float s = k ? -1.0f : 1.0f, r = 0.27f, t = 0.06f;
            const float bar[] = {0.5f - r - t, 0.5f - s * (r - t), 0.5f - r + t, 0.5f - s * (r + t),
                                 0.5f + r + t, 0.5f + s * (r - t), 0.5f + r - t, 0.5f + s * (r + t)};
            art_poly(f, ART_PART, bar, 4);
        }
        break;
    case 7: {
        float s[20];
        for (int i = 0; i < 10; i++) {
            float a = (-90 + 36 * i) * PUZ_PI / 180, r = i % 2 ? 0.15f : 0.36f;
            s[2 * i] = 0.5f + r * cosf(a);
            s[2 * i + 1] = 0.53f + r * sinf(a);
        }
        art_poly(f, ART_PART, s, 10);
        break;
    }
    case 8:
        art_rect(f, ART_PART, 0.42f, 0.18f, 0.58f, 0.82f);
        art_rect(f, ART_PART, 0.18f, 0.42f, 0.82f, 0.58f);
        break;
    case 9:
        art_rect(f, ART_PART, 0.2f, 0.27f, 0.8f, 0.43f);
        art_rect(f, ART_PART, 0.2f, 0.57f, 0.8f, 0.73f);
        break;
    case 10: art_arc(f, ART_PART, 0.5f, 0.5f, 0.17f, 0.33f, 50, 310); break;
    case 11: {
        const float v[] = {0.16f, 0.3f, 0.32f, 0.3f, 0.5f, 0.56f, 0.68f, 0.3f, 0.84f, 0.3f, 0.5f, 0.78f};
        art_poly(f, ART_PART, v, 6);
        break;
    }
    default: {
        float h[12];
        for (int i = 0; i < 6; i++) {
            float a = (30 + 60 * i) * PUZ_PI / 180;
            h[2 * i] = 0.5f + 0.31f * cosf(a);
            h[2 * i + 1] = 0.5f + 0.31f * sinf(a);
        }
        art_poly(f, ART_PART, h, 6);
        break;
    }
    }
}

/* the mark of colour c, white (tint it), size x size: painted k times bigger and each k x k block averaged (the
 * painter's 4 x 4 samples alone leave steps along a slanted edge: a diamond's, a star's) */
static inline void hue_mark(int c, int size, uint8_t *rgba)
{
    art_style st = {{255, 255, 255}, {255, 255, 255}, {255, 255, 255}, {255, 255, 255}, 0, 0};
    art_fig *f = art_fig_new();
    hue_mark_fig(f, c);
    memset(rgba, 0, (size_t)size * size * 4);
    int k = size <= 64 ? 4 : size <= 160 ? 3 : 2, big = size * k;
    uint8_t *b = size > 0 ? malloc((size_t)big * big * 4) : NULL;
    if (!b) {
        art_render(f, &st, size, size, (float)size, 0, 0, rgba);
        art_fig_free(f);
        return;
    }
    art_render(f, &st, big, big, (float)big, 0, 0, b);
    art_fig_free(f);
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            unsigned a = 0;
            for (int j = 0; j < k; j++)
                for (int i = 0; i < k; i++) a += b[4 * ((size_t)(y * k + j) * big + x * k + i) + 3];
            uint8_t *p = rgba + 4 * ((size_t)y * size + x);
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)((a + (unsigned)(k * k) / 2) / (unsigned)(k * k));
        }
    free(b);
}

/* the tone a mark is drawn in on colour c (dark ink on the light colours, cream on the dark), and its alpha */
static inline void hue_mark_tone(int c, uint8_t rgb[3], float *a)
{
    static const uint8_t ink[3] = {40, 28, 20}, cream[3] = {250, 244, 230};
    memcpy(rgb, hue_dark(c) ? cream : ink, 3);
    *a = hue_dark(c) ? 0.78f : 0.62f;
}

/* colour c's mark laid into a picture (w x h), d pixels, centred at (cx, cy), in its tone */
static inline void hue_mark_on(uint8_t *o, int w, int h, int c, float cx, float cy, int d)
{
    if (d < 4) return;
    uint8_t *m = malloc((size_t)d * d * 4), tone[3];
    if (!m) return;
    float a;
    hue_mark(c, d, m);
    hue_mark_tone(c, tone, &a);
    for (int i = 0; i < d * d; i++) {
        memcpy(m + 4 * i, tone, 3);
        m[4 * i + 3] = (uint8_t)(m[4 * i + 3] * a);
    }
    art_blend(o, w, h, m, d, d, (int)lroundf(cx - d / 2.0f), (int)lroundf(cy - d / 2.0f));
    free(m);
}

/* a glossy marble of colour c (the house's go stone in a player's colour; the extras in the same light), its mark on
 * it, size x size */
static inline void hue_marble(int c, int size, uint8_t *rgba)
{
    memset(rgba, 0, (size_t)size * size * 4);
    if (HUE_PLAYER[c] >= 0) art_counter(COUNTER_STONE, HUE_PLAYER[c], size, rgba);
    else {
        uint8_t q[3];
        hue_rgb(c, q);
        art_stone(q, size, rgba);
    }
    hue_mark_on(rgba, size, size, c, size * 0.53f, size * 0.55f, (int)(0.4f * size));
}

/* lacquered wood: the house plank under colour c (its grain showing through), lit from the top; the picture's
 * pixels already hold the plank (w x h) */
static inline void hue_lacquer(uint8_t *o, int w, int h, int c)
{
    static const float plank[3] = {222, 184, 128};
    uint8_t q[3];
    hue_rgb(c, q);
    for (int y = 0; y < h; y++) {
        float t = (y + 0.5f) / h, k = 1.12f - 0.24f * t;
        for (int x = 0; x < w; x++) {
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) {
                float grain = p[i] / plank[i];  /* about 1: the grain's light and dark */
                p[i] = (uint8_t)clampf(q[i] * (0.55f + 0.45f * grain) * k + 6, 0, 255);
            }
        }
    }
}

#endif
