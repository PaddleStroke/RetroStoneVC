/*
 * Tapa: the rules (tapa.h): the ways of reasoning the solver grades with, the maker, the player's grid and undo. No
 * SDL: tested by tests/tapa_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/tapa.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/tapa_levels.h"

#define WHITE 1u                                /* a cell's values: bit 0 unshaded, bit 1 shaded */
#define SHADE 2u
#define BOTH 3u

static const int SIZES[TPA_SIZES] = {6, 8, 10};
static const int8_t RX[8] = {0, 1, 1, 1, 0, -1, -1, -1}, RY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};   /* round, from N */

int tpa_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % TPA_SIZES]; }
int tpa_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

/* the runs of a ring of eight (bit i: the i-th neighbour round from the north, shaded): how many, their lengths from
 * the biggest (none shaded: one run of 0, as a clue 0 reads) */
static int ring_runs(unsigned bits, uint8_t *len)
{
    int k = 0;
    if (bits == 0xff || !bits) {
        len[0] = bits ? 8 : 0;
        return 1;
    }
    int at = 0;
    while (bits >> at & 1) at++;                /* from an unshaded neighbour */
    for (int i = 1, run = 0; i <= 8; i++) {
        int on = bits >> ((at + i) % 8) & 1;
        if (on) run++;
        if ((!on || i == 8) && run) len[k++] = (uint8_t)run, run = 0;
    }
    for (int i = 0; i < k; i++)
        for (int j = i + 1; j < k; j++)
            if (len[j] > len[i]) {
                uint8_t t = len[i];
                len[i] = len[j];
                len[j] = t;
            }
    return k;
}

static int same_runs(unsigned bits, int count, const uint8_t *num)
{
    uint8_t len[4];
    int k = ring_runs(bits, len);
    return k == count && !memcmp(len, num, (size_t)k);
}

int tpa_runs(const uint8_t *sh, int n, int c, uint8_t *len)
{
    unsigned bits = 0;
    for (int i = 0; i < 8; i++) {
        int x = c % n + RX[i], y = c / n + RY[i];
        if (x >= 0 && y >= 0 && x < n && y < n && sh[y * n + x]) bits |= 1u << i;
    }
    return ring_runs(bits, len);
}

/* ---- what the solver knows of a puzzle: the neighbours, each clue's rings that fit it -------------------------- */
typedef struct {
    const tpa_puzzle *p;
    int n, cells, passes, from;                 /* passes: a trial's, at the level graded */
    long *work;                                 /* the solver's work counted (each trial's passes too) */
    uint8_t nn[TPA_CELLS], nb[TPA_CELLS][4];
    int16_t ring[TPA_CELLS][8];                 /* a cell's eight neighbours round from the north (-1: none) */
    uint8_t pat[TPA_CELLS * 64];                /* each clue's rings: pstart[c] .. pstart[c + 1] - 1 */
    int16_t pstart[TPA_CELLS + 1];
} ctx;

static void ctx_make(ctx *k, const tpa_puzzle *p)
{
    int n = p->n, np = 0;
    k->p = p;
    k->n = n;
    k->cells = n * n;
    k->passes = 1;
    k->from = 0;
    k->work = NULL;
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n;
        k->nn[c] = 0;
        if (y > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - n);
        if (x > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - 1);
        if (x < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + 1);
        if (y < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + n);
        unsigned open = 0;                      /* the ring's cells that could be shaded */
        for (int i = 0; i < 8; i++) {
            int xx = x + RX[i], yy = y + RY[i];
            k->ring[c][i] = (int16_t)(xx >= 0 && yy >= 0 && xx < n && yy < n ? yy * n + xx : -1);
            if (k->ring[c][i] >= 0 && p->count[k->ring[c][i]] < 0) open |= 1u << i;
        }
        k->pstart[c] = (int16_t)np;
        if (p->count[c] < 0) continue;
        for (unsigned b = 0; b < 256; b++)
            if (!(b & ~open) && same_runs(b, p->count[c], p->num[c]) && np < TPA_CELLS * 64) k->pat[np++] = (uint8_t)b;
    }
    k->pstart[n * n] = (int16_t)np;
}

