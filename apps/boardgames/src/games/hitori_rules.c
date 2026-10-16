/*
 * Hitori: the rules (hitori.h): the ways of reasoning the solver grades with, the maker, the player's grid and undo.
 * No SDL: tested by tests/hitori_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/hitori.h"

#include <stdio.h>
#include <string.h>

#include "games/puzrand.h"

#define KEEP 1u                                 /* a cell's values: bit 0 kept, bit 1 shaded */
#define SHADE 2u
#define BOTH 3u

static const int SIZES[HTO_SIZES] = {6, 8, 10};

int hto_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % HTO_SIZES]; }
int hto_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

static int at(int n, int way, int i, int j) { return way ? j * n + i : i * n + j; }

/* ---- what the solver knows of a grid: each cell's neighbours, the numbers alone in their row and column ---------- */
typedef struct {
    const hto_puzzle *p;
    int n, cells;
    uint8_t nn[HTO_CELLS], nb[HTO_CELLS][4], lone[HTO_CELLS];
} ctx;

static void ctx_make(ctx *k, const hto_puzzle *p)
{
    int n = p->n;
    k->p = p;
    k->n = n;
    k->cells = n * n;
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n, twins = 0;
        k->nn[c] = 0;
        if (y > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - n);
        if (x > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - 1);
        if (x < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + 1);
        if (y < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + n);
        for (int j = 0; j < n; j++) {
            twins += j != x && p->num[y * n + j] == p->num[c];
            twins += j != y && p->num[j * n + x] == p->num[c];
        }
        k->lone[c] = !twins;
    }
}

static void grid_of(const ctx *k, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < k->cells; c++) g->d[c] = (uint16_t)(k->lone[c] ? KEEP : BOTH);
}

/* a fact found: the cell can only be `to` (a pass reads the grid as it found it, then puts its facts together) */
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

/* easy */
static int easy(bench_grid *g, const ctx *k)
{
    const bench_grid w = *g;
    int n = k->n, cells = k->cells, open = 0;
    for (int c = 0; c < cells; c++) {
        open += w.d[c] != SHADE;
        if (w.d[c] != SHADE) continue;
        for (int j = 0; j < k->nn[c]; j++) {    /* round a shaded cell: kept */
            int o = k->nb[c][j];
            if (w.d[o] == SHADE) return -1;
            force(g, o, KEEP);
        }
    }
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++)
            for (int a = 0; a < n; a++) {
                int ca = at(n, way, i, a), v = k->p->num[ca];
                if (a + 2 < n && k->p->num[at(n, way, i, a + 2)] == v) force(g, at(n, way, i, a + 1), KEEP);
                for (int b = a + 1; b < n; b++) {
                    int cb = at(n, way, i, b);
                    if (k->p->num[cb] != v) continue;
                    if (w.d[ca] == KEEP && w.d[cb] == KEEP) return -1;
                    if (w.d[ca] == KEEP) force(g, cb, SHADE);   /* a number kept: its twins shaded */
                    if (w.d[cb] == KEEP) force(g, ca, SHADE);
                    if (b == a + 1)             /* twins side by side: one is shaded, so the others are */
                        for (int j = 0; j < n; j++)
                            if (j != a && j != b && k->p->num[at(n, way, i, j)] == v) force(g, at(n, way, i, j), SHADE);
                }
            }
    for (int c = 0; c < cells && open > 1; c++) {   /* a kept cell with one way left to the others */
        if (w.d[c] != KEEP) continue;
        int ways = 0, last = -1;
        for (int j = 0; j < k->nn[c]; j++)
            if (w.d[k->nb[c][j]] != SHADE) ways++, last = k->nb[c][j];
        if (!ways) return -1;
        if (ways == 1) force(g, last, KEEP);
    }
    return settled(&w, g, cells);
}

/* the cells not yet known the kept ones cannot do without (shaded, they would cut them apart): the cut points of the
 * unshaded cells, each piece's kept cells counted (depth first, the low points) */
typedef struct {
    const ctx *k;
    const bench_grid *g;
    int disc[HTO_CELLS], low[HTO_CELLS], sub[HTO_CELLS], t, kept;
    uint8_t *cut;
} cuts;

