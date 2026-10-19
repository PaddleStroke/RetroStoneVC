/*
 * Balance Scales' levels: src/games/balance_levels.h. Our own puzzles, from the game's own maker (weights and scales
 * dealt by chance, kept when the scales pin every weight down and the reasoning finds them with the steps the make
 * allows; balance_rules.c). Each candidate's work: the items on its scales, its weights' size, and its pair steps
 * (two scales compared), which weigh most; near-duplicates (the same scales) dropped. Each chapter has three makes
 * (shapes, scales, the heaviest weight, shapes on both pans or not, a pair step needed or not, items on a pan): its
 * opening (two single shapes on one scale first, then two of the chapter's new idea; an opening of more shapes has two
 * on a scale, one found from another), then two makes of more shapes, their levels picked evenly over the harder part
 * of a make's candidates ranked by work; then the chapter put in order. One step of two shapes at level 3, then three
 * shapes (not trivial from level 4), shapes on both pans (up to five shapes), scales to compare, four and five shapes.
 * The per-level table and the chapters' summary go to stderr (-DREPORT: the same for the levels in balance_levels.h).
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/balgen apps/boardgames/tools/balance_gen.c \
 *       apps/boardgames/src/games/balance_rules.c -lm && /tmp/balgen > apps/boardgames/src/games/balance_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/balance.h"

#define CANDS 120

typedef struct {
    bal_puzzle p;
    int plain, pair, items;
    double score;
    char key[256];
} cand;

/* a make: the maker's settings, how many levels, and from where in its candidates ranked by work they are picked evenly
 * (lo: the share skipped, the easiest; -1: the easiest themselves, an opening) */
