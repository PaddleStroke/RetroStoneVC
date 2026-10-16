/*
 * Masyu: the rules (masyu.h): the ways of reasoning the solver grades with, the maker, the player's links and undo.
 * No SDL: tested by tests/masyu_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/masyu.h"

#include <stdio.h>
#include <string.h>

#include "games/puzrand.h"

#define NO 1u                                   /* a link's values: bit 0 no line, bit 1 a line */
#define YES 2u
#define BOTH 3u

static const int SIZES[MSY_SIZES] = {6, 8, 10};

int msy_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % MSY_SIZES]; }
int msy_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

/* ---- what the solver knows of a grid: each cell's links and neighbours by way (left, right, up, down) ---------- */
typedef struct {
    const msy_puzzle *p;
    int n, cells, links, pearls;
    int16_t link[MSY_CELLS][4], next[MSY_CELLS][4];   /* -1: none (the grid's edge) */
    int bad;                                    /* a pass asked for a line where there is no link */
} ctx;

static void ctx_make(ctx *k, const msy_puzzle *p)
{
    int n = p->n;
    k->p = p;
    k->n = n;
    k->cells = n * n;
    k->links = msy_links(n);
    k->pearls = 0;
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n;
        const int nb[4] = {x > 0 ? c - 1 : -1, x < n - 1 ? c + 1 : -1, y > 0 ? c - n : -1, y < n - 1 ? c + n : -1};
        for (int d = 0; d < 4; d++) {
            k->next[c][d] = (int16_t)nb[d];
            k->link[c][d] = (int16_t)(nb[d] < 0 ? -1 : msy_link(n, c, nb[d]));
        }
        k->pearls += p->pearl[c] != MSY_NO_PEARL;
    }
}

static void grid_of(const ctx *k, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int e = 0; e < k->links; e++) g->d[e] = BOTH;
}

static int is_yes(const bench_grid *g, int e) { return e >= 0 && g->d[e] == YES; }
static int is_no(const bench_grid *g, int e) { return e < 0 || g->d[e] == NO; }

/* the link from c's neighbour on way d onwards the same way (-1: none) */
static int onward(const ctx *k, int c, int d)
{
    int o = k->next[c][d];
    return o < 0 ? -1 : k->link[o][d];
}

/* a fact found: the link can only be `to` (a line where there is no link: a contradiction) */
static void force(ctx *k, bench_grid *g, int e, unsigned to)
{
    if (e >= 0) g->d[e] &= (uint16_t)to;
    else if (to == YES) k->bad = 1;
}

static int settled(ctx *k, const bench_grid *was, const bench_grid *g)
{
    int ch = 0;
    if (k->bad) return -1;
    for (int e = 0; e < k->links; e++) {
        if (!g->d[e]) return -1;
        ch |= g->d[e] != was->d[e];
    }
    return ch;
}

static int yes_at(const ctx *k, const bench_grid *g, int c)
{
    int m = 0;
    for (int d = 0; d < 4; d++) m += is_yes(g, k->link[c][d]);
    return m;
}

/* the line's way on from c, come in by the link `from`: the other line's way (-1: none) */
static int way_on(const ctx *k, const bench_grid *g, int c, int from)
{
    for (int d = 0; d < 4; d++)
        if (k->link[c][d] != from && is_yes(g, k->link[c][d])) return d;
    return -1;
}

/* the chains of lines: each end's other end (chain[] -1: not an end) and the links of its chain; the closed loops:
 * -1 one that cannot be the whole (lines or pearls elsewhere), 1 a whole loop, 0 none */
