/*
 * LITS: the rules (lits.h): the ways of reasoning the solver grades with, the maker, the player's grid and undo. No
 * SDL: tested by tests/lits_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/lits.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/lits_levels.h"

#define WHITE 1u                                /* a cell's values: bit 0 unshaded, bit 1 shaded */
#define SHADE 2u
#define BOTH 3u
#define MAX_REGIONS 62
#define ORIENTS 18                              /* the pieces turned and mirrored: 8 L, 2 I, 4 T, 4 S */
#define MAX_PLACES (LTS_CELLS * ORIENTS / 2)

static const int SIZES[LTS_SIZES] = {6, 8, 10};
static const char CODE[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

int lts_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % LTS_SIZES]; }
int lts_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

/* ---- the pieces ------------------------------------------------------------------------------------------------ */
static int8_t orient[ORIENTS][4][2];            /* each way a piece lies: its cells (x, y) from its corner */
static uint8_t orient_shape[ORIENTS];
static int orients;

static void orient_init(void)
{
    if (orients) return;
    static const int8_t BASE[LTS_SHAPES][4][2] = {
        {{0, 0}, {0, 1}, {0, 2}, {1, 2}},       /* L */
        {{0, 0}, {0, 1}, {0, 2}, {0, 3}},       /* I */
        {{0, 0}, {1, 0}, {2, 0}, {1, 1}},       /* T */
        {{1, 0}, {2, 0}, {0, 1}, {1, 1}},       /* S */
    };
    for (int s = 0; s < LTS_SHAPES; s++)
        for (int t = 0; t < 8; t++) {
            int8_t c[4][2], mx = 99, my = 99;
            for (int i = 0; i < 4; i++) {
                int x = BASE[s][i][0], y = BASE[s][i][1];
                if (t & 1) x = -x;
                if (t & 2) y = -y;
                if (t & 4) { int u = x; x = y; y = u; }
                c[i][0] = (int8_t)x;
                c[i][1] = (int8_t)y;
                if (x < mx) mx = (int8_t)x;
                if (y < my) my = (int8_t)y;
            }
            for (int i = 0; i < 4; i++) c[i][0] -= mx, c[i][1] -= my;
            for (int i = 0; i < 4; i++)         /* in reading order */
                for (int j = i + 1; j < 4; j++)
                    if (c[j][1] < c[i][1] || (c[j][1] == c[i][1] && c[j][0] < c[i][0])) {
                        int8_t a = c[i][0], b = c[i][1];
                        c[i][0] = c[j][0], c[i][1] = c[j][1], c[j][0] = a, c[j][1] = b;
                    }
            int dup = 0;
            for (int o = 0; o < orients && !dup; o++) dup = !memcmp(orient[o], c, sizeof c);
            if (dup) continue;
            memcpy(orient[orients], c, sizeof c);
            orient_shape[orients++] = (uint8_t)s;
        }
}

int lts_shape(const int *cells, int n)
{
    if (n <= 0) return -1;
    orient_init();
    int c[4][2], mx = 1 << 20, my = 1 << 20;
    for (int i = 0; i < 4; i++) {
        c[i][0] = cells[i] % n;
        c[i][1] = cells[i] / n;
        if (c[i][0] < mx) mx = c[i][0];
        if (c[i][1] < my) my = c[i][1];
    }
    int8_t k[4][2];
    for (int i = 0; i < 4; i++) {
        if (c[i][0] - mx > 3 || c[i][1] - my > 3) return -1;
        k[i][0] = (int8_t)(c[i][0] - mx);
        k[i][1] = (int8_t)(c[i][1] - my);
    }
    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 4; j++)
            if (k[j][1] < k[i][1] || (k[j][1] == k[i][1] && k[j][0] < k[i][0])) {
                int8_t a = k[i][0], b = k[i][1];
                k[i][0] = k[j][0], k[i][1] = k[j][1], k[j][0] = a, k[j][1] = b;
            }
    for (int o = 0; o < orients; o++)
        if (!memcmp(orient[o], k, sizeof k)) return orient_shape[o];
    return -1;
}

/* ---- what the solver knows of a grid: the neighbours, the regions' cells, the pieces that fit in each ---------- */
typedef struct {
    const lts_puzzle *p;
    int n, cells, regions, passes, from;        /* passes: a trial's, at the level graded */
    long *work;                                 /* the solver's work counted (each trial's passes too) */
    uint8_t nn[LTS_CELLS], nb[LTS_CELLS][4];
    uint8_t rcell[LTS_CELLS];                   /* the cells region by region: rstart[r] .. rstart[r + 1] - 1 */
    int16_t rstart[LTS_CELLS + 1];
    uint8_t pc[MAX_PLACES][4], ps[MAX_PLACES];  /* each region's places for a piece: pstart[r] .. pstart[r + 1] - 1 */
    int16_t pstart[LTS_CELLS + 1];
    /* each place's 2 x 2 squares that cells outside its region could fill: those cells (up to three, 0xff after) */
    uint8_t sq[MAX_PLACES][9][3], nsq[MAX_PLACES];
    uint64_t adj[LTS_CELLS];                    /* each region's neighbours (bit r) */
} ctx;

