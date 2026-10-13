/*
 * Bridges: the rules (hashi.h): the links between islands and their crossings, the solver that grades (bench.h) and
 * the one that counts, the maker, and the player's bridges with their undo. No SDL: tested by tests/hashi_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/hashi.h"

#include <string.h>

#include "games/puzrand.h"

#define CROSS 16                                /* the links one link may cross, at most */

void hsh_links(hsh_puzzle *p)
{
    int n = p->n;
    p->nlink = 0;
    for (int c = 0; c < n * n; c++) {
        int i = p->at[c];
        if (i < 0) continue;
        int x = c % n, y = c / n;
        for (int k = x + 1; k < n; k++)         /* the nearest island to the right */
            if (p->at[y * n + k] >= 0) {
                p->la[p->nlink] = (uint8_t)i;
                p->lb[p->nlink++] = (uint8_t)p->at[y * n + k];
                break;
            }
        for (int k = y + 1; k < n; k++)         /* and below */
            if (p->at[k * n + x] >= 0) {
                p->la[p->nlink] = (uint8_t)i;
                p->lb[p->nlink++] = (uint8_t)p->at[k * n + x];
                break;
            }
    }
}

int hsh_cross(const hsh_puzzle *p, int a, int b)
{
    int ha = p->iy[p->la[a]] == p->iy[p->lb[a]], hb = p->iy[p->la[b]] == p->iy[p->lb[b]];
    if (ha == hb) return 0;
    if (!ha) { int t = a; a = b; b = t; }      /* a across, b down */
    int y = p->iy[p->la[a]], x0 = p->ix[p->la[a]], x1 = p->ix[p->lb[a]];
    int x = p->ix[p->la[b]], y0 = p->iy[p->la[b]], y1 = p->iy[p->lb[b]];
    return x0 < x && x < x1 && y0 < y && y < y1;
}

int hsh_link(const hsh_puzzle *p, int i, int j)
{
    for (int l = 0; l < p->nlink; l++)
        if ((p->la[l] == i && p->lb[l] == j) || (p->la[l] == j && p->lb[l] == i)) return l;
    return -1;
}

/* ---- the solver ------------------------------------------------------------------------------------------------- */
typedef struct {
    const hsh_puzzle *p;
    uint8_t nof[HSH_ISLANDS], of[HSH_ISLANDS][4];   /* an island's links */
    uint8_t ncross[HSH_LINKS], cross[HSH_LINKS][CROSS];
} ctx_t;

static int lo(unsigned m) { return bench_low(m); }
static int hi(unsigned m) { return bench_high(m); }

static int cut(bench_grid *g, int l, unsigned keep)
{
    unsigned m = g->d[l] & keep;
    if (m == g->d[l]) return 0;
    g->d[l] = (uint16_t)m;
    return 1;
}

/* the groups the sure bridges make: each island's group (its first island), how much the group still lacks (the
 * numbers less the sure bridges), its size */
static void groups(const bench_grid *g, const ctx_t *x, int *root, int *lack, int *size)
{
    const hsh_puzzle *p = x->p;
    for (int i = 0; i < p->nisl; i++) root[i] = i;
    for (int l = 0; l < p->nlink; l++) {
        if (lo(g->d[l]) < 1) continue;
        int a = p->la[l], b = p->lb[l];
        while (root[a] != a) a = root[a] = root[root[a]];
        while (root[b] != b) b = root[b] = root[root[b]];
        if (a != b) root[a > b ? a : b] = a < b ? a : b;
    }
    for (int i = 0; i < p->nisl; i++) {
        int r = i;
        while (root[r] != r) r = root[r];
        root[i] = r;
        lack[i] = size[i] = 0;
    }
    for (int i = 0; i < p->nisl; i++) {
        int have = 0;
        for (int k = 0; k < x->nof[i]; k++) have += lo(g->d[x->of[i][k]]);
        lack[root[i]] += p->need[i] - have;
        size[root[i]]++;
    }
}

