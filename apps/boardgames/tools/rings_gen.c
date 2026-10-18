/*
 * Ring Puzzle's levels: src/games/rings_levels.h. Our own levels: no published or commercial set is used. Each
 * chapter has its rings and colours (two small rings of ivory and ebony; a third colour on their far arcs; larger
 * rings, four colours; three rings in a row; three in a triangle, one bead shared by all three; all of them, scrambled
 * deeper) and makes many scrambles from fixed seeds: a target of the chapter (each ring its colour, rg_pattern), random
 * turns from it, the par searched (the fewest moves, rg_solve). Each scramble is measured: its par, the share of
 * greedy players (the move putting most beads right, ties by chance) that solve it, the first moves that start a
 * shortest way, and the fewest moves on a shortest way that put no more beads right (a step back to go forward). A
 * scramble is kept when its par is in the chapter's range, when few greedy players solve it and its shortest ways
 * need the chapter's steps back, when its shortest way turns more than one ring (from a par of three: not one ring
 * turned back), and when it was not met before. Only the opening levels are easy: the first chapter's pars 1, 2, 3,
 * the other chapters' two of their new rings. The rest spread over the chapter's difficulties (no two of the same
 * rings in a row when it can help), its last level, the showpiece, one of the hardest. The per-level table and the
 * chapters' summary go to stderr (-DREPORT: the same for the levels in rings_levels.h). Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/rggen apps/boardgames/tools/rings_gen.c -lm && \
 *       /tmp/rggen > apps/boardgames/src/games/rings_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define RG_GEN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/rings_rules.c"

#define PER (RG_LEVELS / RG_CHAPTERS)
#define POOL 400

typedef struct {
    int kind, n, colours, par, rings_used, pattern;
    char target[RG_PLACES + 1], start[RG_PLACES + 1], way[RG_DEEP + 1];
    double key, greedy;
    int good, dips;
} cand;

/* a chapter: its kinds of rings (kind, n, pattern), the pars wanted (lo..hi); past its opening levels, the share of
 * greedy players that may solve a level at most, and the fewest moves against the beads (dips) its shortest ways need */
typedef struct {
    int kinds[4][3], nk, lo, hi;
    double gmax;
    int dmin;
} chapter;

static const chapter CH[RG_CHAPTERS] = {
    {{{RG_TWO, 8, 0}}, 1, 3, 7, 0.4, 1},
    {{{RG_TWO, 8, 1}, {RG_TWO, 12, 0}}, 2, 4, 8, 0.3, 1},
    {{{RG_TWO, 8, 2}, {RG_TWO, 12, 1}, {RG_TWO, 12, 2}}, 3, 5, 9, 0.25, 2},
    {{{RG_CHAIN, 8, 0}, {RG_CHAIN, 8, 1}}, 2, 5, 10, 0.2, 2},
    {{{RG_TRIANGLE, 12, 0}, {RG_TRIANGLE, 12, 1}}, 2, 6, 11, 0.15, 2},
    {{{RG_TWO, 12, 2}, {RG_CHAIN, 8, 1}, {RG_TRIANGLE, 12, 1}, {RG_TRIANGLE, 12, 0}}, 4, 8, 13, 0.1, 3},
};

/* ---- the difficulty metrics ------------------------------------------------------------------------------------ */
typedef struct {
    double greedy;                              /* the share of greedy players (the move putting most beads right,
                                                 * ties by chance) that solve it */
    int good;                                   /* the first moves that start a shortest way */
    int dips;                                   /* the fewest moves on a shortest way that put no more beads right */
} metrics;

static int right(const rg_map *m, rg_pos p)
{
    int k = 0;
    for (int i = 0; i < m->places; i++) k += rg_colour(p, i) == rg_colour(m->target, i);
    return k;
}

