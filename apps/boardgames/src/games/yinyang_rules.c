/*
 * Yin-Yang: the rules (yinyang.h): the ways of reasoning the solver grades with, the maker, the player's grid and
 * undo. No SDL: tested by tests/yinyang_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/yinyang.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/yinyang_levels.h"

#define IVORY 1u                                /* a cell's values: bit 0 ivory, bit 1 ebony (as YYG_IVORY, _EBONY) */
#define EBONY 2u
#define BOTH 3u

static const int SIZES[YYG_SIZES] = {6, 8, 10};

int yyg_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % YYG_SIZES]; }
int yyg_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

/* ---- what the solver knows of a puzzle -------------------------------------------------------------------------- */
typedef struct {
    const yyg_puzzle *p;
    int n, cells, passes, from;                 /* passes: a trial's, at the level graded */
    long *work;                                 /* the solver's work counted (each trial's passes too) */
    uint8_t nn[YYG_CELLS], nb[YYG_CELLS][4];
    int rim[4 * YYG_MAX], nrim;                 /* the rim's cells round the grid, in order */
} ctx;

static void ctx_make(ctx *k, const yyg_puzzle *p)
{
    int n = p->n;
    memset(k, 0, sizeof *k);
    k->p = p;
    k->n = n;
    k->cells = n * n;
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n;
        if (y > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - n);
        if (x > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - 1);
        if (x < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + 1);
        if (y < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + n);
    }
    for (int x = 0; x < n - 1; x++) k->rim[k->nrim++] = x;                 /* the top, left to right */
    for (int y = 0; y < n - 1; y++) k->rim[k->nrim++] = y * n + n - 1;     /* the right side, down */
    for (int x = n - 1; x > 0; x--) k->rim[k->nrim++] = (n - 1) * n + x;   /* the bottom, right to left */
    for (int y = n - 1; y > 0; y--) k->rim[k->nrim++] = y * n;             /* the left side, up */
}

static void grid_of(const ctx *k, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < k->cells; c++) g->d[c] = (uint16_t)(k->p->given[c] ? k->p->given[c] : BOTH);
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

/* a colour's joins: its stones all in one piece of cells not of the other colour (-1: not), the cells cut off from
 * them the other colour, the cells they cannot do without that colour (the cut points between them, depth first) */
typedef struct {
    const ctx *k;
    const uint16_t *w;
    unsigned col;
    int disc[YYG_CELLS], low[YYG_CELLS], sub[YYG_CELLS], t, mine;
    uint8_t *cut;
} cuts;

static void cut_dfs(cuts *D, int u, int parent)
{
    const ctx *k = D->k;
    int split = 0, apart = 0;
    D->disc[u] = D->low[u] = ++D->t;
    D->sub[u] = D->w[u] == D->col;
    for (int j = 0; j < k->nn[u]; j++) {
        int v = k->nb[u][j];
        if (D->w[v] == (BOTH & ~D->col)) continue;
        if (!D->disc[v]) {
            cut_dfs(D, v, u);
            D->sub[u] += D->sub[v];
            if (D->low[v] < D->low[u]) D->low[u] = D->low[v];
            if (D->low[v] >= D->disc[u] && D->sub[v]) split++, apart += D->sub[v];
        } else if (v != parent && D->disc[v] < D->low[u]) {
            D->low[u] = D->disc[v];
        }
    }
    if (D->w[u] == BOTH && split + (D->mine - apart > 0) >= 2) D->cut[u] = 1;
}

static int joins(bench_grid *g, const uint16_t *w, const ctx *k, unsigned col)
{
    cuts D;
    uint8_t cut[YYG_CELLS];
    int first = -1;
    memset(cut, 0, (size_t)k->cells);
    memset(D.disc, 0, (size_t)k->cells * sizeof D.disc[0]);
    D.k = k;
    D.w = w;
    D.col = col;
    D.t = D.mine = 0;
    D.cut = cut;
    for (int c = 0; c < k->cells; c++)
        if (w[c] == col) {
            D.mine++;
            if (first < 0) first = c;
        }
    if (first < 0) return 0;
    cut_dfs(&D, first, -1);
    if (D.sub[first] != D.mine) return -1;
    for (int c = 0; c < k->cells; c++) {
        if (w[c] != BOTH) continue;
        if (!D.disc[c]) force(g, c, BOTH & ~col);
        else if (cut[c]) force(g, c, col);
    }
    return 0;
}

