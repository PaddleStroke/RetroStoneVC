/*
 * Fillomino: the rules (fillomino.h): the ways of reasoning the solver grades with, the maker, the player's numbers
 * and notes, undo. No SDL: tested by tests/fillomino_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/fillomino.h"

#include <stdio.h>
#include <string.h>

#include "games/puzrand.h"

#define ALL 0x3feu                              /* a cell's values: bit v the number v (1..9) */

static const int SIZES[FLM_SIZES] = {6, 8, 10};

int flm_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % FLM_SIZES]; }
int flm_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

/* ---- what the solver knows of a grid: each cell's neighbours; the regions of decided cells ---------------------- */
typedef struct {
    const flm_puzzle *p;
    int n, cells;
    uint8_t nn[FLM_CELLS], nb[FLM_CELLS][4];
} ctx;

static void ctx_make(ctx *k, const flm_puzzle *p)
{
    int n = p->n;
    k->p = p;
    k->n = n;
    k->cells = n * n;
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n;
        k->nn[c] = 0;
        if (y > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - n);
        if (x > 0) k->nb[c][k->nn[c]++] = (uint8_t)(c - 1);
        if (x < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + 1);
        if (y < n - 1) k->nb[c][k->nn[c]++] = (uint8_t)(c + n);
    }
}

static void grid_of(const ctx *k, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < k->cells; c++) g->d[c] = (uint16_t)(k->p->given[c] ? 1u << k->p->given[c] : ALL);
}

static int value(const bench_grid *g, int c) { return bench_one(g->d[c]) ? bench_low(g->d[c]) : 0; }

/* the regions of decided cells (equal numbers side by side): each cell's (-1: undecided), each one's size and number */
typedef struct {
    int16_t of[FLM_CELLS];
    int count, size[FLM_CELLS], num[FLM_CELLS];
} regions;

static void regions_of(const ctx *k, const bench_grid *g, regions *R)
{
    int stack[FLM_CELLS];
    R->count = 0;
    for (int c = 0; c < k->cells; c++) R->of[c] = -1;
    for (int c = 0; c < k->cells; c++) {
        int v = value(g, c);
        if (!v || R->of[c] >= 0) continue;
        int id = R->count++, sp = 0, size = 0;
        R->of[c] = (int16_t)id;
        stack[sp++] = c;
        while (sp) {
            int x = stack[--sp];
            size++;
            for (int j = 0; j < k->nn[x]; j++) {
                int o = k->nb[x][j];
                if (R->of[o] < 0 && value(g, o) == v) R->of[o] = (int16_t)id, stack[sp++] = o;
            }
        }
        R->size[id] = size;
        R->num[id] = v;
    }
}

/* the cells a growing region (q) may still take: through cells that can hold its number, no further than it has
 * cells missing; `blocked` a cell left out (-1: none). Marks them in seen (== id), returns how many */
