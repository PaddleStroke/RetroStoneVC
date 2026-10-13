/*
 * Calcudoku's levels, made by chance and chosen: src/games/calcudoku_levels.h. Our own puzzles, from the game's own
 * maker (a Latin square by chance, cut into cages, a sign for each; made again, or made harder a step at a time, until
 * the reasoning solver finishes it at its level: one solution). Each candidate:
 *   - its grade (the hardest reasoning the solver needed: the cages' ways and the singles; a cage's digits held in one
 *     line, pairs and triples; trials) and the solver's passes (its effort);
 *   - near-duplicates dropped (the same grid turned or mirrored).
 * Each chapter has two makes (a size and a level); a make's candidates are ranked by effort and picked evenly over
 * them. Then each chapter goes from its smallest and easiest to its biggest and hardest: 4 x 4 easy first, 9 x 9 hard
 * last.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/cdkgen apps/boardgames/tools/calcudoku_gen.c \
 *       apps/boardgames/src/games/calcudoku_rules.c && /tmp/cdkgen > apps/boardgames/src/games/calcudoku_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/calcudoku.h"

#define CANDS 60                                /* candidates made for each make */
#define CODE (3 * CDK_CELLS + 8)

typedef struct {
    cdk_puzzle p;
    int grade;
    long passes;
    double score;
    char key[CODE];
} cand;

typedef struct { int n, level, count; } make;
static const make CHAPTER[CDK_CHAPTERS][2] = {
    {{4, CDK_EASY, 12}, {5, CDK_EASY, 13}},
    {{4, CDK_MEDIUM, 10}, {5, CDK_MEDIUM, 15}},
    {{6, CDK_EASY, 10}, {6, CDK_MEDIUM, 15}},
    {{6, CDK_HARD, 12}, {7, CDK_MEDIUM, 13}},
    {{7, CDK_HARD, 12}, {8, CDK_MEDIUM, 13}},
    {{8, CDK_HARD, 12}, {9, CDK_HARD, 13}},
};

/* the puzzle whichever way it lies: the smallest code of its eight (turned, mirrored) */
static void canon(const cdk_puzzle *p, char *key)
{
    int n = p->n;
    key[0] = 0;
    for (int t = 0; t < 8; t++) {
        cdk_puzzle q = *p;
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int sx = t & 1 ? n - 1 - x : x, sy = t & 2 ? n - 1 - y : y;
                if (t & 4) { int s = sx; sx = sy; sy = s; }
                q.sol[y * n + x] = p->sol[sy * n + sx];
                q.cage_of[y * n + x] = p->cage_of[sy * n + sx];
            }
        char k[CODE];
        cdk_encode(&q, k, sizeof k);
        if (!key[0] || strcmp(k, key) < 0) strcpy(key, k);
    }
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[CDK_STAGES];
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
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 20 * CANDS; seed++) {
        cand *c = &pool[k];
        cdk_make(m->n, m->level, seed, &c->p);
        c->grade = cdk_effort(&c->p, &c->passes);
        if (c->grade != m->level || cdk_count(&c->p, 2) != 1) continue;
        canon(&c->p, c->key);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (dup) continue;
        c->score = (double)c->passes + 0.01 * (seed % 7);
        k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

int main(void)
{
    for (int ch = 0; ch < CDK_CHAPTERS; ch++) {
        int first = nchosen;
        for (int mi = 0; mi < 2; mi++) {
            const make *m = &CHAPTER[ch][mi];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 2 + mi + 1));
            fprintf(stderr, "chapter %d, make %d (%dx%d level %d): %d candidates, effort %ld..%ld\n", ch + 1, mi + 1,
                    m->n, m->n, m->level, k, k ? pool[0].passes : 0, k ? pool[k - 1].passes : 0);
            for (int i = 0; i < m->count; i++) {
                int at = (int)((i + 0.5f) / m->count * (k - 1));
                while (at < k && seen(pool[at].key)) at++;
                if (at >= k) {
                    fprintf(stderr, "chapter %d: not enough of make %d\n", ch + 1, mi + 1);
                    return 1;
                }
                chosen[nchosen++] = pool[at];
            }
        }
        for (int i = first + 1; i < nchosen; i++)   /* the chapter from its easiest to its hardest */
            for (int j = i; j > first; j--) {
                cand *a = &chosen[j - 1], *b = &chosen[j];
                double ka = a->p.n * 1e6 + a->grade * 1e5 + a->score, kb = b->p.n * 1e6 + b->grade * 1e5 + b->score;
                if (ka <= kb) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
    printf("/*\n * Calcudoku's levels: our own, made by tools/calcudoku_gen.c (the game's maker, the reasoning solver's grade\n"
           " * and effort), six chapters of 25 from the easiest. Each: \"<n>:\", the solution's digits, \":\", each cell's\n"
           " * joins to its cage (1 right, 2 down), \":\", the cages' signs (calcudoku.h, cdk_decode; the targets come from\n"
           " * the digits); in the comment the reasoning it needs (0 easy, 1 medium, 2 hard) and the solver's passes.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const cdk_levels[CDK_STAGES] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[CODE];
        cdk_encode(&chosen[i].p, code, sizeof code);
        printf("    \"%s\", /* %d: %dx%d, grade %d, %ld passes */\n", code, i + 1, chosen[i].p.n, chosen[i].p.n,
               chosen[i].grade, chosen[i].passes);
    }
    printf("};\n");
    return nchosen == CDK_STAGES ? 0 : 1;
}
