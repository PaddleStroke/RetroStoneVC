/*
 * Star Battle: the rules (starbattle.h): the grader that reasons as a person does, the solver that counts solutions,
 * the maker, and the player's marks with undo. No SDL: tested by tests/starbattle_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/starbattle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"

#define UNITS (3 * SBX_MAX)

/* ---- sets of cells (128 bits: a grid of 10 x 10 at most) -------------------------------------------------------- */
typedef struct { uint64_t lo, hi; } cells;

static cells c_or(cells a, cells b) { return (cells){a.lo | b.lo, a.hi | b.hi}; }
static cells c_and(cells a, cells b) { return (cells){a.lo & b.lo, a.hi & b.hi}; }
static cells c_not(cells a, cells all) { return (cells){~a.lo & all.lo, ~a.hi & all.hi}; }
static int c_any(cells a) { return (a.lo | a.hi) != 0; }
static int c_count(cells a) { return __builtin_popcountll(a.lo) + __builtin_popcountll(a.hi); }
static int c_has(cells a, int c) { return (int)((c < 64 ? a.lo >> c : a.hi >> (c - 64)) & 1); }
static cells c_one(int c) { return c < 64 ? (cells){1ull << c, 0} : (cells){0, 1ull << (c - 64)}; }
static int c_first(cells a) { return a.lo ? __builtin_ctzll(a.lo) : a.hi ? 64 + __builtin_ctzll(a.hi) : -1; }
static cells c_drop(cells a, int c) { return c < 64 ? (cells){a.lo & ~(1ull << c), a.hi} : (cells){a.lo, a.hi & ~(1ull << (c - 64))}; }

/* the rows (0..n-1), the columns (n..2n-1) and the regions (2n..3n-1) of the puzzle in hand */
static struct {
    int n, k;
    cells all, unit[UNITS], round[SBX_CELLS];   /* round: the eight cells about a cell */
    uint8_t of[SBX_CELLS][3];                   /* a cell's row, column and region */
} T;

static void tables_load(const sbx_puz *p)
{
    int n = p->n;
    T.n = n;
    T.k = p->k;
    memset(T.unit, 0, sizeof T.unit);
    memset(&T.all, 0, sizeof T.all);
    for (int c = 0; c < n * n; c++) {
        T.all = c_or(T.all, c_one(c));
        int u[3] = {c / n, n + c % n, 2 * n + p->region[c]};
        for (int i = 0; i < 3; i++) {
            T.of[c][i] = (uint8_t)u[i];
            T.unit[u[i]] = c_or(T.unit[u[i]], c_one(c));
        }
        cells r = {0, 0};
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int x = c % n + dx, y = c / n + dy;
                if ((dx || dy) && x >= 0 && y >= 0 && x < n && y < n) r = c_or(r, c_one(y * n + x));
            }
        T.round[c] = r;
    }
}

/* what the grader knows: the stars, the empty cells (the others unknown) */
typedef struct { cells star, empty; } grid;

static cells unknown(const grid *g) { return c_not(c_or(g->star, g->empty), T.all); }

static void count(const grid *g, int u, int *stars, int *open)
{
    *stars = c_count(c_and(T.unit[u], g->star));
    *open = c_count(c_and(T.unit[u], unknown(g)));
}

static int broken(const grid *g)
{
    for (int u = 0; u < 3 * T.n; u++) {
        int s, k;
        count(g, u, &s, &k);
        if (s > T.k || s + k < T.k) return 1;
    }
    for (cells m = g->star; c_any(m);) {
        int c = c_first(m);
        m = c_drop(m, c);
        if (c_any(c_and(T.round[c], g->star))) return 1;
    }
    return 0;
}

static cells ring(cells stars)                   /* the cells round some stars */
{
    cells r = {0, 0};
    for (cells m = stars; c_any(m);) {
        int c = c_first(m);
        m = c_drop(m, c);
        r = c_or(r, T.round[c]);
    }
    return r;
}

