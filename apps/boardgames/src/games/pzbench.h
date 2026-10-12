/*
 * What four pencil puzzles share in their rules (Calcudoku, Kakuro, Bridges, Loop): a solver that grades a puzzle by
 * the reasoning it needs, the player's undo history, and a cell's name in the notation. No SDL: the rules and their
 * tests use it. The screen's side is pzbench_ui.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="kakuro" builds
 * kakuro.c and kakuro_*.c), so a shared .c file would be left out of one game's build or another's.
 *
 * The solver: a puzzle is a set of unknowns (a cell's digit, a bridge's count, an edge's line), each with the values
 * still possible (bit v: value v). The game brings its deductions in tiers (pzb_rules.deduce): tier 0 the easy ones,
 * tier 1 the medium ones, maybe a tier 2 of hard ones. The solver adds a hard way itself: a trial (pzb_probe) gives
 * an unknown one of its values, follows the easy deductions from it, and when they reach a contradiction takes that
 * value away. Nothing is ever guessed: what is kept is proved, so a puzzle the solver finishes has that one solution
 * and no other.
 * pzb_grade solves the way a person does, the easiest way that works first each time, and tells the hardest it
 * needed: PZB_EASY, PZB_MEDIUM, PZB_HARD; PZB_GUESS when it is stuck, PZB_NONE when there is no solution.
 */
#ifndef PZBENCH_H
#define PZBENCH_H

#include <stdint.h>
#include <string.h>

#define PZB_VARS 512                            /* the unknowns a puzzle may have (Loop 15 x 15: 480 edges) */
#define PZB_HIST 4096                           /* the changes undo can take back */

enum { PZB_NONE = -1, PZB_EASY, PZB_MEDIUM, PZB_HARD, PZB_GUESS };

typedef struct { uint16_t d[PZB_VARS]; } pzb_grid;

typedef struct {
    int vars;                                   /* the unknowns used: d[0 .. vars-1] */
    int tiers;                                  /* the game's tiers (1 or 2) */
    int probe_tiers;                            /* the tiers a trial follows */
    /* one pass of a tier's deductions: 1 something found, 0 nothing, -1 a contradiction */
    int (*deduce)(pzb_grid *g, int tier, void *ctx);
    /* every unknown decided: 1 a solution (a loop closed, the islands joined) */
    int (*valid)(const pzb_grid *g, void *ctx);
    void *ctx;
    long work, budget;                          /* passes made; a budget (0: none): over it, the grade is a guess */
    int no_trials;                              /* 1: no trials even at PZB_HARD (a maker in a hurry) */
} pzb_rules;

static inline int pzb_bits(unsigned m) { return __builtin_popcount(m); }
static inline int pzb_low(unsigned m) { return __builtin_ctz(m); }
static inline int pzb_high(unsigned m) { return 31 - __builtin_clz(m); }
static inline int pzb_single(unsigned m) { return m && !(m & (m - 1)); }
static inline int pzb_value(const pzb_grid *g, int v) { return pzb_single(g->d[v]) ? pzb_low(g->d[v]) : -1; }

static inline int pzb_solved(const pzb_grid *g, int n)
{
    for (int v = 0; v < n; v++)
        if (!pzb_single(g->d[v])) return 0;
    return 1;
}

/* the tiers 0..top until nothing more comes, the easiest again after each find: -1 a contradiction, else 0. *used:
 * raised to the hardest tier that found something */
static inline int pzb_settle(pzb_grid *g, pzb_rules *r, int top, int *used)
{
    for (;;) {
        int t, res = 0;
        for (t = 0; t <= top && !res; t++) res = r->deduce(g, t, r->ctx);
        r->work++;
        if (res < 0) return -1;
        for (int v = 0; v < r->vars; v++)
            if (!g->d[v]) return -1;
        if (!res) return 0;
        if (t - 1 > *used) *used = t - 1;
        if (r->budget && r->work > r->budget) return 0;
    }
}

/* a trial: a value given to an unknown and followed by the probe tiers; a contradiction takes that value away.
 * 1: one taken away, 0: none, -1: an unknown left with nothing. *from: where to begin (after the last find) */