static int deduce(bench_grid *g, int tier, void *cx)
{
    ctx_t *x = cx;
    const hsh_puzzle *p = x->p;
    int found = 0;
    if (tier == 0) {
        for (int l = 0; l < p->nlink; l++)      /* a sure bridge: none across it */
            if (lo(g->d[l]) >= 1)
                for (int k = 0; k < x->ncross[l]; k++)
                    if (cut(g, x->cross[l][k], 1)) {
                        found = 1;
                        if (!g->d[x->cross[l][k]]) return -1;
                    }
        for (int i = 0; i < p->nisl; i++) {     /* an island's number against what its links can take */
            int sl = 0, sh = 0, need = p->need[i];
            for (int k = 0; k < x->nof[i]; k++) sl += lo(g->d[x->of[i][k]]), sh += hi(g->d[x->of[i][k]]);
            if (sl > need || sh < need) return -1;
            for (int k = 0; k < x->nof[i]; k++) {
                int l = x->of[i][k], m = g->d[l];
                int least = need - (sh - hi(m)), most = need - (sl - lo(m));
                unsigned keep = 0;
                for (int v = least < 0 ? 0 : least; v <= most && v <= 2; v++) keep |= 1u << v;
                found |= cut(g, l, keep);
                if (!g->d[l]) return -1;
            }
        }
        return found;
    }
    int root[HSH_ISLANDS], lack[HSH_ISLANDS], size[HSH_ISLANDS];
    groups(g, x, root, lack, size);
    for (int l = 0; l < p->nlink; l++) {        /* no bridges that close a group off from the rest */
        int a = root[p->la[l]], b = root[p->lb[l]], m = g->d[l];
        int both = a == b ? lack[a] : lack[a] + lack[b], members = a == b ? size[a] : size[a] + size[b];
        if (members >= p->nisl) continue;
        for (int v = lo(m) + 1; v <= 2; v++)
            if (m >> v & 1 && both - 2 * (v - lo(m)) == 0) found |= cut(g, l, ~(1u << v));
        if (lo(m) == 0 && both == 0 && a != b) found |= cut(g, l, 1);   /* two finished groups: kept apart */
        if (!g->d[l]) return -1;
    }
    if (found) return 1;
    for (int i = 0; i < p->nisl; i++) {         /* a group with one way out takes it; none: stuck apart */
        if (root[i] != i || size[i] >= p->nisl) continue;
        int out = -1, ways = 0;
        for (int j = 0; j < p->nisl && ways < 2; j++) {
            if (root[j] != i) continue;
            for (int k = 0; k < x->nof[j]; k++) {
                int l = x->of[j][k];
                if (root[p->la[l]] == root[p->lb[l]] || hi(g->d[l]) < 1) continue;
                if (l != out) out = l, ways++;
            }
        }
        if (!ways) return -1;
        if (ways == 1) found |= cut(g, out, 6);
    }
    return found;
}

static int valid(const bench_grid *g, void *cx)
{
    const ctx_t *x = cx;
    const hsh_puzzle *p = x->p;
    for (int i = 0; i < p->nisl; i++) {
        int have = 0;
        for (int k = 0; k < x->nof[i]; k++) have += bench_low(g->d[x->of[i][k]]);
        if (have != p->need[i]) return 0;
    }
    for (int l = 0; l < p->nlink; l++)
        if (bench_low(g->d[l]))
            for (int k = 0; k < x->ncross[l]; k++)
                if (bench_low(g->d[x->cross[l][k]])) return 0;
    int root[HSH_ISLANDS], lack[HSH_ISLANDS], size[HSH_ISLANDS];
    groups(g, x, root, lack, size);
    return size[0] == p->nisl;
}