static void grid_of(const ctx *k, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < k->cells; c++) g->d[c] = (uint16_t)(k->p->count[c] >= 0 ? WHITE : BOTH);
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

/* the wall's joins: the shaded cells all in one piece of cells not unshaded (-1: not), the cells cut off from them
 * unshaded, the cells they cannot do without shaded (the cut points between shaded cells, depth first) */
typedef struct {
    const ctx *k;
    const uint16_t *w;
    int disc[TPA_CELLS], low[TPA_CELLS], sub[TPA_CELLS], t, shaded;
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
    uint8_t cut[TPA_CELLS];
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

/* the plain ways: each clue's rings that fit what is known (cells shaded in all of them shaded, in none unshaded);
 * three shaded cells of a 2 x 2 leave the fourth; the wall's joins */
static int plain(bench_grid *g, const ctx *k)
{
    int n = k->n;
    uint16_t w[TPA_CELLS];
    memcpy(w, g->d, (size_t)k->cells * sizeof w[0]);
    for (int c = 0; c < k->cells; c++) {
        if (k->p->count[c] < 0) continue;
        unsigned on = 0, off = 0, all = 0xff, any = 0;
        int ways = 0;
        for (int i = 0; i < 8; i++) {
            int o = k->ring[c][i];
            if (o >= 0 && w[o] == SHADE) on |= 1u << i;
            if (o < 0 || w[o] == WHITE) off |= 1u << i;
        }
        for (int q = k->pstart[c]; q < k->pstart[c + 1]; q++) {
            unsigned b = k->pat[q];
            if ((b & off) || (on & ~b)) continue;
            ways++;
            all &= b;
            any |= b;
        }
        if (!ways) return -1;
        for (int i = 0; i < 8; i++) {
            int o = k->ring[c][i];
            if (o < 0 || w[o] != BOTH) continue;
            if (all >> i & 1) force(g, o, SHADE);
            else if (!(any >> i & 1)) force(g, o, WHITE);
        }
    }
    for (int y = 0; y + 1 < n; y++)             /* the 2 x 2 squares */
        for (int x = 0; x + 1 < n; x++) {
            int c = y * n + x, sq[4] = {c, c + 1, c + n, c + n + 1}, s = 0, u = -1, nu = 0;
            for (int i = 0; i < 4; i++) {
                s += w[sq[i]] == SHADE;
                if (w[sq[i]] == BOTH) u = sq[i], nu++;
            }
            if (s == 4) return -1;
            if (s == 3 && nu == 1) force(g, u, WHITE);
        }
    if (joins(g, w, k) < 0) return -1;
    return settled(w, g, k->cells);
}

/* trials: each unknown cell tried each way, the plain ways followed `passes` times; a contradiction rules that way
 * out (one pass: what a glance shows; a few: a short chain). A few finds, then the plain ways first again */
static int trials(bench_grid *g, ctx *k, int passes)
{
    uint16_t w[TPA_CELLS];
    bench_grid t;
    memcpy(w, g->d, (size_t)k->cells * sizeof w[0]);
    for (int i = 0, start = k->from, found = 0; i < k->cells; i++) {
        int c = (start + i) % k->cells;         /* from the last find on */
        if (w[c] != BOTH) continue;
        for (unsigned v = WHITE; v <= SHADE; v++) {
            memcpy(t.d, w, (size_t)k->cells * sizeof w[0]);
            t.d[c] = (uint16_t)v;
            int res = 0;
            for (int pass = 0; pass < passes; pass++) {
                if (k->work) (*k->work)++;
                if ((res = plain(&t, k)) <= 0) break;
            }
            if (res < 0) force(g, c, BOTH & ~v);
        }
        if (g->d[c] != w[c] && ++found >= 4) {
            k->from = c;
            break;
        }
    }
    return settled(w, g, k->cells);
}

static long finds;                              /* the trials' finds (tpa_effort counts them) */

static int deduce(bench_grid *g, int tier, void *cv)
{
    ctx *k = cv;
    int res = tier ? trials(g, k, k->passes) : plain(g, k);
    if (res > 0 && tier) finds++;
    return res;
}

/* a full shading sh keeps every rule: the clues, no 2 x 2, the wall in one piece */
static int valid_cells(const ctx *k, const uint8_t *sh)
{
    int n = k->n, first = -1, total = 0;
    for (int c = 0; c < k->cells; c++) {
        if (k->p->count[c] >= 0) {
            uint8_t len[4];
            if (sh[c]) return 0;
            int m = tpa_runs(sh, n, c, len);
            if (m != k->p->count[c] || memcmp(len, k->p->num[c], (size_t)m)) return 0;
        }
        if (!sh[c]) continue;
        total++;
        if (first < 0) first = c;
        int x = c % n, y = c / n;
        if (x + 1 < n && y + 1 < n && sh[c + 1] && sh[c + n] && sh[c + n + 1]) return 0;
    }
    if (!total) return 1;
    int stack[TPA_CELLS], sp = 0, got = 0;
    uint8_t seen[TPA_CELLS];
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
    uint8_t sh[TPA_CELLS];
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
static int run(const tpa_puzzle *p, int level, bench_grid *g, long *work, long budget, long *passes)
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
static int grade_work(const tpa_puzzle *p, uint8_t *sol, long *work, int cap, long budget, long *passes)
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

int tpa_grade(const tpa_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, BENCH_HARD, 0, NULL); }

int tpa_count(const tpa_puzzle *p, int limit)
{
    static ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, -1, NULL);
    bench_grid g;
    grid_of(&k, &g);
    return bench_search(&g, &r, limit, NULL, NULL, 0);
}

