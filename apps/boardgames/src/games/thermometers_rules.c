/*
 * Thermometers: the rules (thermometers.h): the ways of reasoning the solver grades with, the maker, the player's grid
 * and undo. No SDL: tested by tests/thermometers_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/thermometers.h"

#include <stdio.h>
#include <string.h>

#include "games/puzrand.h"

#define EMPTY 1u                                /* a cell's values: bit 0 empty, bit 1 filled */
#define FULL 2u
#define BOTH 3u

static const int SIZES[THM_SIZES] = {6, 8, 10};

int thm_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % THM_SIZES]; }
int thm_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

static int at(int n, int way, int i, int j) { return way ? j * n + i : i * n + j; }

/* ---- the thermometers --------------------------------------------------------------------------------------------- */
int thm_prev(const thm_puzzle *p, int c)
{
    int n = p->n, x = c % n, y = c / n;
    switch (p->from[c]) {
    case 0: return y > 0 ? c - n : -1;
    case 1: return y < n - 1 ? c + n : -1;
    case 2: return x > 0 ? c - 1 : -1;
    case 3: return x < n - 1 ? c + 1 : -1;
    default: return -1;
    }
}

int thm_link(thm_puzzle *p)
{
    int n = p->n, cells = n * n, next[THM_CELLS];
    for (int c = 0; c < cells; c++) next[c] = -1;
    for (int c = 0; c < cells; c++) {
        if (p->from[c] == THM_BULB) continue;
        int b = thm_prev(p, c);
        if (b < 0 || next[b] >= 0) return 0;    /* off the grid, or two tubes from one cell */
        next[b] = c;
    }
    p->thermos = 0;
    int seen = 0;
    for (int c = 0; c < cells; c++) {
        if (p->from[c] != THM_BULB) continue;
        int t = p->thermos++, k = 0;
        for (int x = c; x >= 0; x = next[x]) {
            if (k >= THM_MAX * 2) return 0;
            p->of[x] = (uint8_t)t;
            p->at[x] = (uint8_t)k;
            p->cell[t][k++] = (uint8_t)x;
        }
        p->len[t] = (uint8_t)k;
        seen += k;
    }
    return seen == cells;                       /* none left over (a loop has no bulb) */
}

/* ---- the solver -------------------------------------------------------------------------------------------------- */
static void grid_of(const thm_puzzle *p, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < p->n * p->n; c++) g->d[c] = BOTH;
}

static void force(bench_grid *g, int c, unsigned to) { g->d[c] &= (uint16_t)to; }

static int settled(const bench_grid *was, const bench_grid *g, int cells)
{
    int ch = 0;
    for (int c = 0; c < cells; c++) {
        if (!g->d[c]) return -1;
        ch |= g->d[c] != was->d[c];
    }
    return ch;
}

/* a thermometer's levels still possible: lo (its cells filled for sure) to hi (those not surely empty); -1: none */
static int span(const thm_puzzle *p, const bench_grid *g, int t, int *lo, int *hi)
{
    *lo = 0;
    *hi = p->len[t];
    for (int k = 0; k < p->len[t]; k++) {
        int d = g->d[p->cell[t][k]];
        if (d == FULL) *lo = k + 1;
        if (d == EMPTY && *hi == p->len[t]) *hi = k;
    }
    return *lo <= *hi ? 0 : -1;
}

/* easy */
static int easy(bench_grid *g, const thm_puzzle *p)
{
    const bench_grid w = *g;
    int n = p->n;
    for (int t = 0; t < p->thermos; t++) {      /* filled from the bulb up */
        int lo, hi;
        if (span(p, &w, t, &lo, &hi) < 0) return -1;
        for (int k = 0; k < p->len[t]; k++)
            if (k < lo) force(g, p->cell[t][k], FULL);
            else if (k >= hi) force(g, p->cell[t][k], EMPTY);
    }
    for (int way = 0; way < 2; way++)           /* a line's count: all there, or just room */
        for (int i = 0; i < n; i++) {
            int want = p->count[way][i], full = 0, open = 0;
            if (want < 0) continue;
            for (int j = 0; j < n; j++) {
                int d = w.d[at(n, way, i, j)];
                full += d == FULL;
                open += d == BOTH;
            }
            if (full > want || full + open < want) return -1;
            if (!open || (full != want && full + open != want)) continue;
            for (int j = 0; j < n; j++)
                if (w.d[at(n, way, i, j)] == BOTH) force(g, at(n, way, i, j), full == want ? EMPTY : FULL);
        }
    return settled(&w, g, n * n);
}