static int chains(const ctx *k, const bench_grid *g, int16_t *other, int16_t *len, int total, int *closed)
{
    uint8_t seen[MSY_CELLS];
    memset(seen, 0, sizeof seen);
    *closed = 0;
    for (int c = 0; c < k->cells; c++) other[c] = -1;
    for (int c = 0; c < k->cells; c++) {
        int deg = yes_at(k, g, c);
        if (seen[c] || deg != 1) continue;
        int at = c, from = -1, steps = 0;
        seen[c] = 1;
        for (;;) {
            int d = way_on(k, g, at, from);
            if (d < 0) break;
            from = k->link[at][d];
            at = k->next[at][d];
            seen[at] = 1;
            steps++;
        }
        other[c] = (int16_t)at;
        other[at] = (int16_t)c;
        len[c] = len[at] = (int16_t)steps;
    }
    for (int c = 0; c < k->cells; c++) {        /* what is left with lines: closed loops */
        if (seen[c] || yes_at(k, g, c) != 2) continue;
        int at = c, from = -1, steps = 0;
        do {
            seen[at] = 1;
            int d = way_on(k, g, at, from);
            from = k->link[at][d];
            at = k->next[at][d];
            steps++;
        } while (at != c && steps <= k->cells);
        if (steps != total) return -1;
        for (int x = 0; x < k->cells; x++)
            if (k->p->pearl[x] && yes_at(k, g, x) < 2) return -1;
        *closed = 1;
    }
    return 0;
}

/* easy */
static int easy(bench_grid *g, ctx *k)
{
    const bench_grid w = *g;
    const uint8_t *pearl = k->p->pearl;
    int total = 0;
    k->bad = 0;
    for (int e = 0; e < k->links; e++) total += w.d[e] == YES;
    for (int c = 0; c < k->cells; c++) {        /* two lines or none (a pearl: two) */
        int yes = 0, und = 0;
        for (int d = 0; d < 4; d++) {
            int e = k->link[c][d];
            yes += is_yes(&w, e);
            und += e >= 0 && w.d[e] == BOTH;
        }
        int want = yes == 2 ? 0 : yes == 1 ? 1 : pearl[c] ? 2 : -1;   /* the undecided links still to be lines */
        if (yes > 2 || (want > und)) return -1;
        for (int d = 0; d < 4; d++) {
            int e = k->link[c][d];
            if (e < 0 || w.d[e] != BOTH) continue;
            if (want == 0 || (want < 0 && und == 1)) force(k, g, e, NO);
            else if (want == und) force(k, g, e, YES);
        }
    }
    for (int c = 0; c < k->cells; c++) {
        if (pearl[c] == MSY_BLACK)              /* turns, and runs on through the next cell */
            for (int d = 0; d < 4; d++) {
                int e = k->link[c][d], on = onward(k, c, d);
                if (is_no(&w, e) || is_no(&w, on)) {
                    force(k, g, e, NO);
                    force(k, g, k->link[c][d ^ 1], YES);
                }
                if (is_yes(&w, e)) {
                    force(k, g, k->link[c][d ^ 1], NO);
                    force(k, g, on, YES);
                }
            }
        if (pearl[c] == MSY_WHITE)              /* straight, never straight on both sides */
            for (int a = 0; a < 2; a++) {
                int e0 = k->link[c][2 * a], e1 = k->link[c][2 * a + 1];
                int o0 = k->link[c][2 - 2 * a], o1 = k->link[c][3 - 2 * a];
                int c0 = onward(k, c, 2 * a), c1 = onward(k, c, 2 * a + 1);
                if (is_no(&w, e0) || is_no(&w, e1) || (is_yes(&w, c0) && is_yes(&w, c1))) {
                    force(k, g, e0, NO);
                    force(k, g, e1, NO);
                    force(k, g, o0, YES);
                    force(k, g, o1, YES);
                }
                if (is_yes(&w, e0) || is_yes(&w, e1)) {
                    force(k, g, e0, YES);
                    force(k, g, e1, YES);
                    force(k, g, o0, NO);
                    force(k, g, o1, NO);
                    if (is_yes(&w, c0)) force(k, g, c1, NO);
                    if (is_yes(&w, c1)) force(k, g, c0, NO);
                }
            }
    }
    int16_t other[MSY_CELLS], len[MSY_CELLS];   /* no small loop */
    int closed;
    if (chains(k, &w, other, len, total, &closed) < 0) return -1;
    if (closed) {                               /* the whole loop is drawn: nothing else */
        for (int e = 0; e < k->links; e++)
            if (w.d[e] == BOTH) force(k, g, e, NO);
    } else {
        int bare = 0;                           /* pearls with no line yet */
        for (int c = 0; c < k->cells; c++) bare += pearl[c] && yes_at(k, &w, c) == 0;
        for (int c = 0; c < k->cells; c++) {
            int o = other[c];
            if (o <= c) continue;
            int e = msy_link(k->n, c, o);
            if (e >= 0 && w.d[e] == BOTH && (len[c] != total || bare)) force(k, g, e, NO);
        }
    }
    return settled(k, &w, g);
}

