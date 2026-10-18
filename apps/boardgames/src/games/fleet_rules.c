/*
 * Hidden Fleet: the rules (fleet.h): the ways of reasoning the solver grades with, the maker, the player's grid and
 * undo. No SDL: tested by tests/fleet_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/fleet.h"

#include <stdio.h>
#include <string.h>

#include "games/puzrand.h"

#define WATER 1u                                /* a cell's values: bit 0 water, bit 1 a ship cell */
#define SHIP 2u
#define BOTH 3u

static const int SIZES[HFL_SIZES] = {6, 8, 10};
static const char SHOWN[] = ".~o^v<>=";         /* HFL_NONE .. HFL_MIDDLE in the levels' code */

int hfl_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % HFL_SIZES]; }
int hfl_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

int hfl_fleet(int n, int *count)
{
    static const int8_t F[5][HFL_LONG + 1] = {
        {0, 3, 2, 1, 0},                        /* 6 x 6 and less */
        {0, 3, 2, 2, 0},                        /* 7 x 7 */
        {0, 3, 2, 2, 1},                        /* 8 x 8 */
        {0, 3, 3, 2, 1},                        /* 9 x 9 */
        {0, 4, 3, 2, 1},                        /* 10 x 10 */
    };
    int i = n <= 6 ? 0 : n >= 10 ? 4 : n - 6, ships = 0;
    for (int l = 0; l <= HFL_LONG; l++) {
        count[l] = F[i][l];
        if (l) ships += F[i][l];
    }
    return ships;
}

int hfl_part(const uint8_t *sh, int n, int c)
{
    int x = c % n, y = c / n;
    int up = y > 0 && sh[c - n], down = y < n - 1 && sh[c + n];
    int left = x > 0 && sh[c - 1], right = x < n - 1 && sh[c + 1];
    if (up + down + left + right == 0) return HFL_ONE;
    if (up + down + left + right > 1) return HFL_MIDDLE;
    return down ? HFL_TOP : up ? HFL_BOTTOM : right ? HFL_LEFT : HFL_RIGHT;
}

/* ---- what the solver knows of a puzzle -------------------------------------------------------------------------- */
typedef struct {
    const hfl_puzzle *p;
    int n, cells, passes, from;                 /* passes: a trial's, at the level graded */
    long *work;                                 /* the solver's work counted (each trial's passes too) */
    int fleet[HFL_LONG + 1], longest;
} ctx;

static void ctx_make(ctx *k, const hfl_puzzle *p)
{
    memset(k, 0, sizeof *k);
    k->p = p;
    k->n = p->n;
    k->cells = p->n * p->n;
    hfl_fleet(p->n, k->fleet);
    for (int l = 1; l <= HFL_LONG; l++)
        if (k->fleet[l]) k->longest = l;
}

static void force(bench_grid *g, int c, unsigned to) { g->d[c] &= (uint16_t)to; }

static void grid_of(const ctx *k, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < k->cells; c++) {
        int s = k->p->given[c];
        g->d[c] = (uint16_t)(s == HFL_NONE ? BOTH : s == HFL_SEA ? WATER : SHIP);
    }
}

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

/* the cell beside c the way d (0 up, 1 down, 2 left, 3 right); -1: the rim */
static int side(int n, int c, int d)
{
    int x = c % n, y = c / n;
    if (d == 0) return y > 0 ? c - n : -1;
    if (d == 1) return y < n - 1 ? c + n : -1;
    if (d == 2) return x > 0 ? c - 1 : -1;
    return x < n - 1 ? c + 1 : -1;
}

static int sea(const uint16_t *w, int c) { return c < 0 || w[c] == WATER; }   /* water, or the rim */

/* the plain ways: round a ship cell's corners water; a ship cell beside another keeps the other two sides water; the
 * shown parts' sides; each line's count; the ships whole (more than the fleet has: a contradiction), a ship that can
 * grow no longer ends in water; a cell that would join a ship longer than any left is water */
