/*
 * What the arithmetic games share on the screen (Math Crossword, Mental Math): the signs of the four operations and
 * the equals sign painted in ink into a picture, round-ended strokes and dots; and a hollow sunk in the wood (a gap
 * for a number).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only, so a shared .c file would
 * be left out of one game's build or another's.
 */
#ifndef MATHSIGNS_H
#define MATHSIGNS_H

#include <math.h>
#include <stdint.h>

#include "games/pzkit.h"

enum { MSG_ADD = 1, MSG_SUB, MSG_MUL, MSG_DIV, MSG_EQ };   /* the signs */

/* a sign d across (its strokes' reach), centred at (cx, cy), in ink */
static inline void msg_sign(uint8_t *o, int w, int h, float cx, float cy, float d, int sign, const uint8_t ink[3])
{
    float r = 0.065f * d, a = 0.3f * d;
    switch (sign) {
    case MSG_ADD:
        pzk_stroke(o, w, h, cx - a, cy, cx + a, cy, r, ink, 1);
        pzk_stroke(o, w, h, cx, cy - a, cx, cy + a, r, ink, 1);
        break;
    case MSG_SUB: pzk_stroke(o, w, h, cx - a, cy, cx + a, cy, r, ink, 1); break;
    case MSG_MUL:
        a *= 0.75f;
        pzk_stroke(o, w, h, cx - a, cy - a, cx + a, cy + a, r, ink, 1);
        pzk_stroke(o, w, h, cx - a, cy + a, cx + a, cy - a, r, ink, 1);
        break;
    case MSG_DIV:
        pzk_stroke(o, w, h, cx - a, cy, cx + a, cy, r, ink, 1);
        pzk_ball(o, w, h, cx, cy - 0.2f * d, 1.25f * r, ink, 1, 0);
        pzk_ball(o, w, h, cx, cy + 0.2f * d, 1.25f * r, ink, 1, 0);
        break;
    case MSG_EQ:
        pzk_stroke(o, w, h, cx - a, cy - 0.11f * d, cx + a, cy - 0.11f * d, r, ink, 1);
        pzk_stroke(o, w, h, cx - a, cy + 0.11f * d, cx + a, cy + 0.11f * d, r, ink, 1);
        break;
    }
}

/* a hollow sunk in the wood of a picture (w x h): a rounded rectangle (x0, y0, sw x sh) darker, its walls shaded at
 * the top and the left */
static inline void msg_hollow(uint8_t *o, int w, int h, float x0, float y0, float sw, float sh)
{
    float s = fminf(sw, sh), rad = 0.16f * s;
    for (int y = (int)y0; y < (int)(y0 + sh) && y < h; y++)
        for (int x = (int)x0; x < (int)(x0 + sw) && x < w; x++) {
            if (x < 0 || y < 0) continue;
            float px = x + 0.5f - x0, py = y + 0.5f - y0;
            float dx = fmaxf(rad - px, px - (sw - rad)), dy = fmaxf(rad - py, py - (sh - rad)), d;
            if (dx > 0 && dy > 0) d = rad - hypotf(dx, dy);
            else d = fminf(fminf(px, sw - px), fminf(py, sh - py));
            float a = clampf(d + 0.5f, 0, 1);
            if (a <= 0) continue;
            float wall = clampf(1 - fminf(px, py) / (0.2f * s), 0, 1), k = 1 - a * (0.3f + 0.25f * wall);
            uint8_t *p = o + 4 * ((size_t)y * w + x);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)(p[i] * k);
        }
}

#endif