static void cut_dfs(cuts *D, int u, int parent)
{
    const ctx *k = D->k;
    int split = 0, apart = 0;
    D->disc[u] = D->low[u] = ++D->t;
    D->sub[u] = D->g->d[u] == KEEP;
    for (int j = 0; j < k->nn[u]; j++) {
        int v = k->nb[u][j];
        if (D->g->d[v] == SHADE) continue;
        if (!D->disc[v]) {
            cut_dfs(D, v, u);
            D->sub[u] += D->sub[v];
            if (D->low[v] < D->low[u]) D->low[u] = D->low[v];
            if (D->low[v] >= D->disc[u] && D->sub[v]) split++, apart += D->sub[v];
        } else if (v != parent && D->disc[v] < D->low[u]) {
            D->low[u] = D->disc[v];
        }
    }
    if (D->g->d[u] == BOTH && split + (D->kept - apart > 0) >= 2) D->cut[u] = 1;
}

/* 0: the kept cells are cut apart already */
static int keep_cuts(const ctx *k, const bench_grid *g, uint8_t *cut)
{
    static cuts D;
    int first = -1;
    memset(cut, 0, (size_t)k->cells);
    memset(D.disc, 0, sizeof D.disc);
    D.k = k;
    D.g = g;
    D.t = D.kept = 0;
    D.cut = cut;
    for (int c = 0; c < k->cells; c++)
        if (g->d[c] == KEEP) {
            D.kept++;
            if (first < 0) first = c;
        }
    if (D.kept < 2) return 1;
    cut_dfs(&D, first, -1);
    return D.sub[first] == D.kept;
}

/* medium */
static int medium(bench_grid *g, const ctx *k)
{
    const bench_grid w = *g;
    uint8_t cut[HTO_CELLS];
    if (!keep_cuts(k, &w, cut)) return -1;
    for (int c = 0; c < k->cells; c++)
        if (cut[c]) force(g, c, KEEP);
    return settled(&w, g, k->cells);
}

static long finds[2];                           /* each tier's finds (hto_effort counts them) */

static int deduce(bench_grid *g, int tier, void *c)
{
    int res = tier ? medium(g, (const ctx *)c) : easy(g, (const ctx *)c);
    if (res > 0) finds[tier ? 1 : 0]++;
    return res;
}

static int valid(const bench_grid *g, void *cv)
{
    const ctx *k = cv;
    int n = k->n;
    for (int c = 0; c < k->cells; c++) {
        if (g->d[c] == SHADE && k->lone[c]) return 0;
        if (g->d[c] != SHADE) continue;
        for (int j = 0; j < k->nn[c]; j++)
            if (g->d[k->nb[c][j]] == SHADE) return 0;
    }
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++) {
            unsigned seen = 0;
            for (int j = 0; j < n; j++) {
                int c = at(n, way, i, j);
                if (g->d[c] == SHADE) continue;
                if (seen >> k->p->num[c] & 1) return 0;
                seen |= 1u << k->p->num[c];
            }
        }
    uint8_t cut[HTO_CELLS];
    return keep_cuts(k, g, cut);
}

static bench_rules rules_of(ctx *k, long *work)
{
    return (bench_rules){.vars = k->cells, .tiers = 2, .probe_tiers = 1,
                        .deduce = deduce, .valid = valid, .ctx = k, .work = work};
}

static int grade_work(const hto_puzzle *p, uint8_t *sol, long *work, int cap, bench_grid *end)
{
    ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, work);
    r.budget = 6000;                            /* one grade's passes at most */
    bench_grid g;
    grid_of(&k, &g);
    int lv = bench_grade(&g, &r, cap);
    if (sol && lv >= BENCH_EASY && lv <= BENCH_HARD)
        for (int c = 0; c < k.cells; c++) sol[c] = g.d[c] == SHADE;
    if (end) *end = g;
    return lv;
}

int hto_grade(const hto_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, BENCH_HARD, NULL); }

int hto_count(const hto_puzzle *p, int limit)
{
    ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid g;
    grid_of(&k, &g);
    return bench_search(&g, &r, limit, NULL, NULL, 0);
}

int hto_effort(const hto_puzzle *p, long *passes, long *mediums)
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
static const char DIGIT[] = "0123456789abcdefghij";