#define GREEDY 16
static double greedy_share(const rg_map *m, rg_pos p0, int par, uint32_t seed)
{
    uint32_t r = pr_seed(seed);
    int won = 0;
    for (int k = 0; k < GREEDY; k++) {
        rg_pos p = p0;
        int last = -1;
        for (int step = 0; step < 2 * par + 6 && p != m->target; step++) {
            int best = -1, nb = 0, cand_[2 * RG_RINGS];
            for (int mv = 0; mv < rg_moves(m); mv++) {
                if (last >= 0 && mv == rg_inverse(last)) continue;
                int v = right(m, rg_apply(m, p, mv));
                if (v > best) best = v, nb = 0;
                if (v == best) cand_[nb++] = mv;
            }
            last = cand_[pr_below(&r, nb)];
            p = rg_apply(m, p, last);
        }
        won += p == m->target;
    }
    return won / (double)GREEDY;
}

#define MEMO (1 << 14)
static struct { rg_pos p; int dist, dips; } memo[MEMO];
static int calls;

static int dist(const rg_map *m, rg_pos p)
{
    unsigned h = (unsigned)((p * 0x9E3779B97F4A7C15ull) >> 50) & (MEMO - 1);
    for (int k = 0; k < MEMO; k++, h = (h + 1) & (MEMO - 1)) {
        if (memo[h].dist < 0) { memo[h].p = p, memo[h].dips = -1; return memo[h].dist = rg_solve(m, p, NULL); }
        if (memo[h].p == p) return memo[h].dist;
    }
    return rg_solve(m, p, NULL);
}

static int fewest_dips(const rg_map *m, rg_pos p, int d)
{
    if (d == 0) return 0;
    if (++calls > 3000) return 0;
    int best = 99, rp = right(m, p);
    for (int mv = 0; mv < rg_moves(m); mv++) {
        rg_pos q = rg_apply(m, p, mv);
        if (dist(m, q) != d - 1) continue;
        int c = (right(m, q) <= rp) + fewest_dips(m, q, d - 1);
        if (c < best) best = c;
    }
    return best;
}

static void measure(const rg_map *m, rg_pos p, int par, uint32_t seed, metrics *o)
{
    o->greedy = greedy_share(m, p, par, seed);
    o->good = 0;
    for (int mv = 0; mv < rg_moves(m); mv++) o->good += rg_solve(m, rg_apply(m, p, mv), NULL) == par - 1;
    calls = 0;
    for (int i = 0; i < MEMO; i++) memo[i].dist = -1;
    o->dips = fewest_dips(m, p, par);
}

static double difficulty(int par, const metrics *x, int moves)
{
    return par + 1.5 * x->dips + 3 * (1 - x->greedy) + 1.0 * (moves - x->good) / moves;
}

static void table(const int *par, const metrics *x, const double *key, int n, int per)
{
    fprintf(stderr, "lvl par greedy good dips  diff\n");
    for (int l = 0; l < n; l++)
        fprintf(stderr, "%4d %3d  %5.2f %4d %4d %5.2f\n", l + 1, par[l], x[l].greedy, x[l].good, x[l].dips, key[l]);
    fprintf(stderr, "chapter: par mean (min..max), greedy share mean, dips mean; trivial (greedy >= 0.5 or par <= 2)\n");
    for (int ch = 0; ch * per < n; ch++) {
        double p = 0, g = 0, d = 0;
        int t = 0, k = 0, lo = 99, hi = 0;
        for (int l = ch * per; l < n && l < (ch + 1) * per; l++, k++) {
            p += par[l], g += x[l].greedy, d += x[l].dips;
            t += x[l].greedy >= 0.5 || par[l] <= 2;
            if (par[l] < lo) lo = par[l];
            if (par[l] > hi) hi = par[l];
        }
        fprintf(stderr, "  %d: par %5.1f (%d..%d)  greedy %.2f  dips %.2f  trivial %d/%d\n", ch + 1, p / k, lo, hi,
                g / k, d / k, t, k);
    }
}