static void rules_of(bench_rules *r, ctx_t *x, const hsh_puzzle *p, bench_grid *g)
{
    x->p = p;
    memset(x->nof, 0, sizeof x->nof);
    for (int l = 0; l < p->nlink; l++) {
        x->of[p->la[l]][x->nof[p->la[l]]++] = (uint8_t)l;
        x->of[p->lb[l]][x->nof[p->lb[l]]++] = (uint8_t)l;
        x->ncross[l] = 0;
    }
    for (int a = 0; a < p->nlink; a++)
        for (int b = a + 1; b < p->nlink; b++)
            if (hsh_cross(p, a, b)) {
                if (x->ncross[a] < CROSS) x->cross[a][x->ncross[a]++] = (uint8_t)b;
                if (x->ncross[b] < CROSS) x->cross[b][x->ncross[b]++] = (uint8_t)a;
            }
    memset(r, 0, sizeof *r);
    r->vars = p->nlink;
    r->tiers = 2;
    r->probe_tiers = 2;
    r->order = BENCH_PAIRS;                     /* the trials the makers were tuned with */
    r->deduce = deduce;
    r->valid = valid;
    r->ctx = x;
    r->budget = 20000;
    for (int l = 0; l < p->nlink; l++) g->d[l] = 7;
}

/* graded up to `most`; the links still open (which may be NULL) */
static int grade(const hsh_puzzle *p, int most, int *open, uint8_t *which)
{
    static bench_rules r;
    static ctx_t x;
    bench_grid g;
    rules_of(&r, &x, p, &g);
    int res = bench_grade(&g, &r, most);
    if (open) {
        *open = 0;
        for (int l = 0; l < p->nlink; l++)
            if (!bench_one(g.d[l])) which[(*open)++] = (uint8_t)l;
    }
    return res;
}

int hsh_rate(const hsh_puzzle *p) { return grade(p, BENCH_HARD, NULL, NULL); }

static int count_rec(bench_grid *g, bench_rules *r, int limit)
{
    int used = 0;
    if (bench_settle(g, r, 1, &used) < 0) return 0;
    int best = -1;
    for (int l = 0; l < r->vars && best < 0; l++)
        if (!bench_one(g->d[l])) best = l;
    if (best < 0) return r->valid(g, r->ctx);
    int found = 0;
    for (unsigned m = g->d[best]; m && found < limit; m &= m - 1) {
        bench_grid t = *g;
        t.d[best] = (uint16_t)(m & -m);
        found += count_rec(&t, r, limit - found);
    }
    return found;
}

int hsh_count(const hsh_puzzle *p, int limit)
{
    static bench_rules r;
    static ctx_t x;
    bench_grid g;
    rules_of(&r, &x, p, &g);
    r.budget = 0;
    return count_rec(&g, &r, limit);
}

/* ---- the maker -------------------------------------------------------------------------------------------------- */
static void numbers(hsh_puzzle *p)
{
    memset(p->need, 0, sizeof p->need);
    for (int l = 0; l < p->nlink; l++) {
        p->need[p->la[l]] = (uint8_t)(p->need[p->la[l]] + p->sol[l]);
        p->need[p->lb[l]] = (uint8_t)(p->need[p->lb[l]] + p->sol[l]);
    }
}

/* the islands laid out: each new one along a line from one already there, a bridge or two between them (nothing in
 * between, no bridge crossed, no island touching another); the bridges then joined to the links */
