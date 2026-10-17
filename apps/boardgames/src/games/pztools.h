/*
 * What five of the one-player puzzles share on the screen (Mines, Klotski, Box Pusher, Lights, Tangram): pictures for
 * their tools on the shell's rail drawn by code (a brass flag, a magnifying glass), two counts in the bar, pieces of
 * the house plank cut and tinted, and filled polygons painted with smooth edges.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="mines" builds
 * mines.c and mines_*.c), so a shared .c file would be left out of one game's build or another's.
 */
#ifndef PZTOOLS_H
#define PZTOOLS_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzzles.h"
#include "pages.h"
#include "shell.h"

static const uint8_t PZT_WALNUT[3] = {102, 68, 45}, PZT_DARK[3] = {50, 33, 22}, PZT_CREAM[3] = {246, 238, 220},
                     PZT_BRASS[3] = {216, 172, 86}, PZT_BRASS_DARK[3] = {120, 84, 30}, PZT_RED[3] = {190, 60, 44};

/* ---- painting -------------------------------------------------------------------------------------------------- */
static inline art_style pzt_style(const uint8_t rgb[3], const uint8_t ink[3], float line, float light)
{
    art_style st = {{0}, {0}, {0}, {0}, light, line};
    for (int k = 0; k < 3; k++) {
        st.fill[k] = rgb[k];
        st.outer[k] = st.inner[k] = ink[k];
        st.mark[k] = ink[k];
    }
    return st;
}

/* a figure (its unit square: size pixels, at ox, oy) painted over a picture w x h, then freed */
static inline void pzt_fig(uint8_t *o, int w, int h, art_fig *f, const uint8_t rgb[3], const uint8_t ink[3],
                           float line, float light, float size, float ox, float oy)
{
    art_style st = pzt_style(rgb, ink, line, light);
    uint8_t *b = calloc((size_t)w * h, 4);
    if (b) {
        art_render(f, &st, w, h, size, ox, oy, b);
        art_blend(o, w, h, b, w, h, 0, 0);
        free(b);
    }
    art_fig_free(f);
}

/* the same over a square picture, the figure filling it */
static inline void pzt_paint(uint8_t *o, int size, art_fig *f, const uint8_t rgb[3], const uint8_t ink[3], float line,
                             float light)
{
    pzt_fig(o, size, size, f, rgb, ink, line, light, (float)size, 0, 0);
}

/* a stroke from (x0, y0) to (x1, y1), wd wide, as a figure's part */
static inline void pzt_band(art_fig *f, float x0, float y0, float x1, float y1, float wd)
{
    float dx = x1 - x0, dy = y1 - y0, l = hypotf(dx, dy) + 1e-6f, nx = -dy / l * wd / 2, ny = dx / l * wd / 2;
    const float q[] = {x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny};
    art_poly(f, ART_PART, q, 4);
}

/* a pixel tinted towards a colour, keeping the wood's grain (the plank's own colour turned into rgb), times k */
static inline void pzt_tint(uint8_t *p, const uint8_t rgb[3], float k)
{
    static const float plank[3] = {222, 184, 128};
    for (int i = 0; i < 3; i++) p[i] = (uint8_t)clampf(p[i] * rgb[i] / plank[i] * k, 0, 255);
}

/* a piece of the house plank, w x h, cut at a place of its own (cut: 0, 1, 2...: another part of the grain) */
static inline void pzt_plank(uint8_t *o, int w, int h, int cut)
{
    int q = (w > h ? w : h) * 2;
    uint8_t *b = malloc((size_t)q * q * 4);
    if (!b) { memset(o, 0, (size_t)w * h * 4); return; }
    art_board_n(1, 1, q, 0, 0, b);
    int ox = (cut * 37) % (q - w + 1), oy = (cut * 53) % (q - h + 1);
    for (int y = 0; y < h; y++) memcpy(o + 4 * (size_t)y * w, b + 4 * ((size_t)(oy + y) * q + ox), (size_t)w * 4);
    free(b);
}

/* how much of the pixel (x, y) .. (x + 1, y + 1) a polygon covers (0..1): 4 x 4 samples */
static inline float pzt_cover(const float *xy, int n, float x, float y)
{
    int in = 0;
    for (int sy = 0; sy < 4; sy++)
        for (int sx = 0; sx < 4; sx++) {
            float px = x + (sx + 0.5f) / 4, py = y + (sy + 0.5f) / 4;
            int c = 0;
            for (int i = 0, j = n - 1; i < n; j = i++) {
                float xi = xy[2 * i], yi = xy[2 * i + 1], xj = xy[2 * j], yj = xy[2 * j + 1];
                if ((yi > py) != (yj > py) && px < (xj - xi) * (py - yi) / (yj - yi) + xi) c = !c;
            }
            in += c;
        }
    return in / 16.0f;
}