/* ---- tier 0, easy: round a star; a full line or region; one with just enough cells left (everything once over) --- */
static int easy(grid *g, int *cell)
{
    int ch = 0;
    cells open = unknown(g), r = c_and(ring(g->star), open);
    if (c_any(r)) g->empty = c_or(g->empty, r), ch = 1;
    for (int u = 0; u < 3 * T.n; u++) {
        int s, k;
        count(g, u, &s, &k);
        if (!k || (s != T.k && s + k != T.k)) continue;
        cells left = c_and(T.unit[u], unknown(g));
        if (s == T.k) {
            g->empty = c_or(g->empty, left);
            ch = 1;
            continue;
        }
        g->star = c_or(g->star, left);         /* stars placed: their surroundings next */
        if (cell && *cell < 0) *cell = c_first(left);
        return 1;
    }
    return ch;
}

static int settle(grid *g)                      /* the easy ways until nothing moves; 0: broken */
{
    for (;;) {
        if (broken(g)) return 0;
        if (!easy(g, NULL)) return 1;
    }
}

/* ---- tier 1 ------------------------------------------------------------------------------------------------------ */
/* a cell whose star would leave some line or region too few cells is empty */
static int starve(grid *g)
{
    cells open = unknown(g);
    int stars[UNITS], left[UNITS];
    for (int u = 0; u < 3 * T.n; u++) count(g, u, &stars[u], &left[u]);
    for (cells m = open; c_any(m);) {
        int c = c_first(m);
        m = c_drop(m, c);
        cells gone = c_or(c_one(c), c_and(T.round[c], open));   /* no longer open: the star, the cells round it */
        for (int i = 0; i < 3; i++) {           /* its own lines and region, if now full */
            int u = T.of[c][i];
            if (stars[u] + 1 == T.k) gone = c_or(gone, c_and(T.unit[u], open));
        }
        unsigned long long units = 0;           /* the lines and regions that lose cells */
        for (cells q = gone; c_any(q);) {
            int e = c_first(q);
            q = c_drop(q, e);
            for (int i = 0; i < 3; i++) units |= 1ull << T.of[e][i];
        }
        for (; units; units &= units - 1) {
            int u = __builtin_ctzll(units);
            cells lost = c_and(T.unit[u], gone);
            if (!c_any(lost)) continue;
            int mine = c_has(T.unit[u], c);
            if (stars[u] + mine + left[u] - c_count(lost) < T.k) {
                g->empty = c_or(g->empty, c_one(c));
                return 1;
            }
        }
    }
    return 0;
}

/* m units of one kind whose cells left all lie in m units of another, missing as many stars between them: the other
 * units' remaining cells are empty (tier 1: one in one; tier 2: two in two; tier 3: three in three). kinds: 0 rows,
 * 1 columns, 2 regions */
static unsigned combo[4][130];                  /* the sets of 1, 2, 3 units of a kind (a grid of n) */
static int ncombo[4], combo_n;

static void combos_for(int n)
{
    if (combo_n == n) return;
    combo_n = n;
    ncombo[1] = ncombo[2] = ncombo[3] = 0;
    for (unsigned set = 1; set < 1u << n; set++) {
        int b = bench_bits(set);
        if (b <= 3) combo[b][ncombo[b]++] = set;
    }
}

static int packed(grid *g, int m)
{
    int n = T.n;
    combos_for(n);
    static const int pairs[6][2] = {{2, 0}, {2, 1}, {0, 2}, {1, 2}, {0, 1}, {1, 0}};
    cells open = unknown(g), uo[UNITS];
    int need[UNITS];
    unsigned live[3] = {0, 0, 0}, touch[UNITS][3];    /* touch: the units of each kind its open cells lie in */
    for (int u = 0; u < 3 * n; u++) {
        uo[u] = c_and(T.unit[u], open);
        need[u] = T.k - c_count(c_and(T.unit[u], g->star));
        if (c_any(uo[u])) live[u / n] |= 1u << (u % n);   /* a unit already settled: not part of a pattern */
        touch[u][0] = touch[u][1] = touch[u][2] = 0;
        for (cells q = uo[u]; c_any(q);) {
            int e = c_first(q);
            q = c_drop(q, e);
            for (int kind = 0; kind < 3; kind++) touch[u][kind] |= 1u << (T.of[e][kind] - kind * n);
        }
    }
    for (int pi = 0; pi < 6; pi++) {
        int ka = pairs[pi][0], kb = pairs[pi][1];
        for (int ci = 0; ci < ncombo[m]; ci++) {
            unsigned set = combo[m][ci];
            if (set & ~live[ka]) continue;
            unsigned bset = 0;
            int need_a = 0;
            for (unsigned q = set; q; q &= q - 1) {
                int u = ka * n + bench_low(q);
                need_a += need[u];
                bset |= touch[u][kb];
            }
            if (bench_bits(bset) != m) continue;
            int need_b = 0;
            cells in = {0, 0}, rest = {0, 0};
            for (unsigned q = bset; q; q &= q - 1) {
                need_b += need[kb * n + bench_low(q)];
                rest = c_or(rest, uo[kb * n + bench_low(q)]);
            }
            if (need_a != need_b) continue;
            for (unsigned q = set; q; q &= q - 1) in = c_or(in, uo[ka * n + bench_low(q)]);
            rest = c_and(rest, c_not(in, T.all));
            if (!c_any(rest)) continue;
            g->empty = c_or(g->empty, rest);
            return 1;
        }
    }
    return 0;
}

