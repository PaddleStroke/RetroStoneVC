/*
 * Loop: the rules (loop.h): the sides, the dots and the numbers, the solver that grades (bench.h) and the one that
 * counts, the maker, and the player's marks with their undo. No SDL: tested by tests/loop_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/loop.h"

#include <string.h>

#include "games/puzrand.h"

#define OFF 1u                                  /* a side's values: bit 0 no line, bit 1 a line */
#define ON 2u
#define DOTS ((SLK_MAX + 1) * (SLK_MAX + 1))

/* ---- the grid's shape ------------------------------------------------------------------------------------------- */
/* a cell's corners (top left, top right, bottom right, bottom left): the two of its sides (0 top, 1 right, 2 bottom,
 * 3 left) meeting there */
static const int CORNER_SIDE[4][2] = {{0, 3}, {0, 1}, {1, 2}, {2, 3}};

typedef struct {
    const slk_puzzle *p;
    int n, sides;
    int16_t cell_side[SLK_CELLS][4];            /* top, right, bottom, left */
    int16_t dot_side[DOTS][4];                  /* left, right, up, down (-1: off the grid) */
    int16_t outer[SLK_CELLS][4][2];             /* at each corner, the two sides leaving the cell (-1: none) */
} ctx_t;

static void shape(ctx_t *x, const slk_puzzle *p)
{
    int n = p->n;
    x->p = p;
    x->n = n;
    x->sides = slk_sides(n);
    for (int y = 0; y < n; y++)
        for (int k = 0; k < n; k++) {
            int16_t *s = x->cell_side[y * n + k];
            s[0] = (int16_t)slk_across(n, k, y);
            s[1] = (int16_t)slk_down(n, k + 1, y);
            s[2] = (int16_t)slk_across(n, k, y + 1);
            s[3] = (int16_t)slk_down(n, k, y);
        }
    for (int y = 0; y <= n; y++)
        for (int k = 0; k <= n; k++) {
            int16_t *s = x->dot_side[y * (n + 1) + k];
            s[0] = (int16_t)(k > 0 ? slk_across(n, k - 1, y) : -1);
            s[1] = (int16_t)(k < n ? slk_across(n, k, y) : -1);
            s[2] = (int16_t)(y > 0 ? slk_down(n, k, y - 1) : -1);
            s[3] = (int16_t)(y < n ? slk_down(n, k, y) : -1);
        }
    for (int c = 0; c < n * n; c++)
        for (int corner = 0; corner < 4; corner++) {
            int dx = c % n + (corner == 1 || corner == 2), dy = c / n + (corner >= 2), m = 0;
            const int16_t *ds = x->dot_side[dy * (n + 1) + dx], *cs = x->cell_side[c];
            x->outer[c][corner][0] = x->outer[c][corner][1] = -1;
            for (int t = 0; t < 4; t++) {
                if (ds[t] < 0 || ds[t] == cs[CORNER_SIDE[corner][0]] || ds[t] == cs[CORNER_SIDE[corner][1]]) continue;
                x->outer[c][corner][m++] = ds[t];
            }
        }
}

/* ---- the solver ------------------------------------------------------------------------------------------------- */
static int cut(bench_grid *g, int s, unsigned keep)
{
    unsigned m = g->d[s] & keep;
    if (m == g->d[s]) return 0;
    g->d[s] = (uint16_t)m;
    return 1;
}

static int find(int *root, int a)
{
    while (root[a] != a) a = root[a] = root[root[a]];
    return a;
}

/* the lines drawn: their pieces (dots joined), each piece's lines, whether one is closed; -1 when a closed loop leaves
 * lines elsewhere; *closed: 1 when the lines are one closed loop */