/* a cell that, entered from way d, can only go straight on */
static int goes_on(const ctx *k, const bench_grid *g, int c, int d)
{
    int p0 = d < 2 ? 2 : 0;
    return is_yes(g, k->link[c][d]) || k->p->pearl[c] == MSY_WHITE ||
           (is_no(g, k->link[c][p0]) && is_no(g, k->link[c][p0 + 1]));
}

/* medium */
static int medium(bench_grid *g, ctx *k)
{
    const bench_grid w = *g;
    const uint8_t *pearl = k->p->pearl;
    k->bad = 0;
    for (int c = 0; c < k->cells; c++) {
        if (pearl[c] == MSY_BLACK)              /* never into a black pearl, nor a cell that turns already */
            for (int d = 0; d < 4; d++) {
                int e = k->link[c][d], o = k->next[c][d], p0 = d < 2 ? 2 : 0;
                if (e < 0 || w.d[e] != BOTH) continue;
                if (pearl[o] == MSY_BLACK || is_yes(&w, k->link[o][p0]) || is_yes(&w, k->link[o][p0 + 1]))
                    force(k, g, e, NO);
            }
        if (pearl[c] == MSY_WHITE)              /* not along a line that must go straight on both sides */
            for (int a = 0; a < 2; a++) {
                int d0 = 2 * a, d1 = 2 * a + 1, e0 = k->link[c][d0], e1 = k->link[c][d1];
                if (e0 < 0 || e1 < 0) continue;
                int s0 = goes_on(k, &w, k->next[c][d0], d0), s1 = goes_on(k, &w, k->next[c][d1], d1);
                if (s0 && s1) {
                    force(k, g, e0, NO);
                    force(k, g, e1, NO);
                }
                if (is_yes(&w, e0) || is_yes(&w, e1)) {
                    if (s0) force(k, g, onward(k, c, d1), NO);
                    if (s1) force(k, g, onward(k, c, d0), NO);
                }
            }
    }
    return settled(k, &w, g);
}

static long finds[2];                           /* each tier's finds (msy_effort counts them) */

static int deduce(bench_grid *g, int tier, void *c)
{
    int res = tier ? medium(g, (ctx *)c) : easy(g, (ctx *)c);
    if (res > 0) finds[tier ? 1 : 0]++;
    return res;
}