/* ---- tier 3: a star that, followed through with the easy ways, comes to a contradiction (tried in the lines and
 * regions with three cells left at most: a person tries the few places a star can still go, not every cell) ------- */
static int contradiction(grid *g)
{
    for (int u = 0; u < 3 * T.n; u++) {
        cells left = c_and(T.unit[u], unknown(g));
        int k = c_count(left);
        if (k < 2 || k > 3) continue;
        for (cells m = left; c_any(m);) {
            int c = c_first(m);
            m = c_drop(m, c);
            grid t = *g;
            t.star = c_or(t.star, c_one(c));
            if (!settle(&t)) {
                g->empty = c_or(g->empty, c_one(c));
                return 1;
            }
        }
    }
    return 0;
}

/* the ways in tiers: 0 the easy ones, 1 a star's room and one unit inside another, 2 two units in two, 3 three in
 * three and contradictions. A variant's level is a range of tiers (the two-star grids need tier 1 from the start). */
#define TIERS 4
static const int TIER_LO[2][SBX_LEVELS] = {{0, 1, 2}, {1, 2, 3}}, TIER_HI[2][SBX_LEVELS] = {{0, 1, 3}, {1, 2, 3}};

/* one step of the easiest way that works, up to the tier cap: its tier; -1: stuck */
static int step(grid *g, int cap, int *cell)
{
    if (easy(g, cell)) return 0;
    if (cap < 1) return -1;
    if (starve(g) || packed(g, 1)) return 1;
    if (cap < 2) return -1;
    if (packed(g, 2)) return 2;
    if (cap < 3) return -1;
    if (packed(g, 3) || contradiction(g)) return 3;
    return -1;
}

static int left_after;                          /* the cells rate_cap() left unknown when it got stuck (0: solved) */
static cells stuck_open;                        /* which ones */
static int effort;                              /* rate_cap's steps, weighed: 1, 3, 10, 30 by their tier */

/* the hardest tier a puzzle needs, the grader going no further than cap; TIERS: stuck */
static int rate_cap(const sbx_puz *p, int cap)
{
    tables_load(p);
    grid g = {{0, 0}, {0, 0}};
    int tier = 0;
    effort = 0;
    for (;;) {
        left_after = c_count(unknown(&g));
        if (broken(&g)) return TIERS;
        if (!left_after) return tier;
        int l = step(&g, cap, NULL);
        effort += l == 0 ? 1 : l == 1 ? 3 : l == 2 ? 10 : l == 3 ? 30 : 0;
        if (l < 0) {
            stuck_open = unknown(&g);
            return TIERS;
        }
        if (l > tier) tier = l;
    }
}

int sbx_rate(const sbx_puz *p)
{
    int t = rate_cap(p, TIERS - 1), k2 = p->k == 2;
    if (t >= TIERS) return SBX_BEYOND;
    for (int level = SBX_EASY; level <= SBX_HARD; level++)
        if (t <= TIER_HI[k2][level]) return level;
    return SBX_BEYOND;
}

int sbx_effort(const sbx_puz *p, int *steps)
{
    int r = sbx_rate(p);
    if (steps) *steps = effort;
    return r;
}

/* ---- the levels' code: "<n>/<k>:" and a letter a cell, row by row: its region ('a' the first...) ---------------- */
int sbx_encode(const sbx_puz *p, char *out, int size)
{
    int n = p->n, k = snprintf(out, (size_t)size, "%d/%d:", n, p->k);
    if (k < 0 || k + n * n + 1 > size) return 0;
    for (int c = 0; c < n * n; c++) out[k++] = (char)('a' + p->region[c]);
    out[k] = 0;
    return 1;
}