static int pieces(const bench_grid *g, const ctx_t *x, int *root, int *lines, int *total, int *closed)
{
    int n = x->n, dots = (n + 1) * (n + 1), loops = 0, loop_root = -1;
    for (int d = 0; d < dots; d++) root[d] = d, lines[d] = 0;
    *total = 0;
    for (int s = 0; s < x->sides; s++) {
        if (g->d[s] != ON) continue;
        int x0, y0, x1, y1;
        slk_ends(n, s, &x0, &y0, &x1, &y1);
        int a = find(root, y0 * (n + 1) + x0), b = find(root, y1 * (n + 1) + x1);
        (*total)++;
        if (a == b) { loops++; lines[a]++; loop_root = a; continue; }
        root[a] = b;
        lines[b] += lines[a] + 1;
    }
    *closed = 0;
    if (!loops) return 0;
    if (loops > 1 || lines[find(root, loop_root)] != *total) return -1;
    *closed = 1;
    return 0;
}

static int deduce(bench_grid *g, int tier, void *cx)
{
    ctx_t *x = cx;
    const slk_puzzle *p = x->p;
    int n = x->n, found = 0;
    if (tier == 0) {
        for (int c = 0; c < n * n; c++) {       /* a number's sides */
            int want = p->clue[c];
            if (want < 0) continue;
            int on = 0, open = 0;
            for (int e = 0; e < 4; e++) {
                unsigned m = g->d[x->cell_side[c][e]];
                on += m == ON;
                open += m == (ON | OFF);
            }
            if (on > want || on + open < want) return -1;
            if (!open) continue;
            unsigned keep = on == want ? OFF : on + open == want ? ON : 3;
            if (keep != 3)
                for (int e = 0; e < 4; e++)
                    if (g->d[x->cell_side[c][e]] == (ON | OFF)) found |= cut(g, x->cell_side[c][e], keep);
        }
        for (int d = 0; d < (n + 1) * (n + 1); d++) {   /* a dot: two lines or none */
            int on = 0, open = 0;
            for (int t = 0; t < 4; t++) {
                int s = x->dot_side[d][t];
                if (s < 0) continue;
                on += g->d[s] == ON;
                open += g->d[s] == (ON | OFF);
            }
            if (on > 2 || (on == 1 && !open)) return -1;
            if (!open) continue;
            unsigned keep = on == 2 || (on == 0 && open == 1) ? OFF : on == 1 && open == 1 ? ON : 3;
            if (keep != 3)
                for (int t = 0; t < 4; t++)
                    if (x->dot_side[d][t] >= 0 && g->d[x->dot_side[d][t]] == (ON | OFF))
                        found |= cut(g, x->dot_side[d][t], keep);
        }
        if (found) return 1;
        int root[DOTS], lines[DOTS], total, closed;
        if (pieces(g, x, root, lines, &total, &closed) < 0) return -1;
        for (int s = 0; s < x->sides; s++) {    /* no loop closed too soon */
            if (g->d[s] != (ON | OFF)) continue;
            if (closed) { found |= cut(g, s, OFF); continue; }
            int x0, y0, x1, y1;
            slk_ends(n, s, &x0, &y0, &x1, &y1);
            int a = find(root, y0 * (n + 1) + x0);
            if (a != find(root, y1 * (n + 1) + x1)) continue;
            int ok = lines[a] == total;         /* it would close every line: the numbers must then be right */
            for (int c = 0; c < n * n && ok; c++) {
                if (p->clue[c] < 0) continue;
                int on = 0;
                for (int e = 0; e < 4; e++) on += g->d[x->cell_side[c][e]] == ON || x->cell_side[c][e] == s;
                ok = on == p->clue[c];
            }
            if (!ok) found |= cut(g, s, OFF);
        }
        return found;
    }
    for (int c = 0; c < n * n; c++) {           /* a number and its corners together */
        if (p->clue[c] < 0) continue;
        const int16_t *cs = x->cell_side[c];
        int open = 0;
        for (int e = 0; e < 4; e++) open += g->d[cs[e]] == (ON | OFF);
        for (int k = 0; k < 4; k++)
            for (int t = 0; t < 2; t++) open += x->outer[c][k][t] >= 0 && g->d[x->outer[c][k][t]] == (ON | OFF);
        if (!open) continue;
        /* each way of the four sides that makes the number; then each corner apart: the two sides leaving it that
         * give its dot two lines or none (the corners share no side, so each is worked out alone) */
        unsigned side_ok[4] = {0}, out_ok[4][2] = {{0}};
        for (unsigned a = 0; a < 16; a++) {
            if (bench_bits(a) != p->clue[c]) continue;
            int ok = 1;
            for (int e = 0; e < 4 && ok; e++) ok = (g->d[cs[e]] >> (a >> e & 1) & 1) != 0;
            unsigned corner_ok[4] = {0};        /* bit o: the outer pair o (bit t: side t a line) fits */
            for (int k = 0; k < 4 && ok; k++) {
                int deg = (a >> CORNER_SIDE[k][0] & 1) + (a >> CORNER_SIDE[k][1] & 1);
                for (unsigned o = 0; o < 4; o++) {
                    int fits = 1;
                    for (int t = 0; t < 2 && fits; t++) {
                        int s = x->outer[c][k][t], v = o >> t & 1;
                        fits = s < 0 ? !v : (g->d[s] >> v & 1) != 0;
                    }
                    int dd = deg + bench_bits(o);
                    if (fits && (dd == 0 || dd == 2)) corner_ok[k] |= 1u << o;
                }
                ok = corner_ok[k] != 0;
            }
            if (!ok) continue;
            for (int e = 0; e < 4; e++) side_ok[e] |= 1u << (a >> e & 1);
            for (int k = 0; k < 4; k++)
                for (unsigned o = 0; o < 4; o++)
                    if (corner_ok[k] >> o & 1)
                        for (int t = 0; t < 2; t++) out_ok[k][t] |= 1u << (o >> t & 1);
        }
        for (int e = 0; e < 4; e++) {
            found |= cut(g, cs[e], side_ok[e]);
            if (!g->d[cs[e]]) return -1;
        }
        for (int k = 0; k < 4; k++)
            for (int t = 0; t < 2; t++) {
                int s = x->outer[c][k][t];
                if (s < 0) continue;
                found |= cut(g, s, out_ok[k][t]);
                if (!g->d[s]) return -1;
            }
    }
    return found;
}