/* every link decided: one loop through every pearl, each pearl's rule kept */
static int valid(const bench_grid *g, void *cv)
{
    const ctx *k = cv;
    int total = 0, start = -1;
    for (int e = 0; e < k->links; e++) total += g->d[e] == YES;
    if (!total) return 0;
    for (int c = 0; c < k->cells; c++) {
        int deg = yes_at(k, g, c), pl = k->p->pearl[c];
        if (deg != 0 && deg != 2) return 0;
        if (deg == 2 && start < 0) start = c;
        if (!pl) continue;
        if (deg != 2) return 0;
        int h = is_yes(g, k->link[c][0]) + is_yes(g, k->link[c][1]);
        if (pl == MSY_BLACK) {
            if (h != 1) return 0;
            for (int d = 0; d < 4; d++)
                if (is_yes(g, k->link[c][d]) && !is_yes(g, onward(k, c, d))) return 0;
        } else {
            if (h == 1) return 0;
            int a = h ? 0 : 2;
            if (is_yes(g, onward(k, c, a)) && is_yes(g, onward(k, c, a + 1))) return 0;
        }
    }
    int at = start, from = -1, steps = 0;       /* one loop: all the lines in it */
    do {
        int d = way_on(k, g, at, from);
        from = k->link[at][d];
        at = k->next[at][d];
        steps++;
    } while (at != start && steps <= total);
    return steps == total;
}

static bench_rules rules_of(ctx *k, long *work)
{
    return (bench_rules){.vars = k->links, .tiers = 2, .probe_tiers = 1,
                        .deduce = deduce, .valid = valid, .ctx = k, .work = work};
}

static int grade_work(const msy_puzzle *p, uint8_t *sol, long *work, int cap, long budget)
{
    ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, work);
    r.budget = budget;
    bench_grid g;
    grid_of(&k, &g);
    int lv = bench_grade(&g, &r, cap);
    if (sol && lv >= BENCH_EASY && lv <= BENCH_HARD)
        for (int e = 0; e < k.links; e++) sol[e] = g.d[e] == YES;
    return lv;
}

int msy_grade(const msy_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, BENCH_HARD, 0); }

int msy_count(const msy_puzzle *p, int limit)
{
    ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid g;
    grid_of(&k, &g);
    return bench_search(&g, &r, limit, NULL, NULL, 0);
}

int msy_effort(const msy_puzzle *p, long *passes, long *mediums)
{
    ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid g;
    grid_of(&k, &g);
    finds[1] = 0;
    int lv = bench_grade(&g, &r, BENCH_HARD);
    if (passes) *passes = r.passes;
    if (mediums) *mediums = finds[1];
    return lv;
}

/* ---- a puzzle as text (the levels) ------------------------------------------------------------------------------- */
static const char PEARL[] = ".wb";

int msy_encode(const msy_puzzle *p, char *out, int size)
{
    int n = p->n, len = snprintf(out, (size_t)size, "%d:", n);
    if (len < 0 || len + n * n + 1 > size) return 0;
    for (int c = 0; c < n * n; c++) out[len++] = PEARL[p->pearl[c] % 3];
    out[len] = 0;
    return len;
}

int msy_decode(const char *s, msy_puzzle *p)
{
    memset(p, 0, sizeof *p);
    int n = 0;
    for (; *s >= '0' && *s <= '9'; s++) n = n * 10 + *s - '0';
    if (*s++ != ':' || n < 3 || n > MSY_MAX) return 0;
    p->n = n;
    for (int c = 0; c < n * n; c++, s++) {
        const char *d = *s ? strchr(PEARL, *s) : NULL;
        if (!d) return 0;
        p->pearl[c] = (uint8_t)(d - PEARL);
    }
    return !*s;
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- *
 * The squares between the cells' middles: (n - 1) x (n - 1), the square x, y with the cells x, y and x + 1, y + 1 at
 * its corners. A region of them: its border is the loop. */
typedef struct {
    int n, m;                                   /* m = n - 1 */
    uint8_t in[MSY_CELLS];
    uint8_t line[MSY_LINKS];
} region;

static int sq_in(const region *r, int x, int y)
{
    return x >= 0 && y >= 0 && x < r->m && y < r->m && r->in[y * r->m + x];
}

/* the border as lines; 1 when it is one loop (no cell met twice) */
static int border(region *r)
{
    int n = r->n, total = 0, deg[MSY_CELLS];
    memset(deg, 0, sizeof deg);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            if (x < n - 1) {
                int e = msy_across(n, x, y), on = sq_in(r, x, y - 1) != sq_in(r, x, y);
                r->line[e] = (uint8_t)on;
                if (on) deg[y * n + x]++, deg[y * n + x + 1]++, total++;
            }
            if (y < n - 1) {
                int e = msy_down(n, x, y), on = sq_in(r, x - 1, y) != sq_in(r, x, y);
                r->line[e] = (uint8_t)on;
                if (on) deg[y * n + x]++, deg[(y + 1) * n + x]++, total++;
            }
        }
    int start = -1;
    for (int c = 0; c < n * n; c++) {
        if (deg[c] != 0 && deg[c] != 2) return 0;
        if (deg[c] && start < 0) start = c;
    }
    if (start < 0) return 0;
    int at = start, from = -1, steps = 0;       /* one loop: walked round, all the lines met */
    do {
        int x = at % n, y = at / n, go = -1;
        const int nb[4] = {x > 0 ? at - 1 : -1, x < n - 1 ? at + 1 : -1, y > 0 ? at - n : -1, y < n - 1 ? at + n : -1};
        for (int d = 0; d < 4 && go < 0; d++) {
            if (nb[d] < 0) continue;
            int e = msy_link(n, at, nb[d]);
            if (e != from && r->line[e]) go = nb[d], from = e;
        }
        if (go < 0) return 0;
        at = go;
        steps++;
    } while (at != start && steps <= total);
    return steps == total;
}

