/*
 * What the one-player puzzles share on the screen (Sudoku, Nonograms, the 15 Puzzle, the Tower of Hanoi): wooden
 * tiles cut from the house plank (rounded, bevelled, lit from the top left), digits burned into them, and the win's
 * light running over the board. The digits on the screen and the move counter are the shell's (ui_number_ex,
 * ui_bar_number).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="sudoku" builds
 * sudoku.c and sudoku_*.c), so a shared .c file would be left out of one game's build or another's.
 */
#ifndef PUZZLES_H
#define PUZZLES_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"
#include "text.h"

#define PUZ_PI 3.14159265f

static const uint8_t PUZ_INK[3] = {52, 34, 22}, PUZ_CREAM[3] = {246, 238, 220}, PUZ_WHITE[3] = {255, 255, 255};

static inline float puz_ease(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

/* a bump 0 -> 1 -> 0 between start and start + len (ms): a light swelling and fading */
static inline float puz_pulse(float t, float start, float len)
{
    float f = (t - start) / len;
    return f <= 0 || f >= 1 ? 0 : sinf(PUZ_PI * f);
}

/* a tile of the picture o (w x h, its wood already there) cut out: its corners rounded (rad), its edges bevelled
 * over b pixels (lit at the top and the left, shaded at the bottom and the right, by depth) */
static inline void puz_bevel(uint8_t *o, int w, int h, float rad, float b, float depth)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float px = x + 0.5f, py = y + 0.5f, nx, ny, d;
            float cx = fmaxf(rad - px, px - (w - rad)), cy = fmaxf(rad - py, py - (h - rad));
            if (cx > 0 && cy > 0) {             /* a rounded corner */
                float l = hypotf(cx, cy);
                d = rad - l;
                nx = (px < w / 2.0f ? -cx : cx) / l;
                ny = (py < h / 2.0f ? -cy : cy) / l;
            } else {                            /* the nearest side */
                float dl = px, dr = w - px, dt = py, db = h - py;
                d = fminf(fminf(dl, dr), fminf(dt, db));
                nx = d == dl ? -1 : d == dr ? 1 : 0;
                ny = nx ? 0 : d == dt ? -1 : 1;
            }
            uint8_t *p = o + 4 * (y * w + x);
            float a = clampf(d + 0.5f, 0, 1), k = 1;
            if (d < b) k = 1 + depth * (1 - d / b) * -(nx + ny) * 0.7071f;
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * k, 0, 255);
            p[3] = (uint8_t)(p[3] * a);
        }
}

/* a rectangle of ink laid over a picture (w x h), its edges smoothed (fractions of pixels), alpha a: lines drawn on
 * the wood */
static inline void puz_ink_rect(uint8_t *o, int w, int h, float x0, float y0, float x1, float y1, const uint8_t rgb[3],
                                float a)
{
    for (int y = (int)floorf(y0); y < (int)ceilf(y1); y++)
        for (int x = (int)floorf(x0); x < (int)ceilf(x1); x++) {
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            float cx = fminf(x + 1, x1) - fmaxf(x, x0), cy = fminf(y + 1, y1) - fmaxf(y, y0), k = a * cx * cy;
            uint8_t *p = o + 4 * (y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] + (rgb[i] - p[i]) * k);
        }
}

/* a number drawn into a picture, centred at (cx, cy), its digits h pixels high, in rgb (alpha a); bold or not */
static inline void puz_digits(uint8_t *o, int w, int h, int value, float cx, float cy, float dh, const uint8_t rgb[3],
                              float a, int bold)
{
    char s[12];
    int n = snprintf(s, sizeof s, "%d", value < 0 ? 0 : value);
    int ih = (int)(dh + 0.5f), iw = (int)(0.62f * dh + 1);
    if (ih < 3) return;
    float step = 0.66f * dh, x = cx - (step * (n - 1) + iw) / 2;
    uint8_t *g = malloc((size_t)iw * ih * 4);
    if (!g) return;
    for (int i = 0; i < n; i++, x += step) {
        memset(g, 0, (size_t)iw * ih * 4);
        text_glyph(s[i], ih * 1.2f, bold, rgb, g, iw, ih);
        if (a < 1)
            for (int k = 0; k < iw * ih; k++) g[4 * k + 3] = (uint8_t)(g[4 * k + 3] * a);
        art_blend(o, w, h, g, iw, ih, (int)lroundf(x), (int)lroundf(cy - ih / 2.0f));
    }
    free(g);
}

/* digits burned into wood: dark ink, a faint light line under each stroke (pressed in) */
static inline void puz_burn(uint8_t *o, int w, int h, int value, float cx, float cy, float dh)
{
    float off = fmaxf(1, dh / 22);
    puz_digits(o, w, h, value, cx + 0.4f * off, cy + off, dh, PUZ_CREAM, 0.55f, 1);
    puz_digits(o, w, h, value, cx, cy, dh, PUZ_INK, 1, 1);
}

#endif
