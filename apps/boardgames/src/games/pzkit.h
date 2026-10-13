/*
 * What the route puzzles share (Pipes, Paths, Mirrors, Tracks, Untangle): the tiles of a board cut in the house plank
 * (each square bevelled, a groove between them), an inlaid cell, strokes and discs painted into a picture with smooth
 * edges (rails, a brass plate, a beam's glow), lines and dots drawn on the screen at any angle with soft edges (a beam
 * of light, a cord between two pins), the chapters' tab pictures, and the stars' rule of the family.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="tracks" builds
 * tracks.c and tracks_*.c), so a shared .c file would be left out of one game's build or another's.
 */
#ifndef PZKIT_H
#define PZKIT_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "shell.h"

static const uint8_t PZK_WALNUT[3] = {108, 72, 47}, PZK_DARK[3] = {50, 33, 22}, PZK_GOLD[3] = {236, 194, 96},
                     PZK_RED[3] = {200, 62, 46};

/* ---- the stars: one rule for the family -------------------------------------------------------------------------
 * A solved puzzle's stars: `base` from its own rule (the moves against par: ui_par_stars; 3 for the puzzles rated by
 * their help alone), then no more than the help allows (the shell's ui_logic_stars: no hint and no mistake 3, up to
 * two of them 2, more 1). */
static inline int pzk_stars(int base, int hints, int mistakes)
{
    int cap = ui_logic_stars(hints, mistakes);
    return base < cap ? base : cap;
}

/* ---- painting into a picture (RGBA, straight alpha) -------------------------------------------------------------- */
/* a colour laid over one pixel, covering it by a (0..1) */
static inline void pzk_over(uint8_t *p, const uint8_t rgb[3], float a)
{
    if (a <= 0) return;
    if (a > 1) a = 1;
    float da = p[3] / 255.0f, oa = a + da * (1 - a);
    for (int i = 0; i < 3; i++) p[i] = (uint8_t)((rgb[i] * a + p[i] * da * (1 - a)) / oa + 0.5f);
    p[3] = (uint8_t)(oa * 255 + 0.5f);
}

/* the distance from (x, y) to the segment a..b */
static inline float pzk_seg_dist(float x, float y, float ax, float ay, float bx, float by)
{
    float dx = bx - ax, dy = by - ay, l2 = dx * dx + dy * dy, t = 0;
    if (l2 > 0) t = clampf(((x - ax) * dx + (y - ay) * dy) / l2, 0, 1);
    return hypotf(x - ax - t * dx, y - ay - t * dy);
}

/* a stroke from a to b, its ends round, r pixels from its middle line, in rgb at opacity a */
static inline void pzk_stroke(uint8_t *o, int w, int h, float ax, float ay, float bx, float by, float r,
                              const uint8_t rgb[3], float a)
{
    int x0 = (int)floorf(fminf(ax, bx) - r - 1), x1 = (int)ceilf(fmaxf(ax, bx) + r + 1);
    int y0 = (int)floorf(fminf(ay, by) - r - 1), y1 = (int)ceilf(fmaxf(ay, by) + r + 1);
    for (int y = y0 < 0 ? 0 : y0; y < h && y <= y1; y++)
        for (int x = x0 < 0 ? 0 : x0; x < w && x <= x1; x++) {
            float d = pzk_seg_dist(x + 0.5f, y + 0.5f, ax, ay, bx, by);
            pzk_over(o + 4 * (y * w + x), rgb, a * clampf(r - d + 0.5f, 0, 1));
        }
}

/* the same stroke into a coverage mask (w x h floats, the most of what covers each pixel): strokes that overlap
 * do not darken where they meet; then the mask painted in one colour */
static inline void pzk_stroke_mask(float *m, int w, int h, float ax, float ay, float bx, float by, float r)
{
    int x0 = (int)floorf(fminf(ax, bx) - r - 1), x1 = (int)ceilf(fmaxf(ax, bx) + r + 1);
    int y0 = (int)floorf(fminf(ay, by) - r - 1), y1 = (int)ceilf(fmaxf(ay, by) + r + 1);
    for (int y = y0 < 0 ? 0 : y0; y < h && y <= y1; y++)
        for (int x = x0 < 0 ? 0 : x0; x < w && x <= x1; x++) {
            float c = clampf(r - pzk_seg_dist(x + 0.5f, y + 0.5f, ax, ay, bx, by) + 0.5f, 0, 1);
            if (c > m[y * w + x]) m[y * w + x] = c;
        }
}