#ifdef REPORT                                   /* -DREPORT: the metrics of the levels in rings_levels.h */
#include "games/rings_levels.h"
static int report(void)
{
    static rg_game g;
    static int par[RG_LEVELS];
    static metrics x[RG_LEVELS];
    static double key[RG_LEVELS];
    for (int l = 0; l < RG_LEVELS; l++) {
        const rg_level_def *d = &rg_levels[l];
        rg_pos t, s;
        rg_build(&g.m, d->kind, d->n);
        rg_parse(&g.m, d->target, &t);
        rg_parse(&g.m, d->start, &s);
        g.m.target = t;
        g.m.colours = d->colours;
        rg_begin(&g, s);
        g.par = d->par;
        par[l] = g.par;
        measure(&g.m, g.p, g.par, 1u + (uint32_t)l, &x[l]);
        key[l] = difficulty(g.par, &x[l], rg_moves(&g.m));
    }
    table(par, x, key, RG_LEVELS, PER);
    return 0;
}
#endif

static int by_key(const void *a, const void *b)
{
    double d = ((const cand *)a)->key - ((const cand *)b)->key;
    return d < 0 ? -1 : d > 0 ? 1 : strcmp(((const cand *)a)->start, ((const cand *)b)->start);
}

/* one scramble of a chapter's kind: its par, its shortest way, its metrics (0: not usable) */
static int scramble(const chapter *C, uint32_t *r, int lo, int hi, double gmax, cand *c)
{
    const int *k = C->kinds[pr_below(r, C->nk)];
    rg_map m;
    rg_build(&m, k[0], k[1]);
    rg_pattern(&m, k[2], 0);
    int want = lo + pr_below(r, hi - lo + 1), steps = want + pr_below(r, 4), last = -1;
    rg_pos p = m.target;
    for (int i = 0; i < steps; i++) {
        int mv;
        do mv = pr_below(r, rg_moves(&m));
        while (last >= 0 && mv == rg_inverse(last));
        p = rg_apply(&m, p, mv);
        last = mv;
    }
    int par = rg_solve(&m, p, NULL);
    if (par < lo || par > hi) return 0;
    uint32_t gs = pr_next(r);
    if (greedy_share(&m, p, par, gs) > gmax) return 0;   /* (cheap: first) */
    rg_pos q = p;                               /* its shortest way, the rings it turns */
    int used = 0;
    for (int i = 0; i < par; i++) {
        int f;
        if (rg_solve(&m, q, &f) != par - i || f < 0) return 0;
        c->way[i] = (char)('a' + f);
        used |= 1 << (f >> 1);
        q = rg_apply(&m, q, f);
    }
    if (q != m.target) return 0;
    c->way[par] = 0;
    int rings = __builtin_popcount((unsigned)used);
    if (par >= 3 && rings < 2) return 0;
    rg_text(&m, m.target, c->target);
    rg_text(&m, p, c->start);
    c->kind = k[0], c->n = k[1], c->pattern = k[2], c->colours = m.colours, c->par = par, c->rings_used = rings;
    metrics x;
    measure(&m, p, par, gs, &x);
    c->greedy = x.greedy, c->good = x.good, c->dips = x.dips;
    c->key = difficulty(par, &x, rg_moves(&m)) + 0.1 * rings + 0.001 * pr_below(r, 100);
    return 1;
}

static int seen(const cand *pool, int n, const cand *c)
{
    for (int i = 0; i < n; i++)
        if (pool[i].kind == c->kind && pool[i].n == c->n && !strcmp(pool[i].start, c->start) &&
            !strcmp(pool[i].target, c->target))
            return 1;
    return 0;
}