static void ctx_make(ctx *k, const lts_puzzle *p)
{
    orient_init();
    int n = p->n;
    k->p = p;
    k->n = n;
    k->cells = n * n;
    k->regions = p->regions;
    k->passes = 1;
    k->from = 0;
    memset(k->nn, 0, sizeof k->nn);
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n;
        if (y > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - n);
        if (x > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - 1);
        if (x < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + 1);
        if (y < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + n);
    }
    int at = 0, np = 0;
    for (int r = 0; r < p->regions; r++) {
        k->rstart[r] = (int16_t)at;
        k->pstart[r] = (int16_t)np;
        for (int c = 0; c < n * n; c++) {
            if (p->region[c] != r) continue;
            k->rcell[at++] = (uint8_t)c;
            for (int o = 0; o < orients; o++) {  /* the places whose first cell is c */
                int ok = 1, cs[4];
                for (int i = 0; i < 4 && ok; i++) {
                    int x = c % n + orient[o][i][0] - orient[o][0][0], y = c / n + orient[o][i][1];
                    ok = x >= 0 && x < n && y < n && p->region[y * n + x] == r;
                    cs[i] = y * n + x;
                }
                if (!ok || np >= MAX_PLACES) continue;
                for (int i = 0; i < 4; i++) k->pc[np][i] = (uint8_t)cs[i];
                k->nsq[np] = 0;
                int done[16], nd = 0;
                for (int e = 0; e < 16; e++) {   /* the squares it overlaps, filled from outside */
                    int x0 = cs[e / 4] % n - (e & 1), y0 = cs[e / 4] / n - (e >> 1 & 1), dup = 0;
                    if (x0 < 0 || y0 < 0 || x0 + 1 >= n || y0 + 1 >= n) continue;
                    for (int i = 0; i < nd; i++) dup |= done[i] == y0 * n + x0;
                    if (dup) continue;
                    done[nd++] = y0 * n + x0;
                    int out = 0, mine = 0;
                    uint8_t rest[3] = {0xff, 0xff, 0xff};
                    for (int j = 0; j < 4; j++) {
                        int d = (y0 + j / 2) * n + x0 + j % 2;
                        if (d == cs[0] || d == cs[1] || d == cs[2] || d == cs[3]) continue;
                        if (p->region[d] == r) mine++;
                        else if (out < 3) rest[out++] = (uint8_t)d;
                    }
                    if (!mine && k->nsq[np] < 9) memcpy(k->sq[np][k->nsq[np]++], rest, 3);
                }
                k->ps[np++] = orient_shape[o];
            }
        }
    }
    k->rstart[p->regions] = (int16_t)at;
    k->pstart[p->regions] = (int16_t)np;
    memset(k->adj, 0, sizeof k->adj);
    for (int c = 0; c < n * n; c++)
        for (int j = 0; j < k->nn[c]; j++) {
            int a = p->region[c], b = p->region[k->nb[c][j]];
            if (a != b) k->adj[a] |= 1ull << b;
        }
}

static void grid_of(const ctx *k, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < k->cells; c++) g->d[c] = BOTH;
}

static void force(bench_grid *g, int c, unsigned to) { g->d[c] &= (uint16_t)to; }

/* -1: a cell left with no value; 1: something changed since was */
static int settled(const uint16_t *was, const bench_grid *g, int cells)
{
    int ch = 0;
    for (int c = 0; c < cells; c++) {
        if (!g->d[c]) return -1;
        ch |= g->d[c] != was[c];
    }
    return ch;
}

/* the shading's joins: the shaded cells all in one piece of cells not unshaded (-1: not), the cells cut off from them
 * unshaded, the cells they cannot do without shaded (the cut points between shaded cells, depth first) */
typedef struct {
    const ctx *k;
    const uint16_t *w;
    int disc[LTS_CELLS], low[LTS_CELLS], sub[LTS_CELLS], t, shaded;
    uint8_t *cut;
} cuts;

static void cut_dfs(cuts *D, int u, int parent)
{
    const ctx *k = D->k;
    int split = 0, apart = 0;
    D->disc[u] = D->low[u] = ++D->t;
    D->sub[u] = D->w[u] == SHADE;
    for (int j = 0; j < k->nn[u]; j++) {
        int v = k->nb[u][j];
        if (D->w[v] == WHITE) continue;
        if (!D->disc[v]) {
            cut_dfs(D, v, u);
            D->sub[u] += D->sub[v];
            if (D->low[v] < D->low[u]) D->low[u] = D->low[v];
            if (D->low[v] >= D->disc[u] && D->sub[v]) split++, apart += D->sub[v];
        } else if (v != parent && D->disc[v] < D->low[u]) {
            D->low[u] = D->disc[v];
        }
    }
    if (D->w[u] == BOTH && split + (D->shaded - apart > 0) >= 2) D->cut[u] = 1;
}

static int joins(bench_grid *g, const uint16_t *w, const ctx *k)
{
    cuts D;
    uint8_t cut[LTS_CELLS];
    int first = -1;
    memset(cut, 0, (size_t)k->cells);
    memset(D.disc, 0, (size_t)k->cells * sizeof D.disc[0]);
    D.k = k;
    D.w = w;
    D.t = D.shaded = 0;
    D.cut = cut;
    for (int c = 0; c < k->cells; c++)
        if (w[c] == SHADE) {
            D.shaded++;
            if (first < 0) first = c;
        }
    if (first < 0) return 0;
    cut_dfs(&D, first, -1);
    if (D.sub[first] != D.shaded) return -1;
    for (int c = 0; c < k->cells; c++) {
        if (w[c] != BOTH) continue;
        if (!D.disc[c]) force(g, c, WHITE);
        else if (cut[c]) force(g, c, SHADE);
    }
    return 0;
}

/* the plain ways (on the cells of cm and the regions of rm only, when given): a 2 x 2 with three shaded cells leaves
 * the fourth; each region's places that fit (no unshaded cell, every shaded one of the region, no 2 x 2 with the
 * shading round it, not beside a shaded cell of a neighbour settled on the same shape): cells in all shaded, in none
 * unshaded; the shading's joins */
