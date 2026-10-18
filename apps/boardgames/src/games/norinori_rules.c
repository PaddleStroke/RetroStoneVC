/*
 * Norinori: the rules (norinori.h): the ways of reasoning the solver grades with, the maker, the player's grid and
 * undo. No SDL: tested by tests/norinori_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/norinori.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/norinori_levels.h"

#define WHITE 1u                                /* a cell's values: bit 0 unshaded, bit 1 shaded */
#define SHADE 2u
#define BOTH 3u

static const int SIZES[NRN_SIZES] = {6, 8, 10};
static const char CODE[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
#define MAX_REGIONS 62

int nrn_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % NRN_SIZES]; }
int nrn_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

/* ---- what the solver knows of a grid: the neighbours, the regions' cells ----------------------------------------- */
typedef struct {
    const nrn_puzzle *p;
    int n, cells, regions, passes, from;           /* passes: a trial's, at the level graded */
    long *work;                                 /* the solver's work counted (each trial's passes too) */
    uint8_t nn[NRN_CELLS], nb[NRN_CELLS][4];
    uint8_t rcell[NRN_CELLS];                   /* the cells region by region: rstart[r] .. rstart[r + 1] - 1 */
    int16_t rstart[NRN_CELLS + 1];
    uint8_t hcell[5 * NRN_CELLS];               /* each region's cells and those round it: hstart[r] .. */
    int16_t hstart[NRN_CELLS + 1];
} ctx;

static void ctx_make(ctx *k, const nrn_puzzle *p)
{
    int n = p->n;
    memset(k, 0, sizeof *k);
    k->p = p;
    k->n = n;
    k->cells = n * n;
    k->regions = p->regions;
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n;
        if (y > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - n);
        if (x > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - 1);
        if (x < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + 1);
        if (y < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + n);
    }
    int at = 0;
    for (int r = 0; r < p->regions; r++) {
        k->rstart[r] = (int16_t)at;
        for (int c = 0; c < n * n; c++)
            if (p->region[c] == r) k->rcell[at++] = (uint8_t)c;
    }
    k->rstart[p->regions] = (int16_t)at;
    at = 0;
    for (int r = 0; r < p->regions; r++) {
        k->hstart[r] = (int16_t)at;
        for (int c = 0; c < n * n; c++) {
            int in = p->region[c] == r;
            for (int j = 0; j < k->nn[c] && !in; j++) in = p->region[k->nb[c][j]] == r;
            if (in) k->hcell[at++] = (uint8_t)c;
        }
    }
    k->hstart[p->regions] = (int16_t)at;
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

/* the shaded cells of a part of t (the cells cell[0..n-1]) each with one partner at most, and one still possible */
static int partners_ok(const uint16_t *t, const ctx *k, const uint8_t *cell, int n)
{
    for (int i = 0; i < n; i++) {
        int c = cell[i], s = 0, u = 0;
        if (t[c] != SHADE) continue;
        for (int j = 0; j < k->nn[c]; j++) {
            s += t[k->nb[c][j]] == SHADE;
            u += t[k->nb[c][j]] == BOTH;
        }
        if (s > 1 || (!s && !u)) return 0;
    }
    return 1;
}

/* a region's ways to have its two shaded cells: each choice of its unknown cells to shade, the rest of it unshaded,
 * that leaves every shaded cell in and round it with one partner at most and one still possible. Cells shaded in
 * every way are shaded, in none unshaded; -1: no way. t: a copy of w, as it was again at the end */