static inline void pzk_mask_paint(uint8_t *o, int w, int h, const float *m, const uint8_t rgb[3], float a)
{
    for (int i = 0; i < w * h; i++) pzk_over(o + 4 * i, rgb, a * m[i]);
}

/* a ring band (radii r0..r1) round (cx, cy), from angle a0 to a1 (degrees, counterclockwise from the right, y up) */
static inline void pzk_arc(uint8_t *o, int w, int h, float cx, float cy, float r0, float r1, float a0, float a1,
                           const uint8_t rgb[3], float a)
{
    int x0 = (int)floorf(cx - r1 - 1), x1 = (int)ceilf(cx + r1 + 1), y0 = (int)floorf(cy - r1 - 1),
        y1 = (int)ceilf(cy + r1 + 1);
    for (int y = y0 < 0 ? 0 : y0; y < h && y <= y1; y++)
        for (int x = x0 < 0 ? 0 : x0; x < w && x <= x1; x++) {
            float dx = x + 0.5f - cx, dy = cy - (y + 0.5f), d = hypotf(dx, dy);
            float ang = atan2f(dy, dx) * 180 / PUZ_PI;
            while (ang < a0) ang += 360;
            while (ang > a0 + 360) ang -= 360;
            if (ang > a1) continue;
            float cov = clampf(r1 - d + 0.5f, 0, 1) * clampf(d - r0 + 0.5f, 0, 1);
            pzk_over(o + 4 * (y * w + x), rgb, a * cov);
        }
}

/* a disc, its edge smoothed; light: how much it is lit from the top left (0: flat) */
static inline void pzk_ball(uint8_t *o, int w, int h, float cx, float cy, float r, const uint8_t rgb[3], float a,
                            float light)
{
    for (int y = (int)fmaxf(0, floorf(cy - r - 1)); y < h && y <= cy + r + 1; y++)
        for (int x = (int)fmaxf(0, floorf(cx - r - 1)); x < w && x <= cx + r + 1; x++) {
            float dx = (x + 0.5f - cx) / r, dy = (y + 0.5f - cy) / r, d = hypotf(dx, dy) * r;
            float cov = clampf(r - d + 0.5f, 0, 1);
            if (cov <= 0) continue;
            float k = 1 + light * (0.55f - 0.9f * hypotf(dx + 0.38f, dy + 0.42f));
            uint8_t c[3];
            for (int i = 0; i < 3; i++) c[i] = (uint8_t)clampf(rgb[i] * k, 0, 255);
            pzk_over(o + 4 * (y * w + x), c, a * cov);
        }
}

/* ---- the boards -------------------------------------------------------------------------------------------------- */
/* the plank in its walnut frame (S x S, frame f), w x h squares of q pixels from (ox, oy) cut in it: each one bevelled
 * (lit at the top left), a dark groove between them */