static int plain_some(bench_grid *g, const ctx *k, const uint8_t *cm, const uint8_t *rm)
{
    int n = k->n;
    uint16_t w[LTS_CELLS];
    memcpy(w, g->d, (size_t)k->cells * sizeof w[0]);
    for (int y = 0; y + 1 < n; y++)             /* the 2 x 2 squares */
        for (int x = 0; x + 1 < n; x++) {
            int c = y * n + x, sq[4] = {c, c + 1, c + n, c + n + 1}, s = 0, u = -1, nu = 0;
            if (cm && !cm[sq[0]] && !cm[sq[1]] && !cm[sq[2]] && !cm[sq[3]]) continue;
            for (int i = 0; i < 4; i++) {
                s += w[sq[i]] == SHADE;
                if (w[sq[i]] == BOTH) u = sq[i], nu++;
            }
            if (s == 4) return -1;
            if (s == 3 && nu == 1) force(g, u, WHITE);
        }
    static uint8_t okp[MAX_PLACES];
    uint8_t mask[LTS_CELLS];                    /* each region's shapes that still fit */
    int shaded[LTS_CELLS];
    for (int r = 0; r < k->regions; r++) {
        mask[r] = 0;
        shaded[r] = 0;
        for (int i = k->rstart[r]; i < k->rstart[r + 1]; i++) shaded[r] += w[k->rcell[i]] == SHADE;
        if (shaded[r] > 4) return -1;
    }
    uint64_t need = 0;                          /* the regions looked at, and their neighbours (their shapes) */
    for (int r = 0; r < k->regions; r++)
        if (!rm || rm[r]) need |= 1ull << r | k->adj[r];
    for (int r = 0; r < k->regions; r++) {
        if (!(need >> r & 1)) continue;
        for (int q = k->pstart[r]; q < k->pstart[r + 1]; q++) {
            const uint8_t *pc = k->pc[q];
            int ok = 1, s = 0;
            for (int i = 0; i < 4 && ok; i++) {
                ok = w[pc[i]] != WHITE;
                s += w[pc[i]] == SHADE;
            }
            ok = ok && s == shaded[r];
            for (int e = 0; e < k->nsq[q] && ok; e++) {   /* no 2 x 2 with the shading round it */
                const uint8_t *sq = k->sq[q][e];
                int full = 1;
                for (int j = 0; j < 3 && sq[j] != 0xff && full; j++) full = w[sq[j]] == SHADE;
                if (full) ok = 0;
            }
            okp[q] = (uint8_t)ok;
            if (ok) mask[r] |= (uint8_t)(1u << k->ps[q]);
        }
    }
    for (int r = 0; r < k->regions; r++) {
        if (rm && !rm[r]) continue;
        int ways = 0, on[LTS_CELLS];
        for (int i = k->rstart[r]; i < k->rstart[r + 1]; i++) on[k->rcell[i]] = 0;
        for (int q = k->pstart[r]; q < k->pstart[r + 1]; q++) {
            if (!okp[q]) continue;
            const uint8_t *pc = k->pc[q];
            int ok = 1;
            for (int i = 0; i < 4 && ok; i++)   /* not beside a settled neighbour of its shape */
                for (int j = 0; j < k->nn[pc[i]] && ok; j++) {
                    int d = k->nb[pc[i]][j], r2 = k->p->region[d];
                    if (r2 != r && w[d] == SHADE && mask[r2] == 1u << k->ps[q]) ok = 0;
                }
            if (!ok) continue;
            ways++;
            for (int i = 0; i < 4; i++) on[pc[i]]++;
        }
        if (!ways) return -1;
        for (int i = k->rstart[r]; i < k->rstart[r + 1]; i++) {
            int c = k->rcell[i];
            if (w[c] == BOTH && (on[c] == ways || !on[c])) force(g, c, on[c] ? SHADE : WHITE);
        }
    }
    if (joins(g, w, k) < 0) return -1;
    return settled(w, g, k->cells);
}

static int plain(bench_grid *g, const ctx *k) { return plain_some(g, k, NULL, NULL); }

/* a change at c followed through the plain ways `passes` times, each pass only where the last one changed something
 * (the rest was settled before; the joins everywhere): -1 a contradiction, 0 it settles, 1 still going */
static int follow(bench_grid *t, const ctx *k, int c, int passes)
{
    uint8_t dirty[LTS_CELLS], cm[LTS_CELLS], rm[LTS_CELLS];
    uint16_t was[LTS_CELLS];
    memset(dirty, 0, (size_t)k->cells);
    dirty[c] = 1;
    for (int pass = 0; pass < passes; pass++) {
        memset(cm, 0, (size_t)k->cells);
        memset(rm, 0, (size_t)k->regions);
        for (int x = 0; x < k->cells; x++) {
            if (!dirty[x]) continue;
            cm[x] = 1;
            rm[k->p->region[x]] = 1;
            for (int j = 0; j < k->nn[x]; j++) {   /* the neighbours' regions: the shapes beside them */
                cm[k->nb[x][j]] = 1;
                rm[k->p->region[k->nb[x][j]]] = 1;
            }
        }
        memcpy(was, t->d, (size_t)k->cells * sizeof was[0]);
        if (k->work) (*k->work)++;
        int res = plain_some(t, k, cm, rm);
        if (res <= 0) return res;
        for (int x = 0; x < k->cells; x++) dirty[x] = t->d[x] != was[x];
    }
    return 1;
}

/* trials: each unknown cell tried each way, the plain ways followed `passes` times; a contradiction rules that way
 * out (one pass: what a glance shows; a few: a short chain). A few finds, then the plain ways first again */
