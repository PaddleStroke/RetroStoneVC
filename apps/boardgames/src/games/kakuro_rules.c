/*
 * Kakuro: the rules (kakuro.h): the runs and the ways of making their clues, the solver that grades (bench.h) and
 * the one that counts, the maker, and the player's grid with its notes and undo. No SDL: tested by tests/kakuro_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/kakuro.h"

#include <string.h>

#include "games/puzrand.h"

#define DIGITS 0x3feu                           /* the digits 1..9 as bits */

static uint8_t SUM[1024], LEN[1024];            /* a set of digits (bit d): its sum, its size */
static uint16_t WAYS[10][46][126];              /* the sets of `len` digits adding up to `sum` */
static uint8_t NWAYS[10][46];
static int ready;

static void init(void)
{
    if (ready) return;
    for (unsigned m = 0; m < 1024; m++) {
        unsigned s = 0, l = 0;
        for (int d = 1; d <= 9; d++)
            if (m >> d & 1) s += (unsigned)d, l++;
        SUM[m] = (uint8_t)s;
        LEN[m] = (uint8_t)l;
        if (!(m & 1) && m) WAYS[l][s][NWAYS[l][s]++] = (uint16_t)m;
    }
    ready = 1;
}

int kkr_runs(const kkr_puzzle *p, kkr_run *runs, uint8_t (*run_of)[2])
{
    int n = p->n, k = 0;
    for (int c = 0; c < n * n; c++) {
        if (p->white[c]) continue;
        int x = c % n, y = c / n;
        for (int dir = 0; dir < 2; dir++) {
            int step = dir ? n : 1, len = 0;
            for (int i = 1; dir ? y + i < n : x + i < n; i++) {
                if (!p->white[c + i * step]) break;
                len++;
            }
            if (len < 1) continue;
            kkr_run *r = &runs[k];
            r->len = (uint8_t)(len > 9 ? 9 : len);
            r->sum = dir ? p->down[c] : p->across[c];
            r->clue = (uint8_t)c;
            for (int i = 0; i < r->len; i++) {
                r->cell[i] = (uint8_t)(c + (i + 1) * step);
                if (run_of) run_of[r->cell[i]][dir] = (uint8_t)k;
            }
            k++;
        }
    }
    return k;
}

/* ---- the solver's tiers ------------------------------------------------------------------------------------------ */
typedef struct {
    const kkr_puzzle *p;
    kkr_run run[KKR_RUNS];
    int nruns;
} ctx_t;

static int cut(bench_grid *g, int c, unsigned keep)
{
    unsigned m = g->d[c] & keep;
    if (m == g->d[c]) return 0;
    g->d[c] = (uint16_t)m;
    return 1;
}

/* the ways of making a run's clue that fit what is known: their count; the digits placed in the run, the digits of
 * any way (uni), the digits of every way (every) */
static int ways_of(const bench_grid *g, const kkr_run *r, unsigned *placed, unsigned *uni, unsigned *every)
{
    unsigned can = 0;
    int ways = 0;
    *placed = 0;
    *uni = 0;
    *every = DIGITS;
    for (int i = 0; i < r->len; i++) {
        unsigned m = g->d[r->cell[i]];
        if (bench_one(m)) {
            if (*placed & m) return 0;          /* twice in the run */
            *placed |= m;
        }
        can |= m;
    }
    if (r->sum > 45) return 0;
    for (int w = 0; w < NWAYS[r->len][r->sum]; w++) {
        unsigned m = WAYS[r->len][r->sum][w];
        if ((m & *placed) != *placed || (m & ~can)) continue;
        int fits = 1;
        for (int i = 0; i < r->len && fits; i++) fits = (g->d[r->cell[i]] & m) != 0;
        if (!fits) continue;
        ways++;
        *uni |= m;
        *every &= m;
    }
    return ways;
}

/* easy: a digit placed is not used again in its run; a square keeps only the digits of the ways */
static int run_easy(bench_grid *g, const kkr_run *r)
{
    unsigned placed, uni, every;
    int found = 0;
    if (!ways_of(g, r, &placed, &uni, &every)) return -1;
    for (int i = 0; i < r->len; i++) {
        unsigned m = g->d[r->cell[i]];
        found |= cut(g, r->cell[i], bench_one(m) ? m : uni & ~placed);
    }
    return found;
}