/* a square in or out, kept when the border stays one loop; 1: done */
static int flip(region *r, int sq)
{
    r->in[sq] ^= 1;
    if (border(r)) return 1;
    r->in[sq] ^= 1;
    border(r);
    return 0;
}

/* a region grown by chance from a square to about `part` % of the squares */
static void grow(region *r, int n, uint32_t *s, int part)
{
    memset(r, 0, sizeof *r);
    r->n = n;
    r->m = n - 1;
    int sq = r->m * r->m, target = sq * part / 100, size = 1;
    r->in[pr_below(s, sq)] = 1;
    border(r);
    for (int tries = 0; tries < 40 * sq && size < target; tries++) {
        int c = pr_below(s, sq), x = c % r->m, y = c / r->m;
        if (r->in[c] || !(sq_in(r, x - 1, y) || sq_in(r, x + 1, y) || sq_in(r, x, y - 1) || sq_in(r, x, y + 1)))
            continue;
        size += flip(r, c);
    }
    border(r);
}

/* the pearls the loop allows: black where it turns with both legs going on straight, white where it goes straight
 * and turns next door */
static void pearls_of(const region *r, msy_puzzle *p)
{
    int n = r->n;
    memset(p, 0, sizeof *p);
    p->n = n;
    ctx k;
    ctx_make(&k, p);
    bench_grid g;
    for (int e = 0; e < k.links; e++) g.d[e] = r->line[e] ? YES : NO;
    for (int c = 0; c < n * n; c++) {
        if (yes_at(&k, &g, c) != 2) continue;
        int h = is_yes(&g, k.link[c][0]) + is_yes(&g, k.link[c][1]);
        if (h == 1) {
            int ok = 1;
            for (int d = 0; d < 4; d++)
                if (is_yes(&g, k.link[c][d]) && !is_yes(&g, onward(&k, c, d))) ok = 0;
            if (ok) p->pearl[c] = MSY_BLACK;
        } else {
            int a = h ? 0 : 2;
            if (!is_yes(&g, onward(&k, c, a)) || !is_yes(&g, onward(&k, c, a + 1))) p->pearl[c] = MSY_WHITE;
        }
    }
}

#define MAKE_BUDGET 30000L                      /* the solver's passes a puzzle may cost (well under a second) */
#define GRADE_BUDGET 4000L                      /* one grade's passes at most */