static int clues_met(const ctx_t *x, const uint8_t *on)
{
    for (int c = 0; c < x->n * x->n; c++) {
        if (x->p->clue[c] < 0) continue;
        int k = 0;
        for (int e = 0; e < 4; e++) k += on[x->cell_side[c][e]];
        if (k != x->p->clue[c]) return 0;
    }
    return 1;
}

static int valid(const bench_grid *g, void *cx)
{
    const ctx_t *x = cx;
    uint8_t on[SLK_SIDES];
    for (int s = 0; s < x->sides; s++) on[s] = g->d[s] == ON;
    return clues_met(x, on) && slk_loop_ok(x->n, on);
}

int slk_loop_ok(int n, const uint8_t *on)
{
    int dots = (n + 1) * (n + 1), deg[DOTS] = {0}, root[DOTS], total = 0, start = -1;
    for (int d = 0; d < dots; d++) root[d] = d;
    for (int s = 0; s < slk_sides(n); s++) {
        if (!on[s]) continue;
        int x0, y0, x1, y1;
        slk_ends(n, s, &x0, &y0, &x1, &y1);
        int a = y0 * (n + 1) + x0, b = y1 * (n + 1) + x1;
        deg[a]++, deg[b]++, total++;
        root[find(root, a)] = find(root, b);
        start = a;
    }
    if (!total) return 0;
    for (int d = 0; d < dots; d++)
        if ((deg[d] && deg[d] != 2) || (deg[d] && find(root, d) != find(root, start))) return 0;
    return 1;
}

static void rules_of(bench_rules *r, ctx_t *x, const slk_puzzle *p, bench_grid *g)
{
    shape(x, p);
    memset(r, 0, sizeof *r);
    r->vars = x->sides;
    r->tiers = 2;
    r->probe_tiers = 1;
    r->order = BENCH_PAIRS;                     /* the trials the makers were tuned with */
    r->deduce = deduce;
    r->valid = valid;
    r->ctx = x;
    r->budget = 30000;
    for (int s = 0; s < x->sides; s++) g->d[s] = ON | OFF;
}

