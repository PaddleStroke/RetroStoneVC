/*
 * The puzzle bench: what the logic grids share in their rules (Sudoku, Nonograms, Light Up, Binairo, Tents,
 * Skyscrapers, Futoshiki, Rectangles, Star Battle, Dominosa, Calcudoku, Kakuro, Bridges, Loop). No SDL: the rules and
 * their tests use it. The screen's side is bench_ui.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Written once, here, as static inline functions: the build takes a game's own files only (GAMES="tents" builds
 * tents.c and tents_*.c), so a shared .c file would be left out of one game's build or another's.
 *
 * - The solver. A puzzle is a set of unknowns (a cell's digit or lamp, a bridge's count, an edge's line), each with
 *   the values still possible (bit v: value v; decided when one is left). The game brings its deductions in tiers
 *   (bench_rules.deduce): tier 0 the easy ones, tier 1 the medium ones, maybe a tier 2 of hard ones. The solver adds a
 *   hard way of its own: a trial (bench_probe) gives an unknown one of its values, follows the easy deductions from
 *   it, and when they reach a contradiction takes that value away. Nothing is ever guessed: what is kept is proved,
 *   so a puzzle the solver finishes has that one solution and no other. bench_grade solves the way a person does,
 *   the easiest way that works first each time, and tells the hardest it needed: BENCH_EASY, BENCH_MEDIUM,
 *   BENCH_HARD; BENCH_GUESS when it is stuck (or stopped: a level cap, a budget), BENCH_NONE when there is no
 *   solution. bench_search counts solutions (or fills a grid by chance), bench_reduce takes clues away while the
 *   puzzle stays at its level, bench_next finds a hint's cell.
 * - The player's history: each action's changes (an index and what it held before), undone together; the moves.
 * - The stars' counts: the hints and the mistakes (the shell's ui_logic_stars rates them: 3 with neither, 2 with two
 *   at most together, else 1). A mistake is a wrong entry (a digit, a lamp, a star, a bridge, a tile, a mark...
 *   that the solution does not have) the player took back with undo: the game tells the bench how many wrong entries
 *   the board holds after each action (bench_done) and each undo (bench_undone); those an undo took away count.
 *   (Written over, erased or tapped round to another state, a wrong entry is not a mistake: tapping through states is
 *   how several of these games are played.)
 * - A cell's name in the notation: "c4", its column's letter a.., its row 1.. from the top.
 */
#ifndef BENCH_H
#define BENCH_H

#include <stdint.h>
#include <string.h>

#include "games/puzrand.h"

#define BENCH_VARS 512                          /* the unknowns a puzzle may have (Loop 15 x 15: 480 edges) */
#define BENCH_HIST 4096                         /* the changes undo can take back */

enum { BENCH_NONE = -1, BENCH_EASY, BENCH_MEDIUM, BENCH_HARD, BENCH_GUESS };

/* the order of the trials: BENCH_SCAN the unknowns in turn from where the last one was found (values a trial without
 * contradiction led to are not tried again: they would lead no further); BENCH_PAIRS those with two values left
 * first (likelier to break), then the others. Each game keeps the one its makers were tuned with. */
enum { BENCH_SCAN, BENCH_PAIRS };

typedef struct { uint16_t d[BENCH_VARS]; } bench_grid;

typedef struct {
    int vars;                                   /* the unknowns used: d[0 .. vars-1] */
    int tiers;                                  /* the game's tiers of deductions (1 to 3) */
    int probe_tiers;                            /* the tiers a trial follows */
    /* one pass of a tier's deductions: 1 something found, 0 nothing, -1 a contradiction */
    int (*deduce)(bench_grid *g, int tier, void *ctx);
    /* every unknown decided: 1 a solution (a loop closed, the islands joined) */
    int (*valid)(const bench_grid *g, void *ctx);
    void *ctx;
    long *work;                                 /* the passes, counted on for a maker's budget (may be NULL) */
    long passes, budget;                        /* the passes of this grade; over the budget (0: none), it stops */
    int no_trials;                              /* 1: no trials even at BENCH_HARD (a maker in a hurry) */
    int order;                                  /* BENCH_SCAN, BENCH_PAIRS */
} bench_rules;