/* how far the reasoning gets (links decided) up to cap: *lv its grade; und the cells by the links left (*nu) */
static int progress(const msy_puzzle *p, long *work, int cap, int *lv, int *und, int *nu)
{
    ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, work);
    r.budget = GRADE_BUDGET;
    bench_grid g;
    grid_of(&k, &g);
    *lv = bench_grade(&g, &r, cap);
    int got = 0;
    *nu = 0;
    for (int e = 0; e < k.links; e++) {
        if (bench_one(g.d[e])) {
            got++;
            continue;
        }
        int a, b;
        msy_ends(p->n, e, &a, &b);
        und[(*nu)++] = a;
    }
    return got;
}

/* the pearls taken away while the puzzle stays at its level (bench_reduce) */
typedef struct {
    msy_puzzle *p;
    const uint8_t *full;
    long *work;
} reducer;

static int red_grade(void *u, int cap)
{
    reducer *R = u;
    return grade_work(R->p, NULL, R->work, cap, GRADE_BUDGET);
}

static void red_clue(void *u, int i, int on)
{
    reducer *R = u;
    R->p->pearl[i] = on ? R->full[i] : MSY_NO_PEARL;
}

/* a puzzle made: a loop by chance (changed a square at a time until the reasoning finishes it with every pearl it
 * allows), then pearls taken away while it stays at its level; the nearest level when that one is not found */
void msy_make_ex(int n, int level, uint32_t seed, long budget, msy_puzzle *p, uint8_t *sol)
{
    if (n < 4 || n > MSY_MAX) n = 6;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    static region r, keep;
    static msy_puzzle best, q;
    int best_lv = -2;
    memset(&best, 0, sizeof best);
    for (int attempt = 0; attempt < 200; attempt++) {
        grow(&keep, n, &s, 52 + pr_below(&s, 18));
        pearls_of(&keep, &q);
        int und[MSY_LINKS], nu, lv, got = progress(&q, &work, level, &lv, und, &nu);
        for (int step = 0; step < 30 * n && (lv < BENCH_EASY || lv > level); step++) {
            r = keep;
            int c = nu && pr_below(&s, 100) < 85 ? und[pr_below(&s, nu)] : pr_below(&s, n * n);
            int x = c % n - pr_below(&s, 2), y = c / n - pr_below(&s, 2);   /* a square at one of its corners */
            if (x < 0 || y < 0 || x >= r.m || y >= r.m || !flip(&r, y * r.m + x)) continue;
            msy_puzzle t;
            pearls_of(&r, &t);
            int u2[MSY_LINKS], n2, lv2, got2 = progress(&t, &work, level, &lv2, u2, &n2);
            if (got2 >= got || (lv2 >= BENCH_EASY && lv2 <= level)) {
                keep = r;
                q = t;
                got = got2;
                lv = lv2;
                nu = n2;
                memcpy(und, u2, sizeof u2);
            }
            if (work > 2 * budget) break;
        }
        if (lv >= BENCH_EASY && lv <= level) {
            uint8_t full[MSY_CELLS];
            memcpy(full, q.pearl, sizeof full);
            int order[MSY_CELLS], no = 0;
            for (int c = 0; c < n * n; c++)
                if (full[c]) order[no++] = c;
            pr_shuffle(&s, order, no);
            reducer R = {&q, full, &work};
            bench_maker mk = {&R, red_grade, red_clue};
            lv = bench_reduce(&mk, order, no, level, &work, 3 * budget);
            if (lv >= BENCH_EASY && lv <= level && lv > best_lv) best_lv = lv, best = q;
        }
        if (best_lv == level || (work >= 3 * budget && best_lv >= BENCH_EASY)) break;
    }
    if (best_lv < BENCH_EASY && n > 5) {        /* never seen: a smaller one rather than none */
        msy_make_ex(n - 1, level, seed + 1, budget, p, sol);
        return;
    }
    *p = best;
    if (sol) grade_work(p, sol, NULL, BENCH_HARD, 0);
}

