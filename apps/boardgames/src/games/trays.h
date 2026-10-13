/*
 * A tray by each player's side, for the two-player boards that fill the screen (game.fill): in the room the board
 * leaves (above and below it when the room is taller than the board, else beside it: the top player's half above the
 * bottom player's, by the side column of bars), what a player holds (pieces still to place) or has taken, near them.
 * Written once, here, as static inline functions (like grid.h); the shell could carry it.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Slots: n of them, the first nearest the player and on their left (a player face to face at the top: turned round).
 */
#ifndef TRAYS_H
#define TRAYS_H

#include <math.h>
#include <string.h>

#include "shell.h"

typedef struct {
    box b[2];                                   /* player p's tray (w 0: no trays) */
    int n, cols, rows;                          /* its slots: n, in rows of cols, the first row nearest the player */
    float pitch;                                /* a slot's side, pixels */
} trays;

/* the biggest slots (side up to `most`) for n things in w x h: cols x rows of side *pitch; of those as big, the
 * fewest rows (wide: a tray along the board) or the fewest columns (one beside it) */
static inline void trays_grid(float w, float h, int n, float most, int wide, int *cols, int *rows, float *pitch)
{
    *cols = n, *rows = 1, *pitch = 0;
    for (int k = 1; k <= n; k++) {
        int c = wide ? n + 1 - k : k, r = (n + c - 1) / c;
        float p = floorf(fminf(fminf(w / c, h / r), most));
        if (p > *pitch) *cols = c, *rows = r, *pitch = p;
    }
}

/* the board (bw x bh pixels) in the room r, with a tray by each player's side for n things in slots of side up to
 * `most` (no trays when they would be smaller than `least`): the board's top left in *x, *y (whole pixels) */
static inline void trays_place(trays *t, box r, float bw, float bh, int n, float most, float least, float *x,
                               float *y)
{
    memset(t, 0, sizeof *t);
    t->n = n;
    *x = floorf(r.x + (r.w - bw) / 2);
    *y = floorf(r.y + (r.h - bh) / 2);
    int bottom = ui_bottom(), c, rows;
    float p, gap = floorf(0.25f * least);
    if (r.h - bh >= r.w - bw) {                 /* above and below the board, as wide as it */
        float d = (r.h - bh) / 2 - gap;
        if (d < least) return;
        trays_grid(bw, d, n, most, 1, &c, &rows, &p);
        if (p < least) return;
        float th = rows * p, y0 = floorf(r.y + (r.h - bh) / 2 - gap - th);
        t->b[!bottom] = (box){*x, y0, bw, th};
        t->b[bottom] = (box){*x, *y + bh + gap, bw, th};
    } else {                                    /* beside it, on the right: the top player's half, the bottom one's */
        float d = r.w - bw - gap, hh = floorf((bh - gap) / 2);
        if (d < least) return;
        trays_grid(d, hh, n, most, 0, &c, &rows, &p);
        if (p < least) return;
        float tw = c * p;
        *x = floorf(r.x + (r.w - bw - gap - tw) / 2);
        t->b[!bottom] = (box){*x + bw + gap, *y, tw, hh};
        t->b[bottom] = (box){*x + bw + gap, *y + bh - hh, tw, hh};
    }
    t->cols = c, t->rows = rows, t->pitch = p;
}

static inline int trays_on(const trays *t) { return t->b[0].w > 0; }

/* slot i's centre in player p's tray: the first row along the side nearest them (the bottom player's: the tray's
 * bottom; the top player's: its top), from their left (turned round when they face us: face to face) */
static inline void trays_slot(const trays *t, int p, int i, float *cx, float *cy)
{
    box b = t->b[p];
    int top = p != ui_bottom(), col = i % t->cols, row = i / t->cols;
    float gw = t->cols * t->pitch, gh = t->rows * t->pitch;
    float x0 = b.x + floorf((b.w - gw) / 2), y0 = top ? b.y : b.y + b.h - gh;
    if (ui_upside(p)) col = t->cols - 1 - col;
    if (!top) row = t->rows - 1 - row;
    *cx = x0 + (col + 0.5f) * t->pitch;
    *cy = y0 + (row + 0.5f) * t->pitch;
}

/* 1: (x, y) in player p's tray */
static inline int trays_hit(const trays *t, int p, float x, float y)
{
    box b = t->b[p];
    return b.w > 0 && x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;
}

#endif