static int plain(bench_grid *g, const ctx *k)
{
    int n = k->n;
    uint16_t w[HFL_CELLS];
    memcpy(w, g->d, (size_t)k->cells * sizeof w[0]);
    for (int c = 0; c < k->cells; c++) {
        int x = c % n, y = c / n;
        if (w[c] == SHIP) {
            for (int dy = -1; dy <= 1; dy += 2)
                for (int dx = -1; dx <= 1; dx += 2)
                    if (x + dx >= 0 && x + dx < n && y + dy >= 0 && y + dy < n) force(g, c + dy * n + dx, WATER);
            int across = (x > 0 && w[c - 1] == SHIP) || (x < n - 1 && w[c + 1] == SHIP);
            int down = (y > 0 && w[c - n] == SHIP) || (y < n - 1 && w[c + n] == SHIP);
            if (across && down) return -1;
            for (int d = 0; d < 4; d++) {
                int o = side(n, c, d);
                if (o >= 0 && ((across && d < 2) || (down && d >= 2))) force(g, o, WATER);
            }
        }
        int s = k->p->given[c];
        if (s <= HFL_SEA) continue;
        int on = s == HFL_TOP ? 1 : s == HFL_BOTTOM ? 0 : s == HFL_LEFT ? 3 : s == HFL_RIGHT ? 2 : -1;
        if (s == HFL_ONE || on >= 0) {          /* a one-cell ship, an end: water all round but its way on */
            for (int d = 0; d < 4; d++) {
                int o = side(n, c, d);
                if (d == on) {
                    if (o < 0) return -1;
                    force(g, o, SHIP);
                } else if (o >= 0) {
                    force(g, o, WATER);
                }
            }
            continue;
        }
        int across = !sea(w, side(n, c, 2)) && !sea(w, side(n, c, 3));   /* a middle: one way or the other */
        int down = !sea(w, side(n, c, 0)) && !sea(w, side(n, c, 1));
        if (!across && !down) return -1;
        if (across && down) continue;
        for (int d = 0; d < 4; d++)
            if (side(n, c, d) >= 0) force(g, side(n, c, d), (d >= 2) == across ? SHIP : WATER);
    }
    for (int way = 0; way < 2; way++)           /* the lines' counts */
        for (int i = 0; i < n; i++) {
            int want = way ? k->p->col[i] : k->p->row[i], s = 0, u = 0;
            if (want < 0) continue;
            for (int j = 0; j < n; j++) {
                int c = way ? j * n + i : i * n + j;
                s += w[c] == SHIP;
                u += w[c] == BOTH;
            }
            if (s > want || s + u < want) return -1;
            if (!u || (s < want && s + u > want)) continue;
            for (int j = 0; j < n; j++) {
                int c = way ? j * n + i : i * n + j;
                if (w[c] == BOTH) force(g, c, s == want ? WATER : SHIP);
            }
        }
    /* the ships so far: each run of ship cells, whole when water or the rim closes it all round */
    int whole[HFL_LONG + 1] = {0}, seen[HFL_CELLS], open[HFL_CELLS][3], no = 0;
    memset(seen, 0, (size_t)k->cells * sizeof seen[0]);
    for (int c = 0; c < k->cells; c++) {
        if (w[c] != SHIP || seen[c]) continue;
        int across = (c % n < n - 1 && w[c + 1] == SHIP), len = 0, e = c;
        for (int d = c;; d += across ? 1 : n) {   /* c is its first cell (reading order) */
            seen[d] = 1;
            len++;
            e = d;
            int nx = across ? (d % n < n - 1 ? d + 1 : -1) : (d / n < n - 1 ? d + n : -1);
            if (nx < 0 || w[nx] != SHIP) break;
        }
        if (len > k->longest) return -1;
        int closed = len == 1 ? sea(w, side(n, c, 0)) && sea(w, side(n, c, 1)) && sea(w, side(n, c, 2)) &&
                                    sea(w, side(n, c, 3))
                              : sea(w, side(n, c, across ? 2 : 0)) && sea(w, side(n, e, across ? 3 : 1));
        if (closed) {
            whole[len]++;
            continue;
        }
        open[no][0] = c;
        open[no][1] = e;
        open[no++][2] = len * 2 + (len > 1 && across);
    }
    int left[HFL_LONG + 1], most = 0;           /* the ships left; the longest of them */
    for (int l = 1; l <= HFL_LONG; l++) {
        left[l] = k->fleet[l] - whole[l];
        if (left[l] < 0) return -1;
        if (left[l]) most = l;
    }
    for (int i = 0; i < no; i++) {              /* an open ship as long as the longest left: closed */
        int len = open[i][2] / 2, across = open[i][2] & 1, a = open[i][0], b = open[i][1];
        if (len > most) return -1;
        if (len < most) continue;
        if (len == 1) {
            for (int d = 0; d < 4; d++)
                if (side(n, a, d) >= 0) force(g, side(n, a, d), WATER);
        } else {
            if (side(n, a, across ? 2 : 0) >= 0) force(g, side(n, a, across ? 2 : 0), WATER);
            if (side(n, b, across ? 3 : 1) >= 0) force(g, side(n, b, across ? 3 : 1), WATER);
        }
    }
    for (int c = 0; c < k->cells; c++) {        /* a cell joining a ship longer than any left */
        if (w[c] != BOTH) continue;
        for (int across = 0; across < 2; across++) {
            int len = 1;
            for (int d = side(n, c, across ? 2 : 0); d >= 0 && w[d] == SHIP; d = side(n, d, across ? 2 : 0)) len++;
            for (int d = side(n, c, across ? 3 : 1); d >= 0 && w[d] == SHIP; d = side(n, d, across ? 3 : 1)) len++;
            if (len > most) force(g, c, WATER);
        }
    }
    return settled(w, g, k->cells);
}