static int reach(const ctx *k, const bench_grid *g, const regions *R, int q, int blocked, int *seen, int id)
{
    int v = R->num[q], left = v - R->size[q], dist[FLM_CELLS], queue[FLM_CELLS], qh = 0, qt = 0, found = 0;
    for (int c = 0; c < k->cells; c++)
        if (R->of[c] == q) seen[c] = id, dist[c] = 0, queue[qt++] = c;
    while (qh < qt) {
        int x = queue[qh++];
        if (dist[x] >= left) continue;
        for (int j = 0; j < k->nn[x]; j++) {
            int o = k->nb[x][j];
            if (seen[o] == id || o == blocked || !(g->d[o] >> v & 1)) continue;
            seen[o] = id;
            dist[o] = dist[x] + 1;
            queue[qt++] = o;
            found++;
        }
    }
    return found;
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

/* easy */
static int easy(bench_grid *g, const ctx *k)
{
    const bench_grid w = *g;
    regions R;
    regions_of(k, &w, &R);
    int cells = k->cells;
    for (int q = 0; q < R.count; q++)
        if (R.size[q] > R.num[q]) return -1;
    for (int c = 0; c < cells; c++) {           /* a number that would join regions too big for it */
        if (bench_one(w.d[c])) continue;
        for (unsigned m = w.d[c]; m; m &= m - 1) {
            int v = bench_low(m), sum = 1, got[4], ng = 0;
            for (int j = 0; j < k->nn[c]; j++) {
                int q = R.of[k->nb[c][j]], dup = 0;
                if (q < 0 || R.num[q] != v) continue;
                for (int i = 0; i < ng; i++) dup |= got[i] == q;
                if (dup) continue;
                got[ng++] = q;
                sum += R.size[q];
            }
            if (sum > v) force(g, c, ~(1u << v));
        }
    }
    static int seen[FLM_CELLS], serial;
    for (int q = 0; q < R.count; q++) {         /* a growing region: one way out, and room enough */
        int v = R.num[q];
        if (R.size[q] == v) continue;
        int exits = 0, exit_at = -1;
        for (int c = 0; c < cells; c++) {
            if (bench_one(w.d[c]) || !(w.d[c] >> v & 1)) continue;
            for (int j = 0; j < k->nn[c]; j++)
                if (R.of[k->nb[c][j]] == q) {
                    exits++;
                    exit_at = c;
                    break;
                }
        }
        if (!exits) return -1;
        if (exits == 1) force(g, exit_at, 1u << v);
        if (R.size[q] + reach(k, &w, &R, q, -1, seen, ++serial) < v) return -1;
    }
    for (int v = 1; v <= FLM_TOP; v++) {        /* room enough for a region of the number */
        int16_t of[FLM_CELLS];
        int stack[FLM_CELLS], size[FLM_CELLS], np = 0;
        for (int c = 0; c < cells; c++) of[c] = -1;
        for (int c = 0; c < cells; c++) {
            if (of[c] >= 0 || !(w.d[c] >> v & 1)) continue;
            int sp = 0;
            size[np] = 0;
            of[c] = (int16_t)np;
            stack[sp++] = c;
            while (sp) {
                int x = stack[--sp];
                size[np]++;
                for (int j = 0; j < k->nn[x]; j++) {
                    int o = k->nb[x][j];
                    if (of[o] < 0 && (w.d[o] >> v & 1)) of[o] = (int16_t)np, stack[sp++] = o;
                }
            }
            np++;
        }
        for (int c = 0; c < cells; c++) {
            if (of[c] < 0 || size[of[c]] >= v) continue;
            if (bench_one(w.d[c])) return -1;
            force(g, c, ~(1u << v));
        }
    }
    return settled(&w, g, cells);
}

/* medium: a cell a growing region cannot do without */
static int medium(bench_grid *g, const ctx *k)
{
    const bench_grid w = *g;
    regions R;
    regions_of(k, &w, &R);
    static int seen[FLM_CELLS], serial;
    for (int q = 0; q < R.count; q++) {
        int v = R.num[q], left = v - R.size[q], id = ++serial, cand[FLM_CELLS], nc = 0;
        if (left <= 0) continue;
        if (reach(k, &w, &R, q, -1, seen, id) < left) return -1;
        for (int c = 0; c < k->cells; c++)
            if (seen[c] == id && !bench_one(w.d[c])) cand[nc++] = c;
        for (int i = 0; i < nc; i++)
            if (reach(k, &w, &R, q, cand[i], seen, ++serial) < left) force(g, cand[i], 1u << v);
    }
    return settled(&w, g, k->cells);
}

static long finds[2];                           /* each tier's finds (flm_effort counts them) */

static int deduce(bench_grid *g, int tier, void *c)
{
    int res = tier ? medium(g, (const ctx *)c) : easy(g, (const ctx *)c);
    if (res > 0) finds[tier ? 1 : 0]++;
    return res;
}

static int valid(const bench_grid *g, void *cv)
{
    const ctx *k = cv;
    regions R;
    regions_of(k, g, &R);
    for (int q = 0; q < R.count; q++)
        if (R.size[q] != R.num[q]) return 0;
    return 1;
}

static bench_rules rules_of(ctx *k, long *work)
{
    return (bench_rules){.vars = k->cells, .tiers = 2, .probe_tiers = 1,
                        .deduce = deduce, .valid = valid, .ctx = k, .work = work};
}

static int grade_work(const flm_puzzle *p, uint8_t *sol, long *work, int cap, long budget)
{
    ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, work);
    r.budget = budget;
    bench_grid g;
    grid_of(&k, &g);
    int lv = bench_grade(&g, &r, cap);
    if (sol && lv >= BENCH_EASY && lv <= BENCH_HARD)
        for (int c = 0; c < k.cells; c++) sol[c] = (uint8_t)value(&g, c);
    return lv;
}

int flm_grade(const flm_puzzle *p, uint8_t *sol) { return grade_work(p, sol, NULL, BENCH_HARD, 0); }