int sbx_decode(const char *s, sbx_puz *p)
{
    int n = 0, k = 0, size[SBX_MAX] = {0};
    while (*s >= '0' && *s <= '9') n = n * 10 + *s++ - '0';
    if (*s++ != '/' || n < 4 || n > SBX_MAX) return 0;
    while (*s >= '0' && *s <= '9') k = k * 10 + *s++ - '0';
    if (*s++ != ':' || k < 1 || k > 2) return 0;
    memset(p, 0, sizeof *p);
    p->n = n;
    p->k = k;
    for (int c = 0; c < n * n; c++, s++) {
        if (*s < 'a' || *s >= 'a' + n) return 0;
        p->region[c] = (uint8_t)(*s - 'a');
        size[p->region[c]]++;
    }
    for (int r = 0; r < n; r++)
        if (!size[r]) return 0;
    return !*s;
}

/* ---- counting solutions: the easy ways, then a cell of the tightest unit, a star or empty ------------------------ */
typedef struct { int count, limit; uint8_t *sol; } counter;

static void search(grid *g, counter *k)
{
    if (!settle(g)) return;
    int best = -1, bk = 999;
    for (int u = 0; u < 3 * T.n; u++) {
        int s, q;
        count(g, u, &s, &q);
        if (q && q < bk) bk = q, best = u;
    }
    if (best < 0) {
        if (!k->count++ && k->sol)
            for (int c = 0; c < T.n * T.n; c++) k->sol[c] = (uint8_t)c_has(g->star, c);
        return;
    }
    int c = c_first(c_and(T.unit[best], unknown(g)));
    grid t = *g;
    t.star = c_or(t.star, c_one(c));
    search(&t, k);
    if (k->count >= k->limit) return;
    t = *g;
    t.empty = c_or(t.empty, c_one(c));
    search(&t, k);
}