/* a thermometer's cells in a line (way, i) at level v (its first v cells filled) */
static int part(const thm_puzzle *p, int t, int v, int way, int i)
{
    int k = 0, n = p->n;
    for (int j = 0; j < v; j++) {
        int c = p->cell[t][j];
        k += (way ? c % n : c / n) == i;
    }
    return k;
}

/* medium */
static int medium(bench_grid *g, const thm_puzzle *p)
{
    const bench_grid w = *g;
    int n = p->n, least[2][THM_MAX], most[2][THM_MAX], lo[THM_CELLS], hi[THM_CELLS];
    memset(least, 0, sizeof least);
    memset(most, 0, sizeof most);
    for (int t = 0; t < p->thermos; t++) {
        if (span(p, &w, t, &lo[t], &hi[t]) < 0) return -1;
        for (int k = 0; k < p->len[t]; k++) {
            int c = p->cell[t][k];
            if (k < lo[t]) least[0][c / n]++, least[1][c % n]++;
            if (k < hi[t]) most[0][c / n]++, most[1][c % n]++;
        }
    }
    for (int t = 0; t < p->thermos; t++) {      /* each level tried against the lines the thermometer crosses */
        if (lo[t] == hi[t]) continue;
        int first = -1, last = -1;
        for (int v = lo[t]; v <= hi[t]; v++) {
            int ok = 1;
            for (int k = 0; k < p->len[t] && ok; k++) {
                int c = p->cell[t][k];
                for (int way = 0; way < 2 && ok; way++) {
                    int i = way ? c % n : c / n, want = p->count[way][i];
                    if (want < 0) continue;
                    int mine = part(p, t, v, way, i);
                    int lo_ = least[way][i] - part(p, t, lo[t], way, i) + mine;
                    int hi_ = most[way][i] - part(p, t, hi[t], way, i) + mine;
                    if (want < lo_ || want > hi_) ok = 0;
                }
            }
            if (!ok) continue;
            if (first < 0) first = v;
            last = v;
        }
        if (first < 0) return -1;
        for (int k = lo[t]; k < hi[t]; k++)
            if (k < first) force(g, p->cell[t][k], FULL);
            else if (k >= last) force(g, p->cell[t][k], EMPTY);
    }
    return settled(&w, g, n * n);
}

static long finds[2];                           /* each tier's finds (thm_effort counts them) */

static int deduce(bench_grid *g, int tier, void *c)
{
    int res = tier ? medium(g, (const thm_puzzle *)c) : easy(g, (const thm_puzzle *)c);
    if (res > 0) finds[tier ? 1 : 0]++;
    return res;
}

static int valid(const bench_grid *g, void *cv)
{
    const thm_puzzle *p = cv;
    int n = p->n;
    for (int t = 0; t < p->thermos; t++) {
        int lo, hi;
        if (span(p, g, t, &lo, &hi) < 0) return 0;
    }
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++) {
            int full = 0;
            for (int j = 0; j < n; j++) full += g->d[at(n, way, i, j)] == FULL;
            if (p->count[way][i] >= 0 && full != p->count[way][i]) return 0;
        }
    return 1;
}

static bench_rules rules_of(const thm_puzzle *p, long *work)
{
    return (bench_rules){.vars = p->n * p->n, .tiers = 2, .probe_tiers = 1,
                        .deduce = deduce, .valid = valid, .ctx = (void *)p, .work = work};
}

