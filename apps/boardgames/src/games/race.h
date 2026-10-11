/*
 * What the race games played with thrown lots share (the Royal Game of Ur's pyramid dice, pachisi's cowries): painting
 * figures and inlays into a board's picture, a steady pseudo-random number for a lot's place, the tumble of a thrown
 * lot (the shell's dice's feel: they spin, bounce and settle a little askew), the hop of a piece square to square.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="ur" builds ur.c and
 * ur_*.c), so a shared .c file would be left out of one game's build or the other's.
 */
#ifndef RACE_H
#define RACE_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "shell.h"

#define RACE_PI 3.14159265f
#define RACE_TUMBLE_MS 650                      /* as long as the shell's dice tumble */

static const uint8_t RACE_INK[3] = {84, 56, 36}, RACE_PALE[3] = {244, 226, 192};

/* a flat style: one colour, an outline of `line` (unit square) in ink */
static inline art_style race_style(const uint8_t rgb[3], const uint8_t ink[3], float line, float light)
{
    art_style st = {{0}, {0}, {0}, {0}, light, line};
    for (int k = 0; k < 3; k++) {
        st.fill[k] = rgb[k];
        st.outer[k] = st.inner[k] = st.mark[k] = ink[k];
    }
    return st;
}

/* a figure (its unit square w pixels) painted over a picture (pw x ph) at (x, y), faded by a (0..1) */
static inline void race_fig(uint8_t *o, int pw, int ph, const art_fig *f, const art_style *st, int x, int y, int w,
                            float a)
{
    if (w < 2) return;
    uint8_t *b = malloc((size_t)w * w * 4);
    if (!b) return;
    art_render(f, st, w, w, (float)w, 0, 0, b);
    if (a < 1)
        for (int i = 0; i < w * w; i++) b[4 * i + 3] = (uint8_t)(b[4 * i + 3] * a);
    art_blend(o, pw, ph, b, w, w, x, y);
    free(b);
}

/* an inlaid tile: the wood under a rounded rectangle tinted (tone: a factor per channel, so the grain stays), a darker
 * seam round it (seam pixels wide) */
static inline void race_inlay(uint8_t *o, int pw, int ph, float x0, float y0, float w, float h, float rad,
                              const float tone[3], float seam)
{
    for (int y = (int)floorf(y0); y < (int)ceilf(y0 + h); y++)
        for (int x = (int)floorf(x0); x < (int)ceilf(x0 + w); x++) {
            if (x < 0 || y < 0 || x >= pw || y >= ph) continue;
            float px = x + 0.5f - x0, py = y + 0.5f - y0;
            float dx = fmaxf(rad - px, px - (w - rad)), dy = fmaxf(rad - py, py - (h - rad)), d;
            if (dx > 0 && dy > 0) d = rad - hypotf(dx, dy);                 /* in a rounded corner */
            else d = fminf(fminf(px, w - px), fminf(py, h - py));           /* inside: to the nearest side */
            float in = clampf(d + 0.5f, 0, 1);
            if (in <= 0) continue;
            float k = d < seam ? 0.72f + 0.28f * clampf(d / seam - 0.4f, 0, 1) : 1;   /* the seam */
            uint8_t *p = o + 4 * ((size_t)y * pw + x);
            for (int c = 0; c < 3; c++) {
                float v = p[c] * tone[c] * k;
                p[c] = (uint8_t)clampf(p[c] + (v - p[c]) * in, 0, 255);
            }
        }
}

/* a steady pseudo-random 0..1 for (a, b): a lot's place and tilt, the same every time it is drawn */
static inline float race_hash(int a, int b)
{
    unsigned h = (unsigned)(a * 73856093) ^ (unsigned)(b * 19349663 + 0x5bd1e995);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (h & 0xffff) / 65535.0f;
}

/* a thrown lot, t ms after the throw (0..RACE_TUMBLE_MS): how far it has turned (degrees, added to its rest tilt) and
 * how high it bounces (a part of its size); i: which lot (each spins its own way) */
static inline void race_tumble(int t, int i, float *turn, float *lift)
{
    float f = clampf(t / (float)RACE_TUMBLE_MS, 0, 1);
    *turn = (1 - f) * (1 - f) * 540 * (i & 1 ? -1 : 1);
    *lift = (1 - f) * 0.6f * fabsf(sinf(f * 9.4f));
}

/* a hop from one square to the next, f 0..1 of the way: eased, and how high (a part of a square) */
static inline float race_ease(float f) { return f * f * (3 - 2 * f); }
static inline float race_lift(float f) { return 0.35f * sinf(RACE_PI * f); }

#endif
