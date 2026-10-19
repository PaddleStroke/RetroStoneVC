/*
 * Cross Math's levels, made by chance and chosen: src/games/mathcross_levels.h. Our own puzzles, from the game's own
 * maker (equations laid crossing one another, numbers drawn within the make's ranges, then numbers taken away while
 * reasoning alone still finds them all; mathcross_rules.c). Each candidate:
 *   - checked to have one solution by a full count, and solved by reasoning alone;
 *   - its work: the gaps, and the steps of reasoning past the plain ones (a gap only one tile fits, a tile only one
 *     gap takes), which weigh three times;
 *   - near-duplicates dropped (the same board mirrored on its diagonal).
 *   - the plain rounds: how many waves of "an equation with one number missing" fill it (one: a list of separate
 *     sums, trivial), or none when they stall and a medium step is needed.
 * Each chapter has three makes (the board, the equations, the operations, the ranges, how many levels, and what each
 * must ask at least: medium steps, gaps, not trivial): an opening of its two easiest, then two makes of bigger boards
 * and more gaps, picked evenly over those asking enough (in the first chapter led by a board of three sums and three
 * gaps, one sum giving the next: level 3 a step up from the opening, a medium step from level 4). Plus alone first
 * (sums to ten), then minus, times, divided, bigger boards (up to 13 x 13), and last equations of three numbers worked
 * out in the usual order. The per-level table and the chapters' summary go to stderr (-DREPORT: the same for the
 * levels in mathcross_levels.h).
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/mcrgen apps/boardgames/tools/mathcross_gen.c \
 *       apps/boardgames/src/games/mathcross_rules.c && /tmp/mcrgen > apps/boardgames/src/games/mathcross_levels.h
 * "/tmp/mcrgen survey CHAPTER MAKE": how the work spreads for that make.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/mathcross.h"

#define CANDS 160                               /* candidates made for each make */
#define TXT 1024

typedef struct {
    mcr_puzzle p;
    int gaps, medium, cells, rounds, open;
    double score;
    char key[TXT];
} cand;

#define A (1u << MCR_ADD)
#define S (1u << MCR_SUB)
#define M (1u << MCR_MUL)
#define D (1u << MCR_DIV)
/* a make: the maker's settings, how many levels it gives, and what each must ask at least: medium steps, gaps, and
 * (nontrivial) more than one plain round or a medium step; lead: a first level of its boards with that share of gaps
 * instead, its easiest asking what the make asks (the step from the opening; 0: none) */
typedef struct { mcr_make m; int count, minmed, mingaps, nontrivial, lead; } make;
#define MAKES 3
static const make CHAPTER[MCR_CHAPTERS][MAKES] = {
    {{{7, 7, 2, A, 10, 10, 0, 40}, 2, 0, 2, 0, 0}, {{7, 7, 3, A, 10, 10, 0, 50}, 10, 0, 3, 1, 40},
     {{9, 9, 4, A, 10, 10, 0, 60}, 13, 0, 4, 1, 0}},
    {{{9, 9, 4, A | S, 20, 10, 0, 50}, 2, 0, 3, 0, 0}, {{9, 9, 5, A | S, 20, 10, 0, 60}, 11, 1, 4, 1, 0},
     {{11, 11, 6, A | S, 20, 10, 0, 65}, 12, 1, 5, 1, 0}},
    {{{9, 9, 5, A | S | M, 30, 10, 0, 55}, 2, 0, 4, 0, 0}, {{11, 11, 6, A | S | M, 40, 10, 0, 60}, 11, 1, 5, 1, 0},
     {{11, 11, 7, A | S | M, 40, 10, 0, 65}, 12, 2, 6, 1, 0}},
    {{{11, 11, 6, A | S | M | D, 50, 10, 0, 55}, 2, 0, 5, 0, 0},
     {{11, 11, 7, A | S | M | D, 50, 10, 0, 65}, 11, 2, 6, 1, 0},
     {{13, 13, 8, A | S | M | D, 50, 10, 0, 70}, 12, 2, 7, 1, 0}},
    {{{11, 11, 8, A | S | M | D, 99, 12, 0, 65}, 2, 1, 7, 0, 0},
     {{13, 13, 9, A | S | M | D, 99, 12, 0, 70}, 11, 3, 9, 1, 0},
     {{13, 13, 10, A | S | M | D, 99, 12, 0, 72}, 12, 3, 10, 1, 0}},
    {{{11, 11, 6, A | S | M | D, 99, 12, 60, 55}, 2, 1, 6, 0, 0},
     {{11, 11, 8, A | S | M | D, 99, 12, 60, 65}, 11, 3, 9, 1, 0},
     {{13, 13, 9, A | S | M | D, 99, 12, 70, 70}, 12, 4, 10, 1, 0}},
};

