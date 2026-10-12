/*
 * Skyscrapers: the rules (skyscrapers.h): the ways of reasoning the solver grades with, the maker, the player's town
 * with its notes and undo. No SDL: tested by tests/skyscrapers_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/skyscrapers.h"

#include <string.h>

#include "games/puzrand.h"

/* a cell's values: bit h - 1 for the height h */
static const int SIZES[SKY_SIZES] = {4, 5, 6, 7};

int sky_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % SKY_SIZES]; }
int sky_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

int sky_line(int n, int edge, int i, int j)
{
    switch (edge) {
    case SKY_TOP: return j * n + i;
    case SKY_RIGHT: return i * n + n - 1 - j;
    case SKY_BOTTOM: return (n - 1 - j) * n + i;
    default: return i * n + j;
    }
}

int sky_seen(const uint8_t *h, int n)
{
    int top = 0, seen = 0;
    for (int j = 0; j < n; j++) {
        if (!h[j]) return -1;
        if (h[j] > top) top = h[j], seen++;
    }
    return seen;
}

typedef struct {
    const sky_puzzle *p;
    int n;
} ctx;

/* a line (0..n-1 the rows, from the left; n.. the columns, from the top): its cells, its numbers at both ends */
static void line_of(const ctx *k, int l, int *cells, int *a, int *b)
{
    int n = k->n, edge = l < n ? SKY_LEFT : SKY_TOP, i = l % n;
    for (int j = 0; j < n; j++) cells[j] = sky_line(n, edge, i, j);
    *a = k->p->clue[edge][i];
    *b = k->p->clue[edge == SKY_LEFT ? SKY_RIGHT : SKY_BOTTOM][i];
}

/* easy */
static int easy(bench_grid *g, const ctx *k)
{
    int n = k->n, ch = 0, all = (1 << n) - 1;
    for (int edge = 0; edge < 4; edge++)        /* the edge numbers */
        for (int i = 0; i < n; i++) {
            int c = k->p->clue[edge][i];
            if (!c) continue;
            for (int j = 0; j < n; j++) {
                int cell = sky_line(n, edge, i, j), cap = n - c + 1 + j;
                unsigned m = cap >= n ? (unsigned)all : (1u << cap) - 1;
                if (c == 1 && j == 0) m = 1u << (n - 1);
                if (c == n) m = 1u << j;
                if (g->d[cell] & ~m) g->d[cell] &= (uint8_t)m, ch = 1;
            }
        }
    for (int l = 0; l < 2 * n; l++) {
        int cells[SKY_MAX], a, b;
        line_of(k, l, cells, &a, &b);
        unsigned used = 0;
        for (int j = 0; j < n; j++) {           /* a height used: not elsewhere */
            uint8_t d = g->d[cells[j]];
            if (!bench_one(d)) continue;
            if (used & d) return -1;
            used |= d;
        }
        for (int j = 0; j < n; j++)
            if (!bench_one(g->d[cells[j]]) && (g->d[cells[j]] & used)) g->d[cells[j]] &= (uint8_t)~used, ch = 1;
        for (int h = 0; h < n; h++) {           /* a height with one place */
            int places = 0, last = -1;
            for (int j = 0; j < n; j++)
                if (g->d[cells[j]] >> h & 1) places++, last = cells[j];
            if (!places) return -1;
            if (places == 1 && g->d[last] != 1u << h) g->d[last] = (uint8_t)(1u << h), ch = 1;
        }
        uint8_t hs[SKY_MAX];                    /* a full line: its numbers */
        int full = 1;
        for (int j = 0; j < n; j++) {
            full &= bench_one(g->d[cells[j]]);
            hs[j] = (uint8_t)(bench_low(g->d[cells[j]]) + 1);
        }
        if (!full) continue;
        if (a && sky_seen(hs, n) != a) return -1;
        uint8_t rev[SKY_MAX];
        for (int j = 0; j < n; j++) rev[j] = hs[n - 1 - j];
        if (b && sky_seen(rev, n) != b) return -1;
    }
    return ch;
}

/* the orders of a line's heights its cells allow (dom, in the order seen from its start) and its numbers (a at the
 * start, b at the end): the heights each cell takes in one of them (poss); stops early once every cell can take all
 * it could */
typedef struct {
    int n, a, b, full;
    const uint8_t *dom;
    uint8_t poss[SKY_MAX], perm[SKY_MAX];
} orders;