static int grade_work(const thm_puzzle *p, uint8_t *sol, long *work, int cap, bench_grid *end)
{
    bench_rules r = rules_of(p, work);
    r.budget = 6000;                            /* one grade's passes at most */
    bench_grid g;
    grid_of(p, &g);
    int lv = bench_grade(&g, &r, cap);
    if (sol && lv >= BENCH_EASY && lv <= BENCH_HARD)
        for (int c = 0; c < p->n * p->n; c++) sol[c] = g.d[c] == FULL;
    if (end) *end = g;
    return lv;
}

int thm_grade(const thm_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, BENCH_HARD, NULL); }

int thm_count(const thm_puzzle *p, int limit)
{
    bench_rules r = rules_of(p, NULL);
    bench_grid g;
    grid_of(p, &g);
    return bench_search(&g, &r, limit, NULL, NULL, 0);
}

int thm_effort(const thm_puzzle *p, long *passes, long *mediums)
{
    bench_rules r = rules_of(p, NULL);
    bench_grid g;
    grid_of(p, &g);
    finds[1] = 0;
    int lv = bench_grade(&g, &r, BENCH_HARD);
    if (passes) *passes = r.passes;
    if (mediums) *mediums = finds[1];
    return lv;
}

/* ---- a puzzle as text (the levels) ------------------------------------------------------------------------------- */
static const char DIGIT[] = "0123456789abc", WAY[] = "udlro";

int thm_encode(const thm_puzzle *p, char *out, int size)
{
    int n = p->n, len = snprintf(out, (size_t)size, "%d:", n);
    if (len < 0 || len + 2 * n + n * n + 1 > size) return 0;
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++) out[len++] = p->count[way][i] < 0 ? '-' : DIGIT[p->count[way][i] % 13];
    for (int c = 0; c < n * n; c++) out[len++] = WAY[p->from[c] > THM_BULB ? THM_BULB : p->from[c]];
    out[len] = 0;
    return len;
}

int thm_decode(const char *s, thm_puzzle *p)
{
    memset(p, 0, sizeof *p);
    int n = 0;
    for (; *s >= '0' && *s <= '9'; s++) n = n * 10 + *s - '0';
    if (*s++ != ':' || n < 3 || n > THM_MAX) return 0;
    p->n = n;
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++, s++) {
            const char *d = *s ? strchr(DIGIT, *s) : NULL;
            if (*s != '-' && (!d || d - DIGIT > n)) return 0;
            p->count[way][i] = (int8_t)(*s == '-' ? -1 : d - DIGIT);
        }
    for (int c = 0; c < n * n; c++, s++) {
        const char *d = *s ? strchr(WAY, *s) : NULL;
        if (!d) return 0;
        p->from[c] = (uint8_t)(d - WAY);
    }
    return !*s && thm_link(p);
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
typedef struct {
    thm_puzzle p;
    uint8_t level[THM_CELLS];                   /* each thermometer's filled cells */
} sheet;

/* the counts and the solution from the levels */
static void counted(sheet *m, uint8_t *sol)
{
    thm_puzzle *p = &m->p;
    int n = p->n;
    memset(p->count, 0, sizeof p->count);
    for (int c = 0; c < n * n; c++) {
        int full = p->at[c] < m->level[p->of[c]];
        if (sol) sol[c] = (uint8_t)full;
        p->count[0][c / n] += (int8_t)full;
        p->count[1][c % n] += (int8_t)full;
    }
}

/* the way from c to its neighbour o: 0 o above, 1 below, 2 left, 3 right */
static int way_to(int n, int c, int o) { return o == c - n ? 0 : o == c + n ? 1 : o == c - 1 ? 2 : 3; }

/* the grid cut into thermometers by chance: walks over the free cells from each free cell in turn (length 2 to most);
 * a cell left alone joins a neighbour's end if it can (else it stays a bulb alone); each laid one way or the other */
