/*
 * Cross Math's levels, made by chance and chosen: src/games/mathcross_levels.h. Our own puzzles, from the game's own
 * maker (equations laid crossing one another, numbers drawn within the make's ranges, then numbers taken away while
 * reasoning alone still finds them all; mathcross_rules.c). Each candidate:
 *   - checked to have one solution by a full count, and solved by reasoning alone;
 *   - its work: the gaps, and the steps of reasoning past the plain ones (a gap only one tile fits, a tile only one
 *     gap takes), which weigh three times;
 *   - near-duplicates dropped (the same board mirrored on its diagonal).
 * Each chapter has its makes (the board, the equations, the operations, the ranges, how many levels, the part of the
 * make's candidates ranked by that work they are picked from evenly): plus alone first (sums to ten, a few gaps),
 * then minus, times, divided, bigger boards, and last equations of three numbers worked out in the usual order. Each
 * chapter then goes from its smallest and easiest to its biggest and hardest.
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

#define CANDS 80                                /* candidates made for each make */
#define TXT 1024

typedef struct {
    mcr_puzzle p;
    int gaps, medium, cells;
    double score;
    char key[TXT];
} cand;

#define A (1u << MCR_ADD)
#define S (1u << MCR_SUB)
#define M (1u << MCR_MUL)
#define D (1u << MCR_DIV)
typedef struct { mcr_make m; int count; float lo, hi; } make;
static const make CHAPTER[MCR_CHAPTERS][2] = {
    {{{7, 7, 2, A, 10, 10, 0, 40}, 10, 0, 1}, {{7, 7, 3, A, 10, 10, 0, 45}, 15, 0, 1}},
    {{{7, 7, 3, A | S, 20, 10, 0, 45}, 12, 0, 1}, {{9, 9, 4, A | S, 20, 10, 0, 50}, 13, 0, 1}},
    {{{9, 9, 4, A | S | M, 30, 10, 0, 50}, 12, 0, 1}, {{9, 9, 5, A | S | M, 40, 10, 0, 55}, 13, 0, 1}},
    {{{9, 9, 5, A | S | M | D, 50, 10, 0, 55}, 12, 0, 1}, {{11, 11, 6, A | S | M | D, 50, 10, 0, 60}, 13, 0, 1}},
    {{{11, 11, 7, A | S | M | D, 99, 12, 0, 60}, 12, 0, 1}, {{11, 11, 8, A | S | M | D, 99, 12, 0, 70}, 13, 0, 1}},
    {{{11, 11, 6, A | S | M | D, 99, 12, 60, 55}, 12, 0, 1}, {{11, 11, 8, A | S | M | D, 99, 12, 60, 65}, 13, 0, 1}},
};

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
    c->score = c->gaps + 3.0 * c->medium;
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

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "survey")) {
        const make *m = &CHAPTER[atoi(argv[2]) % MCR_CHAPTERS][atoi(argv[3]) % 2];
        int k = make_pool(m, 7);
        printf("%d candidates; by tenths (gaps of numbers, medium steps):\n", k);
        for (int i = 0; i < 10 && k; i++) {
            const cand *c = &pool[i * (k - 1) / 9];
            printf("  %d/%d %d  %s\n", c->gaps, c->cells, c->medium, c->key);
        }
        return 0;
    }
    for (int ch = 0; ch < MCR_CHAPTERS; ch++) {
        int first = nchosen;
        for (int p = 0; p < 2; p++) {
            const make *m = &CHAPTER[ch][p];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 2 + p + 1));
            fprintf(stderr, "chapter %d, make %d: %d candidates\n", ch + 1, p + 1, k);
            for (int i = 0; i < m->count; i++) {
                int at = (int)((m->lo + (m->hi - m->lo) * (i + 0.5f) / m->count) * (k - 1));
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
                if (a->cells * 1e3 + a->score <= b->cells * 1e3 + b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
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