static void order_from(orders *o, int pos, unsigned used, int top, int seen)
{
    int n = o->n;
    if (o->full) return;
    if (pos == n) {
        if (o->b) {
            int t = 0, s = 0;
            for (int j = n - 1; j >= 0; j--)
                if (o->perm[j] > t) t = o->perm[j], s++;
            if (s != o->b) return;
        }
        int full = 1;
        for (int j = 0; j < n; j++) {
            o->poss[j] |= (uint8_t)(1u << (o->perm[j] - 1));
            full &= o->poss[j] == o->dom[j];
        }
        o->full = full;
        return;
    }
    for (unsigned m = o->dom[pos] & ~used; m; m &= m - 1) {
        int h = bench_low(m) + 1, s = seen + (h > top), t = h > top ? h : top;
        if (o->a) {
            if (s > o->a) continue;
            int higher = 0;                     /* the most it can still see: the heights above t left */
            for (int q = t; q < n; q++) higher += !(used >> q & 1) && q + 1 != h;
            int rest = n - pos - 1;
            if (s + (higher < rest ? higher : rest) < o->a) continue;
        }
        o->perm[pos] = (uint8_t)h;
        order_from(o, pos + 1, used | 1u << (h - 1), t, s);
    }
}

/* medium: a line seen whole */
static int medium(bench_grid *g, const ctx *k)
{
    int n = k->n;
    for (int l = 0; l < 2 * n; l++) {
        int cells[SKY_MAX], a, b, open = 0;
        line_of(k, l, cells, &a, &b);
        uint8_t dom[SKY_MAX];
        for (int j = 0; j < n; j++) {
            dom[j] = g->d[cells[j]];
            open += !bench_one(dom[j]);
        }
        if (!open) continue;
        orders o;
        memset(&o, 0, sizeof o);
        o.n = n, o.a = a, o.b = b, o.dom = dom;
        order_from(&o, 0, 0, 0, 0);
        int ch = 0;
        for (int j = 0; j < n; j++) {
            if (!o.poss[j]) return -1;
            if (o.poss[j] != dom[j]) g->d[cells[j]] = o.poss[j], ch = 1;
        }
        if (ch) return 1;
    }
    return 0;
}

static int deduce(bench_grid *g, int tier, void *c)
{
    return tier ? medium(g, (const ctx *)c) : easy(g, (const ctx *)c);
}

static int valid(const bench_grid *g, void *cv)
{
    const ctx *k = cv;
    for (int l = 0; l < 2 * k->n; l++) {
        int cells[SKY_MAX], a, b;
        line_of(k, l, cells, &a, &b);
        uint8_t hs[SKY_MAX], rev[SKY_MAX];
        unsigned used = 0;
        for (int j = 0; j < k->n; j++) {
            if (!bench_one(g->d[cells[j]]) || (used & g->d[cells[j]])) return 0;
            used |= g->d[cells[j]];
            hs[j] = (uint8_t)(bench_low(g->d[cells[j]]) + 1);
        }
        for (int j = 0; j < k->n; j++) rev[j] = hs[k->n - 1 - j];
        if ((a && sky_seen(hs, k->n) != a) || (b && sky_seen(rev, k->n) != b)) return 0;
    }
    return 1;
}

static void setup(ctx *k, const sky_puzzle *p, bench_rules *r, long *work)
{
    k->p = p;
    k->n = p->n;
    *r = (bench_rules){.vars = p->n * p->n, .tiers = 2, .probe_tiers = 1,
                      .deduce = deduce, .valid = valid, .ctx = k, .work = work};
}

static void grid_of(const sky_puzzle *p, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < p->n * p->n; c++) g->d[c] = (uint8_t)(p->given[c] ? 1u << (p->given[c] - 1) : (1u << p->n) - 1);
}

static int grade_work(const sky_puzzle *p, uint8_t *sol, long *work, int cap)
{
    ctx k;
    bench_rules r;
    setup(&k, p, &r, work);
    bench_grid g;
    grid_of(p, &g);
    int lv = bench_grade(&g, &r, cap);
    if (sol && lv >= BENCH_EASY && lv <= BENCH_HARD)
        for (int c = 0; c < p->n * p->n; c++) sol[c] = (uint8_t)(bench_low(g.d[c]) + 1);
    return lv;
}

int sky_grade(const sky_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, BENCH_HARD); }

