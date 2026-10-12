/*
 * What four logic grids share in their rules (Futoshiki, Rectangles, Star Battle, Dominosa): the undo history of
 * cell changes (each action's first change marked, the moves counted), and a cell's name in the notation ("c4": the
 * column's letter a.., the row 1.. from the top). No SDL: the rules and their tests use it.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="futoshiki" builds
 * futoshiki.c and futoshiki_*.c), so a shared .c file would be left out of one game's build or another's. The screen's
 * side is logicgrid2_ui.h.
 */
#ifndef LOGICGRID2_H
#define LOGICGRID2_H

#include <stdint.h>
#include <string.h>

#define LG2_HIST 4096                           /* the changes undo can take back */

typedef struct {
    uint16_t cell, first;                       /* first: the first change of an action */
    uint32_t was;                               /* the cell's state before it */
} lg2_change;

typedef struct {
    lg2_change h[LG2_HIST];
    int n;
    int open;                                   /* an action under way: its first change not yet logged */
    int moves;                                  /* the actions done (undo takes one back) */
} lg2_hist;

/* an action begins: room for `most` changes (the oldest actions forgotten if need be) */
static inline void lg2_begin(lg2_hist *k, int most)
{
    k->open = 1;
    if (k->n + most <= LG2_HIST) return;
    int cut = k->n - (LG2_HIST - most);         /* forget the oldest, at an action's start */
    if (cut < LG2_HIST / 2) cut = LG2_HIST / 2;
    while (cut < k->n && !k->h[cut].first) cut++;
    memmove(k->h, k->h + cut, (size_t)(k->n - cut) * sizeof k->h[0]);
    k->n -= cut;
}

/* a cell about to change: its state before */
static inline void lg2_log(lg2_hist *k, int cell, uint32_t was)
{
    if (k->n >= LG2_HIST) return;
    k->h[k->n++] = (lg2_change){(uint16_t)cell, (uint16_t)k->open, was};
    k->open = 0;
}

/* the action done: 1 if it changed something (a move counted) */
static inline int lg2_end(lg2_hist *k)
{
    int changed = !k->open;
    k->open = 0;
    if (changed) k->moves++;
    return changed;
}

/* the last action taken back, each change through put(ctx, cell, was), latest first: a cell it changed; -1: none */
static inline int lg2_undo(lg2_hist *k, void (*put)(void *ctx, int cell, uint32_t was), void *ctx)
{
    if (!k->n) return -1;
    int c = -1;
    while (k->n) {
        lg2_change h = k->h[--k->n];
        put(ctx, h.cell, h.was);
        c = h.cell;
        if (h.first) break;
    }
    k->moves--;
    return c;
}

/* "c4" (a column letter a.., a row 1.. from the top) on a grid nx wide, ny high: the cell (x + y * nx); its length in
 * *len; -1: none */
static inline int lg2_cell(const char *s, int nx, int ny, int *len)
{
    if (s[0] < 'a' || s[0] >= 'a' + nx || s[1] < '1' || s[1] > '9') return -1;
    int y = s[1] - '0', l = 2;
    if (s[2] >= '0' && s[2] <= '9') y = y * 10 + s[2] - '0', l = 3;
    if (y < 1 || y > ny) return -1;
    if (len) *len = l;
    return (y - 1) * nx + s[0] - 'a';
}

static inline int lg2_bits(unsigned m) { return __builtin_popcount(m); }
static inline int lg2_low(unsigned m) { return __builtin_ctz(m); }
static inline int lg2_high(unsigned m) { return 31 - __builtin_clz(m); }

#endif