static int trials(bench_grid *g, ctx *k, int passes)
{
    uint16_t w[LTS_CELLS];
    bench_grid t;
    memcpy(w, g->d, (size_t)k->cells * sizeof w[0]);
    for (int i = 0, start = k->from, found = 0; i < k->cells; i++) {
        int c = (start + i) % k->cells;         /* from the last find on */
        if (w[c] != BOTH) continue;
        for (unsigned v = WHITE; v <= SHADE; v++) {
            memcpy(t.d, w, (size_t)k->cells * sizeof w[0]);
            t.d[c] = (uint16_t)v;
            if (follow(&t, k, c, passes) < 0) force(g, c, BOTH & ~v);
        }
        if (g->d[c] != w[c] && ++found >= 4) {
            k->from = c;
            break;
        }
    }
    return settled(w, g, k->cells);
}

static long finds;                              /* the trials' finds (lts_effort counts them) */

static int deduce(bench_grid *g, int tier, void *cv)
{
    ctx *k = cv;
    int res = tier ? trials(g, k, k->passes) : plain(g, k);
    if (res > 0 && tier) finds++;
    return res;
}

/* the shading of a full grid keeps every rule */
static int valid_cells(const ctx *k, const uint8_t *sh)
{
    int n = k->n, shape[LTS_CELLS], first = -1, total = 0;
    for (int r = 0; r < k->regions; r++) {
        int cs[4], m = 0;
        for (int i = k->rstart[r]; i < k->rstart[r + 1]; i++)
            if (sh[k->rcell[i]]) {
                if (m == 4) return 0;
                cs[m++] = k->rcell[i];
            }
        if (m != 4 || (shape[r] = lts_shape(cs, n)) < 0) return 0;
    }
    for (int c = 0; c < k->cells; c++) {
        if (!sh[c]) continue;
        total++;
        if (first < 0) first = c;
        int x = c % n, y = c / n;
        if (x + 1 < n && y + 1 < n && sh[c + 1] && sh[c + n] && sh[c + n + 1]) return 0;
        for (int j = 0; j < k->nn[c]; j++) {
            int d = k->nb[c][j];
            if (sh[d] && k->p->region[d] != k->p->region[c] && shape[k->p->region[d]] == shape[k->p->region[c]])
                return 0;
        }
    }
    int stack[LTS_CELLS], sp = 0, got = 0;
    uint8_t seen[LTS_CELLS];
    memset(seen, 0, (size_t)k->cells);
    seen[first] = 1;
    stack[sp++] = first;
    while (sp) {
        int c = stack[--sp];
        got++;
        for (int j = 0; j < k->nn[c]; j++) {
            int d = k->nb[c][j];
            if (sh[d] && !seen[d]) seen[d] = 1, stack[sp++] = d;
        }
    }
    return got == total;
}

static int valid(const bench_grid *g, void *cv)
{
    const ctx *k = cv;
    uint8_t sh[LTS_CELLS];
    for (int c = 0; c < k->cells; c++) sh[c] = g->d[c] == SHADE;
    return valid_cells(k, sh);
}

/* the reasoning of a level: the plain ways, and trials followed one pass (easy), three (medium) or eight (hard) */
static bench_rules rules_of(ctx *k, int level, long *work)
{
    k->passes = level == BENCH_EASY ? 1 : level == BENCH_MEDIUM ? 3 : 8;
    k->work = work;
    return (bench_rules){.vars = k->cells, .tiers = level < 0 ? 1 : 2, .probe_tiers = 1,
                        .deduce = deduce, .valid = valid, .ctx = k, .work = work};
}

/* the grid g reasoned at a level: BENCH_EASY (solved), BENCH_GUESS (stuck), BENCH_NONE (no solution) */
static int run(const lts_puzzle *p, int level, bench_grid *g, long *work, long budget, long *passes)
{
    static ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, level, work);
    r.budget = budget;
    grid_of(&k, g);
    int lv = bench_grade(g, &r, BENCH_MEDIUM);
    if (passes) *passes = r.passes;
    return lv >= BENCH_EASY && lv <= BENCH_HARD ? BENCH_EASY : lv;
}

/* the easiest level that solves it, up to cap (the solution in sol; may be NULL); BENCH_GUESS, BENCH_NONE */
static int grade_work(const lts_puzzle *p, uint8_t *sol, long *work, int cap, long budget, long *passes)
{
    bench_grid g;
    for (int lv = BENCH_EASY; lv <= cap; lv++) {
        finds = 0;
        int res = run(p, lv, &g, work, budget, passes);
        if (res == BENCH_NONE) return BENCH_NONE;
        if (res != BENCH_EASY) continue;
        if (sol)
            for (int c = 0; c < p->n * p->n; c++) sol[c] = g.d[c] == SHADE;
        return lv;
    }
    return BENCH_GUESS;
}

int lts_grade(const lts_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, BENCH_HARD, 0, NULL); }

/* the solutions counted (the plain ways between the guesses); the first in sol (may be NULL); past budget passes
 * (0: none) the count so far */
static int search(const lts_puzzle *p, int limit, uint8_t *sol, long budget)
{
    static ctx k;
    ctx_make(&k, p);
    long work = 0;
    bench_rules r = rules_of(&k, -1, &work);
    bench_grid g, s;
    grid_of(&k, &g);
    int got = bench_search(&g, &r, limit, &s, NULL, budget);
    if (got && sol)
        for (int c = 0; c < k.cells; c++) sol[c] = s.d[c] == SHADE;
    return got;
}

int lts_count(const lts_puzzle *p, int limit) { return search(p, limit, NULL, 0); }

int lts_effort(const lts_puzzle *p, long *passes, long *trial_finds)
{
    int lv = grade_work(p, NULL, NULL, BENCH_HARD, 0, passes);
    if (trial_finds) *trial_finds = finds;
    return lv;
}

/* ---- a puzzle as text (the levels) ------------------------------------------------------------------------------- */
/* the regions numbered again in the order of their first cells; 0 when one is in two pieces, under four cells, or
 * there are too many */
