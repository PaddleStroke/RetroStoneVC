/*
 * What the race games played with thrown lots share (the Royal Game of Ur's pyramid dice, pachisi's cowries): inlays
 * in a board's picture, a steady pseudo-random number for a lot's place, the hop of a piece square to square. A thrown
 * lot tumbles as the shell's dice do (ui_die_tumble); figures go into pictures with art_fig_blend.
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
static const uint8_t RACE_INK[3] = {84, 56, 36}, RACE_PALE[3] = {244, 226, 192};

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

/* a hop from one square to the next, f 0..1 of the way: eased, and how high (a part of a square) */
static inline float race_ease(float f) { return f * f * (3 - 2 * f); }
static inline float race_lift(float f) { return 0.35f * sinf(RACE_PI * f); }

#endif