static void lay_out(uint32_t *rng, int n, hsh_puzzle *p)
{
    static uint8_t across[HSH_CELLS], down[HSH_CELLS];   /* a bridge passing over a point */
    int want = n * n * (n <= 7 ? 26 : 23) / 100, reach = n <= 7 ? 4 : n <= 10 ? 5 : 6;
    int bx[HSH_ISLANDS], by[HSH_ISLANDS], bi[HSH_ISLANDS], bj[HSH_ISLANDS], bc[HSH_ISLANDS], nb = 0;
    memset(across, 0, sizeof across);
    memset(down, 0, sizeof down);
    for (int c = 0; c < n * n; c++) p->at[c] = -1;
    p->n = n;
    p->nisl = 1;
    p->ix[0] = (uint8_t)pr_below(rng, n);
    p->iy[0] = (uint8_t)pr_below(rng, n);
    p->at[p->iy[0] * n + p->ix[0]] = 0;
    for (int tries = 0; tries < 60 * want && p->nisl < want && p->nisl < HSH_ISLANDS; tries++) {
        int i = pr_below(rng, p->nisl), dir = pr_below(rng, 4), len = 2 + pr_below(rng, reach - 1);
        int dx = dir == 0 ? 1 : dir == 1 ? -1 : 0, dy = dir == 2 ? 1 : dir == 3 ? -1 : 0;
        int x = p->ix[i] + dx * len, y = p->iy[i] + dy * len;
        if (x < 0 || y < 0 || x >= n || y >= n) continue;
        int c = y * n + x, ok = p->at[c] < 0 && !across[c] && !down[c];
        for (int t = 0; t < 4 && ok; t++) {     /* no island beside it */
            int ax = x + (t == 0) - (t == 1), ay = y + (t == 2) - (t == 3);
            if (ax >= 0 && ay >= 0 && ax < n && ay < n && p->at[ay * n + ax] >= 0) ok = 0;
        }
        for (int k = 1; k < len && ok; k++) {   /* the way clear */
            int q = (p->iy[i] + dy * k) * n + p->ix[i] + dx * k;
            if (p->at[q] >= 0 || across[q] || down[q]) ok = 0;
        }
        if (!ok) continue;
        for (int k = 1; k < len; k++) {
            int q = (p->iy[i] + dy * k) * n + p->ix[i] + dx * k;
            if (dx) across[q] = 1; else down[q] = 1;
        }
        int j = p->nisl++;
        p->ix[j] = (uint8_t)x;
        p->iy[j] = (uint8_t)y;
        p->at[c] = (int16_t)j;
        bi[nb] = i, bj[nb] = j, bc[nb] = pr_below(rng, 3) == 0 ? 2 : 1, bx[nb] = x, by[nb] = y, nb++;
    }
    hsh_links(p);
    memset(p->sol, 0, sizeof p->sol);
    for (int k = 0; k < nb; k++) {
        int l = hsh_link(p, bi[k], bj[k]);
        if (l >= 0) p->sol[l] = (uint8_t)bc[k];
    }
    (void)bx, (void)by;
    for (int l = 0; l < p->nlink; l++) {        /* a few more where they cross nothing */
        if (p->sol[l] || pr_below(rng, 3)) continue;
        int clear = 1;
        for (int k = 0; k < p->nlink && clear; k++)
            if (p->sol[k] && hsh_cross(p, l, k)) clear = 0;
        if (clear) p->sol[l] = 1;
    }
    numbers(p);
}

static int joined_without(const hsh_puzzle *p, int skip)
{
    int root[HSH_ISLANDS], groups = p->nisl;
    for (int i = 0; i < p->nisl; i++) root[i] = i;
    for (int l = 0; l < p->nlink; l++) {
        if (!p->sol[l] || l == skip) continue;
        int a = p->la[l], b = p->lb[l];
        while (root[a] != a) a = root[a];
        while (root[b] != b) b = root[b];
        if (a != b) root[a] = b, groups--;
    }
    return groups == 1;
}

/* a link's bridges changed (added where nothing is crossed, doubled, halved, taken away if all stay joined) */
static int change(uint32_t *rng, hsh_puzzle *p, int l)
{
    int s = p->sol[l];
    if (s == 0) {
        for (int k = 0; k < p->nlink; k++)
            if (p->sol[k] && hsh_cross(p, l, k)) return 0;
        p->sol[l] = 1;
    } else if (s == 2) {
        p->sol[l] = 1;
    } else if (pr_below(rng, 2) && joined_without(p, l)) {
        p->sol[l] = 0;
    } else {
        p->sol[l] = 2;
    }
    numbers(p);
    return 1;
}

static int make_one(uint32_t *rng, int n, int level, hsh_puzzle *p)
{
    uint8_t open[HSH_LINKS], open2[HSH_LINKS];
    lay_out(rng, n, p);
    if (p->nisl < 4) return 0;
    int nopen, stuck = 0, r = grade(p, level, &nopen, open);
    for (int step = 0; r == BENCH_GUESS && step < 20 * n && stuck < 8 * n; step++) {
        hsh_puzzle was = *p;
        if (!nopen || !change(rng, p, open[pr_below(rng, nopen)])) { stuck++; continue; }
        int n2, r2 = grade(p, level, &n2, open2);
        if (r2 >= 0 && (r2 != BENCH_GUESS || n2 <= nopen || pr_below(rng, 10) == 0)) {
            r = r2;
            nopen = n2;
            memcpy(open, open2, (size_t)n2);
        } else {
            *p = was;
            stuck++;
        }
    }
    if (r == BENCH_GUESS || r < 0) return 0;
    for (int step = 0; r < level && step < 20 * n; step++) {   /* then harder, a link at a time */
        hsh_puzzle was = *p;
        if (!change(rng, p, pr_below(rng, p->nlink))) continue;
        int r2 = grade(p, level - 1, NULL, NULL);
        if (r2 == BENCH_GUESS) r2 = grade(p, level, NULL, NULL);
        if (r2 == BENCH_GUESS || r2 < 0) *p = was;
        else r = r2;
    }
    return r == level;
}