static int grade(const slk_puzzle *p, int most, int trials, long budget)
{
    static bench_rules r;
    static ctx_t x;
    bench_grid g;
    rules_of(&r, &x, p, &g);
    r.no_trials = !trials;
    if (budget) r.budget = budget;
    return bench_grade(&g, &r, most);
}

int slk_rate(const slk_puzzle *p) { return grade(p, BENCH_HARD, 1, 0); }

static int count_rec(bench_grid *g, bench_rules *r, int limit)
{
    const ctx_t *x = r->ctx;
    int used = 0, n = x->n;
    if (bench_settle(g, r, 1, &used) < 0) return 0;
    int best = -1;
    for (int d = 0; d < (n + 1) * (n + 1) && best < 0; d++) {   /* a side at a line's loose end first */
        int on = 0, open = -1;
        for (int t = 0; t < 4; t++) {
            int s = x->dot_side[d][t];
            if (s < 0) continue;
            if (g->d[s] == ON) on++;
            else if (g->d[s] == (ON | OFF)) open = s;
        }
        if (on == 1 && open >= 0) best = open;
    }
    for (int s = 0; s < r->vars && best < 0; s++)
        if (!bench_one(g->d[s])) best = s;
    if (best < 0) return r->valid(g, r->ctx);
    int found = 0;
    for (unsigned m = ON; m && found < limit; m >>= 1) {    /* a line first */
        bench_grid t = *g;
        t.d[best] = (uint16_t)m;
        found += count_rec(&t, r, limit - found);
    }
    return found;
}

int slk_count(const slk_puzzle *p, int limit)
{
    static bench_rules r;
    static ctx_t x;
    bench_grid g;
    rules_of(&r, &x, p, &g);
    r.budget = 0;
    return count_rec(&g, &r, limit);
}

/* ---- the maker -------------------------------------------------------------------------------------------------- */
static int inside(const uint8_t *in, int n, int x, int y)
{
    return x >= 0 && y >= 0 && x < n && y < n && in[y * n + x];
}

/* the cell may join the region: no corner where it would touch itself, no hole left outside it */
static int may_join(uint8_t *in, int n, int c)
{
    int x = c % n, y = c / n;
    in[c] = 1;
    int ok = 1;
    for (int dy = 0; dy <= 1 && ok; dy++)       /* the four dots round the cell: no crossing pattern */
        for (int dx = 0; dx <= 1 && ok; dx++) {
            int a = inside(in, n, x + dx - 1, y + dy - 1), b = inside(in, n, x + dx, y + dy - 1);
            int e = inside(in, n, x + dx - 1, y + dy), f = inside(in, n, x + dx, y + dy);
            if (a == f && b == e && a != b) ok = 0;
        }
    if (ok) {                                   /* every cell outside reaches the grid's edge outside */
        int seen[SLK_CELLS] = {0}, stack[SLK_CELLS], sp = 0, out = 0, reached = 0;
        for (int k = 0; k < n * n; k++) {
            if (in[k]) continue;
            out++;
            if (k % n == 0 || k % n == n - 1 || k / n == 0 || k / n == n - 1) seen[k] = 1, stack[sp++] = k;
        }
        while (sp) {
            int k = stack[--sp];
            reached++;
            const int nb[4][2] = {{k % n - 1, k / n}, {k % n + 1, k / n}, {k % n, k / n - 1}, {k % n, k / n + 1}};
            for (int t = 0; t < 4; t++) {
                int ax = nb[t][0], ay = nb[t][1];
                if (ax < 0 || ay < 0 || ax >= n || ay >= n) continue;
                int q = ay * n + ax;
                if (!in[q] && !seen[q]) seen[q] = 1, stack[sp++] = q;
            }
        }
        ok = reached == out;
    }
    in[c] = (uint8_t)ok;
    return ok;
}