/* medium: a digit every way needs, with one square left for it; two or three squares that share two or three
 * digits between them (those digits leave the run's other squares) */
static int run_medium(bench_grid *g, const kkr_run *r)
{
    unsigned placed, uni, every;
    int found = 0, open[9], m = 0;
    if (!ways_of(g, r, &placed, &uni, &every)) return -1;
    for (unsigned need = every & ~placed; need; need &= need - 1) {
        int d = bench_low(need), at = -1, places = 0;
        for (int i = 0; i < r->len; i++)
            if (g->d[r->cell[i]] >> d & 1) places++, at = r->cell[i];
        if (!places) return -1;
        if (places == 1 && g->d[at] != 1u << d) g->d[at] = (uint16_t)(1u << d), found = 1;
    }
    if (found) return 1;
    for (int i = 0; i < r->len; i++)
        if (!bench_one(g->d[r->cell[i]])) open[m++] = r->cell[i];
    for (unsigned set = 3; set < 1u << m; set++) {
        int size = bench_bits(set);
        if (size < 2 || size > 3) continue;
        unsigned u = 0;
        for (int i = 0; i < m; i++)
            if (set >> i & 1) u |= g->d[open[i]];
        if (bench_bits(u) < size) return -1;
        if (bench_bits(u) == size)
            for (int i = 0; i < m; i++)
                if (!(set >> i & 1)) found |= cut(g, open[i], ~u);
    }
    return found;
}

/* the run worked out whole: the digits each square takes in some filling of the run without repeats (the sets of
 * digits a front part can take, layer by layer, then back from the clue) */
static int run_whole(bench_grid *g, const kkr_run *r)
{
    static uint16_t lay[10][512];
    static uint32_t in[10][512], ok[10][512], stamp;
    int L = r->len, nl[10] = {1}, found = 0;
    unsigned keep[9] = {0};
    stamp++;
    lay[0][0] = 0;
    for (int i = 0; i < L; i++) {
        unsigned can = g->d[r->cell[i]] >> 1;
        nl[i + 1] = 0;
        for (int k = 0; k < nl[i]; k++) {
            unsigned m = lay[i][k];
            for (unsigned c = can & ~m; c; c &= c - 1) {
                unsigned m2 = m | (c & -c);
                if (in[i + 1][m2] == stamp || SUM[m2 << 1] > r->sum) continue;
                in[i + 1][m2] = stamp;
                lay[i + 1][nl[i + 1]++] = (uint16_t)m2;
            }
        }
    }
    for (int k = 0; k < nl[L]; k++)
        if (SUM[lay[L][k] << 1] == r->sum) ok[L][lay[L][k]] = stamp;
    for (int i = L - 1; i >= 0; i--) {
        unsigned can = g->d[r->cell[i]] >> 1;
        for (int k = 0; k < nl[i]; k++) {
            unsigned m = lay[i][k];
            for (unsigned c = can & ~m; c; c &= c - 1)
                if (ok[i + 1][m | (c & -c)] == stamp) {
                    ok[i][m] = stamp;
                    keep[i] |= c & -c;
                }
        }
    }
    if (ok[0][0] != stamp) return -1;
    for (int i = 0; i < L; i++) found |= cut(g, r->cell[i], keep[i] << 1);
    return found;
}

static int deduce(bench_grid *g, int tier, void *cx)
{
    ctx_t *x = cx;
    int found = 0;
    for (int k = 0; k < x->nruns; k++) {
        const kkr_run *r = &x->run[k];
        int res = tier == 0 ? run_easy(g, r) : tier == 1 ? run_medium(g, r) : run_whole(g, r);
        if (res < 0) return -1;
        found |= res;
    }
    return found;
}

static int valid(const bench_grid *g, void *cx)
{
    const ctx_t *x = cx;
    for (int k = 0; k < x->nruns; k++) {
        unsigned used = 0, sum = 0;
        for (int i = 0; i < x->run[k].len; i++) {
            unsigned m = g->d[x->run[k].cell[i]];
            if (used & m) return 0;
            used |= m;
            sum += (unsigned)bench_low(m);
        }
        if (sum != x->run[k].sum) return 0;
    }
    return 1;
}

