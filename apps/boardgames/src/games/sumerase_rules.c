/*
 * Sum Erase: the rules (sumerase.h): the ways of reasoning the solver grades with, the maker, the player's grid and
 * undo. No SDL: tested by tests/sumerase_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/sumerase.h"

#include <stdio.h>
#include <string.h>

#include "games/puzrand.h"

#define KEEP 1u                                 /* a cell's values: bit 0 kept, bit 1 erased */
#define GONE 2u
#define BOTH 3u

static const int SIZES[SME_SIZES] = {6, 8, 9};

int sme_size(int variant) { return SIZES[(variant < 0 ? 0 : variant) / 3 % SME_SIZES]; }
int sme_level(int variant) { return (variant < 0 ? 0 : variant) % 3; }

static int at(int n, int way, int i, int j) { return way ? j * n + i : i * n + j; }
static int target(const sme_puzzle *p, int way, int i) { return way ? p->col[i] : p->row[i]; }

void sme_sums(sme_puzzle *p, const uint8_t *erased)
{
    int n = p->n;
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++) {
            int s = 0;
            for (int j = 0; j < n; j++) {
                int c = at(n, way, i, j);
                if (!erased[c]) s += p->num[c];
            }
            if (way) p->col[i] = (uint8_t)s;
            else p->row[i] = (uint8_t)s;
        }
}

/* ---- the solver's ways ------------------------------------------------------------------------------------------- */
/* the sums the cells u[0..nu-1] but skip can make (bit s of r) */
static void reach(const sme_puzzle *p, const int *u, int nu, int skip, uint8_t *r, int top)
{
    memset(r, 0, (size_t)top + 1);
    r[0] = 1;
    for (int i = 0; i < nu; i++) {
        if (i == skip) continue;
        int v = p->num[u[i]];
        for (int s = top; s >= v; s--)
            if (r[s - v]) r[s] = 1;
    }
}

/* one pass over the lines with the easy (tier 0) or the medium (tier 1) ways: 1 found, 0 nothing, -1 contradiction */
static int lines(bench_grid *g, const sme_puzzle *p, int tier)
{
    int n = p->n, found = 0;
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++) {
            int need = target(p, way, i), u[SME_MAX], nu = 0, open = 0;
            for (int j = 0; j < n; j++) {
                int c = at(n, way, i, j);
                if (g->d[c] == KEEP) need -= p->num[c];
                else if (g->d[c] == BOTH) u[nu++] = c, open += p->num[c];
                else if (!g->d[c]) return -1;
            }
            if (need < 0 || need > open) return -1;
            if (!nu) continue;
            for (int k = 0; k < nu; k++) {
                int c = u[k], v = p->num[c];
                unsigned to = BOTH;
                if (tier == 0) {
                    if (v > need) to = GONE;
                    else if (open - v < need) to = KEEP;
                } else {
                    uint8_t r[SME_MAX * 9 + 1];
                    reach(p, u, nu, k, r, open);
                    int keep = need - v >= 0 && r[need - v], gone = r[need];
                    if (!keep && !gone) return -1;
                    to = (keep ? KEEP : 0) | (gone ? GONE : 0);
                }
                if ((g->d[c] & to) != g->d[c]) {
                    g->d[c] &= (uint16_t)to;
                    found = 1;
                    if (!g->d[c]) return -1;
                }
            }
        }
    return found;
}

static long finds[2];                           /* each tier's finds (sme_effort counts them) */

static int deduce(bench_grid *g, int tier, void *c)
{
    int res = lines(g, (const sme_puzzle *)c, tier);
    if (res > 0) finds[tier ? 1 : 0]++;
    return res;
}

static int valid(const bench_grid *g, void *cv)
{
    const sme_puzzle *p = cv;
    int n = p->n;
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++) {
            int s = 0;
            for (int j = 0; j < n; j++) {
                int c = at(n, way, i, j);
                if (g->d[c] == KEEP) s += p->num[c];
            }
            if (s != target(p, way, i)) return 0;
        }
    return 1;
}