int hto_encode(const hto_puzzle *p, char *out, int size)
{
    int n = p->n, len = snprintf(out, (size_t)size, "%d:", n);
    if (len < 0 || len + n * n + 1 > size) return 0;
    for (int c = 0; c < n * n; c++) out[len++] = DIGIT[p->num[c] % 20];
    out[len] = 0;
    return len;
}

int hto_decode(const char *s, hto_puzzle *p)
{
    memset(p, 0, sizeof *p);
    int n = 0;
    for (; *s >= '0' && *s <= '9'; s++) n = n * 10 + *s - '0';
    if (*s++ != ':' || n < 3 || n > HTO_MAX) return 0;
    p->n = n;
    for (int c = 0; c < n * n; c++, s++) {
        const char *d = *s ? strchr(DIGIT, *s) : NULL;
        if (!d || d == DIGIT || d - DIGIT > n) return 0;
        p->num[c] = (uint8_t)(d - DIGIT);
    }
    return !*s;
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
typedef struct {
    int n, cells;
    uint8_t latin[HTO_CELLS];                   /* a Latin square: the numbers the unshaded cells keep */
    uint8_t shade[HTO_CELLS];                   /* the solution */
    uint8_t num[HTO_CELLS];                     /* the puzzle's numbers */
} sheet;

/* a Latin square by chance: rows filled cell by cell with a number not yet in the row or column (a row started again
 * when stuck); 0: stuck too often */
static int latin(sheet *m, uint32_t *s)
{
    int n = m->n;
    for (int y = 0; y < n; y++) {
        int ok = 0;
        for (int tries = 0; tries < 200 && !ok; tries++) {
            ok = 1;
            for (int x = 0; x < n && ok; x++) {
                int opts[HTO_MAX], no = 0;
                for (int v = 1; v <= n; v++) {
                    int used = 0;
                    for (int j = 0; j < x; j++) used |= m->latin[y * n + j] == v;
                    for (int j = 0; j < y; j++) used |= m->latin[j * n + x] == v;
                    if (!used) opts[no++] = v;
                }
                if (!no) ok = 0;
                else m->latin[y * n + x] = (uint8_t)opts[pr_below(s, no)];
            }
        }
        if (!ok) return 0;
    }
    return 1;
}

/* the unshaded cells joined in one piece */
static int joined(const sheet *m)
{
    int n = m->n, stack[HTO_CELLS], sp = 0, open = 0, got = 0, first = -1;
    uint8_t seen[HTO_CELLS];
    memset(seen, 0, (size_t)m->cells);
    for (int c = 0; c < m->cells; c++)
        if (!m->shade[c]) {
            open++;
            if (first < 0) first = c;
        }
    seen[first] = 1;
    stack[sp++] = first;
    while (sp) {
        int x = stack[--sp], px = x % n, py = x / n;
        got++;
        const int nb[4] = {py > 0 ? x - n : -1, px > 0 ? x - 1 : -1, px < n - 1 ? x + 1 : -1, py < n - 1 ? x + n : -1};
        for (int j = 0; j < 4; j++)
            if (nb[j] >= 0 && !seen[nb[j]] && !m->shade[nb[j]]) seen[nb[j]] = 1, stack[sp++] = nb[j];
    }
    return got == open;
}

static int touches_shade(const sheet *m, int c)
{
    int n = m->n, x = c % n, y = c / n;
    return (y > 0 && m->shade[c - n]) || (x > 0 && m->shade[c - 1]) || (x < n - 1 && m->shade[c + 1]) ||
           (y < n - 1 && m->shade[c + n]);
}

/* a shaded cell's number: the number of another unshaded cell of its row or column, by chance */
static void twin_of(sheet *m, int c, uint32_t *s)
{
    int n = m->n, x = c % n, y = c / n, opts[2 * HTO_MAX], no = 0;
    for (int j = 0; j < n; j++) {
        if (j != x && !m->shade[y * n + j]) opts[no++] = m->latin[y * n + j];
        if (j != y && !m->shade[j * n + x]) opts[no++] = m->latin[j * n + x];
    }
    m->num[c] = (uint8_t)(no ? opts[pr_below(s, no)] : m->latin[c]);
}

/* the sheet keeps the rules: the shading never touching, the rest joined, the unshaded numbers once a line, each
 * shaded number seen again in its row or column, and a fair part of the cells shaded */
static int sound(const sheet *m)
{
    int n = m->n;
    int shaded = 0;
    for (int c = 0; c < m->cells; c++) shaded += m->shade[c];
    if (shaded * 100 < 18 * m->cells) return 0;    /* enough to do */
    for (int c = 0; c < m->cells; c++) {
        if (!m->shade[c]) continue;
        if (touches_shade(m, c)) return 0;
        int x = c % n, y = c / n, twin = 0;
        for (int j = 0; j < n; j++) twin |= (j != x && m->num[y * n + j] == m->num[c]) || (j != y && m->num[j * n + x] == m->num[c]);
        if (!twin) return 0;
    }
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++) {
            unsigned seen = 0;
            for (int j = 0; j < n; j++) {
                int c = at(n, way, i, j);
                if (m->shade[c]) continue;
                if (seen >> m->num[c] & 1) return 0;
                seen |= 1u << m->num[c];
            }
        }
    return joined(m);
}