static int region_ways(bench_grid *g, const uint16_t *w, uint16_t *t, const ctx *k, int r)
{
    int un[NRN_CELLS], nu = 0, s = 0, on[NRN_CELLS], ways = 0;
    for (int i = k->rstart[r]; i < k->rstart[r + 1]; i++) {
        int c = k->rcell[i];
        s += w[c] == SHADE;
        if (w[c] == BOTH) on[nu] = 0, un[nu++] = c;
    }
    if (s > 2 || s + nu < 2) return -1;
    if (!nu) return 0;
    int need = 2 - s, nh = 0;
    uint8_t hc[NRN_CELLS + 2];                  /* the cells to check: the shaded ones in and round it, the picks */
    for (int i = k->hstart[r]; i < k->hstart[r + 1]; i++)
        if (w[k->hcell[i]] == SHADE) hc[nh++] = k->hcell[i];
    for (int a = 0; a < (need ? nu : 1); a++)
        for (int b = need == 2 ? a + 1 : a; b < (need == 2 ? nu : a + 1); b++) {
            for (int i = 0; i < nu; i++) t[un[i]] = (uint16_t)(need && (i == a || i == b) ? SHADE : WHITE);
            int m = nh;
            if (need) hc[m++] = (uint8_t)un[a];
            if (need == 2) hc[m++] = (uint8_t)un[b];
            int ok = partners_ok(t, k, hc, m);
            for (int i = 0; i < nu; i++) t[un[i]] = BOTH;
            if (!ok) continue;
            ways++;
            if (need) on[a]++;
            if (need == 2) on[b]++;
        }
    if (!ways) return -1;
    for (int i = 0; i < nu; i++)
        if (on[i] == ways || !on[i]) force(g, un[i], on[i] ? SHADE : WHITE);
    return 0;
}

/* the plain ways: a shaded cell with its partner has nothing else shaded round it, one with a single way left takes
 * it; a cell beside two shaded ones, or with no neighbour that could be shaded, stays unshaded; the regions' ways */
static int plain_some(bench_grid *g, const ctx *k, const uint8_t *cm, const uint8_t *rm)
{
    uint16_t w[NRN_CELLS], t[NRN_CELLS];
    memcpy(w, g->d, (size_t)k->cells * sizeof w[0]);
    memcpy(t, w, sizeof t);
    for (int c = 0; c < k->cells; c++) {
        if (cm && !cm[c]) continue;
        int s = 0, u = 0, last = -1;
        for (int j = 0; j < k->nn[c]; j++) {
            int o = k->nb[c][j];
            if (w[o] == SHADE) s++;
            else if (w[o] == BOTH) u++, last = o;
        }
        if (w[c] == SHADE) {
            if (s > 1 || (!s && !u)) return -1;
            if (s == 1) {
                for (int j = 0; j < k->nn[c]; j++)
                    if (w[k->nb[c][j]] == BOTH) force(g, k->nb[c][j], WHITE);
            } else if (u == 1) {
                force(g, last, SHADE);
            }
        } else if (w[c] == BOTH && (s >= 2 || (!s && !u))) {
            force(g, c, WHITE);
        }
    }
    for (int r = 0; r < k->regions; r++)
        if ((!rm || rm[r]) && region_ways(g, w, t, k, r) < 0) return -1;
    return settled(w, g, k->cells);
}

static int plain(bench_grid *g, const ctx *k) { return plain_some(g, k, NULL, NULL); }

/* a change at c followed through the plain ways `passes` times, each pass only where the last one changed something
 * (the rest was settled before): -1 a contradiction, 0 it settles, 1 still going */