/* the solutions counted: the values every way of reasoning rules out taken away first (trials too: each one taken
 * away led to a contradiction, so no solution is lost), then a search; a search alone can wander for long in a
 * grid of nine values a cell */
int flm_count(const flm_puzzle *p, int limit)
{
    ctx k;
    ctx_make(&k, p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid g;
    grid_of(&k, &g);
    if (bench_grade(&g, &r, BENCH_HARD) == BENCH_NONE) return 0;
    return bench_search(&g, &r, limit, NULL, NULL, 0);
}

int flm_effort(const flm_puzzle *p, long *passes, long *mediums)
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
int flm_encode(const flm_puzzle *p, char *out, int size)
{
    int n = p->n, len = snprintf(out, (size_t)size, "%d:", n);
    if (len < 0 || len + n * n + 1 > size) return 0;
    for (int c = 0; c < n * n; c++) out[len++] = p->given[c] ? (char)('0' + p->given[c] % 10) : '.';
    out[len] = 0;
    return len;
}

int flm_decode(const char *s, flm_puzzle *p)
{
    memset(p, 0, sizeof *p);
    int n = 0;
    for (; *s >= '0' && *s <= '9'; s++) n = n * 10 + *s - '0';
    if (*s++ != ':' || n < 3 || n > FLM_MAX) return 0;
    p->n = n;
    for (int c = 0; c < n * n; c++, s++) {
        if (*s == '.') continue;
        if (*s < '1' || *s > '9') return 0;
        p->given[c] = (uint8_t)(*s - '0');
    }
    return !*s;
}

/* ---- making a puzzle --------------------------------------------------------------------------------------------- */
/* the regions of a filled grid (sol) side by side with others of their size: 0 none */
static int clashes(const ctx *k, const uint8_t *sol)
{
    bench_grid g;
    for (int c = 0; c < k->cells; c++) g.d[c] = (uint16_t)(1u << sol[c]);
    return !valid(&g, (void *)k);
}

/* a size for a region: mostly 2 to 6, now and then 1 or up to 9 */
static int a_size(uint32_t *s)
{
    int t = 1 + pr_below(s, 2) + pr_below(s, 3) + pr_below(s, 4);
    if (!pr_below(s, 4)) t += pr_below(s, 4);
    return t > FLM_TOP ? FLM_TOP : t;
}

/* the grid cut into regions by chance, each grown from a cell to the size it was given (or as far as it can go),
 * then mended; 0: two regions of a size still side by side */
static int cut(const ctx *k, uint8_t *sol, uint32_t *s)
{
    int cells = k->cells, order[FLM_CELLS], reg[FLM_CELLS], size[FLM_CELLS], nr = 0;
    for (int c = 0; c < cells; c++) order[c] = c, reg[c] = -1;
    pr_shuffle(s, order, cells);
    for (int i = 0; i < cells; i++) {
        int c = order[i];
        if (reg[c] >= 0) continue;
        int want = a_size(s), mine[FLM_CELLS], nm = 0;
        reg[c] = nr;
        mine[nm++] = c;
        while (nm < want) {                     /* a cell beside the region, by chance */
            int opts[4 * FLM_TOP], no = 0;
            for (int a = 0; a < nm; a++)
                for (int j = 0; j < k->nn[mine[a]]; j++)
                    if (reg[k->nb[mine[a]][j]] < 0) opts[no++] = k->nb[mine[a]][j];
            if (!no) break;
            int o = opts[pr_below(s, no)];
            reg[o] = nr;
            mine[nm++] = o;
        }
        size[nr++] = nm;
    }
    /* then two regions of a size side by side mended: a cell of one moved into another region beside it, kept when it
     * leaves no more such pairs (and its region in one piece) */
    for (int it = 0; it < 40 * cells; it++) {
        int pairs[2 * FLM_CELLS], np = 0, bad = 0;
        for (int c = 0; c < cells; c++)
            for (int j = 0; j < k->nn[c]; j++) {
                int o = k->nb[c][j];
                if (reg[o] != reg[c] && size[reg[o]] == size[reg[c]]) pairs[np++] = c;
            }
        bad = np;
        if (!bad) break;
        int c = pairs[pr_below(s, np)], to = k->nb[c][pr_below(s, k->nn[c])], from = reg[c];
        if (reg[to] == from || size[reg[to]] >= FLM_TOP) continue;
        int dest = reg[to], rest[FLM_TOP], nrest = 0, stack[FLM_TOP], sp = 0, got = 0;
        for (int x = 0; x < cells; x++)
            if (reg[x] == from && x != c) rest[nrest++] = x;
        if (nrest) {                            /* what is left of its region still in one piece */
            uint8_t in[FLM_CELLS];
            memset(in, 0, sizeof in);
            in[rest[0]] = 2;
            for (int i = 1; i < nrest; i++) in[rest[i]] = 1;
            stack[sp++] = rest[0];
            while (sp) {
                int x = stack[--sp];
                got++;
                for (int j = 0; j < k->nn[x]; j++)
                    if (in[k->nb[x][j]] == 1) in[k->nb[x][j]] = 2, stack[sp++] = k->nb[x][j];
            }
            if (got != nrest) continue;
        }
        reg[c] = dest;
        size[from]--;
        size[dest]++;
        int after = 0;
        for (int x = 0; x < cells; x++)
            for (int j = 0; j < k->nn[x]; j++) {
                int o = k->nb[x][j];
                after += reg[o] != reg[x] && size[reg[o]] == size[reg[x]];
            }
        if (after > bad && pr_below(s, 8)) {    /* worse: taken back (now and then kept, to get out of a corner) */
            reg[c] = from;
            size[from]++;
            size[dest]--;
        }
    }    for (int c = 0; c < cells; c++) sol[c] = (uint8_t)size[reg[c]];
    return !clashes(k, sol);
}

#define MAKE_BUDGET 15000L                      /* the solver's passes a puzzle may cost (well under a second) */
#define GRADE_BUDGET 5000L                      /* one grade's passes at most */

typedef struct {
    flm_puzzle *p;
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
    R->p->given[i] = on ? R->full[i] : 0;
}

/* a puzzle made: regions by chance, every number given, then numbers taken away while it stays at its level (the
 * nearest level when that one is not found) */
void flm_make_ex(int n, int level, uint32_t seed, long budget, flm_puzzle *p, uint8_t *sol)
{
    if (n < 4 || n > FLM_MAX) n = 6;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    uint32_t s = pr_seed(seed);
    long work = 0;
    static flm_puzzle best, q;
    int best_lv = -2;
    memset(&best, 0, sizeof best);
    ctx k;
    memset(&q, 0, sizeof q);
    q.n = n;
    ctx_make(&k, &q);
    for (int attempt = 0; attempt < 400; attempt++) {
        uint8_t full[FLM_CELLS];
        if (!cut(&k, full, &s)) continue;
        memcpy(q.given, full, sizeof full);
        int order[FLM_CELLS];
        for (int c = 0; c < n * n; c++) order[c] = c;
        pr_shuffle(&s, order, n * n);
        reducer R = {&q, full, &work};
        bench_maker mk = {&R, red_grade, red_clue};
        int lv = bench_reduce(&mk, order, n * n, level, &work, 2 * budget);
        if (lv >= BENCH_EASY && lv <= level && lv > best_lv) best_lv = lv, best = q;
        if (best_lv == level || (work >= 3 * budget && best_lv >= BENCH_EASY)) break;
    }
    while (best_lv < BENCH_EASY) {           /* never seen: every number given (it cannot stay so) */
        uint8_t full[FLM_CELLS];
        if (!cut(&k, full, &s)) continue;
        memcpy(best.given, full, sizeof full);
        best.n = n;
        best_lv = BENCH_EASY;
    }
    *p = best;
    if (sol) grade_work(p, sol, NULL, BENCH_HARD, 0);
}

void flm_make(int n, int level, uint32_t seed, flm_puzzle *p, uint8_t *sol)
{
    flm_make_ex(n, level, seed, MAKE_BUDGET, p, sol);
}

/* ---- the player's numbers ---------------------------------------------------------------------------------------- */
int flm_load(flm_game *g, const flm_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = flm_grade(p, g->sol);
    memcpy(g->val, p->given, sizeof g->val);
    return g->level >= BENCH_EASY && g->level <= BENCH_HARD;
}

void flm_new(flm_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= FLM_VARIANTS) variant = 0;
    flm_puzzle p;
    uint8_t sol[FLM_CELLS];
    flm_make(flm_size(variant), flm_level(variant), seed, &p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = flm_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
    memcpy(g->val, p.given, sizeof g->val);
}

static int in_grid(const flm_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n; }

static int wrongs(const flm_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++) k += !g->p.given[c] && g->val[c] && g->val[c] != g->sol[c];
    return k;
}

