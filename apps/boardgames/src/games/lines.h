/*
 * The boards of lines and points (nine men's morris, fox and geese, alquerque): one plank of the house wood in its
 * walnut frame, a square grid of cells on it, the lines drawn dark between the points (straight and diagonal, crisp,
 * anti-aliased), the points as dark dots at the cells' centres; and the point <-> pixel mapping on the screen.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="morris" builds
 * morris.c and morris_*.c), so a shared .c file would be left out of one game's build or the other's.
 * Grid places: gx from the left, gy from the bottom (player 0's side), 0 .. cells - 1; a point sits in the middle of
 * its cell.
 */
#ifndef LINES_H
#define LINES_H

#include <math.h>
#include <stdint.h>

#include "shell.h"

#define LINES_FRAME 0.32f                       /* the plank's frame, in cells */

static const uint8_t LINES_INK[3] = {62, 40, 26};

/* the plank on the screen: its cells, their size and the frame's (pixels), the cells' top left corner */
typedef struct { int cells, sq, fr; float x, y; } lines_geo;

/* ---- painting, over a picture n pixels square -------------------------------------------------------------------- */
static inline float lines_cover(float a, float b, int px)   /* how much of pixel px the span a..b covers */
{
    float c = fminf(px + 1.0f, b) - fmaxf((float)px, a);
    return c < 0 ? 0 : c > 1 ? 1 : c;
}

static inline void lines_mix(uint8_t *p, const uint8_t c[3], float a)
{
    for (int k = 0; k < 3; k++) p[k] = (uint8_t)(p[k] + (c[k] - p[k]) * a + 0.5f);
}

/* a band from (ax, ay) to (bx, by), w wide, its ends square (they reach w / 2 past the points), in colour c at
 * opacity a: along a row or a column, each pixel's exact coverage; slanted, its distance to the band's edges */
static inline void lines_band(uint8_t *o, int n, float ax, float ay, float bx, float by, float w, const uint8_t c[3],
                              float a)
{
    if (ax == bx || ay == by) {
        float x0 = fminf(ax, bx) - w / 2, y0 = fminf(ay, by) - w / 2, x1 = fmaxf(ax, bx) + w / 2,
              y1 = fmaxf(ay, by) + w / 2;
        for (int y = (int)floorf(y0); y < (int)ceilf(y1); y++)
            for (int x = (int)floorf(x0); x < (int)ceilf(x1); x++)
                if (x >= 0 && y >= 0 && x < n && y < n)
                    lines_mix(o + 4 * (y * n + x), c, a * lines_cover(x0, x1, x) * lines_cover(y0, y1, y));
        return;
    }
    float len = hypotf(bx - ax, by - ay), ux = (bx - ax) / len, uy = (by - ay) / len, h = w / 2;
    for (int y = (int)floorf(fminf(ay, by) - w - 1); y <= (int)ceilf(fmaxf(ay, by) + w + 1); y++)
        for (int x = (int)floorf(fminf(ax, bx) - w - 1); x <= (int)ceilf(fmaxf(ax, bx) + w + 1); x++) {
            if (x < 0 || y < 0 || x >= n || y >= n) continue;
            float px = x + 0.5f - ax, py = y + 0.5f - ay, u = px * ux + py * uy, v = py * ux - px * uy;
            float across = fminf(fmaxf(h - fabsf(v) + 0.5f, 0), 1);
            float along = fminf(fmaxf(fminf(u + h, len + h - u) + 0.5f, 0), 1);
            if (across * along > 0) lines_mix(o + 4 * (y * n + x), c, a * across * along);
        }
}

/* a dot of radius r, its edge smoothed */
static inline void lines_dot(uint8_t *o, int n, float cx, float cy, float r, const uint8_t c[3])
{
    for (int y = (int)floorf(cy - r - 1); y <= (int)ceilf(cy + r + 1); y++)
        for (int x = (int)floorf(cx - r - 1); x <= (int)ceilf(cx + r + 1); x++) {
            float d = hypotf(x + 0.5f - cx, y + 0.5f - cy), a = r - d + 0.5f;
            if (x >= 0 && y >= 0 && x < n && y < n && a > 0) lines_mix(o + 4 * (y * n + x), c, a > 1 ? 1 : a);
        }
}