int tpa_effort(const tpa_puzzle *p, long *passes, long *trial_finds)
{
    int lv = grade_work(p, NULL, NULL, BENCH_HARD, 0, passes);
    if (trial_finds) *trial_finds = finds;
    return lv;
}

/* ---- a puzzle as text (the levels) ------------------------------------------------------------------------------- */
int tpa_encode(const tpa_puzzle *p, char *out, int size)
{
    int n = p->n, len = snprintf(out, (size_t)size, "%d:", n);
    if (len < 0) return 0;
    for (int c = 0; c < n * n; c++) {
        if (len + 7 > size) return 0;
        if (p->count[c] < 0) {
            out[len++] = '.';
            continue;
        }
        for (int i = 0; i < p->count[c]; i++) out[len++] = (char)('0' + p->num[c][i]);
        out[len++] = ',';
    }
    out[len] = 0;
    return len;
}

int tpa_decode(const char *s, tpa_puzzle *p)
{
    memset(p, 0, sizeof *p);
    int n = 0;
    for (; *s >= '0' && *s <= '9'; s++) n = n * 10 + *s - '0';
    if (*s++ != ':' || n < 3 || n > TPA_MAX) return 0;
    p->n = n;
    for (int c = 0; c < n * n; c++) {
        if (*s == '.') {
            p->count[c] = -1;
            s++;
            continue;
        }
        int k = 0;
        while (*s >= '0' && *s <= '8') {
            if (k == 4) return 0;
            p->num[c][k++] = (uint8_t)(*s++ - '0');
        }
        if (!k || *s++ != ',') return 0;
        for (int i = 1; i < k; i++)             /* from the biggest, no 0 among others, at most 8 cells */
            if (p->num[c][i] > p->num[c][i - 1] || !p->num[c][i]) return 0;
        p->count[c] = (int8_t)k;
    }
    return !*s;
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
/* a wall grown by chance from one cell, a cell at a time beside it (never closing a 2 x 2), mostly where it touches
 * the wall once (so it winds); up to `want` cells */
static void grow_wall(int n, int want, uint8_t *sh, uint32_t *s)
{
    memset(sh, 0, TPA_CELLS);
    sh[pr_below(s, n * n)] = 1;
    for (int got = 1; got < want;) {
        int cand[TPA_CELLS], nc = 0, thin[TPA_CELLS], nt = 0;
        for (int c = 0; c < n * n; c++) {
            if (sh[c]) continue;
            int x = c % n, y = c / n, touch = (x > 0 && sh[c - 1]) + (x < n - 1 && sh[c + 1]) + (y > 0 && sh[c - n]) +
                                              (y < n - 1 && sh[c + n]);
            if (!touch) continue;
            int square = 0;
            sh[c] = 1;
            for (int dy = -1; dy <= 0 && !square; dy++)
                for (int dx = -1; dx <= 0 && !square; dx++) {
                    int x0 = x + dx, y0 = y + dy, d = y0 * n + x0;
                    if (x0 >= 0 && y0 >= 0 && x0 + 1 < n && y0 + 1 < n && sh[d] && sh[d + 1] && sh[d + n] &&
                        sh[d + n + 1])
                        square = 1;
                }
            sh[c] = 0;
            if (square) continue;
            cand[nc++] = c;
            if (touch == 1) thin[nt++] = c;
        }
        if (!nc) break;
        sh[nt && pr_below(s, 100) < 80 ? thin[pr_below(s, nt)] : cand[pr_below(s, nc)]] = 1;
        got++;
    }
}

typedef struct {
    tpa_puzzle *p, full;
    long *work;
} reducer;

#define GRADE_BUDGET 4000L
static int red_grade(void *u, int cap)
{
    reducer *R = u;
    return grade_work(R->p, NULL, R->work, cap, GRADE_BUDGET, NULL);
}

static void red_clue(void *u, int i, int on)
{
    reducer *R = u;
    R->p->count[i] = on ? R->full.count[i] : -1;
}

#define MAKE_BUDGET 40000L                      /* the solver's work a puzzle may cost (about a second at most) */

/* a puzzle made: a wall by chance, every other cell a clue, then clues taken away while it stays at its level; the
 * nearest level when that one is not found; none (n 0) when none came in time */
void tpa_make_ex(int n, int level, uint32_t seed, long budget, tpa_puzzle *p, uint8_t *sol)
{
    if (n < 4 || n > TPA_MAX) n = 6;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    static tpa_puzzle best, q;
    uint8_t sh[TPA_CELLS];
    int best_lv = -2;
    memset(&best, 0, sizeof best);
    for (int attempt = 0; attempt < 40 && work < 3 * budget; attempt++) {
        grow_wall(n, n * n * (50 + pr_below(&s, 12)) / 100, sh, &s);
        memset(&q, 0, sizeof q);
        q.n = n;
        int order[TPA_CELLS], no = 0;
        for (int c = 0; c < n * n; c++) {
            q.count[c] = -1;
            if (sh[c]) continue;
            q.count[c] = (int8_t)tpa_runs(sh, n, c, q.num[c]);
            order[no++] = c;
        }
        reducer R = {&q, q, &work};
        if (grade_work(&q, NULL, &work, level, GRADE_BUDGET, NULL) < BENCH_EASY) continue;
        pr_shuffle(&s, order, no);
        bench_maker mk = {&R, red_grade, red_clue};
        int lv = bench_reduce(&mk, order, no, level, &work, 3 * budget);
        int rank = lv < BENCH_EASY || lv > BENCH_HARD ? -9 : lv <= level ? lv - level : level - lv - 3;
        int best_rank = best_lv < BENCH_EASY ? -9 : best_lv <= level ? best_lv - level : level - best_lv - 3;
        if (rank > best_rank) {
            best_lv = lv;
            best = q;
        }
        if (best_lv == level) break;
    }
    if (best_lv < BENCH_EASY) {                 /* none found in time: none (tpa_new takes a level instead) */
        memset(p, 0, sizeof *p);
        if (sol) memset(sol, 0, TPA_CELLS);
        return;
    }
    *p = best;
    if (sol) grade_work(p, sol, NULL, BENCH_HARD, 0, NULL);
}

void tpa_make(int n, int level, uint32_t seed, tpa_puzzle *p, uint8_t *sol)
{
    tpa_make_ex(n, level, seed, MAKE_BUDGET, p, sol);
}

/* a level of the size, turned or mirrored by chance (the seed), from the levels of its third of that size's run
 * (they go from the easiest): when the maker found none in time. 0: no level of that size */
static int from_level(int n, int level, uint32_t seed, tpa_puzzle *p)
{
    int at[TPA_LEVELS], na = 0;
    for (int l = 0; l < TPA_LEVELS; l++)
        if (atoi(tpa_levels[l]) == n) at[na++] = l;
    if (!na) return 0;
    uint32_t s = pr_seed(seed ^ 0x9e3779b9u);
    int lo = na * level / 3, hi = na * (level + 1) / 3, t = pr_below(&s, 8);
    tpa_puzzle q;
    if (hi <= lo || !tpa_decode(tpa_levels[at[lo + pr_below(&s, hi - lo)]], &q)) return 0;
    *p = q;
    for (int y = 0; y < n; y++)                 /* the clues' runs stay as they were turned or mirrored */
        for (int x = 0; x < n; x++) {
            int sx = t & 1 ? n - 1 - x : x, sy = t & 2 ? n - 1 - y : y;
            if (t & 4) { int u = sx; sx = sy; sy = u; }
            p->count[y * n + x] = q.count[sy * n + sx];
            memcpy(p->num[y * n + x], q.num[sy * n + sx], 4);
        }
    return 1;
}

/* ---- the player's grid ------------------------------------------------------------------------------------------- */
int tpa_load(tpa_game *g, const tpa_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = tpa_grade(p, g->sol);
    return g->level >= BENCH_EASY && g->level <= BENCH_HARD;
}

void tpa_new(tpa_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= TPA_VARIANTS) variant = 0;
    tpa_puzzle p;
    uint8_t sol[TPA_CELLS];
    tpa_make(tpa_size(variant), tpa_level(variant), seed, &p, sol);
    if (p.n != tpa_size(variant) && from_level(tpa_size(variant), tpa_level(variant), seed, &p)) tpa_grade(&p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = tpa_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
}

int tpa_free(const tpa_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n && g->p.count[c] < 0; }
int tpa_tap_state(const tpa_game *g, int c) { return g->st[c] == TPA_SHADE ? TPA_EMPTY : TPA_SHADE; }
int tpa_hold_state(const tpa_game *g, int c) { return g->st[c] == TPA_DOT ? TPA_EMPTY : TPA_DOT; }

static int wrongs(const tpa_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++)
        k += tpa_free(g, c) && g->st[c] != TPA_EMPTY && (g->st[c] == TPA_SHADE) != g->sol[c];
    return k;
}