/* round the rim each colour lies in one stretch: with both colours on it, the cells between two stones of a colour
 * (going round, no stone between) are that colour; more than two changes: a contradiction */
static int rim(bench_grid *g, const uint16_t *w, const ctx *k)
{
    int known[4 * YYG_MAX], nk = 0, changes = 0;
    for (int i = 0; i < k->nrim; i++)
        if (w[k->rim[i]] != BOTH) known[nk++] = i;
    if (nk < 2) return 0;
    for (int j = 0; j < nk; j++) changes += w[k->rim[known[j]]] != w[k->rim[known[(j + 1) % nk]]];
    if (changes > 2) return -1;
    if (!changes) return 0;
    for (int j = 0; j < nk; j++) {
        int a = known[j], b = known[(j + 1) % nk];
        unsigned col = w[k->rim[a]];
        if (col != w[k->rim[b]]) continue;
        for (int i = (a + 1) % k->nrim; i != b; i = (i + 1) % k->nrim) force(g, k->rim[i], col);
    }
    return 0;
}

/* the plain ways: the 2 x 2 squares (three of a colour leave the fourth the other; none crosswise); each colour's
 * joins; the rim */
static int plain(bench_grid *g, const ctx *k)
{
    int n = k->n;
    uint16_t w[YYG_CELLS];
    memcpy(w, g->d, (size_t)k->cells * sizeof w[0]);
    for (int y = 0; y + 1 < n; y++)
        for (int x = 0; x + 1 < n; x++) {
            int c = y * n + x, sq[4] = {c, c + 1, c + n + 1, c + n};   /* round the square: a, b, d, c */
            for (unsigned col = IVORY; col <= EBONY; col++) {
                int s = 0, u = -1, nu = 0;
                for (int i = 0; i < 4; i++) {
                    s += w[sq[i]] == col;
                    if (w[sq[i]] == BOTH) u = sq[i], nu++;
                }
                if (s == 4) return -1;
                if (s == 3 && nu == 1) force(g, u, BOTH & ~col);
            }
            for (int i = 0; i < 2; i++) {       /* crosswise: one diagonal of a colour, the other of the other */
                int a = sq[i], b = sq[i + 2], e = sq[i + 1], f = sq[(i + 3) % 4];
                if (w[a] == BOTH || w[a] != w[b]) continue;
                unsigned other = BOTH & ~w[a];
                if (w[e] == other && w[f] == other) return -1;
                if (w[e] == other && w[f] == BOTH) force(g, f, w[a]);
                if (w[f] == other && w[e] == BOTH) force(g, e, w[a]);
            }
        }
    if (joins(g, w, k, IVORY) < 0 || joins(g, w, k, EBONY) < 0 || rim(g, w, k) < 0) return -1;
    return settled(w, g, k->cells);
}

/* trials: each unknown cell tried each way, the plain ways followed `passes` times; a contradiction rules that way
 * out (one pass: what a glance shows; a few: a short chain). A few finds, then the plain ways first again */