static void change(flm_game *g, int c, int v, unsigned notes)
{
    if (g->val[c] == v && g->notes[c] == notes) return;
    bench_log(&g->hist, c, g->val[c] | (uint32_t)g->notes[c] << 8);
    g->val[c] = (uint8_t)v;
    g->notes[c] = (uint16_t)notes;
}

static void put_back(void *ctxp, int at, uint32_t was)
{
    flm_game *g = ctxp;
    g->val[at] = (uint8_t)(was & 0xff);
    g->notes[at] = (uint16_t)(was >> 8);
}

int flm_put(flm_game *g, int c, int d)
{
    if (!in_grid(g, c) || d < 0 || d > FLM_TOP || g->p.given[c] || g->val[c] == d) return 0;
    bench_begin(&g->hist, 1);
    change(g, c, d, 0);
    return bench_done(&g->hist, wrongs(g));
}

int flm_note(flm_game *g, int c, int d)
{
    if (!in_grid(g, c) || d < 1 || d > FLM_TOP || g->p.given[c] || g->val[c]) return 0;
    bench_begin(&g->hist, 1);
    change(g, c, 0, g->notes[c] ^ (1u << d));
    return bench_done(&g->hist, wrongs(g));
}

int flm_erase(flm_game *g, int c)
{
    if (!in_grid(g, c) || g->p.given[c]) return 0;
    if (g->val[c]) return flm_put(g, c, 0);
    if (!g->notes[c]) return 0;
    bench_begin(&g->hist, 1);
    change(g, c, 0, 0);
    return bench_done(&g->hist, wrongs(g));
}