/* the distance from (x, y) to a polygon's nearest edge */
static inline float pzt_edge_dist(const float *xy, int n, float x, float y)
{
    float best = 1e9f;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        float ax = xy[2 * j], ay = xy[2 * j + 1], dx = xy[2 * i] - ax, dy = xy[2 * i + 1] - ay;
        float l2 = dx * dx + dy * dy, t = l2 > 0 ? clampf(((x - ax) * dx + (y - ay) * dy) / l2, 0, 1) : 0;
        float d = hypotf(x - ax - t * dx, y - ay - t * dy);
        if (d < best) best = d;
    }
    return best;
}

/* ---- pictures for the shell's buttons (ui_button_paint: in the unit square of a clear picture) -------------- */
/* a brass flag on its pole, standing in the square (x, y, s) of a picture w x h: the pole, a pennant to the right,
 * a small foot */
static inline void pzt_flag(uint8_t *o, int w, int h, float x, float y, float s)
{
    art_fig *f = art_fig_new();
    art_ellipse(f, ART_PART, 0.47f, 0.82f, 0.22f, 0.07f);   /* the foot */
    pzt_fig(o, w, h, f, PZT_DARK, PZT_DARK, 0.0f, 0.3f, s, x, y);
    f = art_fig_new();
    art_rect(f, ART_PART, 0.40f, 0.14f, 0.47f, 0.82f);      /* the pole */
    art_ellipse(f, ART_PART, 0.435f, 0.14f, 0.055f, 0.055f);
    pzt_fig(o, w, h, f, PZT_BRASS_DARK, PZT_DARK, 0.015f, 0.4f, s, x, y);
    f = art_fig_new();
    const float flag[] = {0.47f, 0.18f, 0.80f, 0.33f, 0.47f, 0.50f};
    art_poly(f, ART_PART, flag, 3);
    pzt_fig(o, w, h, f, PZT_BRASS, PZT_DARK, 0.022f, 0.45f, s, x, y);
}

static inline void pzt_pic_flag(uint8_t *o, int size)
{
    pzt_flag(o, size, size, 0.16f * size, 0.14f * size, 0.7f * size);
}

/* a magnifying glass, a plus (zoom in) or a minus (zoom out) in its lens */
static inline void pzt_pic_glass(uint8_t *o, int size, int plus)
{
    art_fig *f = art_fig_new();
    pzt_band(f, 0.56f, 0.56f, 0.75f, 0.75f, 0.1f);
    pzt_paint(o, size, f, PZT_DARK, PZT_DARK, 0, 0.3f);
    f = art_fig_new();
    art_arc(f, ART_PART, 0.44f, 0.44f, 0.17f, 0.23f, 0, 360);
    pzt_paint(o, size, f, PZT_BRASS, PZT_DARK, 0.015f, 0.3f);
    f = art_fig_new();
    pzt_band(f, 0.36f, 0.44f, 0.52f, 0.44f, 0.05f);
    if (plus) pzt_band(f, 0.44f, 0.36f, 0.44f, 0.52f, 0.05f);
    pzt_paint(o, size, f, PZT_CREAM, PZT_CREAM, 0, 0);
}

/* ---- the bar: a count at the left (its picture before it), another at the right end (the fewest moves, the mines
 * left: its picture lit with gold when `glow`); in a narrow bar (landscape) the pictures go first, then the second
 * count ----------------------------------------------------------------------------------------------------------- */
static inline float pzt_number_w(int v, float dh)
{
    int digits = 1;
    for (int k = v < 0 ? -v : v; k >= 10; k /= 10) digits++;
    return (0.74f * (digits - 1) + 0.62f) * dh;
}

static inline void pzt_bar_two(box b, SDL_Texture *left_pic, int left, SDL_Texture *right_pic, int right, int glow)
{
    float u = ui.unit, dh = 0.46f * u, cy = b.y + b.h / 2, sz = 0.6f * u, gap = 0.12f * u;
    if (right < 0) { puz_bar_moves(b, left_pic, left); return; }
    float rw = pzt_number_w(right, dh), lw = pzt_number_w(left, dh);
    int pics = b.w >= 2 * (sz + gap) + lw + rw + 0.5f * u, rpic = b.w >= sz + gap + lw + rw + 0.5f * u;
    float used = puz_bar_moves(b, pics ? left_pic : NULL, left), x = b.x + b.w - rw;
    if (x < used + 0.3f * u) x = used + 0.3f * u;
    if (rpic && right_pic) {
        if (glow) ui_mark_gold(x - gap - sz / 2, cy, 2.2f * sz);
        ui_put(right_pic, x - gap - sz, cy - sz / 2, sz, sz);
    }
    ui_number(x + rw / 2, cy, dh, right, 1);
}

/* the same, the right picture a cream one (the par's flag): dark ink on the light theme's ivory table */
static inline void pzt_bar_two_cream(box b, SDL_Texture *left_pic, int left, SDL_Texture *right_pic, int right,
                                     int glow)
{
    if (right_pic && !TH.dark) SDL_SetTextureColorMod(right_pic, 48, 43, 35);   /* cream x this: the ink */
    pzt_bar_two(b, left_pic, left, right_pic, right, glow);
    if (right_pic) SDL_SetTextureColorMod(right_pic, 255, 255, 255);
}

#endif
