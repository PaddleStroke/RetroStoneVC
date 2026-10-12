/*
 * What three of the one-player puzzles share on the screen (Mirrors, Tracks, Untangle): round walnut buttons at the
 * right end of the bar (the hint's lamp, a mode), lines and dots drawn at any angle with soft edges (a beam of light
 * and its glow, a cord between two pegs), and strokes painted into a picture with smooth edges (rails, a brass plate).
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

/* ---- the buttons: a turned walnut disc, a cream picture on it ---------------------------------------------------- */
static inline void pzk_btn_disc(int size, uint8_t *o)
{
    memset(o, 0, (size_t)size * size * 4);
    float r = 0.47f * size, c = size / 2.0f;
    pzk_ball(o, size, size, c, c, r, PZK_WALNUT, 1, 0.7f);
    for (int y = 0; y < size; y++)               /* the turned rings, and a dark rim */
        for (int x = 0; x < size; x++) {
            uint8_t *p = o + 4 * (y * size + x);
            if (!p[3]) continue;
            float d = hypotf(x + 0.5f - c, y + 0.5f - c) / r;
            float k = 1 - 0.06f * (0.5f + 0.5f * sinf(d * 34)) - (d > 0.9f ? 0.45f * (d - 0.9f) / 0.1f : 0);
            for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * k, 0, 255);
        }
}

/* the hint: an oil lamp's glass, its brass foot, its light round it */
static inline void pzk_lamp(int size, uint8_t *o)
{
    float s = (float)size;
    for (int i = -2; i <= 2; i++) {
        float a = (90 + 38 * i) * PUZ_PI / 180, cx = 0.5f * s, cy = 0.43f * s;
        pzk_stroke(o, size, size, cx + cosf(a) * 0.24f * s, cy - sinf(a) * 0.24f * s, cx + cosf(a) * 0.32f * s,
                   cy - sinf(a) * 0.32f * s, 0.025f * s, PUZ_CREAM, 1);
    }
    pzk_ball(o, size, size, 0.5f * s, 0.43f * s, 0.155f * s, PZK_GOLD, 1, 0.6f);
    pzk_stroke(o, size, size, 0.42f * s, 0.61f * s, 0.58f * s, 0.61f * s, 0.045f * s, PZK_GOLD, 1);
    pzk_stroke(o, size, size, 0.44f * s, 0.69f * s, 0.56f * s, 0.69f * s, 0.03f * s, PUZ_CREAM, 1);
    pzk_stroke(o, size, size, 0.465f * s, 0.75f * s, 0.535f * s, 0.75f * s, 0.022f * s, PUZ_CREAM, 1);
}

/* a set of button pictures made at one size: paint(kind, size, o) paints kind's picture over the disc */
typedef struct { int size; SDL_Texture *t[8]; } pzk_btns;

static inline SDL_Texture *pzk_btn(pzk_btns *b, int kind, int size, void (*paint)(int kind, int size, uint8_t *o))
{
    if (size < 4 || kind < 0 || kind >= 8) return NULL;
    if (b->size != size) {
        for (int i = 0; i < 8; i++) ui_free(&b->t[i]);
        b->size = size;
    }
    if (!b->t[kind]) {
        uint8_t *o = ui_buf(size, size);
        pzk_btn_disc(size, o);
        paint(kind, size, o);
        b->t[kind] = ui_tex(size, size, o);
    }
    return b->t[kind];
}

enum { PZK_DIM, PZK_READY, PZK_ON, PZK_LIT };   /* not now; can be pressed; a mode that is on; glowing (a hint) */
static inline void pzk_btn_draw(SDL_Texture *t, float cx, float cy, float d, int state)
{
    if (!t) return;
    if (state == PZK_ON || state == PZK_LIT) ui_mark_gold(cx, cy, 1.55f * d);
    ui_shadow_flat(cx, cy, d, d, state == PZK_DIM ? 0.25f : 0.6f);
    SDL_SetTextureAlphaMod(t, state == PZK_DIM ? 100 : 255);
    if (state == PZK_ON) SDL_SetTextureColorMod(t, 214, 204, 184);
    float s = state == PZK_ON ? 0.93f : 1;
    ui_put_c(t, cx, cy + (state == PZK_ON ? 0.03f * d : 0), d * s, d * s);
    SDL_SetTextureColorMod(t, 255, 255, 255);
    SDL_SetTextureAlphaMod(t, 255);
}

static inline int pzk_btn_hit(float cx, float cy, float d, float x, float y) { return hypotf(x - cx, y - cy) <= 0.6f * d; }

/* the bar's buttons, from its right end: button i's centre, for buttons d across (pzk_bar's) */
static inline void pzk_bar_btn(box b, int i, float d, float *cx, float *cy)
{
    *cx = b.x + b.w - d / 2 - 0.05f * ui.unit - i * (d + 0.3f * ui.unit);
    *cy = b.y + b.h / 2;
}

/* the bar of a puzzle: counts from its left (each a small picture, or none, and its number in cream: the moves
 * first), and room for n buttons at its right end; their size. A narrow bar (landscape) first drops the first
 * count's picture, then makes everything smaller. */
typedef struct { SDL_Texture *icon; int value; } pzk_count;

static inline float pzk_bar(box b, const pzk_count *it, int ni, int n)
{
    float u = ui.unit, k = 1, d = 0;
    int drop = 0;
    static const float scales[5] = {1, 1, 0.9f, 0.8f, 0.7f};
    for (int pass = 0; pass < 5; pass++) {
        drop = pass >= 1;
        k = scales[pass];
        float w = 0;
        for (int i = 0; i < ni; i++) {
            int digits = 1;
            for (int v = it[i].value; v >= 10; v /= 10) digits++;
            if (it[i].icon && !(drop && i == 0)) w += (0.6f + 0.12f) * u * k;
            w += (0.74f * (digits - 1) + 0.62f) * 0.46f * u * k + (i ? 0.35f * u * k : 0);
        }
        d = (b.w - w - 0.25f * u - (n - 1) * 0.3f * u) / (n ? n : 1);
        if (d >= 0.8f * u) break;
    }
    d = floorf(clampf(d, 0.5f * u, 0.84f * u));
    float x = b.x, cy = b.y + b.h / 2, dh = 0.46f * u * k, sz = 0.6f * u * k;
    for (int i = 0; i < ni; i++) {
        if (i) x += 0.35f * u * k;
        if (it[i].icon && !(drop && i == 0)) {
            ui_put(it[i].icon, x, cy - sz / 2, sz, sz);
            x += sz + 0.12f * u * k;
        }
        int digits = 1;
        for (int v = it[i].value; v >= 10; v /= 10) digits++;
        float w = (0.74f * (digits - 1) + 0.62f) * dh;
        ui_number(x + w / 2, cy, dh, it[i].value < 0 ? 0 : it[i].value, 1);
        x += w;
    }
    return d;
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