int main(void)
{
#ifdef REPORT
    return report();
#endif
    static cand pool[POOL], intro[POOL], out[RG_LEVELS];
    static const char *const KIND[] = {"RG_TWO", "RG_CHAIN", "RG_TRIANGLE"};
    printf("/* Ring Puzzle's levels: made by tools/rings_gen.c (our own scrambles, each par found by the search; do not\n"
           " * edit). Each: the rings, their beads, the colours, the par, the target's and the start's colours (a digit a\n"
           " * place), a shortest way (a letter a move) (rings.h). */\n");
    for (int ch = 0; ch < RG_CHAPTERS; ch++) {
        const chapter *C = &CH[ch];
        uint32_t r = pr_seed(5300u + (uint32_t)ch);
        /* the opening levels: the first chapter's pars 1, 2, 3 (one ring, then both); the others' two of their new
         * kind (its first kind), a little shorter than the rest */
        int nopen = ch == 0 ? 3 : 2, ni = 0;
        for (int t = 0; t < 20000 && ni < nopen; t++) {
            cand *c = &intro[ni];
            chapter one = *C;
            one.nk = 1;
            int p = ch == 0 ? ni + 1 : C->lo - 1;
            if (!scramble(&one, &r, p, p, 1.0, c) || seen(intro, ni, c)) continue;
            if (ch == 0 && ni == 2 && c->greedy < 0.5) continue;   /* (level 3: still obvious) */
            ni++;
        }
        int n = 0;
        for (int t = 0; t < 400 * POOL && n < POOL; t++) {
            cand *c = &pool[n];
            if (!scramble(C, &r, C->lo, C->hi, C->gmax, c) || c->greedy > C->gmax || c->dips < C->dmin) continue;
            if (seen(pool, n, c) || seen(intro, ni, c)) continue;
            n++;
        }
        if (n < PER || ni < nopen) { fprintf(stderr, "chapter %d: only %d, %d opening\n", ch + 1, n, ni); return 1; }
        qsort(pool, (size_t)n, sizeof *pool, by_key);
        int base = ch * PER, at = 0;
        for (int i = 0; i < ni; i++) out[base + at++] = intro[i];
        static char used[POOL];
        memset(used, 0, sizeof used);
        int rest = PER - at - 1;                /* spread over the difficulties; the showpiece last */
        double step = (double)(n - 6) / (rest - 1);
        for (int k = 0; k < rest; k++) {
            int i = (int)(k * step + 0.5);
            while (used[i]) i++;
            for (int j = i; j < n && j < i + 6; j++)   /* not the rings of the level before, when a near one differs */
                if (!used[j] && (pool[j].kind != out[base + at - 1].kind || pool[j].n != out[base + at - 1].n ||
                                 pool[j].pattern != out[base + at - 1].pattern)) { i = j; break; }
            used[i] = 1;
            out[base + at++] = pool[i];
        }
        int show = -1;                          /* the showpiece: of the hardest few, the most colours */
        for (int i = n - 1; i >= n - 5 && i >= 0; i--)
            if (!used[i] && (show < 0 || pool[i].colours * 10 + pool[i].rings_used >
                                             pool[show].colours * 10 + pool[show].rings_used))
                show = i;
        out[base + at++] = pool[show];
        fprintf(stderr, "chapter %d: %d scrambles\n", ch + 1, n);
    }
    {
        static int par[RG_LEVELS];
        static metrics x[RG_LEVELS];
        static double key[RG_LEVELS];
        for (int l = 0; l < RG_LEVELS; l++)
            par[l] = out[l].par, x[l] = (metrics){out[l].greedy, out[l].good, out[l].dips}, key[l] = out[l].key;
        table(par, x, key, RG_LEVELS, PER);
    }
    printf("const rg_level_def rg_levels[RG_LEVELS] = {\n");
    for (int l = 0; l < RG_LEVELS; l++) {
        const cand *c = &out[l];
        printf("    {%s, %d, %d, %d, \"%s\", \"%s\", \"%s\"}, /* %d */\n", KIND[c->kind], c->n, c->colours, c->par,
               c->target, c->start, c->way, l + 1);
    }
    printf("};\n");
    return 0;
}