static int cut(sheet *m, int n, uint32_t *s, int most)
{
    memset(m, 0, sizeof *m);
    thm_puzzle *p = &m->p;
    p->n = n;
    int cells = n * n, order[THM_CELLS], used[THM_CELLS], next[THM_CELLS], walk[THM_CELLS], lone = 0;
    for (int c = 0; c < cells; c++) order[c] = c, used[c] = 0, next[c] = -1, p->from[c] = THM_BULB;
    pr_shuffle(s, order, cells);
    for (int i = 0; i < cells; i++) {
        int c = order[i];
        if (used[c]) continue;
        int want = 2 + pr_below(s, most - 1), k = 0;
        walk[k++] = c;
        used[c] = 1;
        while (k < want) {
            int x = walk[k - 1], px = x % n, py = x / n, opts[4], no = 0;
            const int nb[4] = {py > 0 ? x - n : -1, py < n - 1 ? x + n : -1, px > 0 ? x - 1 : -1, px < n - 1 ? x + 1 : -1};
            for (int j = 0; j < 4; j++)
                if (nb[j] >= 0 && !used[nb[j]]) opts[no++] = nb[j];
            if (!no) break;
            int o = opts[pr_below(s, no)];
            used[o] = 1;
            walk[k++] = o;
        }
        if (k == 1) {                           /* alone: onto a neighbour's tip (a thermometer's last cell) */
            int px = c % n, py = c / n, opts[4], no = 0;
            const int nb[4] = {py > 0 ? c - n : -1, py < n - 1 ? c + n : -1, px > 0 ? c - 1 : -1, px < n - 1 ? c + 1 : -1};
            for (int j = 0; j < 4; j++)
                if (nb[j] >= 0 && used[nb[j]] && next[nb[j]] < 0 && p->from[nb[j]] != THM_BULB) opts[no++] = nb[j];
            if (no) {
                int o = opts[pr_below(s, no)];
                next[o] = c;
                p->from[c] = (uint8_t)way_to(n, c, o);
                continue;
            }
            lone++;
            continue;
        }
        if (pr_below(s, 2))                     /* the bulb at either end */
            for (int a = 0, b = k - 1; a < b; a++, b--) {
                int t = walk[a];
                walk[a] = walk[b];
                walk[b] = t;
            }
        p->from[walk[0]] = THM_BULB;
        for (int j = 1; j < k; j++) {
            p->from[walk[j]] = (uint8_t)way_to(n, walk[j], walk[j - 1]);
            next[walk[j - 1]] = walk[j];
        }
    }
    if (lone > n / 4 || !thm_link(p)) return 0;
    for (int t = 0; t < p->thermos; t++) m->level[t] = (uint8_t)pr_below(s, p->len[t] + 1);
    counted(m, NULL);
    return 1;
}

/* a small change: a thermometer the reasoning left undecided (und: its cells) filled to another level; 0: none */
static int mutate(sheet *m, uint32_t *s, const int *und, int nu)
{
    thm_puzzle *p = &m->p;
    int c = nu && pr_below(s, 100) < 85 ? und[pr_below(s, nu)] : pr_below(s, p->n * p->n), t = p->of[c];
    int was = m->level[t], v = was;
    while (v == was && p->len[t] > 0) v = pr_below(s, p->len[t] + 1);
    m->level[t] = (uint8_t)v;
    counted(m, NULL);
    return v != was;
}

/* how far the reasoning gets (cells decided) up to cap: *lv its grade, und the cells left (*nu) */
static int progress(const sheet *m, long *work, int cap, int *lv, int *und, int *nu)
{
    bench_grid end;
    *lv = grade_work(&m->p, NULL, work, cap, &end);
    int got = 0, cells = m->p.n * m->p.n;
    *nu = 0;
    for (int c = 0; c < cells; c++) {
        if (bench_one(end.d[c])) got++;
        else und[(*nu)++] = c;
    }
    return got;
}

#define MAKE_BUDGET 25000L                      /* the solver's passes a puzzle may cost (well under a second) */