static inline void pzk_tiles(uint8_t *o, int S, int f, int w, int h, int q, int ox, int oy)
{
    art_board_n(1, 1, S - 2 * f, f, 0, o);
    float b = fmaxf(1.5f, 0.07f * q), groove = fmaxf(1, 0.035f * q);
    for (int y = oy < 0 ? 0 : oy; y < oy + h * q && y < S; y++)
        for (int x = ox < 0 ? 0 : ox; x < ox + w * q && x < S; x++) {
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

/* an inlaid cell: the wood under a rounded rectangle tinted (tone: a factor per channel, so the grain stays), a darker
 * seam round it (seam pixels wide) */
static inline void pzk_inlay(uint8_t *o, int pw, int ph, float x0, float y0, float w, float h, float rad,
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

/* a chapter's tab picture: a small plank in its walnut frame, ruled in n x n cells (its boards' size) */
static inline void pzk_grid_icon(uint8_t *rgba, int size, int n)
{
    int S = (int)(0.9f * size), f = (int)fmaxf(1, 0.07f * S), o = (size - S) / 2;
    uint8_t *b = S > 4 && n > 0 ? malloc((size_t)S * S * 4) : NULL;
    if (!b) return;
    art_board_n(1, 1, S - 2 * f, f, 0, b);
    float q = (float)(S - 2 * f) / n, t = fmaxf(1, 0.1f * q);
    for (int i = 1; i < n; i++) {
        float p = f + i * q;
        puz_ink_rect(b, S, S, p - t / 2, (float)f, p + t / 2, (float)(S - f), PUZ_INK, 0.6f);
        puz_ink_rect(b, S, S, (float)f, p - t / 2, (float)(S - f), p + t / 2, PUZ_INK, 0.6f);
    }
    art_blend(rgba, size, size, b, S, S, o, o);
    free(b);
}

/* ---- lines and dots on the screen, soft edged, at any angle ------------------------------------------------------
 * A line is a strip of white (its profile across: wd wide, its edges fading over soft pixels; soft 0: just smoothed)
 * stretched along its length and turned; a dot has the same profile round its centre, so a line ending in a dot joins
 * it without a seam. Tinted when drawn; added to what is under it (light: a beam's glow) or laid over it. The strips
 * and dots are kept (a few sizes at a time). */
typedef struct { Uint8 r, g, b, a; } pzk_rgba;

static inline float pzk_profile(float d, float h, float soft)    /* d: the distance from the middle */
{
    if (soft <= 0) return clampf(h - d + 0.5f, 0, 1);
    if (d <= h) return 1;
    float t = clampf((d - h) / soft, 0, 1);
    return (1 - t) * (1 - t);
}

static inline SDL_Texture *pzk_strip(float wd, float soft, int dot)
{
    static struct { int key; SDL_Texture *t; } cache[24];
    static int next;
    int key = ((int)(wd * 4) * 4096 + (int)(soft * 4)) * 2 + dot;
    for (int i = 0; i < 24; i++)
        if (cache[i].t && cache[i].key == key) return cache[i].t;
    float h = wd / 2, R = h + (soft > 0 ? soft : 1);
    int H = (int)ceilf(2 * R) + 2, W = dot ? H : 4;
    uint8_t *o = ui_buf(W, H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float dy = y + 0.5f - H / 2.0f, dx = dot ? x + 0.5f - H / 2.0f : 0;
            uint8_t *p = o + 4 * (y * W + x);
            p[0] = p[1] = p[2] = 255;
            p[3] = (uint8_t)(255 * pzk_profile(hypotf(dx, dy), h, soft) + 0.5f);
        }
    int i = next++ % 24;
    ui_free(&cache[i].t);
    cache[i].key = key;
    cache[i].t = ui_tex(W, H, o);
    return cache[i].t;
}

static inline void pzk_tint(SDL_Texture *t, pzk_rgba c, int add)
{
    SDL_SetTextureBlendMode(t, add ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(t, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(t, c.a);
}

/* a line from a to b (its ends square, at a and b), wd wide, its edges fading over soft pixels; add: light added */
static inline void pzk_line(float ax, float ay, float bx, float by, float wd, float soft, pzk_rgba c, int add)
{
    float dx = bx - ax, dy = by - ay, l = hypotf(dx, dy);
    if (l < 0.5f) return;
    SDL_Texture *t = pzk_strip(wd, soft, 0);
    if (!t) return;
    int H;
    SDL_QueryTexture(t, NULL, NULL, NULL, &H);
    pzk_tint(t, c, add);
    ui_put_rot(t, (ax + bx) / 2, (ay + by) / 2, l, (float)H, atan2f(dy, dx) * 180 / PUZ_PI);
}

/* a dot of the same profile: wd across */
static inline void pzk_dot(float cx, float cy, float wd, float soft, pzk_rgba c, int add)
{
    SDL_Texture *t = pzk_strip(wd, soft, 1);
    if (!t) return;
    int H;
    SDL_QueryTexture(t, NULL, NULL, NULL, &H);
    pzk_tint(t, c, add);
    ui_put_c(t, cx, cy, (float)H, (float)H);
}

#endif