static int trials(bench_grid *g, ctx *k, int passes)
{
    uint16_t w[YYG_CELLS];
    bench_grid t;
    memcpy(w, g->d, (size_t)k->cells * sizeof w[0]);
    for (int i = 0, start = k->from, found = 0; i < k->cells; i++) {
        int c = (start + i) % k->cells;         /* from the last find on */
        if (w[c] != BOTH) continue;
        for (unsigned v = IVORY; v <= EBONY; v++) {
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

static long finds;                              /* the trials' finds (yyg_effort counts them) */

static int deduce(bench_grid *g, int tier, void *cv)
{
    ctx *k = cv;
    int res = tier ? trials(g, k, k->passes) : plain(g, k);
    if (res > 0 && tier) finds++;
    return res;
}

/* a full grid of stones st (YYG_IVORY, YYG_EBONY) keeps every rule: the given stones, no 2 x 2 of a colour, each
 * colour in one group */
static int valid_cells(const ctx *k, const uint8_t *st)
{
    int n = k->n;
    for (int c = 0; c < k->cells; c++) {
        if (st[c] != YYG_IVORY && st[c] != YYG_EBONY) return 0;
        if (k->p->given[c] && st[c] != k->p->given[c]) return 0;
        int x = c % n, y = c / n;
        if (x + 1 < n && y + 1 < n && st[c + 1] == st[c] && st[c + n] == st[c] && st[c + n + 1] == st[c]) return 0;
    }
    for (int col = YYG_IVORY; col <= YYG_EBONY; col++) {
        int stack[YYG_CELLS], sp = 0, got = 0, total = 0, first = -1;
        uint8_t seen[YYG_CELLS];
        memset(seen, 0, (size_t)k->cells);
        for (int c = 0; c < k->cells; c++)
            if (st[c] == col) {
                total++;
                if (first < 0) first = c;
            }
        if (!total) continue;
        seen[first] = 1;
        stack[sp++] = first;
        while (sp) {
            int c = stack[--sp];
            got++;
            for (int j = 0; j < k->nn[c]; j++) {
                int d = k->nb[c][j];
                if (st[d] == col && !seen[d]) seen[d] = 1, stack[sp++] = d;
            }
        }
        if (got != total) return 0;
    }
    return 1;
}

static int valid(const bench_grid *g, void *cv)
{
    const ctx *k = cv;
    uint8_t st[YYG_CELLS];
    for (int c = 0; c < k->cells; c++) st[c] = (uint8_t)g->d[c];
    return valid_cells(k, st);
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
static int run(const yyg_puzzle *p, int level, bench_grid *g, long *work, long budget, long *passes)
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
static int grade_work(const yyg_puzzle *p, uint8_t *sol, long *work, int cap, long budget, long *passes)
{
    bench_grid g;
    for (int lv = BENCH_EASY; lv <= cap; lv++) {
        finds = 0;
        int res = run(p, lv, &g, work, budget, passes);
        if (res == BENCH_NONE) return BENCH_NONE;
        if (res != BENCH_EASY) continue;
        if (sol)
            for (int c = 0; c < p->n * p->n; c++) sol[c] = (uint8_t)g.d[c];
        return lv;
    }
    return BENCH_GUESS;
}

int yyg_grade(const yyg_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, BENCH_HARD, 0, NULL); }

int yyg_count(const yyg_puzzle *p, int limit)
{
    static ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, -1, NULL);
    bench_grid g;
    grid_of(&k, &g);
    return bench_search(&g, &r, limit, NULL, NULL, 0);
}

int yyg_effort(const yyg_puzzle *p, long *passes, long *trial_finds)
{
    int lv = grade_work(p, NULL, NULL, BENCH_HARD, 0, passes);
    if (trial_finds) *trial_finds = finds;
    return lv;
}

/* ---- a puzzle as text (the levels) ------------------------------------------------------------------------------- */
int yyg_encode(const yyg_puzzle *p, char *out, int size)
{
    int n = p->n, len = snprintf(out, (size_t)size, "%d:", n);
    if (len < 0 || len + n * n + 1 > size) return 0;
    for (int c = 0; c < n * n; c++) out[len++] = ".ox"[p->given[c] % 3];
    out[len] = 0;
    return len;
}

int yyg_decode(const char *s, yyg_puzzle *p)
{
    memset(p, 0, sizeof *p);
    int n = 0;
    for (; *s >= '0' && *s <= '9'; s++) n = n * 10 + *s - '0';
    if (*s++ != ':' || n < 2 || n > YYG_MAX || (int)strlen(s) != n * n) return 0;
    p->n = n;
    for (int c = 0; c < n * n; c++) {
        const char *d = strchr(".ox", s[c]);
        if (!d || !s[c]) return 0;
        p->given[c] = (uint8_t)(d - ".ox");
    }
    return 1;
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
/* the stones of st (YYG_*) of colour col in one group (or none) */
static int one_group(const uint8_t *st, int n, int col)
{
    int stack[YYG_CELLS], sp = 0, got = 0, total = 0, first = -1;
    uint8_t seen[YYG_CELLS];
    memset(seen, 0, sizeof seen);
    for (int c = 0; c < n * n; c++)
        if (st[c] == col) {
            total++;
            if (first < 0) first = c;
        }
    if (!total) return 1;
    seen[first] = 1;
    stack[sp++] = first;
    while (sp) {
        int c = stack[--sp], x = c % n, y = c / n;
        got++;
        const int nb[4] = {y > 0 ? c - n : -1, x > 0 ? c - 1 : -1, x < n - 1 ? c + 1 : -1, y < n - 1 ? c + n : -1};
        for (int j = 0; j < 4; j++)
            if (nb[j] >= 0 && !seen[nb[j]] && st[nb[j]] == col) seen[nb[j]] = 1, stack[sp++] = nb[j];
    }
    return got == total;
}

/* the 2 x 2 squares of one colour (of either: col 0) */
static int squares(const uint8_t *st, int n, int col)
{
    int k = 0;
    for (int y = 0; y + 1 < n; y++)
        for (int x = 0; x + 1 < n; x++) {
            int c = y * n + x, s = st[c];
            k += (!col || s == col) && st[c + 1] == s && st[c + n] == s && st[c + n + 1] == s;
        }
    return k;
}

/* the stones laid by chance: an ebony group grown from a cell of the rim, a cell at a time beside it, never a 2 x 2
 * of ebony, the ivory kept in one group, the ivory 2 x 2s broken first; 1 when no 2 x 2 of one colour is left */
static int lay(int n, uint8_t *st, uint32_t *s)
{
    for (int c = 0; c < n * n; c++) st[c] = YYG_IVORY;
    int first = pr_below(s, 4 * (n - 1)), x0 = first < n ? first : first < 2 * n - 1 ? n - 1 : 0;
    int y0 = first < n ? 0 : first < 2 * n - 1 ? first - n + 1 : n - 1;
    st[y0 * n + x0] = YYG_EBONY;
    int want = n * n * (42 + pr_below(s, 14)) / 100;
    for (int got = 1, guard = 0; guard < 6 * n * n; guard++) {
        int cand[YYG_CELLS], nc = 0, need[YYG_CELLS], nn = 0;
        int ivory = squares(st, n, YYG_IVORY);
        if (got >= want && !ivory) break;
        for (int c = 0; c < n * n; c++) {
            if (st[c] != YYG_IVORY) continue;
            int x = c % n, y = c / n;
            if (!((x > 0 && st[c - 1] == YYG_EBONY) || (x < n - 1 && st[c + 1] == YYG_EBONY) ||
                  (y > 0 && st[c - n] == YYG_EBONY) || (y < n - 1 && st[c + n] == YYG_EBONY)))
                continue;
            st[c] = YYG_EBONY;
            int ok = !squares(st, n, YYG_EBONY) && one_group(st, n, YYG_IVORY);
            int helps = ok && squares(st, n, YYG_IVORY) < ivory;
            st[c] = YYG_IVORY;
            if (!ok) continue;
            cand[nc++] = c;
            if (helps) need[nn++] = c;
        }
        if (!nc) break;
        int c = nn && (got >= want || pr_below(s, 2)) ? need[pr_below(s, nn)] : cand[pr_below(s, nc)];
        st[c] = YYG_EBONY;
        got++;
    }
    return !squares(st, n, 0);
}

typedef struct {
    yyg_puzzle *p;
    uint8_t full[YYG_CELLS];
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
    R->p->given[i] = on ? R->full[i] : YYG_EMPTY;
}

#define MAKE_BUDGET 40000L                      /* the solver's work a puzzle may cost (about a second at most) */

/* a puzzle made: the stones by chance, every one given, then stones taken away while it stays at its level; the
 * nearest level when that one is not found; none (n 0) when none came in time */
void yyg_make_ex(int n, int level, uint32_t seed, long budget, yyg_puzzle *p, uint8_t *sol)
{
    if (n < 4 || n > YYG_MAX) n = 6;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    static yyg_puzzle best, q;
    uint8_t st[YYG_CELLS];
    int best_lv = -2;
    memset(&best, 0, sizeof best);
    for (int attempt = 0; attempt < 40 && work < 3 * budget; attempt++) {
        if (!lay(n, st, &s)) continue;
        memset(&q, 0, sizeof q);
        q.n = n;
        int order[YYG_CELLS];
        for (int c = 0; c < n * n; c++) q.given[c] = st[c], order[c] = c;
        reducer R = {&q, {0}, &work};
        memcpy(R.full, q.given, sizeof R.full);
        pr_shuffle(&s, order, n * n);
        bench_maker mk = {&R, red_grade, red_clue};
        int lv = bench_reduce(&mk, order, n * n, level, &work, 3 * budget);
        int rank = lv < BENCH_EASY || lv > BENCH_HARD ? -9 : lv <= level ? lv - level : level - lv - 3;
        int best_rank = best_lv < BENCH_EASY ? -9 : best_lv <= level ? best_lv - level : level - best_lv - 3;
        if (rank > best_rank) {
            best_lv = lv;
            best = q;
        }
        if (best_lv == level) break;
    }
    if (best_lv < BENCH_EASY) {                 /* none found in time: none (yyg_new takes a level instead) */
        memset(p, 0, sizeof *p);
        if (sol) memset(sol, 0, YYG_CELLS);
        return;
    }
    *p = best;
    if (sol) grade_work(p, sol, NULL, BENCH_HARD, 0, NULL);
}

void yyg_make(int n, int level, uint32_t seed, yyg_puzzle *p, uint8_t *sol)
{
    yyg_make_ex(n, level, seed, MAKE_BUDGET, p, sol);
}

/* a level of the size, turned or mirrored by chance (the seed), from the levels of its third of that size's run
 * (they go from the easiest): when the maker found none in time. 0: no level of that size */
static int from_level(int n, int level, uint32_t seed, yyg_puzzle *p)
{
    int at[YYG_LEVELS], na = 0;
    for (int l = 0; l < YYG_LEVELS; l++)
        if (atoi(yyg_levels[l]) == n) at[na++] = l;
    if (!na) return 0;
    uint32_t s = pr_seed(seed ^ 0x9e3779b9u);
    int lo = na * level / 3, hi = na * (level + 1) / 3, t = pr_below(&s, 8);
    yyg_puzzle q;
    if (hi <= lo || !yyg_decode(yyg_levels[at[lo + pr_below(&s, hi - lo)]], &q)) return 0;
    *p = q;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int sx = t & 1 ? n - 1 - x : x, sy = t & 2 ? n - 1 - y : y;
            if (t & 4) { int u = sx; sx = sy; sy = u; }
            p->given[y * n + x] = q.given[sy * n + sx];
        }
    return 1;
}

/* ---- the player's grid ------------------------------------------------------------------------------------------- */
static void shown(yyg_game *g)                  /* the given stones in place */
{
    for (int c = 0; c < g->p.n * g->p.n; c++)
        if (g->p.given[c]) g->st[c] = g->p.given[c];
}

int yyg_load(yyg_game *g, const yyg_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = yyg_grade(p, g->sol);
    shown(g);
    return g->level >= BENCH_EASY && g->level <= BENCH_HARD;
}

void yyg_new(yyg_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= YYG_VARIANTS) variant = 0;
    yyg_puzzle p;
    uint8_t sol[YYG_CELLS];
    yyg_make(yyg_size(variant), yyg_level(variant), seed, &p, sol);
    if (p.n != yyg_size(variant) && from_level(yyg_size(variant), yyg_level(variant), seed, &p)) yyg_grade(&p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = yyg_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
    shown(g);
}

int yyg_free(const yyg_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n && !g->p.given[c]; }
int yyg_tap_state(const yyg_game *g, int c)   /* empty, ebony, ivory, empty... */
{
    return g->st[c] == YYG_EMPTY ? YYG_EBONY : g->st[c] == YYG_EBONY ? YYG_IVORY : YYG_EMPTY;
}
int yyg_hold_state(const yyg_game *g, int c) { return g->st[c] == YYG_IVORY ? YYG_EMPTY : YYG_IVORY; }

static int wrongs(const yyg_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++) k += yyg_free(g, c) && g->st[c] != YYG_EMPTY && g->st[c] != g->sol[c];
    return k;
}

int yyg_paint(yyg_game *g, const int *cells, int n, int state)
{
    bench_begin(&g->hist, n);
    for (int i = 0; i < n; i++)
        if (yyg_free(g, cells[i])) bench_set(&g->hist, g->st, cells[i], state);
    return bench_done(&g->hist, wrongs(g));
}

static int put(yyg_game *g, int c, int state) { return yyg_paint(g, &c, 1, state); }

static int hint_step(yyg_game *g)
{
    int cells = g->p.n * g->p.n;
    for (int c = 0; c < cells; c++)             /* a wrong stone first */
        if (yyg_free(g, c) && g->st[c] != YYG_EMPTY && g->st[c] != g->sol[c]) return put(g, c, g->sol[c]) ? c : -1;
    static ctx k;
    ctx_make(&k, &g->p);
    bench_rules r = rules_of(&k, BENCH_HARD, NULL);
    bench_grid d;
    grid_of(&k, &d);
    for (int c = 0; c < cells; c++)
        if (g->st[c] != YYG_EMPTY) d.d[c] = g->sol[c];
    int c = bench_next(&d, &r);
    if (c >= 0 && (!yyg_free(g, c) || g->st[c] != YYG_EMPTY)) c = -1;
    for (int i = 0; i < cells && c < 0; i++)
        if (yyg_free(g, i) && g->st[i] == YYG_EMPTY) c = i;
    if (c < 0) return -1;
    return put(g, c, g->sol[c]) ? c : -1;
}

int yyg_hint(yyg_game *g) { return bench_hinted(&g->hist, hint_step(g)); }

int yyg_undo(yyg_game *g)
{
    int c = bench_undo_cells(&g->hist, g->st);
    bench_undone(&g->hist, wrongs(g));
    return c;
}

int yyg_square(const yyg_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n, s = g->st[c];
    if (s == YYG_EMPTY) return 0;
    for (int dy = -1; dy <= 0; dy++)
        for (int dx = -1; dx <= 0; dx++) {
            int x0 = x + dx, y0 = y + dy, d = y0 * n + x0;
            if (x0 >= 0 && y0 >= 0 && x0 + 1 < n && y0 + 1 < n && g->st[d] == s && g->st[d + 1] == s &&
                g->st[d + n] == s && g->st[d + n + 1] == s)
                return 1;
        }
    return 0;
}

int yyg_apart(const yyg_game *g, int c)
{
    int n = g->p.n, col = g->st[c], stack[YYG_CELLS], sp = 0, open = 0, got = 0, total = 0;
    if (col == YYG_EMPTY) return 0;
    uint8_t seen[YYG_CELLS];
    memset(seen, 0, sizeof seen);
    seen[c] = 1;
    stack[sp++] = c;
    while (sp) {                                /* its group, and whether any empty cell borders it */
        int d = stack[--sp], x = d % n, y = d / n;
        got++;
        const int nb[4] = {y > 0 ? d - n : -1, x > 0 ? d - 1 : -1, x < n - 1 ? d + 1 : -1, y < n - 1 ? d + n : -1};
        for (int j = 0; j < 4; j++) {
            if (nb[j] < 0) continue;
            if (g->st[nb[j]] == YYG_EMPTY) open = 1;
            else if (g->st[nb[j]] == col && !seen[nb[j]]) seen[nb[j]] = 1, stack[sp++] = nb[j];
        }
    }
    for (int d = 0; d < n * n; d++) total += g->st[d] == col;
    return !open && got < total;
}

int yyg_solved(const yyg_game *g)
{
    static ctx k;
    ctx_make(&k, &g->p);
    uint8_t st[YYG_CELLS];
    for (int c = 0; c < k.cells; c++) st[c] = (uint8_t)g->st[c];
    return valid_cells(&k, st);
}

int yyg_script(yyg_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (yyg_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if (yyg_free(g, c) && g->st[c] != g->sol[c]) put(g, c, g->sol[c]), k--;
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
        int was = g->st[a], to = hold ? yyg_hold_state(g, a) : yyg_tap_state(g, a), cells[YYG_MAX], k = 0;
        int dx = (b % n > a % n) - (b % n < a % n), dy = (b / n > a / n) - (b / n < a / n);
        for (int c = a;; c += dy * n + dx) {
            if (g->st[c] == was && yyg_free(g, c)) cells[k++] = c;
            if (c == b) break;
        }
        if (!yyg_paint(g, cells, k, to)) return 0;
    }
    return 1;
}