void hsh_make(int n, int level, uint32_t seed, hsh_puzzle *p)
{
    uint32_t rng = pr_seed(seed);
    if (n < 5 || n > HSH_MAX) n = 7;
    if (level < 0 || level >= HSH_LEVELS) level = HSH_EASY;
    for (int tries = 0;; tries++) {
        if (make_one(&rng, n, level, p)) return;
        if (tries > 300 && level > HSH_EASY) level--, tries = 0;   /* never stuck */
    }
}

int hsh_rate_work(const hsh_puzzle *p, long *passes)
{
    static bench_rules r;
    static ctx_t x;
    bench_grid g;
    rules_of(&r, &x, p, &g);
    int res = bench_grade(&g, &r, BENCH_HARD);
    if (passes) *passes = r.passes;
    return res;
}

/* ---- the designed levels ---------------------------------------------------------------------------------------- */
void hsh_encode(const hsh_puzzle *p, char *out)
{
    int k = 0, n = p->n;
    if (n >= 10) out[k++] = (char)('0' + n / 10);
    out[k++] = (char)('0' + n % 10);
    out[k++] = ':';
    for (int c = 0; c < n * n; c++) out[k++] = p->at[c] < 0 ? '.' : (char)('0' + p->need[p->at[c]]);
    out[k] = 0;
}

int hsh_decode(hsh_game *g, const char *s)
{
    memset(g, 0, sizeof *g);
    hsh_puzzle *p = &g->p;
    int n = 0;
    while (*s >= '0' && *s <= '9') n = n * 10 + *s++ - '0';
    if (*s++ != ':' || n < 3 || n > HSH_MAX) return 0;
    p->n = n;
    for (int c = 0; c < n * n; c++) {
        p->at[c] = -1;
        if (!s[c]) return 0;
        if (s[c] == '.') continue;
        if (s[c] < '1' || s[c] > '8' || p->nisl >= HSH_ISLANDS) return 0;
        int i = p->nisl++;
        p->ix[i] = (uint8_t)(c % n);
        p->iy[i] = (uint8_t)(c / n);
        p->need[i] = (uint8_t)(s[c] - '0');
        p->at[c] = (int16_t)i;
    }
    if (s[n * n] || p->nisl < 2) return 0;
    hsh_links(p);
    static bench_rules r;
    static ctx_t x;
    bench_grid sg;
    rules_of(&r, &x, p, &sg);
    r.budget = 0;
    int res = bench_grade(&sg, &r, BENCH_HARD);  /* proved: the one solution */
    if (res < BENCH_EASY || res > BENCH_HARD) return 0;
    uint8_t need[HSH_ISLANDS];
    memcpy(need, p->need, sizeof need);
    for (int l = 0; l < p->nlink; l++) p->sol[l] = (uint8_t)bench_low(sg.d[l]);
    numbers(p);
    if (memcmp(need, p->need, (size_t)p->nisl)) return 0;
    g->level = res;
    return 1;
}

/* ---- the player's bridges --------------------------------------------------------------------------------------- */
void hsh_new(hsh_game *g, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    if (variant < 0 || variant >= HSH_VARIANTS) variant = 0;
    g->variant = variant;
    g->level = hsh_level_of(variant);
    g->seed = seed;
    hsh_make(hsh_size_of(variant), g->level, seed, &g->p);
    bench_clear(&g->hist);
}

static void restore(void *cx, int at, uint32_t was)
{
    hsh_game *g = cx;
    g->br[at] = (uint8_t)was;
}