static int relabel(lts_puzzle *p)
{
    int n = p->n, map[256], next = 0;
    for (int r = 0; r < 256; r++) map[r] = -1;
    for (int c = 0; c < n * n; c++)
        if (map[p->region[c]] < 0) map[p->region[c]] = next++;
    for (int c = 0; c < n * n; c++) p->region[c] = (uint8_t)map[p->region[c]];
    p->regions = next;
    if (next > MAX_REGIONS) return 0;
    for (int r = 0; r < next; r++) {
        int stack[LTS_CELLS], sp = 0, got = 0, size = 0, first = -1;
        uint8_t seen[LTS_CELLS];
        memset(seen, 0, sizeof seen);
        for (int c = 0; c < n * n; c++)
            if (p->region[c] == r) {
                size++;
                if (first < 0) first = c;
            }
        seen[first] = 1;
        stack[sp++] = first;
        while (sp) {
            int c = stack[--sp], x = c % n, y = c / n;
            got++;
            const int nb[4] = {y > 0 ? c - n : -1, x > 0 ? c - 1 : -1, x < n - 1 ? c + 1 : -1, y < n - 1 ? c + n : -1};
            for (int j = 0; j < 4; j++)
                if (nb[j] >= 0 && !seen[nb[j]] && p->region[nb[j]] == r) seen[nb[j]] = 1, stack[sp++] = nb[j];
        }
        if (got != size || size < 4) return 0;
    }
    return 1;
}

int lts_encode(const lts_puzzle *p, char *out, int size)
{
    int n = p->n, len = snprintf(out, (size_t)size, "%d:", n);
    if (len < 0 || len + n * n + 1 > size || p->regions > MAX_REGIONS) return 0;
    for (int c = 0; c < n * n; c++) out[len++] = CODE[p->region[c]];
    out[len] = 0;
    return len;
}