typedef struct { bal_make m; int count; float lo; } make;
#define MAKES 3
/* {k, m, wmax, both, pair, pan} */
static const make CHAPTER[BAL_CHAPTERS][MAKES] = {
    {{{1, 1, 5, 0, 0, 3}, 2, 0}, {{2, 2, 9, 0, 0, 4}, 1, -1}, {{3, 3, 9, 0, 0, 4}, 22, 0.1f}},
    {{{2, 2, 9, 1, 0, 4}, 2, -1}, {{3, 3, 9, 1, 0, 4}, 11, 0.3f}, {{4, 4, 9, 1, 0, 4}, 12, 0.2f}},
    {{{3, 3, 12, 1, 0, 5}, 2, -1}, {{4, 4, 15, 1, 0, 5}, 11, 0.3f}, {{5, 5, 15, 1, 0, 5}, 12, 0.2f}},
    {{{2, 2, 9, 0, 1, 4}, 2, -1}, {{3, 3, 12, 0, 1, 4}, 11, 0.3f}, {{3, 3, 15, 1, 1, 4}, 12, 0.3f}},
    {{{3, 3, 15, 1, 1, 4}, 2, -1}, {{4, 4, 15, 1, 1, 5}, 11, 0.3f}, {{4, 4, 20, 1, 1, 5}, 12, 0.4f}},
    {{{4, 4, 15, 1, 1, 5}, 2, -1}, {{5, 5, 20, 1, 1, 5}, 11, 0.3f}, {{5, 5, 20, 1, 1, 6}, 12, 0.5f}},
};

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS], chosen[BAL_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* ---- the difficulty metrics: the shapes, the scales' items, the heaviest weight, the plain and pair steps. Trivial:
 * two shapes or fewer, found one scale at a time (no pair step). --------------------------------------------------- */
static int trivial(const cand *c) { return c->p.k <= 2 && !c->pair; }

/* a scale holding two kinds of shape: one found from another (what an opening of more than one shape asks) */
static int mixed(const bal_puzzle *p)
{
    for (int i = 0; i < p->m; i++) {
        unsigned kinds = 0;
        for (int s = 0; s < 2; s++)
            for (int j = 0; j < p->sc[i].n[s]; j++) kinds |= 1u << p->sc[i].s[s][j];
        if (kinds & (kinds - 1)) return 1;
    }
    return 0;
}

static void table(const cand *c, int n)
{
    fprintf(stderr, "lvl shapes items most plain pair score\n");
    for (int i = 0; i < n; i++) {
        int most = 0;
        for (int s = 0; s < c[i].p.k; s++) most = c[i].p.wt[s] > most ? c[i].p.wt[s] : most;
        fprintf(stderr, "%4d %6d %5d %4d %5d %4d %5.1f\n", i + 1, c[i].p.k, c[i].items, most, c[i].plain, c[i].pair,
                c[i].score);
    }
    fprintf(stderr, "chapter: shapes mean (min..max), items mean, pair steps mean (levels with one); trivial (two "
                    "shapes or fewer, no pair step)\n");
    for (int ch = 0; ch * 25 < n; ch++) {
        double k = 0, it = 0, pr = 0;
        int lo = 99, hi = 0, wp = 0, t = 0, cnt = 0;
        for (int i = ch * 25; i < n && i < ch * 25 + 25; i++, cnt++) {
            k += c[i].p.k, it += c[i].items, pr += c[i].pair, wp += c[i].pair > 0, t += trivial(&c[i]);
            lo = c[i].p.k < lo ? c[i].p.k : lo;
            hi = c[i].p.k > hi ? c[i].p.k : hi;
        }
        fprintf(stderr, "  %d: shapes %.1f (%d..%d)  items %.1f  pair %.2f (%d)  trivial %d/%d\n", ch + 1, k / cnt, lo,
                hi, it / cnt, pr / cnt, wp, t, cnt);
    }
}

static void measure(cand *c)
{
    c->items = 0;
    for (int i = 0; i < c->p.m; i++) c->items += c->p.sc[i].n[0] + c->p.sc[i].n[1];
    int most = 0;
    for (int s = 0; s < c->p.k; s++) most = c->p.wt[s] > most ? c->p.wt[s] : most;
    c->score = c->items + 0.3 * most + 4.0 * c->pair;
}

#ifdef REPORT                                   /* -DREPORT: the metrics of the levels in balance_levels.h */
#include "games/balance_levels.h"
static int report(void)
{
    static cand lv[BAL_LEVELS];
    for (int i = 0; i < BAL_LEVELS; i++) {
        bal_decode(bal_levels[i], &lv[i].p);
        bal_reason(&lv[i].p, &lv[i].plain, &lv[i].pair);
        measure(&lv[i]);
    }
    table(lv, BAL_LEVELS);
    return 0;
}
#endif

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 40 * CANDS; seed++) {
        cand *c = &pool[k];
        bal_make_puzzle(&m->m, seed, &c->p);
        if (c->p.k != m->m.k || !bal_reason(&c->p, &c->plain, &c->pair) || bal_rank(&c->p) != c->p.k) continue;
        if (m->m.pair ? c->pair < 1 : c->pair > 0) continue;
        measure(c);
        bal_encode(&c->p, c->key, sizeof c->key);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (!dup) k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

int main(void)
{
#ifdef REPORT
    return report();
#endif
    for (int ch = 0; ch < BAL_CHAPTERS; ch++) {
        int first = nchosen;
        for (int p = 0; p < MAKES; p++) {
            const make *m = &CHAPTER[ch][p];
            int k = make_pool(m, 1000u * (uint32_t)(ch * MAKES + p + 1));
            fprintf(stderr, "chapter %d, make %d: %d candidates\n", ch + 1, p + 1, k);
            for (int i = 0; i < m->count; i++) {
                int at = m->lo < 0 ? i : (int)((m->lo + (1 - m->lo) * (i + 0.5f) / m->count) * (k - 1));
                while (at < k && (seen(pool[at].key) || (m->lo < 0 && m->m.k > 1 && !mixed(&pool[at].p)))) at++;
                if (at >= k) {
                    fprintf(stderr, "chapter %d: not enough of make %d\n", ch + 1, p + 1);
                    return 1;
                }
                chosen[nchosen++] = pool[at];
            }
        }
        for (int i = first + 1; i < nchosen; i++)   /* the chapter from its easiest to its hardest */
            for (int j = i; j > first; j--) {
                cand *a = &chosen[j - 1], *b = &chosen[j];
                if (a->p.k * 1e3 + a->score <= b->p.k * 1e3 + b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
    table(chosen, nchosen);
    printf("/*\n"
           " * Balance Scales' levels: our own, made by tools/balance_gen.c (the game's maker; each scale balanced, the\n"
           " * weights pinned down and found by reasoning), six chapters of 25 from the easiest. Each: the weights, ';',\n"
           " * then the scales \"left/right\" separated by '|', a pan's items a shape a.. or a number (balance.h,\n"
           " * bal_decode); in the comment the plain and pair steps it takes.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "static const char *const bal_levels[BAL_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++)
        printf("    \"%s\", /* %d: %d plain, %d pair */\n", chosen[i].key, i + 1, chosen[i].plain, chosen[i].pair);
    printf("};\n");
    return 0;
}