static int hint_step(flm_game *g)
{
    int cells = g->p.n * g->p.n;
    for (int c = 0; c < cells; c++)             /* a wrong number first */
        if (!g->p.given[c] && g->val[c] && g->val[c] != g->sol[c]) return flm_put(g, c, g->sol[c]) ? c : -1;
    ctx k;
    ctx_make(&k, &g->p);
    bench_rules r = rules_of(&k, NULL);
    bench_grid d;
    grid_of(&k, &d);
    for (int c = 0; c < cells; c++)
        if (g->val[c]) d.d[c] = (uint16_t)(1u << g->val[c]);
    int c = bench_next(&d, &r);
    if (c >= 0 && g->val[c]) c = -1;
    for (int i = 0; i < cells && c < 0; i++)
        if (!g->val[i]) c = i;
    if (c < 0) return -1;
    return flm_put(g, c, g->sol[c]) ? c : -1;
}

int flm_hint(flm_game *g) { return bench_hinted(&g->hist, hint_step(g)); }

int flm_undo(flm_game *g)
{
    int c = bench_undo(&g->hist, put_back, g);
    bench_undone(&g->hist, wrongs(g));
    return c;
}

int flm_region(const flm_game *g, int c)
{
    int n = g->p.n, v = in_grid(g, c) ? g->val[c] : 0, stack[FLM_CELLS], sp = 0, size = 0;
    if (!v) return 0;
    uint8_t seen[FLM_CELLS];
    memset(seen, 0, sizeof seen);
    seen[c] = 1;
    stack[sp++] = c;
    while (sp) {
        int x = stack[--sp], px = x % n, py = x / n;
        size++;
        const int nb[4] = {py > 0 ? x - n : -1, px > 0 ? x - 1 : -1, px < n - 1 ? x + 1 : -1, py < n - 1 ? x + n : -1};
        for (int j = 0; j < 4; j++)
            if (nb[j] >= 0 && !seen[nb[j]] && g->val[nb[j]] == v) seen[nb[j]] = 1, stack[sp++] = nb[j];
    }
    return size;
}

int flm_clash(const flm_game *g, int c) { return in_grid(g, c) && g->val[c] && flm_region(g, c) > g->val[c]; }

int flm_solved(const flm_game *g)
{
    for (int c = 0; c < g->p.n * g->p.n; c++)
        if (g->val[c] != g->sol[c]) return 0;
    return 1;
}

int flm_script(flm_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (flm_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if (!g->val[c]) { flm_put(g, c, g->sol[c]); k--; }
            continue;
        }
        int len, c = bench_cell(s, n, n, &len);
        if (c < 0 || (s[len] != '=' && s[len] != '\'') || s[len + 1] < '0' || s[len + 1] > '9') return 0;
        int ok = s[len] == '=' ? flm_put(g, c, s[len + 1] - '0') : flm_note(g, c, s[len + 1] - '0');
        if (!ok) return 0;
        s += len + 2;
    }
    return 1;
}