static bench_rules rules_of(const sme_puzzle *p, long *work)
{
    return (bench_rules){.vars = p->n * p->n, .tiers = 2, .probe_tiers = 1,
                        .deduce = deduce, .valid = valid, .ctx = (void *)p, .work = work};
}

static void grid_of(const sme_puzzle *p, bench_grid *g)
{
    memset(g, 0, sizeof *g);
    for (int c = 0; c < p->n * p->n; c++) g->d[c] = BOTH;
}

static int grade_work(const sme_puzzle *p, uint8_t *sol, long *work, int cap, bench_grid *end)
{
    bench_rules r = rules_of(p, work);
    r.budget = 6000;                            /* one grade's passes at most */
    bench_grid g;
    grid_of(p, &g);
    int lv = bench_grade(&g, &r, cap);
    if (sol && lv >= BENCH_EASY && lv <= BENCH_HARD)
        for (int c = 0; c < p->n * p->n; c++) sol[c] = g.d[c] == GONE;
    if (end) *end = g;
    return lv;
}

int sme_grade(const sme_puzzle *p, int cap, uint8_t *sol) { return grade_work(p, sol, NULL, cap, NULL); }

int sme_count(const sme_puzzle *p, int limit)
{
    bench_rules r = rules_of(p, NULL);
    bench_grid g;
    grid_of(p, &g);
    return bench_search(&g, &r, limit, NULL, NULL, 0);
}

int sme_effort(const sme_puzzle *p, long *passes, long *mediums)
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
int sme_encode(const sme_puzzle *p, const uint8_t *sol, char *out, int size)
{
    int n = p->n, len = snprintf(out, (size_t)size, "%d:", n);
    if (len < 0 || len + n * n + 1 > size) return 0;
    for (int c = 0; c < n * n; c++) out[len++] = (char)(sol[c] ? 'a' + p->num[c] - 1 : '0' + p->num[c]);
    out[len] = 0;
    return len;
}

int sme_decode(const char *s, sme_puzzle *p, uint8_t *sol)
{
    memset(p, 0, sizeof *p);
    int n = s[0] - '0';
    if (n < 2 || n > SME_MAX || s[1] != ':') return 0;
    p->n = n;
    for (int c = 0; c < n * n; c++) {
        char ch = s[2 + c];
        if (ch >= '1' && ch <= '9') p->num[c] = (uint8_t)(ch - '0'), sol[c] = 0;
        else if (ch >= 'a' && ch <= 'i') p->num[c] = (uint8_t)(ch - 'a' + 1), sol[c] = 1;
        else return 0;
    }
    if (s[2 + n * n]) return 0;
    sme_sums(p, sol);
    return 1;
}

/* ---- the maker --------------------------------------------------------------------------------------------------- */
typedef struct {
    sme_puzzle p;
    uint8_t gone[SME_CELLS];
} sheet;

/* every line keeps one digit at least and loses one at least */
static int sound(const sheet *m)
{
    int n = m->p.n;
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++) {
            int k = 0;
            for (int j = 0; j < n; j++) k += m->gone[at(n, way, i, j)];
            if (k == 0 || k == n) return 0;
        }
    return 1;
}

static int deal(sheet *m, int n, int maxd, uint32_t *s)
{
    memset(m, 0, sizeof *m);
    m->p.n = n;
    int share = 38 + pr_below(s, 12);           /* the part erased, in hundredths */
    for (int tries = 0; tries < 50; tries++) {
        for (int c = 0; c < n * n; c++) {
            m->p.num[c] = (uint8_t)(1 + pr_below(s, maxd));
            m->gone[c] = (uint8_t)(pr_below(s, 100) < share);
        }
        if (sound(m)) {
            sme_sums(&m->p, m->gone);
            return 1;
        }
    }
    return 0;
}

/* a small change, most often where the reasoning got stuck (und: those cells): a digit changed, a cell kept or erased
 * instead; 0: none that keeps the lines sound */