/* ---- the difficulty metrics ------------------------------------------------------------------------------------- */
/* the plain way: each round, every equation with one number missing gets it. The rounds it takes (1: every gap sits
 * alone in an equation from the start, a list of separate sums), or -1 when it stalls (the medium steps needed);
 * *open the gaps it fills in the first round */
static int plain_rounds(const mcr_puzzle *p0, int *open)
{
    mcr_puzzle p = *p0;
    mcr_eq eq[MCR_EQS];
    int n = mcr_parse(&p, eq), rounds = 0, left = 0;
    for (int c = 0; c < p.w * p.h; c++) left += p.gap[c];
    *open = 0;
    while (left) {
        int fill[MCR_GAPS], nf = 0;
        for (int e = 0; e < n; e++) {
            int miss = 0, at = -1;
            for (int k = 0; k < eq[e].len; k += 2)
                if (p.gap[eq[e].cell[k]]) miss++, at = eq[e].cell[k];
            if (miss == 1) {
                int dup = 0;
                for (int i = 0; i < nf; i++) dup |= fill[i] == at;
                if (!dup) fill[nf++] = at;
            }
        }
        if (!nf) return -1;
        if (!rounds) *open = nf;
        for (int i = 0; i < nf; i++) p.gap[fill[i]] = 0;
        left -= nf;
        rounds++;
    }
    return rounds;
}

/* trivial: the plain way in one round (each gap alone in its sum), no medium step */
static int trivial(int rounds, int medium) { return rounds == 1 && !medium; }

static void table(const cand *c, int n)
{
    fprintf(stderr, "lvl gaps nums open rounds medium score\n");
    for (int i = 0; i < n; i++)
        fprintf(stderr, "%4d %4d %4d %4d %6d %6d %5.1f\n", i + 1, c[i].gaps, c[i].cells, c[i].open, c[i].rounds,
                c[i].medium, c[i].score);
    fprintf(stderr, "chapter: gaps mean (min..max), medium steps mean, levels the plain way stalls on, plain rounds "
                    "mean (stalls left out); trivial (one plain round, no medium step)\n");
    for (int ch = 0; ch * 25 < n; ch++) {
        double g = 0, m = 0, r = 0;
        int lo = 99, hi = 0, st = 0, t = 0, k = 0, nr = 0;
        for (int i = ch * 25; i < n && i < ch * 25 + 25; i++, k++) {
            g += c[i].gaps, m += c[i].medium;
            if (c[i].rounds < 0) st++;
            else r += c[i].rounds, nr++;
            t += trivial(c[i].rounds, c[i].medium);
            lo = c[i].gaps < lo ? c[i].gaps : lo;
            hi = c[i].gaps > hi ? c[i].gaps : hi;
        }
        fprintf(stderr, "  %d: gaps %4.1f (%d..%d)  medium %.2f  stalls %2d  rounds %.2f  trivial %d/%d\n", ch + 1,
                g / k, lo, hi, m / k, st, nr ? r / nr : 0, t, k);
    }
}

static void transpose(const mcr_puzzle *p, mcr_puzzle *q)
{
    memset(q, 0, sizeof *q);
    q->w = p->h;
    q->h = p->w;
    for (int y = 0; y < p->h; y++)
        for (int x = 0; x < p->w; x++) {
            int a = y * p->w + x, b = x * q->w + y;
            q->kind[b] = p->kind[a];
            q->val[b] = p->val[a];
            q->gap[b] = p->gap[a];
        }
}