void msy_make(int n, int level, uint32_t seed, msy_puzzle *p, uint8_t *sol)
{
    msy_make_ex(n, level, seed, MAKE_BUDGET, p, sol);
}

/* ---- the player's links ------------------------------------------------------------------------------------------ */
int msy_load(msy_game *g, const msy_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = msy_grade(p, g->sol);
    return g->level >= BENCH_EASY && g->level <= BENCH_HARD;
}

void msy_new(msy_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= MSY_VARIANTS) variant = 0;
    msy_puzzle p;
    uint8_t sol[MSY_LINKS];
    msy_make(msy_size(variant), msy_level(variant), seed, &p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = msy_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
}

static int in_grid(const msy_game *g, int e) { return e >= 0 && e < msy_links(g->p.n); }

int msy_tap_state(const msy_game *g, int e) { return g->st[e] == MSY_LINE ? MSY_EMPTY : MSY_LINE; }
int msy_hold_state(const msy_game *g, int e) { return g->st[e] == MSY_CROSS ? MSY_EMPTY : MSY_CROSS; }

/* the wrong marks on the board (the stars' mistakes): a line the solution has not, a cross where it has one */
static int wrongs(const msy_game *g)
{
    int k = 0;
    for (int e = 0; e < msy_links(g->p.n); e++)
        k += (g->st[e] == MSY_LINE && !g->sol[e]) || (g->st[e] == MSY_CROSS && g->sol[e]);
    return k;
}

int msy_set(msy_game *g, int e, int state)
{
    if (!in_grid(g, e) || state < MSY_EMPTY || state > MSY_CROSS || g->st[e] == state) return 0;
    bench_begin(&g->hist, 1);
    bench_set(&g->hist, g->st, e, state);
    return bench_done(&g->hist, wrongs(g));
}

int msy_stroke(msy_game *g, int e, int state, int first)
{
    if (first || !g->hist.n) return msy_set(g, e, state);
    if (!in_grid(g, e) || state < MSY_EMPTY || state > MSY_CROSS || g->st[e] == state) return 0;
    bench_begin(&g->hist, 1);
    g->hist.open = 0;                           /* joined to the action before */
    bench_log(&g->hist, e, g->st[e]);
    g->st[e] = (uint16_t)state;
    g->hist.wrongs = wrongs(g);
    return 1;
}