static void grow(uint32_t *rng, int n, slk_puzzle *p)
{
    uint8_t in[SLK_CELLS] = {0};
    int want = n * n * (40 + pr_below(rng, 16)) / 100, size = 1;
    in[pr_below(rng, n * n)] = 1;
    for (int tries = 0; size < want && tries < 60 * n * n; tries++) {
        int c = pr_below(rng, n * n);
        if (in[c]) continue;
        int x = c % n, y = c / n, around = inside(in, n, x - 1, y) + inside(in, n, x + 1, y) +
                                           inside(in, n, x, y - 1) + inside(in, n, x, y + 1);
        if (!around || (around > 1 && pr_below(rng, 3))) continue;   /* winding: one neighbour inside, mostly */
        if (may_join(in, n, c)) size++;
    }
    for (int s = 0; s < slk_sides(n); s++) {    /* the loop: the sides with the region on one side only */
        int x0, y0, x1, y1;
        slk_ends(n, s, &x0, &y0, &x1, &y1);
        int a, b;
        if (y0 == y1) a = inside(in, n, x0, y0 - 1), b = inside(in, n, x0, y0);
        else a = inside(in, n, x0 - 1, y0), b = inside(in, n, x0, y0);
        p->sol[s] = (uint8_t)(a != b);
    }
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n;
        p->clue[c] = (int8_t)(p->sol[slk_across(n, x, y)] + p->sol[slk_across(n, x, y + 1)] +
                              p->sol[slk_down(n, x, y)] + p->sol[slk_down(n, x + 1, y)]);
    }
}

static int make_one(uint32_t *rng, int n, int level, slk_puzzle *p)
{
    p->n = n;
    grow(rng, n, p);
    int order[SLK_CELLS], top = level < SLK_HARD ? level : SLK_MEDIUM;
    for (int c = 0; c < n * n; c++) order[c] = c;
    pr_shuffle(rng, order, n * n);
    for (int k = 0; k < n * n; k++) {           /* numbers taken away while the level's ways still solve it */
        int c = order[k], was = p->clue[c];
        p->clue[c] = -1;
        int r = grade(p, top, 0, 0);
        if (r < 0 || r == BENCH_GUESS) p->clue[c] = (int8_t)was;
    }
    int r = grade(p, top, 0, 0);
    if (level < SLK_HARD) return r == level;
    if (r < 0 || r == BENCH_GUESS) return 0;
    for (int k = 0, tries = 0; k < n * n && tries < 6; k++) {   /* hard: one more taken away, trials needed */
        int c = order[k], was = p->clue[c];
        if (was < 0) continue;
        p->clue[c] = -1;
        tries++;
        if (grade(p, SLK_HARD, 1, 8000) == SLK_HARD) return 1;
        p->clue[c] = (int8_t)was;
    }
    return 0;
}

void slk_make(int n, int level, uint32_t seed, slk_puzzle *p)
{
    uint32_t rng = pr_seed(seed);
    if (n < 3 || n > SLK_MAX) n = 5;
    if (level < 0 || level >= SLK_LEVELS) level = SLK_EASY;
    for (int tries = 0;; tries++) {
        if (make_one(&rng, n, level, p)) return;
        if (tries > 60 && level > SLK_EASY) level--, tries = 0;   /* never stuck */
    }
}

int slk_rate_work(const slk_puzzle *p, long *passes)
{
    static bench_rules r;
    static ctx_t x;
    bench_grid g;
    rules_of(&r, &x, p, &g);
    r.budget = 0;
    int res = bench_grade(&g, &r, BENCH_HARD);
    if (passes) *passes = r.passes;
    return res;
}

/* ---- the designed levels ---------------------------------------------------------------------------------------- */
static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