/* a Latin square, then shaded cells by chance (never touching, the rest joined) to about part % of the cells */
static int layout(sheet *m, int n, uint32_t *s, int part)
{
    memset(m, 0, sizeof *m);
    m->n = n;
    m->cells = n * n;
    if (!latin(m, s)) return 0;
    int order[HTO_CELLS], want = m->cells * part / 100, shaded = 0;
    for (int c = 0; c < m->cells; c++) order[c] = c;
    pr_shuffle(s, order, m->cells);
    for (int i = 0; i < m->cells && shaded < want; i++) {
        int c = order[i];
        if (touches_shade(m, c)) continue;
        m->shade[c] = 1;
        if (joined(m)) shaded++;
        else m->shade[c] = 0;
    }
    for (int c = 0; c < m->cells; c++) m->num[c] = m->latin[c];
    for (int c = 0; c < m->cells; c++)
        if (m->shade[c]) twin_of(m, c, s);
    return sound(m);
}

/* a small change, most often where the reasoning got stuck (und: those cells): a shaded cell's number changed, a
 * cell shaded or unshaded; 0: none that keeps the rules */
static int mutate(sheet *m, uint32_t *s, const int *und, int nu)
{
    static sheet was;
    for (int tries = 0; tries < 30; tries++) {
        int c = nu && pr_below(s, 100) < 85 ? und[pr_below(s, nu)] : pr_below(s, m->cells);
        was = *m;
        if (m->shade[c] && pr_below(s, 2)) {
            twin_of(m, c, s);
        } else if (m->shade[c]) {
            m->shade[c] = 0;
            m->num[c] = m->latin[c];
        } else {
            m->shade[c] = 1;
            twin_of(m, c, s);
        }
        int changed = memcmp(m->num, was.num, sizeof m->num) || memcmp(m->shade, was.shade, sizeof m->shade);
        if (changed && sound(m)) return 1;
        *m = was;
    }
    return 0;
}

static void puzzle_of(const sheet *m, hto_puzzle *p, uint8_t *sol)
{
    memset(p, 0, sizeof *p);
    p->n = m->n;
    memcpy(p->num, m->num, (size_t)m->cells);
    memcpy(sol, m->shade, (size_t)m->cells);
}

/* how far the reasoning gets (cells decided) up to cap: *lv its grade, und the cells left (*nu) */
static int progress(const sheet *m, long *work, int cap, int *lv, int *und, int *nu)
{
    hto_puzzle q;
    uint8_t qs[HTO_CELLS];
    bench_grid end;
    puzzle_of(m, &q, qs);
    *lv = grade_work(&q, NULL, work, cap, &end);
    int got = 0;
    *nu = 0;
    for (int c = 0; c < m->cells; c++) {
        if (bench_one(end.d[c])) got++;
        else und[(*nu)++] = c;
    }
    return got;
}

#define MAKE_BUDGET 25000L                      /* the solver's passes a puzzle may cost (well under a second) */

/* a puzzle made: a sheet by chance, then changed a little at a time (the change kept when the reasoning gets as far
 * or further) until it is solved at the level (and by no harder way) */
