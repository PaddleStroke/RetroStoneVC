/*
 * Magic Squares' levels: src/games/magicsquare_levels.h. Our own puzzles, from the game's own maker (a magic filling by
 * chance, numbers taken away while the solution stays the only one; magicsquare_rules.c). Each candidate's work: its
 * gaps, the search a full count of its solutions takes, and the plain rounds (waves of "a line with one number
 * missing"; one round, or two gaps or fewer: trivial); near-duplicates (the same numbers and gaps) dropped. Each
 * chapter has three makes (the figure, the gaps at most, the numbers): its opening (three small triangles of 1..6
 * first, then two of the chapter's easiest), then two makes whose levels are picked evenly over the harder part of
 * their candidates ranked by work, no trivial one; then the chapter put in order: the small and the middle triangle,
 * the 3 x 3 of 1..9, the middle and big triangles, 3 x 3 squares of any numbers, the 4 x 4, and last the big triangle
 * and 4 x 4s of larger numbers. The per-level table and the chapters' summary go to stderr (-DREPORT: the same for
 * the levels in magicsquare_levels.h).
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/msqgen apps/boardgames/tools/magicsquare_gen.c \
 *       apps/boardgames/src/games/magicsquare_rules.c && /tmp/msqgen > apps/boardgames/src/games/magicsquare_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/magicsquare.h"

#define CANDS 100

typedef struct {
    msq_puzzle p;
    int gaps, rounds;
    long nodes;
    double score;
    char key[160];
} cand;

/* a make: the maker's settings, how many levels, and from where in its candidates ranked by work they are picked evenly
 * (lo: the share skipped, the easiest; -1: the easiest themselves, an opening; past an opening, no trivial one) */