int sbx_count(const sbx_puz *p, int limit, uint8_t *sol)
{
    tables_load(p);
    grid g = {{0, 0}, {0, 0}};
    counter k = {0, limit, sol};
    search(&g, &k);
    return k.count;
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
/* k stars a row and a column, none touching, rows in turn (the columns tried in a random order) */
static int place_stars(int n, int k, int row, uint8_t *st, int *colc, uint32_t *rng)
{
    if (row == n) return 1;
    int opts[SBX_MAX * SBX_MAX][2], no = 0;
    for (int a = 0; a < n; a++)
        for (int b = k == 1 ? a : a + 2; b < n; b++) {
            opts[no][0] = a, opts[no][1] = b;
            no++;
            if (k == 1) break;
        }
    int order[SBX_MAX * SBX_MAX];
    for (int i = 0; i < no; i++) order[i] = i;
    pr_shuffle(rng, order, no);
    for (int oi = 0; oi < no; oi++) {
        int cols[2] = {opts[order[oi]][0], opts[order[oi]][1]}, ok = 1;
        for (int j = 0; j < k && ok; j++) {
            int x = cols[j];
            if (colc[x] >= k) ok = 0;
            if (row > 0)
                for (int dx = -1; dx <= 1; dx++)
                    if (x + dx >= 0 && x + dx < n && st[(row - 1) * n + x + dx]) ok = 0;
        }
        if (!ok) continue;
        for (int j = 0; j < k; j++) st[row * n + cols[j]] = 1, colc[cols[j]]++;
        if (place_stars(n, k, row + 1, st, colc, rng)) return 1;
        for (int j = 0; j < k; j++) st[row * n + cols[j]] = 0, colc[cols[j]]--;
    }
    return 0;
}

static const int DX4[4] = {1, 0, -1, 0}, DY4[4] = {0, 1, 0, -1};

/* the regions grown round the stars (two stars of a region joined by a path first); 0: no way */
static int grow(int n, int k, const uint8_t *st, uint8_t *reg, uint32_t *rng)
{
    int stars[2 * SBX_MAX], ns = 0, ids[SBX_MAX];
    memset(reg, 0xff, SBX_CELLS);
    for (int c = 0; c < n * n; c++)
        if (st[c]) stars[ns++] = c;
    pr_shuffle(rng, stars, ns);
    for (int i = 0; i < n; i++) ids[i] = i;
    pr_shuffle(rng, ids, n);
    int r = 0;
    for (int i = 0; i < ns; i++) {
        int a = stars[i];
        if (reg[a] != 0xff) continue;
        reg[a] = (uint8_t)ids[r];
        if (k == 2) {                           /* the nearest free star, joined by the shortest free path */
            int prev[SBX_CELLS], queue[SBX_CELLS], qh = 0, qt = 0, found = -1;
            for (int c = 0; c < n * n; c++) prev[c] = -2;
            prev[a] = -1;
            queue[qt++] = a;
            while (qh < qt && found < 0) {
                int c = queue[qh++], dirs[4] = {0, 1, 2, 3};
                pr_shuffle(rng, dirs, 4);
                for (int d = 0; d < 4; d++) {
                    int x = c % n + DX4[dirs[d]], y = c / n + DY4[dirs[d]], e = y * n + x;
                    if (x < 0 || y < 0 || x >= n || y >= n || prev[e] != -2 || reg[e] != 0xff) continue;
                    prev[e] = c;
                    if (st[e]) { found = e; break; }
                    queue[qt++] = e;
                }
            }
            if (found < 0) return 0;
            for (int c = found; c != a; c = prev[c]) reg[c] = (uint8_t)ids[r];
        }
        r++;
    }
    for (;;) {                                  /* a free cell next to a region joins it, at random */
        int cand[4 * SBX_CELLS][2], nc = 0;
        for (int c = 0; c < n * n; c++) {
            if (reg[c] != 0xff) continue;
            for (int d = 0; d < 4; d++) {
                int x = c % n + DX4[d], y = c / n + DY4[d];
                if (x >= 0 && y >= 0 && x < n && y < n && reg[y * n + x] != 0xff) cand[nc][0] = c, cand[nc++][1] = reg[y * n + x];
            }
        }
        if (!nc) break;
        int pick = pr_below(rng, nc);
        reg[cand[pick][0]] = (uint8_t)cand[pick][1];
    }
    for (int c = 0; c < n * n; c++)
        if (reg[c] == 0xff) return 0;
    return 1;
}

/* a region still in one piece without the cell c */
static int whole_without(int n, const uint8_t *reg, int c)
{
    int r = reg[c], seen[SBX_CELLS] = {0}, queue[SBX_CELLS], qh = 0, qt = 0, size = 0, start = -1;
    for (int i = 0; i < n * n; i++)
        if (reg[i] == r && i != c) {
            size++;
            if (start < 0) start = i;
        }
    if (start < 0) return 0;
    seen[start] = 1;
    queue[qt++] = start;
    while (qh < qt) {
        int e = queue[qh++];
        for (int d = 0; d < 4; d++) {
            int x = e % n + DX4[d], y = e / n + DY4[d], f = y * n + x;
            if (x < 0 || y < 0 || x >= n || y >= n || seen[f] || f == c || reg[f] != r) continue;
            seen[f] = 1;
            queue[qt++] = f;
        }
    }
    return qt == size;
}

/* a cell (not a star) passed from its region to a neighbouring one, both staying in one piece; most often one of
 * the cells the grader could not settle (focus) */
static void nudge(int n, uint8_t *reg, const uint8_t *st, uint32_t *rng, cells focus)
{
    int nf = c_count(focus);
    for (int tries = 0; tries < 50; tries++) {
        int c = pr_below(rng, n * n), d = pr_below(rng, 4);
        if (nf && pr_below(rng, 4)) {
            cells f = focus;
            for (int i = pr_below(rng, nf); i > 0; i--) f = c_drop(f, c_first(f));
            c = c_first(f);
        }
        int x = c % n + DX4[d], y = c / n + DY4[d];
        if (st[c] || x < 0 || y < 0 || x >= n || y >= n || reg[y * n + x] == reg[c]) continue;
        if (!whole_without(n, reg, c)) continue;
        reg[c] = reg[y * n + x];
        return;
    }
}

/* how far a puzzle is from its level (lower is better): 0 it needs that level's ways; 1000 and more: the grader
 * gets stuck at that level (plus the cells left unknown); 3000: easier ways do */
static int score(const sbx_puz *p, int level)
{
    int lo = TIER_LO[p->k == 2][level], hi = TIER_HI[p->k == 2][level];
    if (rate_cap(p, hi) >= TIERS) return 1000 + left_after;
    if (lo == 0 || rate_cap(p, lo - 1) >= TIERS) return 0;
    return 3000;                                /* too easy: worse than stuck (the climb stays at the edge) */
}

void sbx_make(int n, int k, int level, uint32_t seed, sbx_puz *out, uint8_t *sol)
{
    if (n < 4 || n > SBX_MAX) n = 6;
    if (k < 1 || k > 2) k = 1;
    if (level < SBX_EASY || level > SBX_HARD) level = SBX_EASY;
    uint32_t s = pr_seed(seed);
    sbx_puz p, q, best;
    uint8_t st[SBX_CELLS], best_st[SBX_CELLS];
    int best_score = 1 << 30;
    p.n = q.n = best.n = n;
    p.k = q.k = best.k = k;
    /* each attempt: stars and regions by chance, then cells passed between regions, keeping each change that brings
     * the puzzle no further from its level (a climb), until it is at its level */
    for (int attempt = 0; attempt < 2000 && (attempt < 40 || best_score >= 1000); attempt++) {
        int colc[SBX_MAX] = {0};
        memset(st, 0, sizeof st);
        if (!place_stars(n, k, 0, st, colc, &s) || !grow(n, k, st, p.region, &s)) continue;
        int cur = score(&p, level);
        cells focus = cur >= 1000 && cur < 3000 ? stuck_open : (cells){0, 0};
        for (int tweak = 0; tweak < 150 && cur; tweak++) {
            q = p;
            nudge(n, q.region, st, &s, focus);
            int sc = score(&q, level);
            if (sc <= cur) {
                p = q;
                cur = sc;
                focus = sc >= 1000 && sc < 3000 ? stuck_open : (cells){0, 0};
            }
        }
        if (cur < best_score) {
            best_score = cur;
            best = p;
            memcpy(best_st, st, SBX_CELLS);
        }
        if (!cur) break;
    }
    *out = best;
    memcpy(sol, best_st, SBX_CELLS);
}

/* ---- the player's marks ------------------------------------------------------------------------------------------ */
void sbx_load(sbx_game *g, int level, const sbx_puz *p, const uint8_t *sol)
{
    memset(g, 0, sizeof *g);
    g->level = level;
    g->p = *p;
    memcpy(g->sol, sol, SBX_CELLS);
}

int sbx_open(sbx_game *g, const char *code)
{
    sbx_puz p;
    uint8_t sol[SBX_CELLS];
    if (!sbx_decode(code, &p) || sbx_count(&p, 2, sol) != 1) return 0;
    sbx_load(g, -1, &p, sol);
    return 1;
}

void sbx_new(sbx_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= SBX_VARIANTS) variant = 0;
    sbx_puz p;
    uint8_t sol[SBX_CELLS];
    sbx_make(sbx_size_of(variant), sbx_stars_of(variant), sbx_level_of(variant), seed, &p, sol);
    sbx_load(g, sbx_level_of(variant), &p, sol);
    g->seed = seed;
}