static int follow(bench_grid *t, const ctx *k, int c, int passes)
{
    uint8_t dirty[NRN_CELLS], cm[NRN_CELLS], rm[NRN_CELLS];
    uint16_t was[NRN_CELLS];
    memset(dirty, 0, (size_t)k->cells);
    dirty[c] = 1;
    for (int pass = 0; pass < passes; pass++) {
        memset(cm, 0, (size_t)k->cells);
        memset(rm, 0, (size_t)k->regions);
        for (int x = 0; x < k->cells; x++) {
            if (!dirty[x]) continue;
            cm[x] = 1;
            rm[k->p->region[x]] = 1;
            for (int j = 0; j < k->nn[x]; j++) {
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
 * out (one pass: what a glance shows; a few: a short chain). One find at a time: the plain ways come first again */
static int trials(bench_grid *g, ctx *k, int passes)
{
    uint16_t w[NRN_CELLS];
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
        if (g->d[c] != w[c] && ++found >= 4) {   /* a few finds, then the plain ways first again */
            k->from = c;
            break;
        }
    }
    return settled(w, g, k->cells);
}

static long finds;                              /* the trials' finds (nrn_effort counts them) */

static int deduce(bench_grid *g, int tier, void *cv)
{
    ctx *k = cv;
    int res = tier ? trials(g, k, k->passes) : plain(g, k);
    if (res > 0 && tier) finds++;
    return res;
}

static int valid(const bench_grid *g, void *cv)
{
    const ctx *k = cv;
    for (int c = 0; c < k->cells; c++) {
        if (g->d[c] != SHADE) continue;
        int s = 0;
        for (int j = 0; j < k->nn[c]; j++) s += g->d[k->nb[c][j]] == SHADE;
        if (s != 1) return 0;
    }
    for (int r = 0; r < k->regions; r++) {
        int s = 0;
        for (int i = k->rstart[r]; i < k->rstart[r + 1]; i++) s += g->d[k->rcell[i]] == SHADE;
        if (s != 2) return 0;
    }
    return 1;
}

/* the reasoning of a level: easy, the plain ways and trials of one pass; medium, trials of three; hard, long chains
 * too (bench_probe: a trial followed by the plain ways to the end) */
static bench_rules rules_of(ctx *k, int level, long *work)
{
    k->passes = level == BENCH_EASY ? 1 : level == BENCH_MEDIUM ? 3 : 8;
    k->work = work;
    return (bench_rules){.vars = k->cells, .tiers = level < 0 ? 1 : 2, .probe_tiers = 1,
                        .deduce = deduce, .valid = valid, .ctx = k, .work = work};
}

/* the grid g reasoned at a level: BENCH_EASY (solved), BENCH_GUESS (stuck), BENCH_NONE (no solution) */
static int run(const nrn_puzzle *p, int level, bench_grid *g, long *work, long budget, long *passes)
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
static int grade_work(const nrn_puzzle *p, uint8_t *sol, long *work, int cap, long budget, long *passes)
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

int nrn_grade(const nrn_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, BENCH_HARD, 0, NULL); }

/* the solutions counted (the plain ways between the guesses); the first in sol (may be NULL), the values tried in a
 * random order when rng is given; past budget passes the count so far */
static int search(const nrn_puzzle *p, int limit, uint8_t *sol, uint32_t *rng, long budget)
{
    static ctx k;
    ctx_make(&k, p);
    long work = 0;
    bench_rules r = rules_of(&k, BENCH_EASY, &work);
    r.tiers = 1;
    bench_grid g, s;
    grid_of(&k, &g);
    int got = bench_search(&g, &r, limit, &s, rng, budget);
    if (got && sol)
        for (int c = 0; c < k.cells; c++) sol[c] = s.d[c] == SHADE;
    return got;
}

int nrn_count(const nrn_puzzle *p, int limit) { return search(p, limit, NULL, NULL, 0); }

int nrn_effort(const nrn_puzzle *p, long *passes, long *mediums)
{
    int lv = grade_work(p, NULL, NULL, BENCH_HARD, 0, passes);
    if (mediums) *mediums = finds;
    return lv;
}

/* ---- a puzzle as text (the levels) ------------------------------------------------------------------------------- */
/* the regions numbered again in the order of their first cells; 0 when one is in two pieces (or too many) */
static int relabel(nrn_puzzle *p)
{
    int n = p->n, map[256], next = 0;
    for (int r = 0; r < 256; r++) map[r] = -1;
    for (int c = 0; c < n * n; c++)
        if (map[p->region[c]] < 0) map[p->region[c]] = next++;
    for (int c = 0; c < n * n; c++) p->region[c] = (uint8_t)map[p->region[c]];
    p->regions = next;
    if (next > MAX_REGIONS) return 0;
    for (int r = 0; r < next; r++) {            /* each in one piece */
        int stack[NRN_CELLS], sp = 0, got = 0, size = 0, first = -1;
        uint8_t seen[NRN_CELLS];
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
        if (got != size) return 0;
    }
    return 1;
}

int nrn_encode(const nrn_puzzle *p, char *out, int size)
{
    int n = p->n, len = snprintf(out, (size_t)size, "%d:", n);
    if (len < 0 || len + n * n + 1 > size || p->regions > MAX_REGIONS) return 0;
    for (int c = 0; c < n * n; c++) out[len++] = CODE[p->region[c]];
    out[len] = 0;
    return len;
}

int nrn_decode(const char *s, nrn_puzzle *p)
{
    memset(p, 0, sizeof *p);
    int n = 0;
    for (; *s >= '0' && *s <= '9'; s++) n = n * 10 + *s - '0';
    if (*s++ != ':' || n < 2 || n > NRN_MAX || (int)strlen(s) != n * n) return 0;
    p->n = n;
    for (int c = 0; c < n * n; c++) {
        const char *d = strchr(CODE, s[c]);
        if (!d || !s[c]) return 0;
        p->region[c] = (uint8_t)(d - CODE);
    }
    return relabel(p);
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
/* regions grown by chance from spread seeds, each new cell to its smallest neighbour mostly; the ones under 3 cells
 * merged into a neighbour */
static void grow(nrn_puzzle *p, int n, int avg10, uint32_t *s)
{
    memset(p, 0, sizeof *p);
    p->n = n;
    int cells = n * n, R = (cells * 10 + avg10 / 2) / avg10, size[NRN_CELLS];
    if (R > MAX_REGIONS - 2) R = MAX_REGIONS - 2;
    memset(p->region, 0xff, sizeof p->region);
    for (int r = 0; r < R; r++) {
        for (int t = 0; t < 50; t++) {          /* a seed not beside another */
            int c = pr_below(s, cells), x = c % n, y = c / n, ok = p->region[c] == 0xff;
            for (int dy = -1; dy <= 1 && ok; dy++)
                for (int dx = -1; dx <= 1 && ok; dx++) {
                    int xx = x + dx, yy = y + dy;
                    if (xx >= 0 && yy >= 0 && xx < n && yy < n && p->region[yy * n + xx] != 0xff && t < 40) ok = 0;
                }
            if (ok) {
                p->region[c] = (uint8_t)r;
                break;
            }
        }
        size[r] = 0;
    }
    for (int c = 0; c < cells; c++)
        if (p->region[c] != 0xff) size[p->region[c]]++;
    for (;;) {
        int open[NRN_CELLS], no = 0;
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
        if (pr_below(s, 4) == 0) best = cand[pr_below(s, nc)];
        p->region[c] = (uint8_t)best;
        size[best]++;
    }
    for (int r = 0; r < R; r++) {               /* the small ones merged */
        if (!size[r] || size[r] >= 3) continue;
        int to = -1;
        for (int c = 0; c < cells; c++) {
            if (p->region[c] != r) continue;
            int x = c % n, y = c / n;
            const int nb[4] = {y > 0 ? c - n : -1, x > 0 ? c - 1 : -1, x < n - 1 ? c + 1 : -1, y < n - 1 ? c + n : -1};
            for (int j = 0; j < 4; j++)
                if (nb[j] >= 0 && p->region[nb[j]] != r && (to < 0 || size[p->region[nb[j]]] < size[to]))
                    to = p->region[nb[j]];
        }
        if (to < 0) continue;
        for (int c = 0; c < cells; c++)
            if (p->region[c] == r) p->region[c] = (uint8_t)to;
        size[to] += size[r];
        size[r] = 0;
    }
    relabel(p);
}

/* how far the reasoning of a level gets (cells decided; cap -1: the plain ways alone): *done 1 when it solves it,
 * und the cells left (*nu) */
#define GRADE_BUDGET 4000L
static int progress(const nrn_puzzle *p, long *work, int cap, int *done, int *und, int *nu)
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

/* the unshaded cell c passed to the region of its neighbour d: 1 when its own region stays in one piece of 3 or more */
static int pass_cell(nrn_puzzle *p, int c, int d)
{
    int n = p->n, from = p->region[c], size = 0;
    if (from == p->region[d]) return 0;
    for (int i = 0; i < n * n; i++) size += p->region[i] == from;
    if (size <= 3) return 0;
    p->region[c] = p->region[d];
    int stack[NRN_CELLS], sp = 0, got = 0, first = -1;
    uint8_t seen[NRN_CELLS];
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

/* the domino at a (with its partner) slid one cell (dir: 0 up, 1 left, 2 right, 3 down) in the shading sh: 1 when the
 * cells it leaves and takes lie in the same regions and the shading still keeps the rules (the puzzle as it is then
 * has both: the maker's next moves may tell them apart) */
static int slide(const nrn_puzzle *p, uint8_t *sh, int a, int dir)
{
    int n = p->n, x = a % n, y = a / n, b = -1;
    if (!sh[a]) return 0;
    if (x < n - 1 && sh[a + 1]) b = a + 1;
    else if (x > 0 && sh[a - 1]) b = a - 1;
    else if (y < n - 1 && sh[a + n]) b = a + n;
    else if (y > 0 && sh[a - n]) b = a - n;
    if (b < 0) return 0;
    int dx = dir == 1 ? -1 : dir == 2 ? 1 : 0, dy = dir == 0 ? -1 : dir == 3 ? 1 : 0;
    int ax = x + dx, ay = y + dy, bx = b % n + dx, by = b / n + dy;
    if (ax < 0 || ay < 0 || ax >= n || ay >= n || bx < 0 || by < 0 || bx >= n || by >= n) return 0;
    int a2 = ay * n + ax, b2 = by * n + bx, ra = p->region[a], rb = p->region[b];
    int r2a = p->region[a2], r2b = p->region[b2];
    if (!((ra == r2a && rb == r2b) || (ra == r2b && rb == r2a))) return 0;
    uint8_t t[NRN_CELLS];
    memcpy(t, sh, (size_t)n * n);
    t[a] = t[b] = 0;
    if (t[a2] || t[b2]) return 0;
    t[a2] = t[b2] = 1;
    for (int c = 0; c < n * n; c++) {
        if (!t[c]) continue;
        int cx = c % n, cy = c / n;
        if ((cx > 0 && t[c - 1]) + (cx < n - 1 && t[c + 1]) + (cy > 0 && t[c - n]) + (cy < n - 1 && t[c + n]) != 1)
            return 0;
    }
    memcpy(sh, t, (size_t)n * n);
    return 1;
}

/* a move of the maker in m: an unshaded cell (of the shading sh) on a wall passed to its neighbour's region, mostly
 * near the cells und[0..nu-1] the reasoning left; 1 when one was made */
static int move(nrn_puzzle *m, const uint8_t *sh, const int *und, int nu, uint32_t *s)
{
    int n = m->n, moves[4 * NRN_CELLS], nm = 0, near = nu > 0 && pr_below(s, 100) < 85;
    uint8_t left[NRN_CELLS];
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

#define NRN_AVG 40                              /* the regions' cells on average, in tenths (and up to 1 more) */
#define MAKE_BUDGET 40000L                      /* the solver's work a puzzle may cost (about a second at most) */

/* a puzzle made: regions by chance, a shading found for them, then unshaded cells passed between regions while the
 * reasoning gets further, until it finishes at the level (or below); the nearest level when that one is not found */
void nrn_make_ex(int n, int level, uint32_t seed, long budget, nrn_puzzle *p, uint8_t *sol)
{
    if (n < 4 || n > NRN_MAX) n = 6;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    static nrn_puzzle best, q;
    uint8_t sh[NRN_CELLS];
    int best_lv = -2;
    memset(&best, 0, sizeof best);
    for (int attempt = 0; attempt < 200 && work < 3 * budget; attempt++) {
        grow(&q, n, NRN_AVG + pr_below(&s, 10), &s);
        uint32_t rs = pr_next(&s);
        if (search(&q, 1, sh, &rs, 3000) != 1) continue;
        /* the climb: moves kept while the level's reasoning gets as far or further, until it solves it */
        int und[NRN_CELLS], nu, done, stall = 0, got = progress(&q, &work, level, &done, und, &nu);
        for (int step = 0; step < 150 * n && !done && stall <= 25 * n && work <= 2 * budget; step++) {
            if (pr_below(&s, 100) < 20) {       /* the other shading where the regions allow two */
                slide(&q, sh, nu ? und[pr_below(&s, nu)] : pr_below(&s, n * n), pr_below(&s, 4));
                continue;
            }
            nrn_puzzle m = q;
            if (!move(&m, sh, und, nu, &s)) continue;
            int u2[NRN_CELLS], n2, done2, got2 = progress(&m, &work, level, &done2, u2, &n2);
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
            int low = level - 1, ul[NRN_CELLS], nl, dl;
            finds = 0;
            progress(&q, &work, low, &dl, ul, &nl);
            long fl = finds;
            for (int step = 0; step < 60 * n && dl && work <= 3 * budget; step++) {
                nrn_puzzle m = q;
                if (!move(&m, sh, NULL, 0, &s)) continue;
                int u2[NRN_CELLS], n2, d2, d3;
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
    if (best_lv < BENCH_EASY) {                 /* none found in time: none (nrn_new takes a level instead) */
        memset(p, 0, sizeof *p);
        if (sol) memset(sol, 0, NRN_CELLS);
        return;
    }
    relabel(&best);
    *p = best;
    if (sol) grade_work(p, sol, NULL, BENCH_HARD, 0, NULL);
}

void nrn_make(int n, int level, uint32_t seed, nrn_puzzle *p, uint8_t *sol)
{
    nrn_make_ex(n, level, seed, MAKE_BUDGET, p, sol);
}

/* a level of the size, turned or mirrored by chance (the seed), from the levels of its third of that size's run
 * (they go from the easiest): when the maker found none in time. 0: no level of that size */
static int from_level(int n, int level, uint32_t seed, nrn_puzzle *p)
{
    int at[NRN_LEVELS], na = 0;
    for (int l = 0; l < NRN_LEVELS; l++)
        if (atoi(nrn_levels[l]) == n) at[na++] = l;
    if (!na) return 0;
    uint32_t s = pr_seed(seed ^ 0x9e3779b9u);
    int lo = na * level / 3, hi = na * (level + 1) / 3, t = pr_below(&s, 8);
    nrn_puzzle q;
    if (hi <= lo || !nrn_decode(nrn_levels[at[lo + pr_below(&s, hi - lo)]], &q)) return 0;
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
int nrn_load(nrn_game *g, const nrn_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = nrn_grade(p, g->sol);
    return g->level >= BENCH_EASY && g->level <= BENCH_HARD;
}

void nrn_new(nrn_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= NRN_VARIANTS) variant = 0;
    nrn_puzzle p;
    uint8_t sol[NRN_CELLS];
    nrn_make(nrn_size(variant), nrn_level(variant), seed, &p, sol);
    if (p.n != nrn_size(variant) && from_level(nrn_size(variant), nrn_level(variant), seed, &p)) nrn_grade(&p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = nrn_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
}

int nrn_free(const nrn_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n; }
int nrn_tap_state(const nrn_game *g, int c) { return g->st[c] == NRN_SHADE ? NRN_EMPTY : NRN_SHADE; }
int nrn_hold_state(const nrn_game *g, int c) { return g->st[c] == NRN_DOT ? NRN_EMPTY : NRN_DOT; }

static int wrongs(const nrn_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++) k += g->st[c] != NRN_EMPTY && (g->st[c] == NRN_SHADE) != g->sol[c];
    return k;
}

int nrn_paint(nrn_game *g, const int *cells, int n, int state)
{
    bench_begin(&g->hist, n);
    for (int i = 0; i < n; i++)
        if (nrn_free(g, cells[i])) bench_set(&g->hist, g->st, cells[i], state);
    return bench_done(&g->hist, wrongs(g));
}

static int put(nrn_game *g, int c, int state) { return nrn_paint(g, &c, 1, state); }

static int hint_step(nrn_game *g)
{
    int cells = g->p.n * g->p.n;
    for (int c = 0; c < cells; c++)             /* a wrong cell first */
        if (g->st[c] != NRN_EMPTY && (g->st[c] == NRN_SHADE) != g->sol[c])
            return put(g, c, g->sol[c] ? NRN_SHADE : NRN_DOT) ? c : -1;
    static ctx k;
    ctx_make(&k, &g->p);
    bench_rules r = rules_of(&k, BENCH_MEDIUM, NULL);
    bench_grid d;
    grid_of(&k, &d);
    for (int c = 0; c < cells; c++)
        if (g->st[c] != NRN_EMPTY) d.d[c] = (uint16_t)(g->sol[c] ? SHADE : WHITE);
    int c = bench_next(&d, &r);
    if (c >= 0 && g->st[c] != NRN_EMPTY) c = -1;
    for (int i = 0; i < cells && c < 0; i++)
        if (g->st[i] == NRN_EMPTY && g->sol[i]) c = i;
    for (int i = 0; i < cells && c < 0; i++)
        if (g->st[i] == NRN_EMPTY) c = i;
    if (c < 0) return -1;
    return put(g, c, g->sol[c] ? NRN_SHADE : NRN_DOT) ? c : -1;
}

int nrn_hint(nrn_game *g) { return bench_hinted(&g->hist, hint_step(g)); }

int nrn_undo(nrn_game *g)
{
    int c = bench_undo_cells(&g->hist, g->st);
    bench_undone(&g->hist, wrongs(g));
    return c;
}

static int shaded_nb(const nrn_game *g, int c, int *nb)   /* the shaded neighbours of c; how many */
{
    int n = g->p.n, x = c % n, y = c / n, k = 0;
    const int o[4] = {y > 0 ? c - n : -1, x > 0 ? c - 1 : -1, x < n - 1 ? c + 1 : -1, y < n - 1 ? c + n : -1};
    for (int j = 0; j < 4; j++)
        if (o[j] >= 0 && g->st[o[j]] == NRN_SHADE) {
            if (nb) nb[k] = o[j];
            k++;
        }
    return k;
}

int nrn_partner(const nrn_game *g, int c)
{
    int nb[4];
    if (g->st[c] != NRN_SHADE || shaded_nb(g, c, nb) != 1) return -1;
    return shaded_nb(g, nb[0], NULL) == 1 ? nb[0] : -1;
}

int nrn_crowded(const nrn_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n;
    if (g->st[c] != NRN_SHADE) return 0;
    int stack[NRN_CELLS], sp = 0, got = 0;     /* in a piece of three or more */
    uint8_t seen[NRN_CELLS];
    memset(seen, 0, sizeof seen);
    seen[c] = 1;
    stack[sp++] = c;
    while (sp) {
        int nb[4], k = shaded_nb(g, stack[--sp], nb);
        got++;
        for (int j = 0; j < k; j++)
            if (!seen[nb[j]]) seen[nb[j]] = 1, stack[sp++] = nb[j];
    }
    if (got != 1) return got >= 3;
    const int o[4] = {y > 0 ? c - n : -1, x > 0 ? c - 1 : -1, x < n - 1 ? c + 1 : -1, y < n - 1 ? c + n : -1};
    for (int j = 0; j < 4; j++)                 /* alone: any way out left? */
        if (o[j] >= 0 && g->st[o[j]] == NRN_EMPTY) return 0;
    return 1;
}

int nrn_region(const nrn_game *g, int r)
{
    int s = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++) s += g->p.region[c] == r && g->st[c] == NRN_SHADE;
    return s > 2 ? 1 : s == 2 ? 2 : 0;
}

int nrn_solved(const nrn_game *g)
{
    static ctx k;
    ctx_make(&k, &g->p);
    bench_grid d;
    memset(&d, 0, sizeof d);
    for (int c = 0; c < k.cells; c++) d.d[c] = (uint16_t)(g->st[c] == NRN_SHADE ? SHADE : WHITE);
    return valid(&d, &k);
}

int nrn_script(nrn_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (nrn_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if ((g->st[c] == NRN_SHADE) != g->sol[c]) put(g, c, g->sol[c] ? NRN_SHADE : NRN_EMPTY), k--;
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
        int was = g->st[a], to = hold ? nrn_hold_state(g, a) : nrn_tap_state(g, a), cells[NRN_MAX], k = 0;
        int dx = (b % n > a % n) - (b % n < a % n), dy = (b / n > a / n) - (b / n < a / n);
        for (int c = a;; c += dy * n + dx) {
            if (g->st[c] == was) cells[k++] = c;
            if (c == b) break;
        }
        if (!nrn_paint(g, cells, k, to)) return 0;
    }
    return 1;
}