static inline int bench_bits(unsigned m) { return __builtin_popcount(m); }
static inline int bench_low(unsigned m) { return __builtin_ctz(m); }
static inline int bench_high(unsigned m) { return 31 - __builtin_clz(m); }
static inline int bench_one(unsigned m) { return m && !(m & (m - 1)); }

static inline int bench_solved(const bench_grid *g, int n)
{
    for (int v = 0; v < n; v++)
        if (!bench_one(g->d[v])) return 0;
    return 1;
}

static inline int bench_over(const bench_rules *r) { return r->budget && r->passes > r->budget; }

/* the tiers 0..top until nothing more comes, the easiest again after each find: -1 a contradiction, else 0 (also
 * past the budget). *used: raised to the hardest tier that found something */
static inline int bench_settle(bench_grid *g, bench_rules *r, int top, int *used)
{
    for (;;) {
        int t, res = 0;
        for (t = 0; t <= top && !res; t++) res = r->deduce(g, t, r->ctx);
        r->passes++;
        if (r->work) (*r->work)++;
        if (res < 0) return -1;
        for (int v = 0; v < r->vars; v++)
            if (!g->d[v]) return -1;
        if (!res) return 0;
        if (t - 1 > *used) *used = t - 1;
        if (bench_over(r)) return 0;
    }
}

/* a trial: a value given to an unknown and followed by the probe tiers; a contradiction takes that value away.
 * 1: one taken away, 0: none, -1: an unknown left with nothing. *from: where to begin (after the last find) */
static inline int bench_probe(bench_grid *g, bench_rules *r, int *from)
{
    int n = r->vars, scan = r->order == BENCH_SCAN;
    uint16_t follows[BENCH_VARS];               /* BENCH_SCAN: values a trial without contradiction led to */
    if (scan) memset(follows, 0, (size_t)n * sizeof follows[0]);
    for (int k = 0; k < (scan ? n : 2 * n); k++) {
        int v = (*from + k) % n;
        unsigned m = g->d[v];
        if (bench_one(m)) continue;
        if (scan) m &= ~(unsigned)follows[v];
        else if ((k < n) != (bench_bits(m) == 2)) continue;
        for (; m; m &= m - 1) {
            unsigned bit = m & -m;
            bench_grid t = *g;
            t.d[v] = (uint16_t)bit;
            int used = 0, res = bench_settle(&t, r, r->probe_tiers - 1, &used);
            if (res == 0 && bench_solved(&t, n) && !r->valid(&t, r->ctx)) res = -1;
            if (res < 0) {
                g->d[v] &= (uint16_t)~bit;
                *from = v;
                return g->d[v] ? 1 : -1;
            }
            if (scan)
                for (int x = 0; x < n; x++)
                    if (t.d[x] != g->d[x] && bench_one(t.d[x])) follows[x] |= t.d[x];
            if (bench_over(r)) return 0;
        }
    }
    return 0;
}

/* solved the way a person would, up to the level `most` (BENCH_EASY: tier 0 alone; BENCH_MEDIUM: the game's tiers;
 * BENCH_HARD: trials too): the hardest way it needed, BENCH_GUESS (stuck, or stopped by `most`, no_trials or the
 * budget), BENCH_NONE (no solution). g ends as far as it got (the solution, or where the reasoning stopped). */
static inline int bench_grade(bench_grid *g, bench_rules *r, int most)
{
    int used = 0, from = 0, top = most < r->tiers - 1 ? most : r->tiers - 1;
    r->passes = 0;
    for (;;) {
        if (bench_settle(g, r, top, &used) < 0) return BENCH_NONE;
        if (bench_solved(g, r->vars)) return r->valid(g, r->ctx) ? used : BENCH_NONE;
        if (most < BENCH_HARD || r->no_trials || bench_over(r)) return BENCH_GUESS;
        int p = bench_probe(g, r, &from);
        if (p < 0) return BENCH_NONE;
        if (!p) return BENCH_GUESS;
        used = BENCH_HARD;
    }
}