int tpa_paint(tpa_game *g, const int *cells, int n, int state)
{
    bench_begin(&g->hist, n);
    for (int i = 0; i < n; i++)
        if (tpa_free(g, cells[i])) bench_set(&g->hist, g->st, cells[i], state);
    return bench_done(&g->hist, wrongs(g));
}

static int put(tpa_game *g, int c, int state) { return tpa_paint(g, &c, 1, state); }

static int hint_step(tpa_game *g)
{
    int cells = g->p.n * g->p.n;
    for (int c = 0; c < cells; c++)             /* a wrong cell first */
        if (tpa_free(g, c) && g->st[c] != TPA_EMPTY && (g->st[c] == TPA_SHADE) != g->sol[c])
            return put(g, c, g->sol[c] ? TPA_SHADE : TPA_DOT) ? c : -1;
    static ctx k;
    ctx_make(&k, &g->p);
    bench_rules r = rules_of(&k, BENCH_HARD, NULL);
    bench_grid d;
    grid_of(&k, &d);
    for (int c = 0; c < cells; c++)
        if (tpa_free(g, c) && g->st[c] != TPA_EMPTY) d.d[c] = (uint16_t)(g->sol[c] ? SHADE : WHITE);
    int c = bench_next(&d, &r);
    if (c >= 0 && (!tpa_free(g, c) || g->st[c] != TPA_EMPTY)) c = -1;
    for (int i = 0; i < cells && c < 0; i++)
        if (tpa_free(g, i) && g->st[i] == TPA_EMPTY && g->sol[i]) c = i;
    for (int i = 0; i < cells && c < 0; i++)
        if (tpa_free(g, i) && g->st[i] == TPA_EMPTY) c = i;
    if (c < 0) return -1;
    return put(g, c, g->sol[c] ? TPA_SHADE : TPA_DOT) ? c : -1;
}