void hto_make_ex(int n, int level, uint32_t seed, long budget, hto_puzzle *p, uint8_t *sol)
{
    if (n < 4 || n > HTO_MAX) n = 6;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    static sheet m, keep, best;
    int best_lv = -2;
    int climb = level < BENCH_HARD ? level : BENCH_MEDIUM;   /* hard: climbed by the medium ways, then tried */
    for (int attempt = 0; attempt < 500; attempt++) {
        if (!layout(&keep, n, &s, 26 + 2 * level + pr_below(&s, 6))) continue;
        int und[HTO_CELLS], nu, lv, got = progress(&keep, &work, climb, &lv, und, &nu);
        for (int step = 0; step < 40 * n; step++) {
            int sol_lv = lv >= BENCH_EASY && lv <= climb ? lv : -1;
            if (sol_lv < 0 && level == BENCH_HARD && got * 5 >= 3 * keep.cells) {
                int u2[HTO_CELLS], n2;
                progress(&keep, &work, BENCH_HARD, &lv, u2, &n2);
                if (lv == BENCH_HARD) sol_lv = lv;
            }
            if (sol_lv >= 0) {
                if (sol_lv > best_lv) best_lv = sol_lv, best = keep;
                break;
            }
            m = keep;
            if (!mutate(&m, &s, und, nu)) break;
            int u3[HTO_CELLS], n3, lv3, got3 = progress(&m, &work, climb, &lv3, u3, &n3);
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
        hto_make_ex(n - 1, level, seed + 1, budget, p, sol);
        return;
    }
    puzzle_of(&best, p, sol);
}

void hto_make(int n, int level, uint32_t seed, hto_puzzle *p, uint8_t *sol)
{
    hto_make_ex(n, level, seed, MAKE_BUDGET, p, sol);
}

/* ---- the player's grid ------------------------------------------------------------------------------------------- */
int hto_load(hto_game *g, const hto_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = hto_grade(p, g->sol);
    return g->level >= BENCH_EASY && g->level <= BENCH_HARD;
}

void hto_new(hto_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= HTO_VARIANTS) variant = 0;
    hto_puzzle p;
    uint8_t sol[HTO_CELLS];
    hto_make(hto_size(variant), hto_level(variant), seed, &p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = hto_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
}

int hto_free(const hto_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n; }
int hto_tap_state(const hto_game *g, int c) { return g->st[c] == HTO_SHADE ? HTO_EMPTY : HTO_SHADE; }
int hto_hold_state(const hto_game *g, int c) { return g->st[c] == HTO_RING ? HTO_EMPTY : HTO_RING; }

/* the wrong entries on the board (the stars' mistakes) */
static int wrongs(const hto_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++) k += g->st[c] != HTO_EMPTY && (g->st[c] == HTO_SHADE) != g->sol[c];
    return k;
}

int hto_paint(hto_game *g, const int *cells, int n, int state)
{
    bench_begin(&g->hist, n);
    for (int i = 0; i < n; i++)
        if (hto_free(g, cells[i])) bench_set(&g->hist, g->st, cells[i], state);
    return bench_done(&g->hist, wrongs(g));
}

static int put(hto_game *g, int c, int state) { return hto_paint(g, &c, 1, state); }