static int most_of(int n) { return n <= 6 ? 4 : n <= 8 ? 5 : 6; }

/* a puzzle made: thermometers and levels by chance, then the levels changed a little at a time (the change kept when
 * the reasoning gets as far or further) until it is solved at the level (and by no harder way) */
void thm_make_ex(int n, int level, uint32_t seed, long budget, thm_puzzle *p, uint8_t *sol)
{
    if (n < 4 || n > THM_MAX) n = 6;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    static sheet m, keep, best;
    int best_lv = -2;
    int climb = level;
    for (int attempt = 0; attempt < 500; attempt++) {
        if (!cut(&keep, n, &s, most_of(n))) continue;
        int und[THM_CELLS], nu, lv, got = progress(&keep, &work, climb, &lv, und, &nu);
        for (int step = 0; step < 40 * n; step++) {
            int sol_lv = lv >= BENCH_EASY && lv <= climb ? lv : -1;
            if (sol_lv >= 0) {
                if (sol_lv > best_lv) best_lv = sol_lv, best = keep;
                break;
            }
            m = keep;
            if (!mutate(&m, &s, und, nu)) continue;
            int u3[THM_CELLS], n3, lv3, got3 = progress(&m, &work, climb, &lv3, u3, &n3);
            if (got3 >= got || (lv3 >= BENCH_EASY && lv3 <= climb)) {
                keep = m;
                got = got3;
                lv = lv3;
                nu = n3;
                memcpy(und, u3, sizeof u3);
            }
            if (work >= 4 * budget && best_lv >= BENCH_EASY) break;
        }
        if (best_lv == level || (work >= 3 * budget && best_lv >= BENCH_EASY)) break;   /* past it: the nearest */
    }
    if (best_lv < BENCH_EASY && n > 4) {         /* never seen: a smaller one rather than none */
        thm_make_ex(n - 1, level, seed + 1, budget, p, sol);
        return;
    }
    counted(&best, sol);
    *p = best.p;
}

void thm_make(int n, int level, uint32_t seed, thm_puzzle *p, uint8_t *sol)
{
    thm_make_ex(n, level, seed, MAKE_BUDGET, p, sol);
}

/* ---- the player's grid ------------------------------------------------------------------------------------------- */
int thm_load(thm_game *g, const thm_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = thm_grade(p, g->sol);
    return g->level >= BENCH_EASY && g->level <= BENCH_HARD;
}

void thm_new(thm_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= THM_VARIANTS) variant = 0;
    static thm_puzzle p;
    uint8_t sol[THM_CELLS];
    thm_make(thm_size(variant), thm_level(variant), seed, &p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = thm_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
}

int thm_tap_state(const thm_game *g, int c) { return g->st[c] == THM_FULL ? THM_EMPTY : THM_FULL; }
int thm_hold_state(const thm_game *g, int c) { return g->st[c] == THM_CROSS ? THM_EMPTY : THM_CROSS; }

/* one cell set to state, and the thermometer kept in order: filled below a filled cell, crossed above a cross */
static void set_one(const thm_puzzle *p, uint16_t *st, int c, int state)
{
    int t = p->of[c], k = p->at[c], len = p->len[t];
    if (state == THM_FULL) {
        for (int j = 0; j <= k; j++) st[p->cell[t][j]] = THM_FULL;
    } else if (state == THM_CROSS) {
        for (int j = k; j < len; j++) st[p->cell[t][j]] = THM_CROSS;
    } else if (st[c] == THM_FULL) {
        for (int j = k; j < len; j++)
            if (st[p->cell[t][j]] == THM_FULL) st[p->cell[t][j]] = THM_EMPTY;
    } else {
        st[c] = THM_EMPTY;
    }
}

void thm_apply(const thm_puzzle *p, uint16_t *st, int a, int b, int state)
{
    int n = p->n, was = st[a], run[THM_MAX], k = 0;
    int dx = (b % n > a % n) - (b % n < a % n), dy = (b / n > a / n) - (b / n < a / n);
    for (int c = a;; c += dy * n + dx) {
        if (st[c] == was) run[k++] = c;
        if (c == b || k >= THM_MAX) break;
    }
    for (int i = 0; i < k; i++)
        if (i == 0 || st[run[i]] == was) set_one(p, st, run[i], state);
}