int sky_count(const sky_puzzle *p, int limit)
{
    ctx k;
    bench_rules r;
    setup(&k, p, &r, NULL);
    bench_grid g;
    grid_of(p, &g);
    return bench_search(&g, &r, limit, NULL, NULL, 0);
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
#define MAKE_BUDGET 30000L                      /* the solver's passes a puzzle may cost (well under a second) */

/* the clues: 0 .. 4n - 1 the edge numbers (edge * n + i), then a given height each cell (4n + cell) */
typedef struct {
    sky_puzzle *q;
    uint8_t clue[4][SKY_MAX], sol[SKY_CELLS];
    long *work;
} maker;

static int maker_grade(void *u, int cap)
{
    maker *m = u;
    return grade_work(m->q, NULL, m->work, cap);
}

static void maker_clue(void *u, int i, int on)
{
    maker *m = u;
    int n = m->q->n;
    if (i < 4 * n) m->q->clue[i / n][i % n] = on ? m->clue[i / n][i % n] : 0;
    else m->q->given[i - 4 * n] = on ? m->sol[i - 4 * n] : 0;
}

void sky_make(int n, int level, uint32_t seed, sky_puzzle *p, uint8_t *sol)
{
    if (n < 3 || n > SKY_MAX) n = 4;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    sky_puzzle best, q;
    uint8_t best_sol[SKY_CELLS];
    int best_lv = -2, cells = n * n;
    maker m;
    memset(&m, 0, sizeof m);
    m.q = &q;
    m.work = &work;
    bench_maker mk = {&m, maker_grade, maker_clue};
    for (int attempt = 0; attempt < 400; attempt++) {
        memset(&q, 0, sizeof q);                /* a town by chance */
        q.n = n;
        ctx k;
        bench_rules r;
        setup(&k, &q, &r, &work);
        bench_grid g, full;
        grid_of(&q, &g);
        if (bench_search(&g, &r, 1, &full, &s, work + 4000) != 1) continue;
        for (int c = 0; c < cells; c++) m.sol[c] = (uint8_t)(bench_low(full.d[c]) + 1);
        for (int edge = 0; edge < 4; edge++)    /* every number told */
            for (int i = 0; i < n; i++) {
                uint8_t hs[SKY_MAX];
                for (int j = 0; j < n; j++) hs[j] = m.sol[sky_line(n, edge, i, j)];
                q.clue[edge][i] = m.clue[edge][i] = (uint8_t)sky_seen(hs, n);
            }
        int order[4 * SKY_MAX + SKY_CELLS], no = 0, cell_order[SKY_CELLS];
        for (int c = 0; c < cells; c++) cell_order[c] = c;
        pr_shuffle(&s, cell_order, cells);
        int lv = grade_work(&q, NULL, &work, level);
        for (int i = 0; i < cells && (lv < BENCH_EASY || lv > level); i++) {   /* heights given, if need be */
            q.given[cell_order[i]] = m.sol[cell_order[i]];
            lv = grade_work(&q, NULL, &work, level);
        }
        if (lv < BENCH_EASY || lv > level) continue;
        for (int i = 0; i < cells; i++)         /* the heights first: a town told by its edges is nicer */
            if (q.given[cell_order[i]]) order[no++] = 4 * n + cell_order[i];
        int first_edge = no;
        for (int i = 0; i < 4 * n; i++) order[no++] = i;
        pr_shuffle(&s, order + first_edge, 4 * n);
        lv = bench_reduce(&mk, order, no, level, &work, 3 * MAKE_BUDGET);
        if (lv > best_lv && lv <= level) {
            best_lv = lv;
            best = q;
            memcpy(best_sol, m.sol, sizeof best_sol);
        }
        if (best_lv == level || (work >= 3 * MAKE_BUDGET && best_lv >= BENCH_EASY)) break;   /* past it: the nearest */
    }
    *p = best;
    memcpy(sol, best_sol, (size_t)cells);
}

/* ---- the player's town ------------------------------------------------------------------------------------------- */
static void start_grid(sky_game *g)
{
    for (int c = 0; c < g->p.n * g->p.n; c++) g->st[c] = g->p.given[c];
}

void sky_load(sky_game *g, const sky_puzzle *p, int level)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = level;
    sky_grade(p, g->sol);
    start_grid(g);
}

void sky_new(sky_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= SKY_VARIANTS) variant = 0;
    sky_puzzle p;
    uint8_t sol[SKY_CELLS];
    sky_make(sky_size(variant), sky_level(variant), seed, &p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = sky_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
    start_grid(g);
}

int sky_free(const sky_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n && !g->p.given[c]; }

static int same_line(int n, int a, int b) { return a != b && (a / n == b / n || a % n == b % n); }