int tpa_hint(tpa_game *g) { return bench_hinted(&g->hist, hint_step(g)); }

int tpa_undo(tpa_game *g)
{
    int c = bench_undo_cells(&g->hist, g->st);
    bench_undone(&g->hist, wrongs(g));
    return c;
}

int tpa_clue(const tpa_game *g, int c)
{
    int n = g->p.n;
    if (g->p.count[c] < 0) return 0;
    unsigned on = 0, open = 0;
    for (int i = 0; i < 8; i++) {
        int x = c % n + RX[i], y = c / n + RY[i], o = y * n + x;
        if (x < 0 || y < 0 || x >= n || y >= n || g->p.count[o] >= 0) continue;
        if (g->st[o] == TPA_SHADE) on |= 1u << i;
        else if (g->st[o] == TPA_EMPTY) open |= 1u << i;
    }
    if (same_runs(on, g->p.count[c], g->p.num[c])) return open ? 0 : 2;   /* kept only once its ring is all decided */
    for (unsigned b = 0; b < 256; b++)          /* a way the empty cells round it could still make its runs */
        if ((b & on) == on && !(b & ~(on | open)) && same_runs(b, g->p.count[c], g->p.num[c])) return 0;
    return 1;
}

int tpa_square(const tpa_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n;
    if (g->st[c] != TPA_SHADE) return 0;
    for (int dy = -1; dy <= 0; dy++)
        for (int dx = -1; dx <= 0; dx++) {
            int x0 = x + dx, y0 = y + dy, d = y0 * n + x0;
            if (x0 >= 0 && y0 >= 0 && x0 + 1 < n && y0 + 1 < n && g->st[d] == TPA_SHADE &&
                g->st[d + 1] == TPA_SHADE && g->st[d + n] == TPA_SHADE && g->st[d + n + 1] == TPA_SHADE)
                return 1;
        }
    return 0;
}

int tpa_solved(const tpa_game *g)
{
    static ctx k;
    ctx_make(&k, &g->p);
    uint8_t sh[TPA_CELLS];
    int any = 0;
    for (int c = 0; c < k.cells; c++) {
        sh[c] = g->st[c] == TPA_SHADE;
        any |= sh[c];
    }
    return any && valid_cells(&k, sh);
}

int tpa_script(tpa_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (tpa_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if (tpa_free(g, c) && (g->st[c] == TPA_SHADE) != g->sol[c])
                    put(g, c, g->sol[c] ? TPA_SHADE : TPA_EMPTY), k--;
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
        int was = g->st[a], to = hold ? tpa_hold_state(g, a) : tpa_tap_state(g, a), cells[TPA_MAX], k = 0;
        int dx = (b % n > a % n) - (b % n < a % n), dy = (b / n > a / n) - (b / n < a / n);
        for (int c = a;; c += dy * n + dx) {
            if (g->st[c] == was && tpa_free(g, c)) cells[k++] = c;
            if (c == b) break;
        }
        if (!tpa_paint(g, cells, k, to)) return 0;
    }
    return 1;
}