static int hint_step(hto_game *g)
{
    int cells = g->p.n * g->p.n;
    for (int c = 0; c < cells; c++)             /* a wrong cell first */
        if (g->st[c] != HTO_EMPTY && (g->st[c] == HTO_SHADE) != g->sol[c])
            return put(g, c, g->sol[c] ? HTO_SHADE : HTO_RING) ? c : -1;
    ctx k;
    ctx_make(&k, &g->p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid d;
    grid_of(&k, &d);
    for (int c = 0; c < cells; c++)
        if (g->st[c] != HTO_EMPTY) d.d[c] = (uint16_t)(g->sol[c] ? SHADE : KEEP);
    int known[HTO_CELLS], c = -1;
    for (int i = 0; i < cells; i++) known[i] = g->st[i] != HTO_EMPTY;
    for (int round = 0; round < cells && c < 0; round++) {   /* the next cell found, not one the player already has */
        c = bench_next(&d, &r);
        if (c < 0) break;
        if (known[c]) {
            d.d[c] = (uint16_t)(g->sol[c] ? SHADE : KEEP);
            c = -1;
        }
    }
    for (int i = 0; i < cells && c < 0; i++)
        if (g->st[i] == HTO_EMPTY && g->sol[i]) c = i;
    for (int i = 0; i < cells && c < 0; i++)
        if (g->st[i] == HTO_EMPTY) c = i;
    if (c < 0) return -1;
    return put(g, c, g->sol[c] ? HTO_SHADE : HTO_RING) ? c : -1;
}

int hto_hint(hto_game *g) { return bench_hinted(&g->hist, hint_step(g)); }

int hto_undo(hto_game *g)
{
    int c = bench_undo_cells(&g->hist, g->st);
    bench_undone(&g->hist, wrongs(g));
    return c;
}

int hto_touching(const hto_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n;
    if (g->st[c] != HTO_SHADE) return 0;
    return (y > 0 && g->st[c - n] == HTO_SHADE) || (x > 0 && g->st[c - 1] == HTO_SHADE) ||
           (x < n - 1 && g->st[c + 1] == HTO_SHADE) || (y < n - 1 && g->st[c + n] == HTO_SHADE);
}

int hto_twin(const hto_game *g, int c)
{
    int n = g->p.n, x = c % n, y = c / n, v = g->p.num[c];
    if (g->st[c] != HTO_RING) return 0;
    for (int j = 0; j < n; j++) {
        if (j != x && g->st[y * n + j] == HTO_RING && g->p.num[y * n + j] == v) return 1;
        if (j != y && g->st[j * n + x] == HTO_RING && g->p.num[j * n + x] == v) return 1;
    }
    return 0;
}

/* the unshaded cells' piece holding c: how many (seen marks it) */
static int piece(const hto_game *g, int c, uint8_t *seen)
{
    int n = g->p.n, stack[HTO_CELLS], sp = 0, got = 0;
    seen[c] = 1;
    stack[sp++] = c;
    while (sp) {
        int x = stack[--sp], px = x % n, py = x / n;
        got++;
        const int nb[4] = {py > 0 ? x - n : -1, px > 0 ? x - 1 : -1, px < n - 1 ? x + 1 : -1, py < n - 1 ? x + n : -1};
        for (int j = 0; j < 4; j++)
            if (nb[j] >= 0 && !seen[nb[j]] && g->st[nb[j]] != HTO_SHADE) seen[nb[j]] = 1, stack[sp++] = nb[j];
    }
    return got;
}

int hto_apart(const hto_game *g, int c)
{
    int cells = g->p.n * g->p.n, open = 0, biggest = 0, mine = 0;
    uint8_t seen[HTO_CELLS];
    if (g->st[c] == HTO_SHADE) return 0;
    memset(seen, 0, sizeof seen);
    for (int x = 0; x < cells; x++) open += g->st[x] != HTO_SHADE;
    mine = piece(g, c, seen);
    if (mine == open) return 0;
    memset(seen, 0, sizeof seen);               /* cut off: in a piece smaller than the biggest */
    for (int x = 0; x < cells; x++)
        if (g->st[x] != HTO_SHADE && !seen[x]) {
            int k = piece(g, x, seen);
            if (k > biggest) biggest = k;
        }
    return mine < biggest || mine * 2 <= open;
}

int hto_solved(const hto_game *g)
{
    ctx k;
    ctx_make(&k, &g->p);
    bench_grid d;
    memset(&d, 0, sizeof d);
    for (int c = 0; c < k.cells; c++) d.d[c] = (uint16_t)(g->st[c] == HTO_SHADE ? SHADE : KEEP);
    return valid(&d, &k);
}

int hto_script(hto_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (hto_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if ((g->st[c] == HTO_SHADE) != g->sol[c]) put(g, c, g->sol[c] ? HTO_SHADE : HTO_EMPTY), k--;
            continue;
        }
        int hold = *s == '.', len = 0;
        if (hold) s++;
        int a = bench_cell(s, n, n, &len);
        if (!hto_free(g, a)) return 0;
        s += len;
        int b = a;
        if (*s == '-') {
            b = bench_cell(s + 1, n, n, &len);
            if (b < 0 || (b % n != a % n && b / n != a / n)) return 0;
            s += 1 + len;
        }
        int was = g->st[a], to = hold ? hto_hold_state(g, a) : hto_tap_state(g, a), cells[HTO_MAX], k = 0;
        int dx = (b % n > a % n) - (b % n < a % n), dy = (b / n > a / n) - (b / n < a / n);
        for (int c = a;; c += dy * n + dx) {
            if (g->st[c] == was) cells[k++] = c;
            if (c == b) break;
        }
        if (!hto_paint(g, cells, k, to)) return 0;
    }
    return 1;
}