static void rules_of(bench_rules *r, ctx_t *x, const kkr_puzzle *p, bench_grid *g)
{
    init();
    x->p = p;
    x->nruns = kkr_runs(p, x->run, NULL);
    memset(r, 0, sizeof *r);
    r->vars = p->n * p->n;
    r->tiers = 3;
    r->probe_tiers = 1;
    r->order = BENCH_PAIRS;                     /* the trials the makers were tuned with */
    r->deduce = deduce;
    r->valid = valid;
    r->ctx = x;
    r->budget = 6000;
    for (int c = 0; c < r->vars; c++) g->d[c] = (uint16_t)(p->white[c] ? DIGITS : 1u);
}

/* graded up to `most` (trials: 1 allowed): the level, or BENCH_GUESS / BENCH_NONE; the squares still open */
static int grade(const kkr_puzzle *p, int most, int trials, int *open, uint8_t *which)
{
    static bench_rules r;
    static ctx_t x;
    bench_grid g;
    rules_of(&r, &x, p, &g);
    r.no_trials = !trials;
    int res = bench_grade(&g, &r, most);
    if (open) {
        *open = 0;
        for (int c = 0; c < r.vars; c++)
            if (!bench_one(g.d[c])) which[(*open)++] = (uint8_t)c;
    }
    return res;
}

int kkr_rate(const kkr_puzzle *p) { return grade(p, BENCH_HARD, 1, NULL, NULL); }

static int count_rec(bench_grid *g, bench_rules *r, int limit)
{
    int used = 0;
    if (bench_settle(g, r, 0, &used) < 0) return 0;
    int best = -1, bn = 99;
    for (int c = 0; c < r->vars; c++) {
        int k = bench_bits(g->d[c]);
        if (k > 1 && k < bn) bn = k, best = c;
    }
    if (best < 0) return r->valid(g, r->ctx);
    int found = 0;
    for (unsigned m = g->d[best]; m && found < limit; m &= m - 1) {
        bench_grid t = *g;
        t.d[best] = (uint16_t)(m & -m);
        found += count_rec(&t, r, limit - found);
    }
    return found;
}

int kkr_count(const kkr_puzzle *p, int limit)
{
    static bench_rules r;
    static ctx_t x;
    bench_grid g;
    rules_of(&r, &x, p, &g);
    r.budget = 0;
    return count_rec(&g, &r, limit);
}

/* ---- the maker -------------------------------------------------------------------------------------------------- */
static int run_len(const uint8_t *white, int n, int c, int step)
{
    int len = 1;
    for (int k = c - step; k >= 0 && white[k] && (step == n || k / n == c / n); k -= step) len++;
    for (int k = c + step; k < n * n && white[k] && (step == n || k / n == c / n); k += step) len++;
    return len;
}

static void darken(uint8_t *white, int n, int c)
{
    int x = c % n - 1, y = c / n - 1, m = n - 1;    /* and its partner, half a turn round the middle */
    white[c] = 0;
    white[(m - 1 - y + 1) * n + (m - 1 - x + 1)] = 0;
}

/* the white squares all joined, every run of them two squares or more and `most` at most (the longest found in *top) */
static int sound(const uint8_t *white, int n, int most, int *top)
{
    int total = 0, start = -1;
    *top = 0;
    for (int c = 0; c < n * n; c++) {
        if (!white[c]) continue;
        int h = run_len(white, n, c, 1), v = run_len(white, n, c, n);
        if (h < 2 || v < 2) return 0;
        if (h > *top) *top = h;
        if (v > *top) *top = v;
        total++;
        start = c;
    }
    if (start < 0) return 0;
    int seen[KKR_CELLS] = {0}, stack[KKR_CELLS], sp = 0, reached = 0;
    stack[sp++] = start;
    seen[start] = 1;
    while (sp) {
        int c = stack[--sp];
        reached++;
        const int nb[4] = {c - 1, c + 1, c - n, c + n};
        for (int t = 0; t < 4; t++) {
            int k = nb[t];
            if (k < 0 || k >= n * n || (t < 2 && k / n != c / n) || !white[k] || seen[k]) continue;
            seen[k] = 1;
            stack[sp++] = k;
        }
    }
    (void)most;
    return reached == total;
}

/* the dark squares: added a pair at a time (each the other's turned half round the middle), each kept only when the
 * white squares stay sound, until there are as many as the size wants and no run is too long; 0: none found */