static int mutate(sheet *m, int maxd, uint32_t *s, const int *und, int nu)
{
    sheet was = *m;
    int cells = m->p.n * m->p.n;
    for (int tries = 0; tries < 30; tries++) {
        int c = nu && pr_below(s, 100) < 85 ? und[pr_below(s, nu)] : pr_below(s, cells);
        if (pr_below(s, 2)) m->p.num[c] = (uint8_t)(1 + pr_below(s, maxd));
        else m->gone[c] = (uint8_t)!m->gone[c];
        if (sound(m) && memcmp(m, &was, sizeof was)) {
            sme_sums(&m->p, m->gone);
            return 1;
        }
        *m = was;
    }
    return 0;
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

#define MAKE_BUDGET 1500000L                    /* the solver's passes a puzzle may cost (well under a second) */

/* solved at exactly the level: easy by the easy ways; medium needing the medium ways; hard needing trials */
static int at_level(const sheet *m, long *work, int level, int lv)
{
    if (lv != level) return 0;
    if (level == BENCH_EASY) return 1;
    int u[SME_CELLS], nu, lower;
    progress(m, work, level - 1, &lower, u, &nu);
    return lower == BENCH_GUESS;
}

void sme_make(int n, int maxd, int level, uint32_t seed, long budget, sme_puzzle *p, uint8_t *sol)
{
    if (n < 3 || n > SME_MAX) n = 4;
    if (maxd < 3 || maxd > 9) maxd = 9;
    if (level < BENCH_EASY || level > BENCH_HARD) level = BENCH_EASY;
    if (budget <= 0) budget = MAKE_BUDGET;
    uint32_t s = pr_seed(seed);
    long work = 0;
    sheet keep, m, best;
    int best_lv = -2, have = 0;
    for (int attempt = 0; attempt < 3000; attempt++) {
        if (!deal(&keep, n, maxd, &s)) continue;
        int und[SME_CELLS], nu, lv, got = progress(&keep, &work, level, &lv, und, &nu);
        for (int step = 0; step < 30 * n; step++) {
            if (lv >= BENCH_EASY && lv <= level) {
                int exact = at_level(&keep, &work, level, lv);
                if (!have || exact || lv > best_lv) best = keep, best_lv = lv, have = 1;
                if (exact) best_lv = level + 10;
                break;
            }
            m = keep;
            if (!mutate(&m, maxd, &s, und, nu)) break;
            int u3[SME_CELLS], n3, lv3, got3 = progress(&m, &work, level, &lv3, u3, &n3);
            if (got3 >= got || (lv3 >= BENCH_EASY && lv3 <= level)) {
                keep = m;
                got = got3;
                lv = lv3;
                nu = n3;
                memcpy(und, u3, sizeof u3);
            }
            if (work >= 4 * budget && have) break;
        }
        if (best_lv > level || (work >= 3 * budget && have)) break;   /* past the budget: the nearest */
    }
    if (!have) {                                /* never seen: a smaller one rather than none */
        sme_make(n > 3 ? n - 1 : 3, maxd, BENCH_EASY, seed + 1, budget, p, sol);
        return;
    }
    *p = best.p;
    memcpy(sol, best.gone, sizeof best.gone);
}

/* ---- the player's grid ------------------------------------------------------------------------------------------- */
int sme_load(sme_game *g, const sme_puzzle *p)
{
    memset(g, 0, sizeof *g);
    g->p = *p;
    g->level = sme_grade(p, BENCH_HARD, g->sol);
    return g->level >= BENCH_EASY && g->level <= BENCH_HARD;
}

void sme_new(sme_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= SME_VARIANTS) variant = 0;
    sme_puzzle p;
    uint8_t sol[SME_CELLS];
    sme_make(sme_size(variant), 9, sme_level(variant), seed, 0, &p, sol);
    memset(g, 0, sizeof *g);
    g->p = p;
    g->level = sme_level(variant);
    g->seed = seed;
    memcpy(g->sol, sol, sizeof sol);
}

int sme_free(const sme_game *g, int c) { return c >= 0 && c < g->p.n * g->p.n; }
int sme_tap_state(const sme_game *g, int c) { return g->st[c] == SME_ERASE ? SME_EMPTY : SME_ERASE; }
int sme_hold_state(const sme_game *g, int c) { return g->st[c] == SME_KEEP ? SME_EMPTY : SME_KEEP; }