static int in_grid(const sbx_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n; }

/* the wrong entries on the board (the stars' mistakes) */
static int wrongs(const sbx_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++)
        k += (g->mark[c] == SBX_STAR && !g->sol[c]) || (g->mark[c] == SBX_DOT && g->sol[c]);
    return k;
}

static void mark(sbx_game *g, int c, int m)
{
    if (g->mark[c] == m) return;
    bench_log(&g->hist, c, g->mark[c]);
    g->mark[c] = (uint8_t)m;
}

int sbx_set(sbx_game *g, int c, int m)
{
    if (!in_grid(g, c) || m < SBX_NONE || m > SBX_DOT) return 0;
    bench_begin(&g->hist, 1);
    mark(g, c, m);
    return bench_done(&g->hist, wrongs(g));
}

int sbx_tap(sbx_game *g, int c)
{
    if (!in_grid(g, c)) return 0;
    return sbx_set(g, c, (g->mark[c] + 1) % 3);
}

int sbx_run(sbx_game *g, int a, int b, int to)
{
    int n = g->p.n;
    if (!in_grid(g, a) || !in_grid(g, b) || (a % n != b % n && a / n != b / n) || to < SBX_NONE || to > SBX_DOT)
        return 0;
    int step = a == b ? 1 : a % n == b % n ? (b > a ? n : -n) : (b > a ? 1 : -1), from = g->mark[a];
    bench_begin(&g->hist, SBX_MAX);
    for (int c = a;; c += step) {
        if (g->mark[c] == from) mark(g, c, to);
        if (c == b) break;
    }
    return bench_done(&g->hist, wrongs(g));
}