static int pattern(uint32_t *rng, int n, uint8_t *white)
{
    int m = n - 1, most = m < 9 ? m : 9, top;
    if (n >= 10) most = 7;                     /* long runs: hard to make unique */
    for (int c = 0; c < n * n; c++) white[c] = c % n > 0 && c / n > 0;
    int want = m * m * (n <= 6 ? 16 : n <= 8 ? 22 : 26) / 100, dark = 0;
    sound(white, n, most, &top);
    for (int guard = 0; guard < 60 * n && (dark < want || top > most); guard++) {
        int c = (1 + pr_below(rng, m)) * n + 1 + pr_below(rng, m);
        if (!white[c]) continue;
        if (dark >= want) {                     /* enough: only where a run is too long */
            if (run_len(white, n, c, 1) <= most && run_len(white, n, c, n) <= most) continue;
        }
        uint8_t was[KKR_CELLS];
        memcpy(was, white, (size_t)(n * n));
        darken(white, n, c);
        int t;
        if (!sound(white, n, most, &t)) { memcpy(white, was, (size_t)(n * n)); continue; }
        top = t;
        dark = 0;
        for (int k = 0; k < n * n; k++) dark += k % n > 0 && k / n > 0 && !white[k];
    }
    return top <= most;
}

/* digits by chance, no repeat in a run (a search that steps back when a square is left with none) */
static int fill_rec(uint32_t *rng, const kkr_puzzle *p, const uint8_t (*run_of)[2], unsigned *used, uint8_t *sol,
                    int c, int *budget)
{
    int N = p->n * p->n;
    while (c < N && !p->white[c]) c++;
    if (c == N) return 1;
    if (--*budget < 0) return 0;
    int a = run_of[c][0], b = run_of[c][1], order[9];
    for (int i = 0; i < 9; i++) order[i] = i + 1;
    pr_shuffle(rng, order, 9);
    for (int i = 0; i < 9; i++) {
        unsigned bit = 1u << order[i];
        if ((used[a] | used[b]) & bit) continue;
        used[a] |= bit;
        used[b] |= bit;
        sol[c] = (uint8_t)order[i];
        if (fill_rec(rng, p, run_of, used, sol, c + 1, budget)) return 1;
        used[a] &= ~bit;
        used[b] &= ~bit;
    }
    return 0;
}

static void clues(kkr_puzzle *p)
{
    int n = p->n;
    memset(p->across, 0, sizeof p->across);
    memset(p->down, 0, sizeof p->down);
    for (int c = 0; c < n * n; c++) {
        if (!p->white[c]) continue;
        int k = c;
        while (p->white[k - 1]) k--;
        p->across[k - 1] = (uint8_t)(p->across[k - 1] + p->sol[c]);
        k = c;
        while (p->white[k - n]) k -= n;
        p->down[k - n] = (uint8_t)(p->down[k - n] + p->sol[c]);
    }
}

/* a square given another digit that its runs allow, mostly the one whose new clues have the fewest ways (a clue
 * with few ways pins its squares down): 0 when none can */
static int change(uint32_t *rng, kkr_puzzle *p, const uint8_t (*run_of)[2], const kkr_run *runs, int c)
{
    unsigned used = 0;
    int sum[2] = {0, 0}, len[2];
    for (int t = 0; t < 2; t++) {
        const kkr_run *r = &runs[run_of[c][t]];
        len[t] = r->len;
        for (int i = 0; i < r->len; i++) {
            used |= 1u << p->sol[r->cell[i]];
            sum[t] += p->sol[r->cell[i]];
        }
    }
    unsigned free = DIGITS & ~used;
    if (!free) return 0;
    int pick = -1;
    if (pr_below(rng, 4)) {                     /* the fewest ways, mostly */
        int best = 1 << 30, ties = 0;
        for (unsigned f = free; f; f &= f - 1) {
            int d = bench_low(f), s0 = sum[0] - p->sol[c] + d, s1 = sum[1] - p->sol[c] + d;
            int score = NWAYS[len[0]][s0] + NWAYS[len[1]][s1];
            if (score < best) best = score, pick = d, ties = 1;
            else if (score == best && pr_below(rng, ++ties) == 0) pick = d;
        }
    } else {
        int k = pr_below(rng, bench_bits(free));
        while (k--) free &= free - 1;
        pick = bench_low(free);
    }
    p->sol[c] = (uint8_t)pick;
    clues(p);
    return 1;
}