/* bench_search's step on its own copy g (one grid a level deep) */
static inline int bench_search_in(bench_grid *g, bench_rules *r, int limit, bench_grid *sol, uint32_t *rng, long budget)
{
    int used = 0, count = 0;
    if (budget && r->work && *r->work > budget) return 0;
    if (bench_settle(g, r, r->tiers - 1, &used) < 0) return 0;
    int best = -1, bn = 99;
    for (int v = 0; v < r->vars; v++) {
        int k = bench_bits(g->d[v]);
        if (k > 1 && k < bn) bn = k, best = v;
    }
    if (best < 0) {
        if (!r->valid(g, r->ctx)) return 0;
        if (sol) *sol = *g;
        return 1;
    }
    int vs[16], nv = 0;
    for (unsigned m = g->d[best]; m; m &= m - 1) vs[nv++] = bench_low(m);
    if (rng) pr_shuffle(rng, vs, nv);
    for (int i = 0; i < nv && count < limit; i++) {
        bench_grid t = *g;
        t.d[best] = (uint16_t)(1u << vs[i]);
        count += bench_search_in(&t, r, limit - count, count ? NULL : sol, rng, budget);
        if (budget && r->work && *r->work > budget) break;
    }
    return count;
}

/* the solutions counted up to limit (depth first, every tier between the guesses); the first found in sol (may be
 * NULL); rng: the values tried in a random order (a grid filled by chance); budget: passes allowed on *r->work (0: no
 * end), past them the search stops (the count so far) */
static inline int bench_search(const bench_grid *start, bench_rules *r, int limit, bench_grid *sol, uint32_t *rng,
                               long budget)
{
    bench_grid g = *start;
    return bench_search_in(&g, r, limit, sol, rng, budget);
}

/* ---- making a puzzle: a full one (every clue), then clues taken away ------------------------------------------- */
typedef struct {
    void *u;
    int (*grade)(void *u, int cap);             /* the puzzle as it is now, graded no further than cap */
    void (*clue)(void *u, int i, int on);       /* clue i taken away (on 0) or put back (on 1) */
} bench_maker;

/* clues taken away in the order given while the puzzle stays solvable within the level: first all those the easy ways
 * can spare, then those the level's own ways can (the costly ones only for the last few: fast); within a pass a clue
 * that cannot go now cannot go later either (fewer clues are never easier). Stops past the budget (the solver's
 * passes on *work); the grade at the end */
static inline int bench_reduce(const bench_maker *m, const int *order, int n, int level, const long *work, long budget)
{
    uint8_t gone[BENCH_VARS];
    memset(gone, 0, sizeof gone);
    for (int cap = BENCH_EASY; cap <= level; cap = cap < level ? level : cap + 1)
        for (int i = 0; i < n && i < BENCH_VARS; i++) {
            if (gone[i] || (work && *work > budget)) continue;
            m->clue(m->u, order[i], 0);
            int g = m->grade(m->u, cap);
            if (g < BENCH_EASY || g > cap) m->clue(m->u, order[i], 1);
            else gone[i] = 1;
        }
    return m->grade(m->u, level);
}

/* the next unknown the easiest reasoning decides from what is known (a hint); -1: none (a contradiction, or stuck) */
static inline int bench_next(bench_grid *g, bench_rules *r)
{
    uint8_t known[BENCH_VARS];
    int from = 0;
    for (int v = 0; v < r->vars; v++) known[v] = (uint8_t)bench_one(g->d[v]);
    for (int round = 0; round < 4 * BENCH_VARS; round++) {
        int res = 0;
        for (int t = 0; t < r->tiers && !res; t++) res = r->deduce(g, t, r->ctx);
        if (!res) res = bench_probe(g, r, &from);
        if (res <= 0) return -1;
        for (int v = 0; v < r->vars; v++)
            if (!g->d[v]) return -1;
        for (int v = 0; v < r->vars; v++)
            if (!known[v] && bench_one(g->d[v])) return v;
    }
    return -1;
}

/* ---- the player's history: each action's changes (an index and what it held before), undone together ---------- */
typedef struct {
    uint16_t at, first;                         /* first: an action's first change */
    uint32_t was;
} bench_change;