static int hint_step(sbx_game *g)
{
    int n = g->p.n;
    for (int c = 0; c < n * n; c++)             /* a wrong star or dot first */
        if ((g->mark[c] == SBX_STAR && !g->sol[c]) || (g->mark[c] == SBX_DOT && g->sol[c]))
            return sbx_set(g, c, SBX_NONE) ? c : -1;
    tables_load(&g->p);
    grid t = {{0, 0}, {0, 0}};
    for (int c = 0; c < n * n; c++) {
        if (g->mark[c] == SBX_STAR) t.star = c_or(t.star, c_one(c));
        if (g->mark[c] == SBX_DOT) t.empty = c_or(t.empty, c_one(c));
    }
    for (int guard = 0; guard < 4 * SBX_CELLS; guard++) {   /* the easiest ways until a new star is sure */
        for (int c = 0; c < n * n; c++)
            if (c_has(t.star, c) && g->mark[c] != SBX_STAR) return sbx_set(g, c, SBX_STAR) ? c : -1;
        if (broken(&t) || step(&t, TIERS - 1, NULL) < 0) break;
    }
    for (int c = 0; c < n * n; c++)             /* stuck: the first star not placed */
        if (g->sol[c] && g->mark[c] != SBX_STAR) return sbx_set(g, c, SBX_STAR) ? c : -1;
    return -1;
}

static void put_back(void *ctx, int cell, uint32_t was) { ((sbx_game *)ctx)->mark[cell] = (uint8_t)was; }

int sbx_hint(sbx_game *g)
{
    return bench_hinted(&g->hist, hint_step(g));
}

int sbx_undo(sbx_game *g)
{
    int c = bench_undo(&g->hist, put_back, g);
    bench_undone(&g->hist, wrongs(g));
    return c;
}


static int unit_stars(const sbx_game *g, int way, int i)
{
    int n = g->p.n, k = 0;
    for (int c = 0; c < n * n; c++) {
        int u = way == 0 ? c / n : way == 1 ? c % n : g->p.region[c];
        if (u == i && g->mark[c] == SBX_STAR) k++;
    }
    return k;
}

int sbx_unit_full(const sbx_game *g, int way, int i) { return unit_stars(g, way, i) == g->p.k; }

int sbx_clash(const sbx_game *g, int c)
{
    int n = g->p.n;
    if (!in_grid(g, c) || g->mark[c] != SBX_STAR) return 0;
    for (int d = 0; d < n * n; d++)
        if (g->mark[d] == SBX_STAR && d != c && abs(d % n - c % n) <= 1 && abs(d / n - c / n) <= 1) return 1;
    return unit_stars(g, 0, c / n) > g->p.k || unit_stars(g, 1, c % n) > g->p.k ||
           unit_stars(g, 2, g->p.region[c]) > g->p.k;
}

int sbx_solved(const sbx_game *g)
{
    for (int c = 0; c < g->p.n * g->p.n; c++)
        if ((g->mark[c] == SBX_STAR) != (g->sol[c] != 0)) return 0;
    return 1;
}

int sbx_script(sbx_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (sbx_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if (g->sol[c] && g->mark[c] != SBX_STAR) { sbx_set(g, c, SBX_STAR); k--; }
            continue;
        }
        int m = *s == '*' ? SBX_STAR : *s == '.' ? SBX_DOT : *s == '-' ? SBX_NONE : -1, len, len2;
        if (m < 0) return 0;
        int a = bench_cell(s + 1, n, n, &len);
        if (a < 0) return 0;
        s += 1 + len;
        if (*s == '-') {
            int b = bench_cell(s + 1, n, n, &len2);
            if (b < 0 || !sbx_run(g, a, b, m)) return 0;
            s += 1 + len2;
        } else if (!sbx_set(g, a, m)) {
            return 0;
        }
    }
    return 1;
}