static int make_one(uint32_t *rng, int n, int level, kkr_puzzle *p)
{
    static kkr_run runs[KKR_RUNS];
    static uint8_t run_of[KKR_CELLS][2];
    uint8_t open[KKR_CELLS], open2[KKR_CELLS];
    p->n = n;
    if (!pattern(rng, n, p->white)) return 0;
    kkr_runs(p, runs, run_of);
    unsigned used[KKR_RUNS] = {0};
    int budget = 20000;
    if (!fill_rec(rng, p, (const uint8_t (*)[2])run_of, used, p->sol, 0, &budget)) return 0;
    clues(p);
    /* first the squares where the reasoning (up to the level) is stuck changed, until it is stuck nowhere */
    int nopen, stuck = 0, r = grade(p, level, 0, &nopen, open);
    for (int step = 0; r == BENCH_GUESS && step < 40 * n && stuck < 12 * n; step++) {
        kkr_puzzle was = *p;
        if (!change(rng, p, (const uint8_t (*)[2])run_of, runs, open[pr_below(rng, nopen)])) continue;
        int n2, r2 = grade(p, level, 0, &n2, open2);
        if (r2 != BENCH_GUESS || n2 <= nopen || pr_below(rng, 10) == 0) {
            r = r2;
            nopen = n2;
            memcpy(open, open2, (size_t)n2);
        } else {
            *p = was;
            stuck++;
        }
    }
    if (r == BENCH_GUESS || r < 0) return 0;
    /* then made harder a digit at a time: kept while it stays solved, done when it needs the level's ways */
    int whites[KKR_CELLS], nw = 0;
    for (int c = 0; c < n * n; c++)
        if (p->white[c]) whites[nw++] = c;
    for (int step = 0; r < level && step < 20 * n; step++) {
        kkr_puzzle was = *p;
        if (!change(rng, p, (const uint8_t (*)[2])run_of, runs, whites[pr_below(rng, nw)])) continue;
        int r2 = grade(p, level - 1, 0, NULL, NULL);   /* the easier ways first: quicker */
        if (r2 == BENCH_GUESS) r2 = grade(p, level, 0, NULL, NULL);
        if (r2 == BENCH_GUESS || r2 < 0) *p = was;
        else r = r2;
    }
    return r == level;
}

void kkr_make(int n, int level, uint32_t seed, kkr_puzzle *p)
{
    uint32_t rng = pr_seed(seed);
    if (n < 4 || n > KKR_MAX) n = 6;
    if (level < 0 || level >= KKR_LEVELS) level = KKR_EASY;
    for (int tries = 0;; tries++) {
        if (make_one(&rng, n, level, p)) return;
        if (tries > 200 && level > KKR_EASY) level--;   /* never stuck: one level easier at the very worst */
    }
}

/* ---- the player's grid ------------------------------------------------------------------------------------------ */
void kkr_new(kkr_game *g, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    if (variant < 0 || variant >= KKR_VARIANTS) variant = 0;
    g->variant = variant;
    g->level = kkr_level_of(variant);
    g->seed = seed;
    kkr_make(kkr_size_of(variant), g->level, seed, &g->p);
    g->nruns = kkr_runs(&g->p, g->run, g->run_of);
    bench_clear(&g->hist);
}

static void restore(void *cx, int at, uint32_t was)
{
    kkr_game *g = cx;
    g->val[at] = (uint8_t)(was >> 16);
    g->notes[at] = (uint16_t)was;
}

/* the wrong entries on the board (the stars' mistakes) */
static int wrongs(const kkr_game *g)
{
    int k = 0;
    for (int c = 0; c < g->p.n * g->p.n; c++)
        k += g->p.white[c] && g->val[c] && g->val[c] != g->p.sol[c];
    return k;
}

static void set(kkr_game *g, int c, int val, unsigned notes)
{
    if (g->val[c] == val && g->notes[c] == notes) return;
    bench_log(&g->hist, c, (uint32_t)g->val[c] << 16 | g->notes[c]);
    g->val[c] = (uint8_t)val;
    g->notes[c] = (uint16_t)notes;
}

static int cells(const kkr_game *g) { return g->p.n * g->p.n; }