/* the wrong entries on the board (the stars' mistakes) */
static int wrongs(const thm_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++) k += g->st[c] != THM_EMPTY && (g->st[c] == THM_FULL) != g->sol[c];
    return k;
}

int thm_act(thm_game *g, int a, int b, int state)
{
    int cells = g->p.n * g->p.n;
    uint16_t to[THM_CELLS];
    memcpy(to, g->st, sizeof to);
    thm_apply(&g->p, to, a, b, state);
    bench_begin(&g->hist, cells);
    for (int c = 0; c < cells; c++) bench_set(&g->hist, g->st, c, to[c]);
    return bench_done(&g->hist, wrongs(g));
}

static int hint_step(thm_game *g)
{
    int cells = g->p.n * g->p.n;
    for (int c = 0; c < cells; c++)             /* a wrong cell first: put right with its thermometer */
        if (g->st[c] != THM_EMPTY && (g->st[c] == THM_FULL) != g->sol[c]) {
            int t = g->p.of[c], first = -1;
            for (int k = 0; k < g->p.len[t] && first < 0; k++) {
                int x = g->p.cell[t][k];
                if (g->st[x] != THM_EMPTY && (g->st[x] == THM_FULL) != g->sol[x]) first = x;
            }
            return thm_act(g, first, first, g->sol[first] ? THM_FULL : THM_CROSS) ? first : -1;
        }
    bench_rules r = rules_of(&g->p, NULL);
    bench_grid d;
    grid_of(&g->p, &d);
    for (int c = 0; c < cells; c++)
        if (g->st[c] != THM_EMPTY) d.d[c] = (uint16_t)(g->sol[c] ? FULL : EMPTY);
    int c = bench_next(&d, &r);
    for (int i = 0; i < cells && c < 0; i++)
        if (g->st[i] == THM_EMPTY) c = i;
    if (c < 0) return -1;
    if (!g->sol[c]) {                           /* an empty cell: crossed, and those above it (all empty too) */
        return thm_act(g, c, c, THM_CROSS) ? c : -1;
    }
    return thm_act(g, c, c, THM_FULL) ? c : -1;
}

int thm_hint(thm_game *g) { return bench_hinted(&g->hist, hint_step(g)); }

int thm_undo(thm_game *g)
{
    int c = bench_undo_cells(&g->hist, g->st);
    bench_undone(&g->hist, wrongs(g));
    return c;
}

int thm_line_full(const thm_game *g, int way, int i)
{
    int k = 0;
    for (int j = 0; j < g->p.n; j++) k += g->st[at(g->p.n, way, i, j)] == THM_FULL;
    return k;
}

int thm_solved(const thm_game *g)
{
    int n = g->p.n;
    bench_grid d;
    memset(&d, 0, sizeof d);
    for (int c = 0; c < n * n; c++) d.d[c] = (uint16_t)(g->st[c] == THM_FULL ? FULL : EMPTY);
    if (!valid(&d, (void *)&g->p)) return 0;
    for (int way = 0; way < 2; way++)           /* the counts not told too (the one solution's) */
        for (int i = 0; i < n; i++) {
            int want = 0;
            for (int j = 0; j < n; j++) want += g->sol[at(n, way, i, j)];
            if (thm_line_full(g, way, i) != want) return 0;
        }
    return 1;
}

int thm_script(thm_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (thm_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if ((g->st[c] == THM_FULL) != g->sol[c]) {
                    thm_act(g, c, c, g->sol[c] ? THM_FULL : THM_EMPTY);
                    k--;
                }
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
        int to = hold || b != a ? thm_hold_state(g, a) : thm_tap_state(g, a);
        if (!thm_act(g, a, b, to)) return 0;
    }
    return 1;
}