int lts_decode(const char *s, lts_puzzle *p)
{
    memset(p, 0, sizeof *p);
    int n = 0;
    for (; *s >= '0' && *s <= '9'; s++) n = n * 10 + *s - '0';
    if (*s++ != ':' || n < 3 || n > LTS_MAX || (int)strlen(s) != n * n) return 0;
    p->n = n;
    for (int c = 0; c < n * n; c++) {
        const char *d = strchr(CODE, s[c]);
        if (!d || !s[c]) return 0;
        p->region[c] = (uint8_t)(d - CODE);
    }
    return relabel(p);
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
/* the pieces laid by chance, each touching those before (never two of a shape side by side, never a 2 x 2), up to
 * `want`; the regions grown round them over the cells left, each cell to its smallest neighbour mostly. sh: the
 * shading. The pieces laid */
static int lay(lts_puzzle *p, int n, int want, uint8_t *sh, uint32_t *s)
{
    orient_init();
    memset(p, 0, sizeof *p);
    p->n = n;
    int cells = n * n, pieces = 0, size[LTS_CELLS], shape[LTS_CELLS];
    memset(p->region, 0xff, sizeof p->region);
    memset(sh, 0, LTS_CELLS);
    for (int tries = 0; tries < 60 * want && pieces < want; tries++) {
        int c, front[LTS_CELLS], nf = 0;
        for (int i = 0; i < cells; i++) {       /* a free cell beside the shading (anywhere for the first) */
            if (sh[i]) continue;
            int x = i % n, y = i / n;
            if (!pieces || (x > 0 && sh[i - 1]) || (x < n - 1 && sh[i + 1]) || (y > 0 && sh[i - n]) ||
                (y < n - 1 && sh[i + n]))
                front[nf++] = i;
        }
        if (!nf) break;
        c = front[pr_below(s, nf)];
        int o = pr_below(s, orients), a = pr_below(s, 4), cs[4], ok = 1;
        for (int i = 0; i < 4 && ok; i++) {
            int x = c % n + orient[o][i][0] - orient[o][a][0], y = c / n + orient[o][i][1] - orient[o][a][1];
            ok = x >= 0 && y >= 0 && x < n && y < n && !sh[y * n + x];
            cs[i] = y * n + x;
        }
        if (!ok) continue;
        for (int i = 0; i < 4; i++) sh[cs[i]] = 1;
        for (int i = 0; i < 4 && ok; i++) {
            int x = cs[i] % n, y = cs[i] / n;
            for (int dy = -1; dy <= 0 && ok; dy++)  /* no 2 x 2 */
                for (int dx = -1; dx <= 0 && ok; dx++) {
                    int x0 = x + dx, y0 = y + dy;
                    if (x0 >= 0 && y0 >= 0 && x0 + 1 < n && y0 + 1 < n && sh[y0 * n + x0] && sh[y0 * n + x0 + 1] &&
                        sh[(y0 + 1) * n + x0] && sh[(y0 + 1) * n + x0 + 1])
                        ok = 0;
                }
            const int nb[4] = {y > 0 ? cs[i] - n : -1, x > 0 ? cs[i] - 1 : -1, x < n - 1 ? cs[i] + 1 : -1,
                               y < n - 1 ? cs[i] + n : -1};
            for (int j = 0; j < 4 && ok; j++)   /* no piece of its shape beside it */
                if (nb[j] >= 0 && p->region[nb[j]] != 0xff && shape[p->region[nb[j]]] == orient_shape[o]) ok = 0;
        }
        if (!ok) {
            for (int i = 0; i < 4; i++) sh[cs[i]] = 0;
            continue;
        }
        for (int i = 0; i < 4; i++) p->region[cs[i]] = (uint8_t)pieces;
        shape[pieces] = orient_shape[o];
        size[pieces++] = 4;
    }
    for (;;) {                                  /* the regions grown over the cells left */
        int open[LTS_CELLS], no = 0;
        for (int c = 0; c < cells; c++) {
            if (p->region[c] != 0xff) continue;
            int x = c % n, y = c / n;
            if ((x > 0 && p->region[c - 1] != 0xff) || (x < n - 1 && p->region[c + 1] != 0xff) ||
                (y > 0 && p->region[c - n] != 0xff) || (y < n - 1 && p->region[c + n] != 0xff))
                open[no++] = c;
        }
        if (!no) break;
        int c = open[pr_below(s, no)], x = c % n, y = c / n, best = -1, cand[4], nc = 0;
        const int nb[4] = {y > 0 ? c - n : -1, x > 0 ? c - 1 : -1, x < n - 1 ? c + 1 : -1, y < n - 1 ? c + n : -1};
        for (int j = 0; j < 4; j++)
            if (nb[j] >= 0 && p->region[nb[j]] != 0xff) {
                int r = p->region[nb[j]];
                cand[nc++] = r;
                if (best < 0 || size[r] < size[best]) best = r;
            }
        if (pr_below(s, 3) == 0) best = cand[pr_below(s, nc)];
        p->region[c] = (uint8_t)best;
        size[best]++;
    }
    p->regions = pieces;
    return pieces;
}

/* how far the reasoning of a level gets (cells decided): *done 1 when it solves it, und the cells left (*nu) */
#define GRADE_BUDGET 4000L
static int progress(const lts_puzzle *p, long *work, int cap, int *done, int *und, int *nu)
{
    bench_grid g;
    *done = run(p, cap, &g, work, GRADE_BUDGET, NULL) == BENCH_EASY;
    int got = 0;
    *nu = 0;
    for (int c = 0; c < p->n * p->n; c++) {
        if (bench_one(g.d[c])) got++;
        else und[(*nu)++] = c;
    }
    return got;
}

/* the unshaded cell c passed to the region of its neighbour d: 1 when its own region stays in one piece */
static int pass_cell(lts_puzzle *p, int c, int d)
{
    int n = p->n, from = p->region[c], size = 0;
    if (from == p->region[d]) return 0;
    for (int i = 0; i < n * n; i++) size += p->region[i] == from;
    if (size <= 4) return 0;
    p->region[c] = p->region[d];
    int stack[LTS_CELLS], sp = 0, got = 0, first = -1;
    uint8_t seen[LTS_CELLS];
    memset(seen, 0, sizeof seen);
    for (int i = 0; i < n * n && first < 0; i++)
        if (p->region[i] == from) first = i;
    seen[first] = 1;
    stack[sp++] = first;
    while (sp) {
        int x = stack[--sp], px = x % n, py = x / n;
        got++;
        const int nb[4] = {py > 0 ? x - n : -1, px > 0 ? x - 1 : -1, px < n - 1 ? x + 1 : -1, py < n - 1 ? x + n : -1};
        for (int j = 0; j < 4; j++)
            if (nb[j] >= 0 && !seen[nb[j]] && p->region[nb[j]] == from) seen[nb[j]] = 1, stack[sp++] = nb[j];
    }
    if (got == size - 1) return 1;
    p->region[c] = (uint8_t)from;
    return 0;
}

/* another piece in the region of c, as the shading sh would keep every rule with it (the puzzle as it is then has
 * both: the maker's next moves may tell them apart): 1 when one was found */
static int switch_piece(const lts_puzzle *p, uint8_t *sh, int c, uint32_t *s)
{
    static ctx k;
    ctx_make(&k, p);
    int r = p->region[c], np = k.pstart[r + 1] - k.pstart[r];
    if (np < 2) return 0;
    uint8_t t[LTS_CELLS];
    for (int tries = 0; tries < 4; tries++) {
        int q = k.pstart[r] + pr_below(s, np);
        memcpy(t, sh, (size_t)k.cells);
        for (int i = k.rstart[r]; i < k.rstart[r + 1]; i++) t[k.rcell[i]] = 0;
        for (int i = 0; i < 4; i++) t[k.pc[q][i]] = 1;
        if (memcmp(t, sh, (size_t)k.cells) && valid_cells(&k, t)) {
            memcpy(sh, t, (size_t)k.cells);
            return 1;
        }
    }
    return 0;
}

/* a move of the maker in m: an unshaded cell (of the shading sh) on a wall passed to its neighbour's region, mostly
 * near the cells und[0..nu-1] the reasoning left; 1 when one was made */
static int move(lts_puzzle *m, const uint8_t *sh, const int *und, int nu, uint32_t *s)
{
    int n = m->n, moves[4 * LTS_CELLS], nm = 0, near = nu > 0 && pr_below(s, 100) < 85;
    uint8_t left[LTS_CELLS];
    memset(left, 0, sizeof left);
    for (int i = 0; i < nu && near; i++) left[und[i]] = 1;
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n;
        if (sh[c] || (near && !left[c] && !(x > 0 && left[c - 1]) && !(x < n - 1 && left[c + 1]) &&
                      !(y > 0 && left[c - n]) && !(y < n - 1 && left[c + n])))
            continue;
        const int nb[4] = {y > 0 ? c - n : -1, x > 0 ? c - 1 : -1, x < n - 1 ? c + 1 : -1, y < n - 1 ? c + n : -1};
        for (int j = 0; j < 4; j++)
            if (nb[j] >= 0 && m->region[nb[j]] != m->region[c]) moves[nm++] = c * 4 + j;
    }
    if (!nm) return 0;
    int mv = moves[pr_below(s, nm)], c = mv / 4, j = mv % 4;
    return pass_cell(m, c, j == 0 ? c - n : j == 1 ? c - 1 : j == 2 ? c + 1 : c + n);
}

#define MAKE_BUDGET 40000L                      /* the solver's work a puzzle may cost (about a second at most) */