int kkr_put(kkr_game *g, int c, int d)
{
    if (c < 0 || c >= cells(g) || !g->p.white[c] || d < 0 || d > 9) return 0;
    if (d && g->val[c] == d) return 0;
    if (!d && !g->val[c]) return 0;
    bench_begin(&g->hist, 19);
    set(g, c, d, d ? 0 : g->notes[c]);
    if (d)
        for (int t = 0; t < 2; t++) {           /* the note d goes from its runs */
            const kkr_run *r = &g->run[g->run_of[c][t]];
            for (int i = 0; i < r->len; i++) {
                int k = r->cell[i];
                if (g->notes[k] >> d & 1) set(g, k, g->val[k], g->notes[k] & ~(1u << d));
            }
        }
    return bench_done(&g->hist, wrongs(g));
}

int kkr_note(kkr_game *g, int c, int d)
{
    if (c < 0 || c >= cells(g) || !g->p.white[c] || d < 1 || d > 9 || g->val[c]) return 0;
    bench_begin(&g->hist, 1);
    set(g, c, 0, g->notes[c] ^ (1u << d));
    return bench_done(&g->hist, wrongs(g));
}

int kkr_erase(kkr_game *g, int c)
{
    if (c < 0 || c >= cells(g) || !g->p.white[c] || (!g->val[c] && !g->notes[c])) return 0;
    bench_begin(&g->hist, 1);
    set(g, c, 0, g->val[c] ? g->notes[c] : 0);
    return bench_done(&g->hist, wrongs(g));
}

static int hint_step(kkr_game *g)
{
    int N = cells(g);
    for (int c = 0; c < N; c++)
        if (g->p.white[c] && g->val[c] && g->val[c] != g->p.sol[c]) return kkr_put(g, c, g->p.sol[c]) ? c : -1;
    static bench_rules r;
    static ctx_t x;
    bench_grid s;
    rules_of(&r, &x, &g->p, &s);
    for (int c = 0; c < N; c++)
        if (g->val[c]) s.d[c] = (uint16_t)(1u << g->val[c]);
    int used = 0;
    bench_settle(&s, &r, 1, &used);
    int best = -1, bn = 99;
    for (int c = 0; c < N; c++) {
        if (!g->p.white[c] || g->val[c]) continue;
        int b = bench_bits(s.d[c]);
        if (b < bn) bn = b, best = c;
    }
    if (best < 0) return -1;
    return kkr_put(g, best, g->p.sol[best]) ? best : -1;
}

int kkr_hint(kkr_game *g)
{
    return bench_hinted(&g->hist, hint_step(g));
}

int kkr_undo(kkr_game *g)
{
    int c = bench_undo(&g->hist, restore, g);
    bench_undone(&g->hist, wrongs(g));
    return c;
}


int kkr_clash(const kkr_game *g, int c)
{
    if (!g->p.white[c] || !g->val[c]) return 0;
    for (int t = 0; t < 2; t++) {
        const kkr_run *r = &g->run[g->run_of[c][t]];
        int sum = 0, full = 1;
        for (int i = 0; i < r->len; i++) {
            int k = r->cell[i];
            if (k != c && g->val[k] == g->val[c]) return 1;
            if (!g->val[k]) full = 0;
            sum += g->val[k];
        }
        if ((full && sum != r->sum) || sum > r->sum) return 1;
    }
    return 0;
}

int kkr_solved(const kkr_game *g)
{
    for (int c = 0; c < cells(g); c++)
        if (g->p.white[c] && g->val[c] != g->p.sol[c]) return 0;
    return 1;
}

int kkr_script(kkr_game *g, const char *s)
{
    int n = g->p.n;
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == 'h') {
            if (kkr_hint(g) < 0) return 0;
            s++;
            continue;
        }
        if (*s == '+') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int c = 0; c < cells(g) && k > 0; c++)
                if (g->p.white[c] && g->val[c] != g->p.sol[c]) { kkr_put(g, c, g->p.sol[c]); k--; }
            continue;
        }
        int len, c = bench_cell(s, n, n, &len);
        if (c < 0 || (s[len] != '=' && s[len] != '\'') || s[len + 1] < '0' || s[len + 1] > '9') return 0;
        int ok = s[len] == '=' ? kkr_put(g, c, s[len + 1] - '0') : kkr_note(g, c, s[len + 1] - '0');
        if (!ok) return 0;
        s += len + 2;
    }
    return 1;
}