/* the wrong entries on the board (the stars' mistakes) */
static int wrongs(const hsh_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.nlink; c++)
        k += (g->br[c] > g->p.sol[c]) * (g->br[c] - g->p.sol[c]);
    return k;
}

int hsh_blocked(const hsh_game *g, int l)
{
    for (int k = 0; k < g->p.nlink; k++)
        if (g->br[k] && hsh_cross(&g->p, l, k)) return k;
    return -1;
}

int hsh_set(hsh_game *g, int l, int count)
{
    if (l < 0 || l >= g->p.nlink || count < 0 || count > 2 || g->br[l] == count) return 0;
    if (count && hsh_blocked(g, l) >= 0) return 0;
    bench_begin(&g->hist, 1);
    bench_log(&g->hist, l, g->br[l]);
    g->br[l] = (uint8_t)count;
    return bench_done(&g->hist, wrongs(g));
}

int hsh_cycle(hsh_game *g, int l)
{
    if (l < 0 || l >= g->p.nlink) return 0;
    return hsh_set(g, l, (g->br[l] + 1) % 3);
}

int hsh_has(const hsh_game *g, int i)
{
    int k = 0;
    for (int l = 0; l < g->p.nlink; l++)
        if (g->p.la[l] == i || g->p.lb[l] == i) k += g->br[l];
    return k;
}

static int hint_step(hsh_game *g)
{
    const hsh_puzzle *p = &g->p;
    for (int l = 0; l < p->nlink; l++)          /* too many bridges somewhere: taken down to the solution's */
        if (g->br[l] > p->sol[l]) return hsh_set(g, l, p->sol[l]) ? l : -1;
    static bench_rules r;
    static ctx_t x;
    bench_grid s;
    rules_of(&r, &x, p, &s);
    for (int l = 0; l < p->nlink; l++) s.d[l] = (uint16_t)(7u & ~((1u << g->br[l]) - 1));  /* at least those */
    int used = 0;
    bench_settle(&s, &r, 1, &used);
    int best = -1;
    for (int l = 0; l < p->nlink && best < 0; l++)   /* a link reasoning settles that is not yet as it should be */
        if (g->br[l] != p->sol[l] && bench_one(s.d[l])) best = l;
    for (int l = 0; l < p->nlink && best < 0; l++)
        if (g->br[l] != p->sol[l]) best = l;
    if (best < 0) return -1;
    return hsh_set(g, best, p->sol[best]) ? best : -1;
}

int hsh_hint(hsh_game *g)
{
    return bench_hinted(&g->hist, hint_step(g));
}

int hsh_undo(hsh_game *g)
{
    int c = bench_undo(&g->hist, restore, g);
    bench_undone(&g->hist, wrongs(g));
    return c;
}


int hsh_solved(const hsh_game *g)
{
    for (int l = 0; l < g->p.nlink; l++)
        if (g->br[l] != g->p.sol[l]) return 0;
    return 1;
}

static int island_at(const hsh_game *g, const char *s, int *len)
{
    int c = bench_cell(s, g->p.n, g->p.n, len);
    return c < 0 ? -1 : g->p.at[c];
}

int hsh_script(hsh_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (hsh_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int pass = 0; pass < 2; pass++)   /* the wrong ones down first, so nothing is blocked */
                for (int l = 0; l < g->p.nlink && k > 0; l++)
                    if (g->br[l] != g->p.sol[l] && (pass || g->br[l] > g->p.sol[l])) {
                        if (hsh_set(g, l, g->p.sol[l])) k--;
                    }
            continue;
        }
        int la, lb, a = island_at(g, s, &la);
        if (a < 0 || s[la] != '-') return 0;
        int b = island_at(g, s + la + 1, &lb), l = b < 0 ? -1 : hsh_link(&g->p, a, b);
        if (l < 0) return 0;
        s += la + 1 + lb;
        if (*s == '=') {
            if (s[1] < '0' || s[1] > '2' || !hsh_set(g, l, s[1] - '0')) return 0;
            s += 2;
        } else if (!hsh_cycle(g, l)) {
            return 0;
        }
    }
    return 1;
}