/* a puzzle made: pieces and regions by chance, then unshaded cells passed between regions while the reasoning gets
 * further, until it finishes at the level; the nearest level when that one is not found */
void lts_make_ex(int n, int level, uint32_t seed, long budget, lts_puzzle *p, uint8_t *sol)
{
    if (n < 5 || n > LTS_MAX) n = 6;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    static lts_puzzle best, q;
    uint8_t sh[LTS_CELLS];
    int best_lv = -2;
    memset(&best, 0, sizeof best);
    for (int attempt = 0; attempt < 200 && work < 3 * budget; attempt++) {
        int want = n * n * 10 / (60 + pr_below(&s, 15));
        if (lay(&q, n, want, sh, &s) < want * 3 / 4) continue;
        /* the climb: moves kept while the level's reasoning gets as far or further, until it solves it */
        int und[LTS_CELLS], nu, done, stall = 0, got = progress(&q, &work, level, &done, und, &nu);
        for (int step = 0; step < 150 * n && !done && stall <= 25 * n && work <= 2 * budget; step++) {
            if (pr_below(&s, 100) < 15) {       /* the other shading where the regions allow two */
                switch_piece(&q, sh, nu ? und[pr_below(&s, nu)] : pr_below(&s, n * n), &s);
                continue;
            }
            lts_puzzle m = q;
            if (!move(&m, sh, und, nu, &s)) continue;
            int u2[LTS_CELLS], n2, done2, got2 = progress(&m, &work, level, &done2, u2, &n2);
            stall = got2 > got ? 0 : stall + 1;
            if (got2 >= got || done2) {
                q = m;
                got = got2;
                done = done2;
                nu = n2;
                memcpy(und, u2, sizeof u2);
            }
        }
        int lv = BENCH_GUESS;                   /* its grade (one near the end graded further) */
        if (done) lv = grade_work(&q, NULL, &work, level, GRADE_BUDGET, NULL);
        else if (got >= n * n * 9 / 10) lv = grade_work(&q, NULL, &work, BENCH_HARD, GRADE_BUDGET, NULL);
        /* too easy: moves kept while the level still solves it and the level below needs as many trials or more,
         * until that one no longer solves it */
        if (lv >= BENCH_EASY && lv < level) {
            int low = level - 1, ul[LTS_CELLS], nl, dl;
            finds = 0;
            progress(&q, &work, low, &dl, ul, &nl);
            long fl = finds;
            for (int step = 0; step < 60 * n && dl && work <= 3 * budget; step++) {
                lts_puzzle m = q;
                if (!move(&m, sh, NULL, 0, &s)) continue;
                int u2[LTS_CELLS], n2, d2, d3;
                finds = 0;
                progress(&m, &work, low, &d2, u2, &n2);
                long f2 = finds;
                if (d2 && f2 < fl) continue;
                progress(&m, &work, level, &d3, u2, &n2);
                if (!d3) continue;
                q = m;
                dl = d2;
                fl = f2;
            }
            if (!dl) lv = level;
        }
        /* the best so far: its level, else the nearest below, else the nearest above */
        int rank = lv < BENCH_EASY || lv > BENCH_HARD ? -9 : lv <= level ? lv - level : level - lv - 3;
        int best_rank = best_lv < BENCH_EASY ? -9 : best_lv <= level ? best_lv - level : level - best_lv - 3;
        if (rank > best_rank) {
            best_lv = lv;
            best = q;
        }
        if (best_lv == level) break;
    }
    if (best_lv < BENCH_EASY) {                 /* none found in time: none (lts_new takes a level instead) */
        memset(p, 0, sizeof *p);
        if (sol) memset(sol, 0, LTS_CELLS);
        return;
    }
    relabel(&best);
    *p = best;
    if (sol) grade_work(p, sol, NULL, BENCH_HARD, 0, NULL);
}

void lts_make(int n, int level, uint32_t seed, lts_puzzle *p, uint8_t *sol)
{
    lts_make_ex(n, level, seed, MAKE_BUDGET, p, sol);
}

/* a level of the size, turned or mirrored by chance (the seed), from the levels of its third of that size's run
 * (they go from the easiest): when the maker found none in time. 0: no level of that size */
static int from_level(int n, int level, uint32_t seed, lts_puzzle *p)
{
    int at[LTS_LEVELS], na = 0;
    for (int l = 0; l < LTS_LEVELS; l++)
        if (atoi(lts_levels[l]) == n) at[na++] = l;
    if (!na) return 0;
    uint32_t s = pr_seed(seed ^ 0x9e3779b9u);
    int lo = na * level / 3, hi = na * (level + 1) / 3, t = pr_below(&s, 8);
    lts_puzzle q;
    if (hi <= lo || !lts_decode(lts_levels[at[lo + pr_below(&s, hi - lo)]], &q)) return 0;
    *p = q;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int sx = t & 1 ? n - 1 - x : x, sy = t & 2 ? n - 1 - y : y;
            if (t & 4) { int u = sx; sx = sy; sy = u; }
            p->region[y * n + x] = q.region[sy * n + sx];
        }
    return relabel(p);
}

/* ---- the player's grid ------------------------------------------------------------------------------------------- */
int lts_load(lts_game *g, const lts_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = lts_grade(p, g->sol);
    return g->level >= BENCH_EASY && g->level <= BENCH_HARD;
}

void lts_new(lts_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= LTS_VARIANTS) variant = 0;
    lts_puzzle p;
    uint8_t sol[LTS_CELLS];
    lts_make(lts_size(variant), lts_level(variant), seed, &p, sol);
    if (p.n != lts_size(variant) && from_level(lts_size(variant), lts_level(variant), seed, &p)) lts_grade(&p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = lts_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
}