typedef struct { msq_make m; int count; float lo; } make;
#define MAKES 3
static const make CHAPTER[MSQ_CHAPTERS][MAKES] = {
    {{{MSQ_TRI3, 2, 0}, 3, -1}, {{MSQ_TRI3, 4, 0}, 7, 0.3f}, {{MSQ_TRI4, 5, 0}, 15, 0.2f}},
    {{{MSQ_SQ3, 3, 0}, 2, -1}, {{MSQ_SQ3, 5, 1}, 11, 0.3f}, {{MSQ_SQ3, 6, 1}, 12, 0.3f}},
    {{{MSQ_TRI4, 5, 1}, 2, -1}, {{MSQ_TRI4, 7, 1}, 11, 0.3f}, {{MSQ_TRI5, 7, 0}, 12, 0.3f}},
    {{{MSQ_SQ3, 5, 2}, 2, -1}, {{MSQ_SQ3, 6, 2}, 11, 0.3f}, {{MSQ_SQ3, 7, 2}, 12, 0.4f}},
    {{{MSQ_SQ4, 6, 0}, 2, -1}, {{MSQ_SQ4, 8, 0}, 11, 0.3f}, {{MSQ_SQ4, 10, 0}, 12, 0.3f}},
    {{{MSQ_TRI5, 8, 1}, 2, -1}, {{MSQ_TRI5, 10, 1}, 11, 0.3f}, {{MSQ_SQ4, 11, 1}, 12, 0.4f}},
};

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS], chosen[MSQ_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* ---- the difficulty metrics ------------------------------------------------------------------------------------- */
/* the plain way: each round, every line with one number missing gets it. The rounds it takes (1: each gap alone on a
 * line from the start), -1 when it stalls (the tray and the crossing lines must be weighed) */
static int plain_rounds(const msq_puzzle *p)
{
    uint8_t known[MSQ_PLACES];
    int left = 0, rounds = 0;
    for (int i = 0; i < p->np; i++) known[i] = p->given[i], left += !p->given[i];
    while (left) {
        int fill[MSQ_PLACES], nf = 0;
        for (int l = 0; l < p->nl; l++) {
            int miss = 0, at = -1;
            for (int k = 0; k < p->len[l]; k++)
                if (!known[p->line[l][k]]) miss++, at = p->line[l][k];
            if (miss == 1) {
                int dup = 0;
                for (int i = 0; i < nf; i++) dup |= fill[i] == at;
                if (!dup) fill[nf++] = at;
            }
        }
        if (!nf) return -1;
        for (int i = 0; i < nf; i++) known[fill[i]] = 1;
        left -= nf;
        rounds++;
    }
    return rounds;
}

/* trivial: two gaps or fewer, or every gap alone on a line from the start */
static int trivial(const cand *c) { return c->gaps <= 2 || c->rounds == 1; }

static void measure(cand *c)
{
    c->gaps = 0;
    for (int i = 0; i < c->p.np; i++) c->gaps += !c->p.given[i];
    c->rounds = plain_rounds(&c->p);
    c->score = c->gaps + 0.5 * log((double)c->nodes + 1) + 1.5 * (c->rounds < 0 ? 3 : c->rounds - 1);
}

static void table(const cand *c, int n)
{
    static const char FIG[] = "abcsq";
    fprintf(stderr, "lvl fig places gaps rounds nodes score\n");
    for (int i = 0; i < n; i++)
        fprintf(stderr, "%4d   %c %6d %4d %6d %5ld %5.1f\n", i + 1, FIG[c[i].p.kind], c[i].p.np, c[i].gaps, c[i].rounds,
                c[i].nodes, c[i].score);
    fprintf(stderr, "chapter: places mean, gaps mean (min..max), levels the plain way stalls on, search nodes mean; "
                    "trivial (two gaps or fewer, or each gap alone on a line)\n");
    for (int ch = 0; ch * 25 < n; ch++) {
        double pl = 0, g = 0, nd = 0;
        int lo = 99, hi = 0, st = 0, t = 0, k = 0;
        for (int i = ch * 25; i < n && i < ch * 25 + 25; i++, k++) {
            pl += c[i].p.np, g += c[i].gaps, nd += c[i].nodes, st += c[i].rounds < 0, t += trivial(&c[i]);
            lo = c[i].gaps < lo ? c[i].gaps : lo;
            hi = c[i].gaps > hi ? c[i].gaps : hi;
        }
        fprintf(stderr, "  %d: places %.1f  gaps %.1f (%d..%d)  stalls %2d  nodes %.0f  trivial %d/%d\n", ch + 1, pl / k,
                g / k, lo, hi, st, nd / k, t, k);
    }
}

#ifdef REPORT                                   /* -DREPORT: the metrics of the levels in magicsquare_levels.h */
#include "games/magicsquare_levels.h"
static int report(void)
{
    static cand lv[MSQ_LEVELS];
    for (int i = 0; i < MSQ_LEVELS; i++) {
        msq_decode(msq_levels[i], &lv[i].p);
        msq_count(&lv[i].p, 2, &lv[i].nodes);
        measure(&lv[i]);
    }
    table(lv, MSQ_LEVELS);
    return 0;
}
#endif

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 30 * CANDS; seed++) {
        cand *c = &pool[k];
        msq_make_puzzle(&m->m, seed, &c->p);
        if (c->p.kind != m->m.kind || msq_count(&c->p, 2, &c->nodes) != 1) continue;
        measure(c);
        if (c->gaps < (m->m.blanks + 1) / 2 || (m->lo >= 0 && trivial(c))) continue;
        msq_encode(&c->p, c->key, sizeof c->key);
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
    for (int ch = 0; ch < MSQ_CHAPTERS; ch++) {
        int first = nchosen;
        for (int p = 0; p < MAKES; p++) {
            const make *m = &CHAPTER[ch][p];
            int k = make_pool(m, 1000u * (uint32_t)(ch * MAKES + p + 1));
            fprintf(stderr, "chapter %d, make %d: %d candidates\n", ch + 1, p + 1, k);
            for (int i = 0; i < m->count; i++) {
                int at = m->lo < 0 ? i : (int)((m->lo + (1 - m->lo) * (i + 0.5f) / m->count) * (k - 1));
                while (at < k && seen(pool[at].key)) at++;
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
                if (a->p.np * 1e3 + a->score <= b->p.np * 1e3 + b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
    table(chosen, nchosen);
    printf("/*\n"
           " * Magic Squares' levels: our own, made by tools/magicsquare_gen.c (the game's maker; each checked to have\n"
           " * one solution), six chapters of 25 from the easiest. Each: the figure (a b c the triangles of 3, 4, 5 a\n"
           " * side; s the 3 x 3, q the 4 x 4), the target, then the numbers place by place, '?' before a missing one\n"
           " * (magicsquare.h, msq_decode); in the comment the gaps and the full count's search.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "static const char *const msq_levels[MSQ_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++)
        printf("    \"%s\", /* %d: %d gaps, %ld nodes */\n", chosen[i].key, i + 1, chosen[i].gaps, chosen[i].nodes);
    printf("};\n");
    return 0;
}