/* the wrong entries on the board (the stars' mistakes) */
static int wrongs(const sme_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++) k += g->st[c] != SME_EMPTY && (g->st[c] == SME_ERASE) != g->sol[c];
    return k;
}

int sme_paint(sme_game *g, const int *cells, int n, int state)
{
    bench_begin(&g->hist, n);
    for (int i = 0; i < n; i++)
        if (sme_free(g, cells[i])) bench_set(&g->hist, g->st, cells[i], state);
    return bench_done(&g->hist, wrongs(g));
}

static int put(sme_game *g, int c, int state) { return sme_paint(g, &c, 1, state); }
static int right(const sme_game *g, int c) { return g->sol[c] ? SME_ERASE : SME_KEEP; }

static int hint_step(sme_game *g)
{
    int cells = g->p.n * g->p.n;
    for (int c = 0; c < cells; c++)             /* a wrong cell first */
        if (g->st[c] != SME_EMPTY && (g->st[c] == SME_ERASE) != g->sol[c]) return put(g, c, right(g, c)) ? c : -1;
    bench_rules r = rules_of(&g->p, NULL);
    bench_grid d;
    grid_of(&g->p, &d);
    for (int c = 0; c < cells; c++)
        if (g->st[c] != SME_EMPTY) d.d[c] = (uint16_t)(g->sol[c] ? GONE : KEEP);
    int c = -1;
    for (int round = 0; round < cells && c < 0; round++) {   /* the next cell found, not one the player already has */
        c = bench_next(&d, &r);
        if (c < 0) break;
        if (g->st[c] != SME_EMPTY) {
            d.d[c] = (uint16_t)(g->sol[c] ? GONE : KEEP);
            c = -1;
        }
    }
    for (int i = 0; i < cells && c < 0; i++)
        if (g->st[i] == SME_EMPTY && g->sol[i]) c = i;
    if (c < 0) return -1;
    return put(g, c, right(g, c)) ? c : -1;
}

int sme_hint(sme_game *g) { return bench_hinted(&g->hist, hint_step(g)); }

int sme_undo(sme_game *g)
{
    int c = bench_undo_cells(&g->hist, g->st);
    bench_undone(&g->hist, wrongs(g));
    return c;
}

int sme_line(const sme_game *g, int way, int i)
{
    int n = g->p.n, left = 0, ringed = 0, t = target(&g->p, way, i);
    for (int j = 0; j < n; j++) {
        int c = at(n, way, i, j);
        if (g->st[c] != SME_ERASE) left += g->p.num[c];
        if (g->st[c] == SME_KEEP) ringed += g->p.num[c];
    }
    if (left == t) return 1;
    return left < t || ringed > t ? -1 : 0;
}

int sme_solved(const sme_game *g)
{
    if (!g->p.n) return 0;
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < g->p.n; i++)
            if (sme_line(g, way, i) != 1) return 0;
    return 1;
}

int sme_script(sme_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (sme_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < n * n && k > 0; c++)
                if ((g->st[c] == SME_ERASE) != g->sol[c]) put(g, c, g->sol[c] ? SME_ERASE : SME_EMPTY), k--;
            continue;
        }
        int hold = *s == '.', len = 0;
        if (hold) s++;
        int a = bench_cell(s, n, n, &len);
        if (!sme_free(g, a)) return 0;
        s += len;
        int b = a;
        if (*s == '-') {
            b = bench_cell(s + 1, n, n, &len);
            if (b < 0 || (b % n != a % n && b / n != a / n)) return 0;
            s += 1 + len;
        }
        int was = g->st[a], to = hold ? sme_hold_state(g, a) : sme_tap_state(g, a), cells[SME_MAX], k = 0;
        int dx = (b % n > a % n) - (b % n < a % n), dy = (b / n > a / n) - (b / n < a / n);
        for (int c = a;; c += dy * n + dx) {
            if (g->st[c] == was) cells[k++] = c;
            if (c == b) break;
        }
        if (!sme_paint(g, cells, k, to)) return 0;
    }
    return 1;
}