/* a grid place's centre in a picture of cells of s pixels whose first cell starts at (o, o) */
static inline float lines_px(int gx, float s, float o) { return o + (gx + 0.5f) * s; }
static inline float lines_py(int cells, int gy, float s, float o) { return o + (cells - 1 - gy + 0.5f) * s; }

/* the plank with its lines and points: cells of s pixels in a frame of f, (cells s + 2 f) squared. seg: nseg lines,
 * each from one grid place to another (gx0, gy0, gx1, gy1); pt: npt points (gx, gy); w: the lines' width, r: the
 * dots' radius (pixels) */
static inline void lines_paint(int cells, int s, int f, const int8_t (*seg)[4], int nseg, const int8_t (*pt)[2],
                               int npt, float w, float r, uint8_t *o)
{
    int n = cells * s + 2 * f;
    float S = (float)s, F = (float)f;
    art_board_n(cells, cells, s, f, 0, o);
    for (int i = 0; i < nseg; i++)
        lines_band(o, n, lines_px(seg[i][0], S, F), lines_py(cells, seg[i][1], S, F), lines_px(seg[i][2], S, F),
                   lines_py(cells, seg[i][3], S, F), w, LINES_INK, 1);
    for (int i = 0; i < npt; i++)
        lines_dot(o, n, f + (pt[i][0] + 0.5f) * s, f + (cells - 1 - pt[i][1] + 0.5f) * s, r, LINES_INK);
}

/* ---- the screen -------------------------------------------------------------------------------------------------- */
/* fits the plank (g->cells set) in the board's square; 1: the cells changed size (make the pictures again) */
static inline int lines_fit(lines_geo *g, box b)
{
    int sq = (int)floorf(b.w / (g->cells + 2 * LINES_FRAME)), changed = sq != g->sq;
    g->sq = sq;
    g->fr = (int)(sq * LINES_FRAME);
    int n = g->cells * sq + 2 * g->fr;
    g->x = floorf(b.x + (b.w - n) / 2) + g->fr;
    g->y = floorf(b.y + (b.h - n) / 2) + g->fr;
    return changed;
}

/* the plank's side, frame included (pixels) */
static inline float lines_side(const lines_geo *g) { return (float)g->cells * g->sq + 2 * g->fr; }

/* a grid place's centre on the screen (turned round when the board is flipped) */
static inline void lines_xy(const lines_geo *g, int gx, int gy, float *x, float *y)
{
    int c = gx, r = g->cells - 1 - gy;
    if (ui.flipped) { c = g->cells - 1 - c; r = g->cells - 1 - r; }
    *x = g->x + (c + 0.5f) * g->sq;
    *y = g->y + (r + 0.5f) * g->sq;
}

/* the grid place whose cell holds (x, y): 1, or 0 off the grid */
static inline int lines_at(const lines_geo *g, float x, float y, int *gx, int *gy)
{
    int c = (int)floorf((x - g->x) / g->sq), r = (int)floorf((y - g->y) / g->sq), m = g->cells - 1;
    if (c < 0 || c > m || r < 0 || r > m) return 0;
    if (ui.flipped) { c = m - c; r = m - r; }
    *gx = c;
    *gy = m - r;
    return 1;
}

/* the last move's mark on a point's cell */
static inline void lines_mark_last(const lines_geo *g, int gx, int gy)
{
    float x, y;
    lines_xy(g, gx, gy, &x, &y);
    ui_mark_last(x - g->sq / 2.0f, y - g->sq / 2.0f, (float)g->sq, (float)g->sq);
}

static inline void lines_mark_select(const lines_geo *g, int gx, int gy)
{
    float x, y;
    lines_xy(g, gx, gy, &x, &y);
    ui_mark_select(x - g->sq / 2.0f, y - g->sq / 2.0f, (float)g->sq, (float)g->sq);
}

#endif