static inline int pzb_probe(pzb_grid *g, pzb_rules *r, int *from)
{
    int n = r->vars;
    for (int k = 0; k < 2 * n; k++) {           /* the unknowns with two values first: likelier to break */
        int v = (*from + k) % n;
        unsigned m = g->d[v];
        if (pzb_single(m) || (k < n) != (pzb_bits(m) == 2)) continue;
        for (unsigned left = m; left; left &= left - 1) {
            unsigned bit = left & -left;
            pzb_grid t = *g;
            t.d[v] = (uint16_t)bit;
            int used = 0, res = pzb_settle(&t, r, r->probe_tiers - 1, &used);
            if (res == 0 && pzb_solved(&t, n) && !r->valid(&t, r->ctx)) res = -1;
            if (res < 0) {
                g->d[v] &= (uint16_t)~bit;
                *from = v;
                return g->d[v] ? 1 : -1;
            }
            if (r->budget && r->work > r->budget) return 0;
        }
    }
    return 0;
}

/* solved the way a person would, up to the level `most` (PZB_EASY: tier 0 alone; PZB_MEDIUM: the game's tiers;
 * PZB_HARD: trials too): the hardest way it needed, PZB_GUESS (stuck), PZB_NONE (no solution). g ends as far as it
 * got. */
static inline int pzb_grade(pzb_grid *g, pzb_rules *r, int most)
{
    int used = 0, from = 0, top = most < r->tiers - 1 ? most : r->tiers - 1;
    r->work = 0;
    for (;;) {
        if (pzb_settle(g, r, top, &used) < 0) return PZB_NONE;
        if (pzb_solved(g, r->vars)) return r->valid(g, r->ctx) ? used : PZB_NONE;
        if (most < PZB_HARD || r->no_trials || (r->budget && r->work > r->budget)) return PZB_GUESS;
        int p = pzb_probe(g, r, &from);
        if (p < 0) return PZB_NONE;
        if (!p) return PZB_GUESS;
        used = PZB_HARD;
    }
}

/* ---- the player's history: each action's changes (an index and what it held before), undone together ---------- */
typedef struct {
    uint16_t at, first;                         /* first: an action's first change */
    uint32_t was;
} pzb_change;

typedef struct {
    pzb_change h[PZB_HIST];
    int n, open;                                /* open: an action begun, nothing logged yet */
    int moves;                                  /* the actions done (undo takes one back) */
} pzb_hist;

static inline void pzb_hist_clear(pzb_hist *k) { k->n = k->open = k->moves = 0; }

/* an action begins: room for `most` changes (the oldest actions forgotten when it is full) */
static inline void pzb_begin(pzb_hist *k, int most)
{
    k->open = 1;
    if (k->n + most <= PZB_HIST) return;
    int cut = k->n + most - PZB_HIST;
    if (cut < PZB_HIST / 2) cut = PZB_HIST / 2;
    while (cut < k->n && !k->h[cut].first) cut++;
    memmove(k->h, k->h + cut, (size_t)(k->n - cut) * sizeof k->h[0]);
    k->n -= cut;
}

static inline void pzb_log(pzb_hist *k, int at, uint32_t was)
{
    if (k->n >= PZB_HIST) return;
    k->h[k->n++] = (pzb_change){(uint16_t)at, (uint16_t)k->open, was};
    k->open = 0;
}

/* the action over: 1 when it changed something (a move counted) */
static inline int pzb_end(pzb_hist *k)
{
    int changed = !k->open;
    k->open = 0;
    if (changed) k->moves++;
    return changed;
}

/* the last action taken back, latest change first, through put(ctx, at, was): the last index put back; -1: none */
static inline int pzb_undo(pzb_hist *k, void (*put)(void *ctx, int at, uint32_t was), void *ctx)
{
    if (!k->n) return -1;
    int at = -1;
    while (k->n) {
        pzb_change c = k->h[--k->n];
        put(ctx, c.at, c.was);
        at = c.at;
        if (c.first) break;
    }
    k->moves--;
    return at;
}

/* "c4": a column letter a.., a row 1.. from the top, on a grid nx x ny: the cell x + y * nx (*len: its length);
 * -1: none */
static inline int pzb_cell(const char *s, int nx, int ny, int *len)
{
    if (s[0] < 'a' || s[0] >= 'a' + nx || s[1] < '1' || s[1] > '9') return -1;
    int y = s[1] - '0', l = 2;
    if (s[2] >= '0' && s[2] <= '9') y = y * 10 + s[2] - '0', l = 3;
    if (y > ny) return -1;
    if (len) *len = l;
    return (y - 1) * nx + s[0] - 'a';
}

#endif
