/*
 * Magic Squares' levels: src/games/magicsquare_levels.h. Our own puzzles, from the game's own maker (a magic filling by
 * chance, numbers taken away while the solution stays the only one; magicsquare_rules.c). Each candidate's work: its
 * gaps, and the search a full count of its solutions takes; near-duplicates (the same numbers and gaps) dropped. Each
 * chapter has two makes (the figure, the gaps at most, the numbers), its levels picked evenly over a make's
 * candidates ranked by work, then the chapter put in order: the small triangle of 1..6 first, the 3 x 3 of 1..9, the
 * triangle of 1..9, 3 x 3 squares of any numbers, the 4 x 4, and last the big triangle and 4 x 4s of larger numbers.
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
    int gaps;
    long nodes;
    double score;
    char key[160];
} cand;

typedef struct { msq_make m; int count; } make;
static const make CHAPTER[MSQ_CHAPTERS][2] = {
    {{{MSQ_TRI3, 2, 0}, 10}, {{MSQ_TRI3, 3, 0}, 15}},
    {{{MSQ_SQ3, 3, 0}, 12}, {{MSQ_SQ3, 5, 1}, 13}},
    {{{MSQ_TRI4, 4, 0}, 12}, {{MSQ_TRI4, 6, 1}, 13}},
    {{{MSQ_SQ3, 5, 2}, 12}, {{MSQ_SQ3, 6, 2}, 13}},
    {{{MSQ_SQ4, 6, 0}, 12}, {{MSQ_SQ4, 8, 0}, 13}},
    {{{MSQ_TRI5, 8, 1}, 12}, {{MSQ_SQ4, 10, 1}, 13}},
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

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 30 * CANDS; seed++) {
        cand *c = &pool[k];
        msq_make_puzzle(&m->m, seed, &c->p);
        if (c->p.kind != m->m.kind || msq_count(&c->p, 2, &c->nodes) != 1) continue;
        c->gaps = 0;
        for (int i = 0; i < c->p.np; i++) c->gaps += !c->p.given[i];
        if (c->gaps < (m->m.blanks + 1) / 2) continue;
        c->score = c->gaps + 0.5 * log((double)c->nodes + 1);
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
    for (int ch = 0; ch < MSQ_CHAPTERS; ch++) {
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
                if (a->p.np * 1e3 + a->score <= b->p.np * 1e3 + b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
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