static int hint_step(msy_game *g)
{
    int links = msy_links(g->p.n);
    for (int e = 0; e < links; e++)             /* a wrong mark first */
        if ((g->st[e] == MSY_LINE && !g->sol[e]) || (g->st[e] == MSY_CROSS && g->sol[e]))
            return msy_set(g, e, g->sol[e] ? MSY_LINE : MSY_CROSS) ? e : -1;
    ctx k;
    ctx_make(&k, &g->p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid d;
    grid_of(&k, &d);
    for (int e = 0; e < links; e++)
        if (g->st[e] != MSY_EMPTY) d.d[e] = (uint16_t)(g->sol[e] ? YES : NO);
    int e = bench_next(&d, &r);                 /* the next link reasoning finds: a line, or a cross */
    if (e >= 0 && g->st[e] != MSY_EMPTY) e = -1;
    for (int i = 0; i < links && e < 0; i++)
        if (g->st[i] == MSY_EMPTY && g->sol[i]) e = i;
    for (int i = 0; i < links && e < 0; i++)
        if (g->st[i] == MSY_EMPTY) e = i;
    if (e < 0) return -1;
    return msy_set(g, e, g->sol[e] ? MSY_LINE : MSY_CROSS) ? e : -1;
}

int msy_hint(msy_game *g) { return bench_hinted(&g->hist, hint_step(g)); }

int msy_undo(msy_game *g)
{
    int e = bench_undo_cells(&g->hist, g->st);
    bench_undone(&g->hist, wrongs(g));
    return e;
}

/* a cell's neighbour on way d (-1: none) */
static int step_to(int n, int c, int d)
{
    int x = c % n, y = c / n;
    if (d == 0) return x > 0 ? c - 1 : -1;
    if (d == 1) return x < n - 1 ? c + 1 : -1;
    if (d == 2) return y > 0 ? c - n : -1;
    return y < n - 1 ? c + n : -1;
}
static int line_at(const msy_game *g, int c, int d)
{
    int o = c < 0 ? -1 : step_to(g->p.n, c, d);
    return o >= 0 && g->st[msy_link(g->p.n, c, o)] == MSY_LINE;
}

int msy_lines(const msy_game *g, int c)
{
    int k = 0;
    for (int d = 0; d < 4; d++) k += line_at(g, c, d);
    return k;
}


int msy_broken(const msy_game *g, int c)
{
    int n = g->p.n, pl = g->p.pearl[c];
    if (!pl) return 0;
    int l[4];
    for (int d = 0; d < 4; d++) l[d] = line_at(g, c, d);
    if (l[0] + l[1] + l[2] + l[3] > 2) return 1;
    if (pl == MSY_BLACK) {
        if ((l[0] && l[1]) || (l[2] && l[3])) return 1;
        for (int d = 0; d < 4; d++) {           /* a leg that turns or ends at once */
            int o = step_to(n, c, d);
            if (!l[d] || o < 0) continue;
            int p0 = d < 2 ? 2 : 0;
            if (line_at(g, o, p0) || line_at(g, o, p0 + 1)) return 1;
        }
        return 0;
    }
    if ((l[0] || l[1]) && (l[2] || l[3])) return 1;   /* white: a turn on it */
    for (int a = 0; a < 2; a++) {
        if (!l[2 * a] || !l[2 * a + 1]) continue;
        int o0 = step_to(n, c, 2 * a), o1 = step_to(n, c, 2 * a + 1);
        if (line_at(g, o0, 2 * a) && line_at(g, o1, 2 * a + 1)) return 1;   /* straight on on both sides */
    }
    return 0;
}

int msy_solved(const msy_game *g)
{
    ctx k;
    ctx_make(&k, &g->p);
    bench_grid d;
    memset(&d, 0, sizeof d);
    for (int e = 0; e < k.links; e++) d.d[e] = (uint16_t)(g->st[e] == MSY_LINE ? YES : NO);
    return valid(&d, &k);
}

int msy_script(msy_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (msy_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int e = 0; e < msy_links(n) && k > 0; e++)
                if ((g->st[e] == MSY_LINE) != g->sol[e]) msy_set(g, e, g->sol[e] ? MSY_LINE : MSY_EMPTY), k--;
            continue;
        }
        int hold = *s == '.', len = 0;
        if (hold) s++;
        int a = bench_cell(s, n, n, &len);
        if (a < 0) return 0;
        s += len;
        if (*s != '-' && *s != '/') return 0;
        int drag = *s == '-', b = bench_cell(s + 1, n, n, &len);
        if (b < 0) return 0;
        s += 1 + len;
        if (!drag) {
            int e = msy_link(n, a, b);
            if (e < 0 || !msy_set(g, e, hold ? msy_hold_state(g, e) : msy_tap_state(g, e))) return 0;
            continue;
        }
        if (hold || a == b || (a % n != b % n && a / n != b / n)) return 0;
        int dx = (b % n > a % n) - (b % n < a % n), dy = (b / n > a / n) - (b / n < a / n), first = 1, erase = 0;
        for (int c = a; c != b; c += dy * n + dx) {
            int e = msy_link(n, c, c + dy * n + dx);
            if (first) erase = g->st[e] == MSY_LINE;
            int to = erase ? MSY_EMPTY : MSY_LINE;
            if (g->st[e] != to && (!erase || g->st[e] == MSY_LINE) && msy_stroke(g, e, to, first)) first = 0;
        }
        if (first) return 0;
    }
    return 1;
}