int lts_free(const lts_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n; }
int lts_tap_state(const lts_game *g, int c) { return g->st[c] == LTS_SHADE ? LTS_EMPTY : LTS_SHADE; }
int lts_hold_state(const lts_game *g, int c) { return g->st[c] == LTS_DOT ? LTS_EMPTY : LTS_DOT; }

static int wrongs(const lts_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++) k += g->st[c] != LTS_EMPTY && (g->st[c] == LTS_SHADE) != g->sol[c];
    return k;
}

int lts_paint(lts_game *g, const int *cells, int n, int state)
{
    bench_begin(&g->hist, n);
    for (int i = 0; i < n; i++)
        if (lts_free(g, cells[i])) bench_set(&g->hist, g->st, cells[i], state);
    return bench_done(&g->hist, wrongs(g));
}

static int put(lts_game *g, int c, int state) { return lts_paint(g, &c, 1, state); }

static int hint_step(lts_game *g)
{
    int cells = g->p.n * g->p.n;
    for (int c = 0; c < cells; c++)             /* a wrong cell first */
        if (g->st[c] != LTS_EMPTY && (g->st[c] == LTS_SHADE) != g->sol[c])
            return put(g, c, g->sol[c] ? LTS_SHADE : LTS_DOT) ? c : -1;
    static ctx k;
    ctx_make(&k, &g->p);
    bench_rules r = rules_of(&k, BENCH_HARD, NULL);
    bench_grid d;
    grid_of(&k, &d);
    for (int c = 0; c < cells; c++)
        if (g->st[c] != LTS_EMPTY) d.d[c] = (uint16_t)(g->sol[c] ? SHADE : WHITE);
    int c = bench_next(&d, &r);
    if (c >= 0 && g->st[c] != LTS_EMPTY) c = -1;
    for (int i = 0; i < cells && c < 0; i++)
        if (g->st[i] == LTS_EMPTY && g->sol[i]) c = i;
    for (int i = 0; i < cells && c < 0; i++)
        if (g->st[i] == LTS_EMPTY) c = i;
    if (c < 0) return -1;
    return put(g, c, g->sol[c] ? LTS_SHADE : LTS_DOT) ? c : -1;
}

int lts_hint(lts_game *g) { return bench_hinted(&g->hist, hint_step(g)); }

int lts_undo(lts_game *g)
{
    int c = bench_undo_cells(&g->hist, g->st);
    bench_undone(&g->hist, wrongs(g));
    return c;
}

int lts_piece(const lts_game *g, int r)
{
    int cs[4], m = 0, n = g->p.n;
    for (int c = 0; c < n * n; c++)
        if (g->p.region[c] == r && g->st[c] == LTS_SHADE) {
            if (m == 4) return -1;
            cs[m++] = c;
        }
    return m == 4 ? lts_shape(cs, n) : -1;
}

int lts_square(const lts_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n;
    if (g->st[c] != LTS_SHADE) return 0;
    for (int dy = -1; dy <= 0; dy++)
        for (int dx = -1; dx <= 0; dx++) {
            int x0 = x + dx, y0 = y + dy, d = y0 * n + x0;
            if (x0 >= 0 && y0 >= 0 && x0 + 1 < n && y0 + 1 < n && g->st[d] == LTS_SHADE &&
                g->st[d + 1] == LTS_SHADE && g->st[d + n] == LTS_SHADE && g->st[d + n + 1] == LTS_SHADE)
                return 1;
        }
    return 0;
}

int lts_over(const lts_game *g, int r)
{
    int s = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++) s += g->p.region[c] == r && g->st[c] == LTS_SHADE;
    return s > 4;
}

int lts_twins(const lts_game *g, int r)
{
    int n = g->p.n, shape = lts_piece(g, r);
    if (shape < 0) return 0;
    for (int c = 0; c < n * n; c++) {
        if (g->p.region[c] != r || g->st[c] != LTS_SHADE) continue;
        int x = c % n, y = c / n;
        const int nb[4] = {y > 0 ? c - n : -1, x > 0 ? c - 1 : -1, x < n - 1 ? c + 1 : -1, y < n - 1 ? c + n : -1};
        for (int j = 0; j < 4; j++)
            if (nb[j] >= 0 && g->st[nb[j]] == LTS_SHADE && g->p.region[nb[j]] != r &&
                lts_piece(g, g->p.region[nb[j]]) == shape)
                return 1;
    }
    return 0;
}

int lts_solved(const lts_game *g)
{
    static ctx k;
    ctx_make(&k, &g->p);
    uint8_t sh[LTS_CELLS];
    for (int c = 0; c < k.cells; c++) sh[c] = g->st[c] == LTS_SHADE;
    return valid_cells(&k, sh);
}

int lts_script(lts_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (lts_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if ((g->st[c] == LTS_SHADE) != g->sol[c]) put(g, c, g->sol[c] ? LTS_SHADE : LTS_EMPTY), k--;
            continue;
        }
        int hold = *s == '.', len = 0;
        if (hold) s++;
        int a = bench_cell(s, n, n, &len);
        if (a < 0) return 0;
        s += len;
        int b = a;
        if (*s == '-') {
            b = bench_cell(s + 1, n, n, &len);
            if (b < 0 || (b % n != a % n && b / n != a / n)) return 0;
            s += 1 + len;
        }
        int was = g->st[a], to = hold ? lts_hold_state(g, a) : lts_tap_state(g, a), cells[LTS_MAX], k = 0;
        int dx = (b % n > a % n) - (b % n < a % n), dy = (b / n > a / n) - (b / n < a / n);
        for (int c = a;; c += dy * n + dx) {
            if (g->st[c] == was) cells[k++] = c;
            if (c == b) break;
        }
        if (!lts_paint(g, cells, k, to)) return 0;
    }
    return 1;
}