/* trials: each unknown cell tried each way, the plain ways followed `passes` times; a contradiction rules that way
 * out (one pass: what a glance shows; a few: a short chain). A few finds, then the plain ways first again */
static int trials(bench_grid *g, ctx *k, int passes)
{
    uint16_t w[HFL_CELLS];
    bench_grid t;
    memcpy(w, g->d, (size_t)k->cells * sizeof w[0]);
    for (int i = 0, start = k->from, found = 0; i < k->cells; i++) {
        int c = (start + i) % k->cells;         /* from the last find on */
        if (w[c] != BOTH) continue;
        for (unsigned v = WATER; v <= SHIP; v++) {
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

static long finds;                              /* the trials' finds (hfl_effort counts them) */

static int deduce(bench_grid *g, int tier, void *cv)
{
    ctx *k = cv;
    int res = tier ? trials(g, k, k->passes) : plain(g, k);
    if (res > 0 && tier) finds++;
    return res;
}

/* a full grid's ship cells (sh) keep every rule: straight ships of the fleet, none touching, the counts, the cells
 * shown */
static int valid_cells(const ctx *k, const uint8_t *sh)
{
    int n = k->n, ships[HFL_LONG + 1] = {0};
    uint8_t seen[HFL_CELLS];
    memset(seen, 0, (size_t)k->cells);
    for (int i = 0; i < n; i++) {
        int r = 0, col = 0;
        for (int j = 0; j < n; j++) r += sh[i * n + j], col += sh[j * n + i];
        if ((k->p->row[i] >= 0 && r != k->p->row[i]) || (k->p->col[i] >= 0 && col != k->p->col[i])) return 0;
    }
    for (int c = 0; c < k->cells; c++) {
        int s = k->p->given[c], x = c % n, y = c / n;
        if (s == HFL_SEA && sh[c]) return 0;
        if (s > HFL_SEA && (!sh[c] || hfl_part(sh, n, c) != s)) return 0;
        if (!sh[c]) continue;
        if ((x > 0 && y > 0 && sh[c - n - 1]) || (x < n - 1 && y > 0 && sh[c - n + 1])) return 0;
        if (seen[c]) continue;
        int across = x < n - 1 && sh[c + 1], len = 0;
        for (int d = c; d >= 0 && sh[d]; d = side(n, d, across ? 3 : 1)) seen[d] = 1, len++;
        if (len > HFL_LONG) return 0;
        ships[len]++;
    }
    for (int l = 1; l <= HFL_LONG; l++)
        if (ships[l] != k->fleet[l]) return 0;
    return 1;
}

static int valid(const bench_grid *g, void *cv)
{
    const ctx *k = cv;
    uint8_t sh[HFL_CELLS];
    for (int c = 0; c < k->cells; c++) sh[c] = g->d[c] == SHIP;
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
static int run(const hfl_puzzle *p, int level, bench_grid *g, long *work, long budget, long *passes)
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
static int grade_work(const hfl_puzzle *p, uint8_t *sol, long *work, int cap, long budget, long *passes)
{
    bench_grid g;
    for (int lv = BENCH_EASY; lv <= cap; lv++) {
        finds = 0;
        int res = run(p, lv, &g, work, budget, passes);
        if (res == BENCH_NONE) return BENCH_NONE;
        if (res != BENCH_EASY) continue;
        if (sol)
            for (int c = 0; c < p->n * p->n; c++) sol[c] = g.d[c] == SHIP;
        return lv;
    }
    return BENCH_GUESS;
}

int hfl_grade(const hfl_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, BENCH_HARD, 0, NULL); }

int hfl_count(const hfl_puzzle *p, int limit)
{
    static ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, -1, NULL);
    bench_grid g;
    grid_of(&k, &g);
    return bench_search(&g, &r, limit, NULL, NULL, 0);
}

int hfl_effort(const hfl_puzzle *p, long *passes, long *trial_finds)
{
    int lv = grade_work(p, NULL, NULL, BENCH_HARD, 0, passes);
    if (trial_finds) *trial_finds = finds;
    return lv;
}

/* ---- a puzzle as text (the levels) ------------------------------------------------------------------------------- */
static char digit(int v) { return (char)(v < 10 ? '0' + v : 'a' + v - 10); }
static int undigit(char ch) { return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'z' ? ch - 'a' + 10 : -1; }

int hfl_encode(const hfl_puzzle *p, char *out, int size)
{
    int n = p->n, len = snprintf(out, (size_t)size, "%d:", n), shown = 0;
    for (int c = 0; c < n * n; c++) shown += p->given[c] != HFL_NONE;
    if (len < 0 || len + 2 * n + 2 + 3 * shown > size) return 0;
    for (int i = 0; i < n; i++) out[len++] = digit(p->row[i]);
    for (int i = 0; i < n; i++) out[len++] = digit(p->col[i]);
    out[len++] = ':';
    for (int c = 0; c < n * n; c++) {
        if (p->given[c] == HFL_NONE) continue;
        out[len++] = (char)('a' + c % n);
        out[len++] = (char)('a' + c / n);
        out[len++] = SHOWN[p->given[c]];
    }
    out[len] = 0;
    return len;
}

int hfl_decode(const char *s, hfl_puzzle *p)
{
    memset(p, 0, sizeof *p);
    int n = 0;
    for (; *s >= '0' && *s <= '9'; s++) n = n * 10 + *s - '0';
    if (*s++ != ':' || n < 4 || n > HFL_MAX || (int)strlen(s) < 2 * n + 1) return 0;
    p->n = n;
    for (int i = 0; i < 2 * n; i++) {
        int v = undigit(s[i]);
        if (v < 0 || v > n) return 0;
        if (i < n) p->row[i] = (int8_t)v;
        else p->col[i - n] = (int8_t)v;
    }
    s += 2 * n;
    if (*s++ != ':') return 0;
    for (; *s; s += 3) {
        if (!s[1] || !s[2]) return 0;
        int x = s[0] - 'a', y = s[1] - 'a';
        const char *t = strchr(SHOWN, s[2]);
        if (x < 0 || y < 0 || x >= n || y >= n || !t || t == SHOWN) return 0;
        p->given[y * n + x] = (uint8_t)(t - SHOWN);
    }
    return 1;
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
/* the fleet laid by chance, the longest ships first, none touching another (not even at a corner); 0: no room */
static int lay(int n, uint8_t *sh, uint32_t *s)
{
    int fleet[HFL_LONG + 1];
    hfl_fleet(n, fleet);
    for (int again = 0; again < 50; again++) {
        memset(sh, 0, HFL_CELLS);
        int ok = 1;
        for (int l = HFL_LONG; l >= 1 && ok; l--)
            for (int i = 0; i < fleet[l] && ok; i++) {
                ok = 0;
                for (int t = 0; t < 200 && !ok; t++) {
                    int across = pr_below(s, 2), x = pr_below(s, across ? n - l + 1 : n);
                    int y = pr_below(s, across ? n : n - l + 1), room = 1;
                    for (int j = 0; j < l && room; j++) {
                        int cx = x + (across ? j : 0), cy = y + (across ? 0 : j);
                        for (int dy = -1; dy <= 1 && room; dy++)
                            for (int dx = -1; dx <= 1 && room; dx++) {
                                int ax = cx + dx, ay = cy + dy;
                                if (ax >= 0 && ay >= 0 && ax < n && ay < n && sh[ay * n + ax]) room = 0;
                            }
                    }
                    if (!room) continue;
                    for (int j = 0; j < l; j++) sh[(y + (across ? 0 : j)) * n + x + (across ? j : 0)] = 1;
                    ok = 1;
                }
            }
        if (ok) return 1;
    }
    return 0;
}

typedef struct {
    hfl_puzzle *p;
    uint8_t full[HFL_CELLS];
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
    R->p->given[i] = on ? R->full[i] : HFL_NONE;
}

#define MAKE_BUDGET 15000L                      /* the solver's passes a puzzle may cost (well under a second) */

/* a puzzle made: the fleet by chance, the counts, every ship cell shown, then shown cells taken away while it stays
 * at its level; the nearest level when that one is not found */
void hfl_make_ex(int n, int level, uint32_t seed, long budget, hfl_puzzle *p, uint8_t *sol)
{
    if (n < 5 || n > HFL_MAX) n = 6;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    static hfl_puzzle best, q;
    uint8_t sh[HFL_CELLS];
    int best_lv = -2;
    memset(&best, 0, sizeof best);
    for (int attempt = 0; attempt < 60 && work < 3 * budget; attempt++) {
        if (!lay(n, sh, &s)) continue;
        memset(&q, 0, sizeof q);
        q.n = n;
        for (int c = 0; c < n * n; c++) {
            q.row[c / n] = (int8_t)(q.row[c / n] + sh[c]);
            q.col[c % n] = (int8_t)(q.col[c % n] + sh[c]);
            q.given[c] = sh[c] ? (uint8_t)hfl_part(sh, n, c) : HFL_NONE;
        }
        reducer R = {&q, {0}, &work};
        memcpy(R.full, q.given, sizeof R.full);
        int order[HFL_CELLS], no = 0;
        for (int c = 0; c < n * n; c++)
            if (sh[c]) order[no++] = c;
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
    *p = best;
    if (sol) grade_work(p, sol, NULL, BENCH_HARD, 0, NULL);
}

void hfl_make(int n, int level, uint32_t seed, hfl_puzzle *p, uint8_t *sol)
{
    hfl_make_ex(n, level, seed, MAKE_BUDGET, p, sol);
}

/* ---- the player's grid ------------------------------------------------------------------------------------------- */
static void shown(hfl_game *g)                  /* the shown cells as they show */
{
    for (int c = 0; c < g->p.n * g->p.n; c++)
        if (g->p.given[c]) g->st[c] = g->p.given[c] == HFL_SEA ? HFL_WATER : HFL_SHIP;
}

int hfl_load(hfl_game *g, const hfl_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = hfl_grade(p, g->sol);
    shown(g);
    return g->level >= BENCH_EASY && g->level <= BENCH_HARD;
}

void hfl_new(hfl_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= HFL_VARIANTS) variant = 0;
    hfl_puzzle p;
    uint8_t sol[HFL_CELLS];
    hfl_make(hfl_size(variant), hfl_level(variant), seed, &p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = hfl_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
    shown(g);
}

int hfl_free(const hfl_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n && !g->p.given[c]; }
int hfl_tap_state(const hfl_game *g, int c) { return g->st[c] == HFL_SHIP ? HFL_EMPTY : HFL_SHIP; }
int hfl_hold_state(const hfl_game *g, int c) { return g->st[c] == HFL_WATER ? HFL_EMPTY : HFL_WATER; }

static int wrongs(const hfl_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++)
        k += hfl_free(g, c) && g->st[c] != HFL_EMPTY && (g->st[c] == HFL_SHIP) != g->sol[c];
    return k;
}

int hfl_paint(hfl_game *g, const int *cells, int n, int state)
{
    bench_begin(&g->hist, n);
    for (int i = 0; i < n; i++)
        if (hfl_free(g, cells[i])) bench_set(&g->hist, g->st, cells[i], state);
    return bench_done(&g->hist, wrongs(g));
}

static int put(hfl_game *g, int c, int state) { return hfl_paint(g, &c, 1, state); }

static int hint_step(hfl_game *g)
{
    int cells = g->p.n * g->p.n;
    for (int c = 0; c < cells; c++)             /* a wrong cell first */
        if (hfl_free(g, c) && g->st[c] != HFL_EMPTY && (g->st[c] == HFL_SHIP) != g->sol[c])
            return put(g, c, g->sol[c] ? HFL_SHIP : HFL_WATER) ? c : -1;
    static ctx k;
    ctx_make(&k, &g->p);
    bench_rules r = rules_of(&k, BENCH_HARD, NULL);
    bench_grid d;
    grid_of(&k, &d);
    for (int c = 0; c < cells; c++)
        if (g->st[c] != HFL_EMPTY) d.d[c] = (uint16_t)(g->sol[c] ? SHIP : WATER);
    int c = bench_next(&d, &r);
    if (c >= 0 && g->st[c] != HFL_EMPTY) c = -1;
    for (int i = 0; i < cells && c < 0; i++)
        if (g->st[i] == HFL_EMPTY && g->sol[i]) c = i;
    for (int i = 0; i < cells && c < 0; i++)
        if (g->st[i] == HFL_EMPTY) c = i;
    if (c < 0) return -1;
    return put(g, c, g->sol[c] ? HFL_SHIP : HFL_WATER) ? c : -1;
}

int hfl_hint(hfl_game *g) { return bench_hinted(&g->hist, hint_step(g)); }

int hfl_undo(hfl_game *g)
{
    int c = bench_undo_cells(&g->hist, g->st);
    bench_undone(&g->hist, wrongs(g));
    return c;
}

int hfl_line(const hfl_game *g, int way, int i)
{
    int n = g->p.n, s = 0;
    for (int j = 0; j < n; j++) s += g->st[way ? j * n + i : i * n + j] == HFL_SHIP;
    return s;
}

int hfl_clash(const hfl_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n, fleet[HFL_LONG + 1], longest = 1;
    if (g->st[c] != HFL_SHIP) return 0;
    for (int dy = -1; dy <= 1; dy += 2)         /* a corner touched */
        for (int dx = -1; dx <= 1; dx += 2)
            if (x + dx >= 0 && x + dx < n && y + dy >= 0 && y + dy < n && g->st[c + dy * n + dx] == HFL_SHIP) return 1;
    hfl_fleet(n, fleet);
    for (int l = 1; l <= HFL_LONG; l++)
        if (fleet[l]) longest = l;
    for (int across = 0; across < 2; across++) {   /* a ship too long */
        int len = 1;
        for (int d = side(n, c, across ? 2 : 0); d >= 0 && g->st[d] == HFL_SHIP; d = side(n, d, across ? 2 : 0)) len++;
        for (int d = side(n, c, across ? 3 : 1); d >= 0 && g->st[d] == HFL_SHIP; d = side(n, d, across ? 3 : 1)) len++;
        if (len > longest) return 1;
    }
    return 0;
}

void hfl_found(const hfl_game *g, int *count)
{
    int n = g->p.n, fleet[HFL_LONG + 1], longest = 1;
    uint8_t seen[HFL_CELLS];
    memset(seen, 0, sizeof seen);
    hfl_fleet(n, fleet);
    for (int l = 0; l <= HFL_LONG; l++) {
        count[l] = 0;
        if (l && fleet[l]) longest = l;
    }
    for (int c = 0; c < n * n; c++) {
        if (g->st[c] != HFL_SHIP || seen[c]) continue;
        int x = c % n, across = x < n - 1 && g->st[c + 1] == HFL_SHIP, len = 0, e = c, ok = 1;
        for (int d = c; d >= 0 && g->st[d] == HFL_SHIP; d = side(n, d, across ? 3 : 1)) seen[d] = 1, len++, e = d;
        for (int d = c;; d = side(n, d, across ? 3 : 1)) {   /* water or the rim on every side */
            for (int j = 0; j < 4; j++) {
                int o = side(n, d, j);
                if (o >= 0 && g->st[o] != HFL_WATER && !(g->st[o] == HFL_SHIP && (o == side(n, d, across ? 3 : 1) ||
                                                                                   o == side(n, d, across ? 2 : 0))))
                    ok = 0;
            }
            if (d == e) break;
        }
        if ((ok || len == longest) && len <= HFL_LONG) count[len]++;   /* the longest: closed by its length */
    }
}

int hfl_solved(const hfl_game *g)
{
    static ctx k;
    ctx_make(&k, &g->p);
    uint8_t sh[HFL_CELLS];
    for (int c = 0; c < k.cells; c++) sh[c] = g->st[c] == HFL_SHIP;
    return valid_cells(&k, sh);
}

int hfl_script(hfl_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (hfl_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if (hfl_free(g, c) && (g->st[c] == HFL_SHIP) != g->sol[c])
                    put(g, c, g->sol[c] ? HFL_SHIP : HFL_EMPTY), k--;
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
            hold = 1;                           /* a drag: water in or out */
        }
        int was = g->st[a], to = hold ? hfl_hold_state(g, a) : hfl_tap_state(g, a), cells[HFL_MAX], k = 0;
        int dx = (b % n > a % n) - (b % n < a % n), dy = (b / n > a / n) - (b / n < a / n);
        for (int c = a;; c += dy * n + dx) {
            if (g->st[c] == was && hfl_free(g, c)) cells[k++] = c;
            if (c == b) break;
        }
        if (!hfl_paint(g, cells, k, to)) return 0;
    }
    return 1;
}