void slk_encode(const slk_puzzle *p, char *out)
{
    int n = p->n, k = 0, sides = slk_sides(n);
    if (n >= 10) out[k++] = (char)('0' + n / 10);
    out[k++] = (char)('0' + n % 10);
    out[k++] = ':';
    for (int c = 0; c < n * n; c++) out[k++] = p->clue[c] < 0 ? '.' : (char)('0' + p->clue[c]);
    out[k++] = ':';
    for (int s = 0; s < sides; s += 6) {
        int v = 0;
        for (int b = 0; b < 6 && s + b < sides; b++) v |= (p->sol[s + b] != 0) << b;
        out[k++] = B64[v];
    }
    out[k] = 0;
}

int slk_decode(slk_game *g, const char *s)
{
    memset(g, 0, sizeof *g);
    slk_puzzle *p = &g->p;
    int n = 0;
    while (*s >= '0' && *s <= '9') n = n * 10 + *s++ - '0';
    if (*s++ != ':' || n < 3 || n > SLK_MAX) return 0;
    p->n = n;
    for (int c = 0; c < n * n; c++) {
        if (s[c] != '.' && (s[c] < '0' || s[c] > '3')) return 0;
        p->clue[c] = (int8_t)(s[c] == '.' ? -1 : s[c] - '0');
    }
    s += n * n;
    if (*s++ != ':') return 0;
    int sides = slk_sides(n);
    for (int i = 0; i < (sides + 5) / 6; i++) {
        const char *at = s[i] ? strchr(B64, s[i]) : NULL;
        if (!at) return 0;
        for (int b = 0; b < 6 && 6 * i + b < sides; b++) p->sol[6 * i + b] = (uint8_t)((at - B64) >> b & 1);
    }
    if (s[(sides + 5) / 6] || !slk_loop_ok(n, p->sol)) return 0;
    for (int c = 0; c < n * n; c++) {           /* the numbers the loop's */
        int x = c % n, y = c / n;
        int k = p->sol[slk_across(n, x, y)] + p->sol[slk_across(n, x, y + 1)] + p->sol[slk_down(n, x, y)] +
                p->sol[slk_down(n, x + 1, y)];
        if (p->clue[c] >= 0 && p->clue[c] != k) return 0;
    }
    g->variant = 0;
    g->level = SLK_EASY;
    return 1;
}

/* ---- the player's marks ----------------------------------------------------------------------------------------- */
void slk_new(slk_game *g, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    if (variant < 0 || variant >= SLK_VARIANTS) variant = 0;
    g->variant = variant;
    g->level = slk_level_of(variant);
    g->seed = seed;
    slk_make(slk_size_of(variant), g->level, seed, &g->p);
    bench_clear(&g->hist);
}

static void restore(void *cx, int at, uint32_t was)
{
    slk_game *g = cx;
    g->side[at] = (uint8_t)was;
}

/* the wrong entries on the board (the stars' mistakes) */
static int wrongs(const slk_game *g)
{
    int k = 0;
    for (int c = 0; c < slk_sides(g->p.n); c++)
        k += (g->side[c] == SLK_LINE && !g->p.sol[c]) || (g->side[c] == SLK_CROSS && g->p.sol[c]);
    return k;
}

int slk_set(slk_game *g, int s, int state)
{
    if (s < 0 || s >= slk_sides(g->p.n) || state < 0 || state > 2 || g->side[s] == state) return 0;
    bench_begin(&g->hist, 1);
    bench_log(&g->hist, s, g->side[s]);
    g->side[s] = (uint8_t)state;
    return bench_done(&g->hist, wrongs(g));
}

int slk_cycle(slk_game *g, int s)
{
    if (s < 0 || s >= slk_sides(g->p.n)) return 0;
    return slk_set(g, s, (g->side[s] + 1) % 3);
}

int slk_stroke(slk_game *g, int s, int state, int first)
{
    if (first || !g->hist.n) return slk_set(g, s, state);
    if (s < 0 || s >= slk_sides(g->p.n) || state < 0 || state > 2 || g->side[s] == state) return 0;
    bench_begin(&g->hist, 1);
    g->hist.open = 0;                           /* joined to the action before */
    bench_log(&g->hist, s, g->side[s]);
    g->side[s] = (uint8_t)state;
    g->hist.wrongs = wrongs(g);
    return 1;
}

