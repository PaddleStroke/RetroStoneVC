/*
 * Balance Scales' levels: src/games/balance_levels.h. Our own puzzles, from the game's own maker (weights and scales
 * dealt by chance, kept when the scales pin every weight down and the reasoning finds them with the steps the make
 * allows; balance_rules.c). Each candidate's work: the items on its scales, its weights' size, and its pair steps
 * (two scales compared), which weigh most; near-duplicates (the same scales) dropped. Each chapter has two makes
 * (shapes, scales, the heaviest weight, shapes on both pans or not, a pair step needed or not, items on a pan), its
 * levels picked evenly over a make's candidates ranked by work, then the chapter put in order: one shape on one scale
 * first (weights to five), then two and three shapes, shapes on both pans, scales to compare, four and five shapes.
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

typedef struct { bal_make m; int count; } make;
/* {k, m, wmax, both, pair, pan} */
static const make CHAPTER[BAL_CHAPTERS][2] = {
    {{{1, 1, 5, 0, 0, 3}, 10}, {{2, 2, 6, 0, 0, 3}, 15}},
    {{{2, 2, 9, 0, 0, 4}, 12}, {{3, 3, 9, 0, 0, 4}, 13}},
    {{{2, 2, 9, 1, 0, 4}, 12}, {{3, 3, 12, 1, 0, 4}, 13}},
    {{{2, 2, 9, 0, 1, 4}, 12}, {{3, 3, 12, 0, 1, 4}, 13}},
    {{{3, 3, 15, 1, 1, 4}, 12}, {{4, 4, 12, 1, 0, 4}, 13}},
    {{{4, 4, 15, 1, 1, 5}, 12}, {{5, 5, 20, 1, 1, 5}, 13}},
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

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 40 * CANDS; seed++) {
        cand *c = &pool[k];
        bal_make_puzzle(&m->m, seed, &c->p);
        if (c->p.k != m->m.k || !bal_reason(&c->p, &c->plain, &c->pair) || bal_rank(&c->p) != c->p.k) continue;
        if (m->m.pair ? c->pair < 1 : c->pair > 0) continue;
        c->items = 0;
        for (int i = 0; i < c->p.m; i++) c->items += c->p.sc[i].n[0] + c->p.sc[i].n[1];
        int most = 0;
        for (int s = 0; s < c->p.k; s++) most = c->p.wt[s] > most ? c->p.wt[s] : most;
        c->score = c->items + 0.3 * most + 4.0 * c->pair;
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
    for (int ch = 0; ch < BAL_CHAPTERS; ch++) {
        int first = nchosen;
        for (int p = 0; p < 2; p++) {
            const make *m = &CHAPTER[ch][p];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 2 + p + 1));
            fprintf(stderr, "chapter %d, make %d: %d candidates\n", ch + 1, p + 1, k);
            for (int i = 0; i < m->count; i++) {
                int at = (int)((i + 0.5f) / m->count * (k - 1));
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
                if (a->p.k * 1e3 + a->score <= b->p.k * 1e3 + b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
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