/* the wrong entries on the board (the stars' mistakes) */
static int wrongs(const sky_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++)
        k += sky_free(g, c) && SKY_HEIGHT(g->st[c]) && SKY_HEIGHT(g->st[c]) != g->sol[c];
    return k;
}

int sky_put(sky_game *g, int c, int h)
{
    int n = g->p.n;
    if (!sky_free(g, c) || h < 0 || h > n || g->st[c] == h) return 0;
    bench_begin(&g->hist, 2 * n);
    bench_set(&g->hist, g->st, c, h);
    if (h)
        for (int o = 0; o < n * n; o++)
            if (same_line(n, c, o) && !SKY_HEIGHT(g->st[o]) && SKY_NOTE(g->st[o], h))
                bench_set(&g->hist, g->st, o, g->st[o] & ~(1u << (3 + h)));
    return bench_done(&g->hist, wrongs(g));
}

int sky_note(sky_game *g, int c, int h)
{
    if (!sky_free(g, c) || h < 1 || h > g->p.n || SKY_HEIGHT(g->st[c])) return 0;
    bench_begin(&g->hist, 1);
    bench_set(&g->hist, g->st, c, g->st[c] ^ (1u << (3 + h)));
    return bench_done(&g->hist, wrongs(g));
}

int sky_clear(sky_game *g, int c)
{
    if (!sky_free(g, c) || !g->st[c]) return 0;
    bench_begin(&g->hist, 1);
    bench_set(&g->hist, g->st, c, 0);
    return bench_done(&g->hist, wrongs(g));
}

static int hint_step(sky_game *g)
{
    int n = g->p.n, cells = n * n;
    for (int c = 0; c < cells; c++) {           /* a wrong height first */
        int h = SKY_HEIGHT(g->st[c]);
        if (h && h != g->sol[c]) return sky_put(g, c, g->sol[c]) ? c : -1;
    }
    ctx k;
    bench_rules r;
    setup(&k, &g->p, &r, NULL);
    bench_grid d;
    grid_of(&g->p, &d);
    for (int c = 0; c < cells; c++)
        if (SKY_HEIGHT(g->st[c])) d.d[c] = (uint8_t)(1u << (SKY_HEIGHT(g->st[c]) - 1));
    int c = bench_next(&d, &r);
    for (int i = 0; i < cells && c < 0; i++)
        if (!SKY_HEIGHT(g->st[i])) c = i;
    if (c < 0) return -1;
    return sky_put(g, c, g->sol[c]) ? c : -1;
}

int sky_hint(sky_game *g)
{
    return bench_hinted(&g->hist, hint_step(g));
}

int sky_undo(sky_game *g)
{
    int c = bench_undo_cells(&g->hist, g->st);
    bench_undone(&g->hist, wrongs(g));
    return c;
}


int sky_clash(const sky_game *g, int c)
{
    int n = g->p.n, h = SKY_HEIGHT(g->st[c]);
    if (!h) return 0;
    for (int o = 0; o < n * n; o++)
        if (same_line(n, c, o) && SKY_HEIGHT(g->st[o]) == h) return 1;
    return 0;
}

int sky_edge_state(const sky_game *g, int edge, int i)
{
    int n = g->p.n, want = g->p.clue[edge][i];
    uint8_t hs[SKY_MAX];
    for (int j = 0; j < n; j++) hs[j] = (uint8_t)SKY_HEIGHT(g->st[sky_line(n, edge, i, j)]);
    int seen = sky_seen(hs, n);
    if (seen < 0 || !want) return 0;
    return seen == want ? 1 : -1;
}

int sky_count_height(const sky_game *g, int h)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++) k += SKY_HEIGHT(g->st[c]) == h;
    return k;
}

int sky_solved(const sky_game *g)
{
    for (int c = 0; c < g->p.n * g->p.n; c++)
        if (SKY_HEIGHT(g->st[c]) != g->sol[c]) return 0;
    return 1;
}

int sky_script(sky_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (sky_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if (SKY_HEIGHT(g->st[c]) != g->sol[c]) sky_put(g, c, g->sol[c]), k--;
            continue;
        }
        int len = 0, c = bench_cell(s, n, n, &len);
        if (c < 0) return 0;
        s += len;
        if ((*s != '=' && *s != '\'') || s[1] < '0' || s[1] > '9') return 0;
        int ok = *s == '=' ? sky_put(g, c, s[1] - '0') : sky_note(g, c, s[1] - '0');
        if (!ok) return 0;
        s += 2;
    }
    return 1;
}