typedef struct {
    bench_change h[BENCH_HIST];
    int n, open;                                /* open: an action begun, nothing logged yet */
    int moves;                                  /* the actions done (undo takes one back) */
    int hints, mistakes;                        /* the stars' slips (never taken back) */
    int wrongs;                                 /* the wrong entries on the board (after the last change) */
} bench_hist;

static inline void bench_clear(bench_hist *k) { memset(k, 0, sizeof *k); }

/* an action begins: room for `most` changes (the oldest actions forgotten when it is full) */
static inline void bench_begin(bench_hist *k, int most)
{
    k->open = 1;
    if (k->n + most <= BENCH_HIST) return;
    int cut = k->n + most - BENCH_HIST;
    if (cut < BENCH_HIST / 2) cut = BENCH_HIST / 2;
    while (cut < k->n && !k->h[cut].first) cut++;
    memmove(k->h, k->h + cut, (size_t)(k->n - cut) * sizeof k->h[0]);
    k->n -= cut;
}

/* a change within the action: what was at `at` before it */
static inline void bench_log(bench_hist *k, int at, uint32_t was)
{
    if (k->n >= BENCH_HIST) return;
    k->h[k->n++] = (bench_change){(uint16_t)at, (uint16_t)k->open, was};
    k->open = 0;
}

/* a cell of an array of states set within the action (logged when it changes) */
static inline void bench_set(bench_hist *k, uint16_t *cells, int c, int state)
{
    if (cells[c] == state) return;
    bench_log(k, c, cells[c]);
    cells[c] = (uint16_t)state;
}

/* the action over: 1 when it changed something (a move counted) */
static inline int bench_end(bench_hist *k)
{
    int changed = !k->open;
    k->open = 0;
    if (changed) k->moves++;
    return changed;
}

/* the last action taken back, latest change first, through put(ctx, at, was): the last index put back; -1: none */
static inline int bench_undo(bench_hist *k, void (*put)(void *ctx, int at, uint32_t was), void *ctx)
{
    if (!k->n) return -1;
    int at = -1;
    while (k->n) {
        bench_change c = k->h[--k->n];
        put(ctx, c.at, c.was);
        at = c.at;
        if (c.first) break;
    }
    k->moves--;
    return at;
}

/* the same for an array of states (bench_set's) */
static inline int bench_undo_cells(bench_hist *k, uint16_t *cells)
{
    if (!k->n) return -1;
    int at = -1;
    while (k->n) {
        bench_change c = k->h[--k->n];
        cells[c.at] = (uint16_t)c.was;
        at = c.at;
        if (c.first) break;
    }
    k->moves--;
    return at;
}

/* ---- the stars: the player's hints and mistakes ------------------------------------------------------------------ */
/* an action over (bench_end), the wrong entries now on the board noted: 1 when it changed something */
static inline int bench_done(bench_hist *k, int wrongs)
{
    int changed = bench_end(k);
    k->wrongs = wrongs;
    return changed;
}

/* an action undone (bench_undo), the wrong entries now on the board: those that went were mistakes */
static inline void bench_undone(bench_hist *k, int wrongs)
{
    if (wrongs < k->wrongs) k->mistakes += k->wrongs - wrongs;
    k->wrongs = wrongs;
}

/* a hint given: the place it showed (-1: none, not counted) */
static inline int bench_hinted(bench_hist *k, int at)
{
    if (at >= 0) k->hints++;
    return at;
}


/* ---- the notation ------------------------------------------------------------------------------------------------ */
/* "c4": a column letter a.., a row 1.. from the top, on a grid nx x ny: the cell x + y * nx (*len: its length; may be
 * NULL); -1: none */
static inline int bench_cell(const char *s, int nx, int ny, int *len)
{
    if (s[0] < 'a' || s[0] >= 'a' + nx || s[1] < '1' || s[1] > '9') return -1;
    int y = s[1] - '0', l = 2;
    if (s[2] >= '0' && s[2] <= '9') y = y * 10 + s[2] - '0', l = 3;
    if (y > ny) return -1;
    if (len) *len = l;
    return (y - 1) * nx + s[0] - 'a';
}

#endif