int slk_lines(const slk_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n;
    return (g->side[slk_across(n, x, y)] == SLK_LINE) + (g->side[slk_across(n, x, y + 1)] == SLK_LINE) +
           (g->side[slk_down(n, x, y)] == SLK_LINE) + (g->side[slk_down(n, x + 1, y)] == SLK_LINE);
}

int slk_dot_lines(const slk_game *g, int x, int y)
{
    int n = g->p.n, k = 0;
    if (x > 0) k += g->side[slk_across(n, x - 1, y)] == SLK_LINE;
    if (x < n) k += g->side[slk_across(n, x, y)] == SLK_LINE;
    if (y > 0) k += g->side[slk_down(n, x, y - 1)] == SLK_LINE;
    if (y < n) k += g->side[slk_down(n, x, y)] == SLK_LINE;
    return k;
}

static int hint_step(slk_game *g)
{
    const slk_puzzle *p = &g->p;
    int S = slk_sides(p->n);
    for (int s = 0; s < S; s++)                 /* a wrong mark first: a line off the loop, a cross on it */
        if ((g->side[s] == SLK_LINE && !p->sol[s]) || (g->side[s] == SLK_CROSS && p->sol[s]))
            return slk_set(g, s, p->sol[s] ? SLK_LINE : SLK_CROSS) ? s : -1;
    static bench_rules r;
    static ctx_t x;
    bench_grid d;
    rules_of(&r, &x, p, &d);
    for (int s = 0; s < S; s++)
        if (g->side[s]) d.d[s] = g->side[s] == SLK_LINE ? ON : OFF;
    int used = 0;
    bench_settle(&d, &r, 1, &used);
    int best = -1;
    for (int pass = 0; pass < 3 && best < 0; pass++)   /* a line reasoning finds, a cross, else any line missing */
        for (int s = 0; s < S && best < 0; s++) {
            if (pass == 0 && !g->side[s] && d.d[s] == ON) best = s;
            if (pass == 1 && !g->side[s] && d.d[s] == OFF) best = s;
            if (pass == 2 && !g->side[s] && p->sol[s]) best = s;
        }
    if (best < 0) return -1;
    return slk_set(g, best, p->sol[best] ? SLK_LINE : SLK_CROSS) ? best : -1;
}

int slk_hint(slk_game *g)
{
    return bench_hinted(&g->hist, hint_step(g));
}

int slk_undo(slk_game *g)
{
    int c = bench_undo(&g->hist, restore, g);
    bench_undone(&g->hist, wrongs(g));
    return c;
}


int slk_solved(const slk_game *g)
{
    for (int s = 0; s < slk_sides(g->p.n); s++)
        if ((g->side[s] == SLK_LINE) != g->p.sol[s]) return 0;
    return 1;
}

int slk_script(slk_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (slk_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int e = 0; e < slk_sides(n) && k > 0; e++)
                if (g->p.sol[e] && g->side[e] != SLK_LINE) { slk_set(g, e, SLK_LINE); k--; }
            continue;
        }
        int la, lb, a = bench_cell(s, n + 1, n + 1, &la);
        if (a < 0 || s[la] != '-') return 0;
        int b = bench_cell(s + la + 1, n + 1, n + 1, &lb);
        if (b < 0) return 0;
        s += la + 1 + lb;
        int ax = a % (n + 1), ay = a / (n + 1), bx = b % (n + 1), by = b / (n + 1), side;
        if (ay == by && (bx - ax == 1 || ax - bx == 1)) side = slk_across(n, ax < bx ? ax : bx, ay);
        else if (ax == bx && (by - ay == 1 || ay - by == 1)) side = slk_down(n, ax, ay < by ? ay : by);
        else return 0;
        if (*s == '=') {
            if (s[1] < '0' || s[1] > '2' || !slk_set(g, side, s[1] - '0')) return 0;
            s += 2;
        } else if (!slk_cycle(g, side)) {
            return 0;
        }
    }
    return 1;
}