static void canon(const mcr_puzzle *p, char *key)
{
    char a[TXT], b[TXT];
    mcr_puzzle t;
    transpose(p, &t);
    mcr_encode(p, a, TXT);
    mcr_encode(&t, b, TXT);
    strcpy(key, strcmp(a, b) < 0 ? a : b);
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[MCR_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

static int candidate(cand *c, const make *m, uint32_t seed)
{
    mcr_make_puzzle(&m->m, seed, &c->p);
    mcr_eq eq[MCR_EQS];
    mcr_puzzle q = c->p;
    if (mcr_parse(&q, eq) != m->m.eqs) return 0;
    if (!mcr_reason(&c->p, &c->medium) || mcr_count(&c->p, 2) != 1) return 0;
    c->gaps = c->cells = 0;
    for (int i = 0; i < c->p.w * c->p.h; i++) {
        c->gaps += c->p.gap[i];
        c->cells += c->p.kind[i] == MCR_NUM;
    }
    if (c->gaps < 2 || c->gaps * 100 < (m->m.gaps - 15) * c->cells) return 0;
    c->rounds = plain_rounds(&c->p, &c->open);
    c->score = c->gaps + 3.0 * c->medium + 2.0 * (c->rounds < 0 ? 3 : c->rounds - 1);
    canon(&c->p, c->key);
    return 1;
}

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 30 * CANDS; seed++) {
        cand *c = &pool[k];
        if (!candidate(c, m, seed)) continue;
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (!dup) k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

#ifdef REPORT                                   /* -DREPORT: the metrics of the levels in mathcross_levels.h */
#include "games/mathcross_levels.h"
static int report(void)
{
    static cand lv[MCR_LEVELS];
    for (int i = 0; i < MCR_LEVELS; i++) {
        cand *c = &lv[i];
        mcr_decode(mcr_levels[i], &c->p);
        mcr_reason(&c->p, &c->medium);
        c->gaps = c->cells = 0;
        for (int k = 0; k < c->p.w * c->p.h; k++) c->gaps += c->p.gap[k], c->cells += c->p.kind[k] == MCR_NUM;
        c->rounds = plain_rounds(&c->p, &c->open);
        c->score = c->gaps + 3.0 * c->medium + 2.0 * (c->rounds < 0 ? 3 : c->rounds - 1);
    }
    table(lv, MCR_LEVELS);
    return 0;
}
#endif

int main(int argc, char **argv)
{
#ifdef REPORT
    (void)argc, (void)argv;
    return report();
#endif
    if (argc == 4 && !strcmp(argv[1], "survey")) {
        const make *m = &CHAPTER[atoi(argv[2]) % MCR_CHAPTERS][atoi(argv[3]) % MAKES];
        int k = make_pool(m, 7);
        printf("%d candidates; by tenths (gaps of numbers, medium steps):\n", k);
        for (int i = 0; i < 10 && k; i++) {
            const cand *c = &pool[i * (k - 1) / 9];
            printf("  %d/%d %d  %s\n", c->gaps, c->cells, c->medium, c->key);
        }
        return 0;
    }
    for (int ch = 0; ch < MCR_CHAPTERS; ch++) {
        for (int p = 0; p < MAKES; p++) {
            const make *m = &CHAPTER[ch][p];
            uint32_t seed = 1000u * (uint32_t)(ch * MAKES + p + 1);
            int n = m->count - !!m->lead, first = nchosen;
            if (m->lead) {                      /* its lead: its easiest board with fewer gaps */
                make l = *m;
                l.m.gaps = m->lead;
                int kl = make_pool(&l, seed), j = 0;
                while (j < kl && (pool[j].medium < m->minmed || pool[j].gaps < m->mingaps || seen(pool[j].key) ||
                                  (m->nontrivial && trivial(pool[j].rounds, pool[j].medium))))
                    j++;
                if (j >= kl) {
                    fprintf(stderr, "chapter %d: no lead for make %d\n", ch + 1, p + 1);
                    return 1;
                }
                chosen[nchosen++] = pool[j];
            }
            int k = make_pool(m, seed), ok[CANDS], nk = 0;
            for (int i = 0; i < k; i++)         /* the ones asking what the make asks */
                if (pool[i].medium >= m->minmed && pool[i].gaps >= m->mingaps &&
                    !(m->nontrivial && trivial(pool[i].rounds, pool[i].medium)) && !seen(pool[i].key))
                    ok[nk++] = i;
            fprintf(stderr, "chapter %d, make %d: %d candidates, %d asking enough\n", ch + 1, p + 1, k, nk);
            if (nk < n) {
                fprintf(stderr, "chapter %d: not enough of make %d\n", ch + 1, p + 1);
                return 1;
            }
            for (int i = 0; i < n; i++)         /* spread over them (an opening: the easiest) */
                chosen[nchosen++] = pool[ok[!m->nontrivial ? i : n > 1 ? i * (nk - 1) / (n - 1) : 0]];
            for (int i = first + 1; i < nchosen; i++)   /* the make's from its smallest and easiest */
                for (int j = i; j > first; j--) {
                    cand *a = &chosen[j - 1], *b = &chosen[j];
                    if (a->cells * 1e3 + a->score <= b->cells * 1e3 + b->score) break;
                    cand t = *a;
                    *a = *b;
                    *b = t;
                }
        }
    }
    table(chosen, nchosen);
    printf("/*\n"
           " * Cross Math's levels: our own, made by tools/mathcross_gen.c (the game's maker; each solved by reasoning\n"
           " * alone and checked to have one solution), six chapters of 25 from the easiest. Each: \"<w>x<h>:\", a\n"
           " * character a cell ('.' empty, '#' a number, + - * / =), ':' and the numbers in reading order, a missing\n"
           " * one after '?' (mathcross.h, mcr_decode); in the comment the gaps and the steps of reasoning past the\n"
           " * plain ones.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "static const char *const mcr_levels[MCR_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char s[TXT];
        mcr_encode(&chosen[i].p, s, TXT);
        printf("    \"%s\", /* %d: %d gaps, %d medium */\n", s, i + 1, chosen[i].gaps, chosen[i].medium);
    }
    printf("};\n");
    return 0;
}
