/*
 * Dominosa's levels, made by chance and chosen: src/games/dominosa_levels.h. Our own puzzles, from the game's own
 * maker (the grid tiled with dominoes by chance, the set dealt onto them, dominoes swapped or turned round until the
 * reasoning grader finishes it at its level: one solution). Each candidate:
 *   - its grade (the hardest reasoning the grader needed: a cell with one neighbour left, a domino with one place
 *     left; a cell whose pairs are all one domino, a domino whose places share a cell, a pair leaving a cell alone; a
 *     pair followed to a contradiction) and its steps, weighed by their level (its effort);
 *   - near-duplicates dropped (the same numbers mirrored or turned half round).
 * Each chapter has two makes (a set and a level); a make's candidates are ranked by effort and picked evenly over
 * them (the last chapter over its harder part). Then each chapter goes from its smallest and easiest to its biggest
 * and hardest: double-3 (5 x 4 cells) easy first, double-8 (10 x 9) hard last.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/dsagen apps/boardgames/tools/dominosa_gen.c \
 *       apps/boardgames/src/games/dominosa_rules.c && /tmp/dsagen > apps/boardgames/src/games/dominosa_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/dominosa.h"

#define CANDS 60                                /* candidates made for each make */
#define CODE (DSA_CELLS + 8)

typedef struct {
    dsa_puz p;
    int grade, steps;
    double score;
    char key[CODE];
} cand;

typedef struct { int n, level, count, lo; } make;   /* lo: the picks from that part (in a hundred) of the pool up */
static const make CHAPTER[DSA_CHAPTERS][2] = {
    {{3, DSA_EASY, 12, 0}, {4, DSA_EASY, 13, 0}},
    {{4, DSA_MEDIUM, 10, 0}, {5, DSA_EASY, 15, 0}},
    {{5, DSA_MEDIUM, 12, 0}, {6, DSA_MEDIUM, 13, 0}},
    {{5, DSA_HARD, 12, 0}, {6, DSA_HARD, 13, 0}},
    {{7, DSA_MEDIUM, 10, 0}, {7, DSA_HARD, 15, 0}},
    {{8, DSA_HARD, 12, 25}, {8, DSA_HARD, 13, 50}},
};

/* the numbers whichever way they lie: the smallest code of their four (mirrored across, down, both) */
static void canon(const dsa_puz *p, char *key)
{
    int w = p->w, h = p->h;
    key[0] = 0;
    for (int t = 0; t < 4; t++) {
        dsa_puz q = *p;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                q.num[y * w + x] = p->num[(t & 2 ? h - 1 - y : y) * w + (t & 1 ? w - 1 - x : x)];
        char k[CODE];
        dsa_encode(&q, k, sizeof k);
        if (!key[0] || strcmp(k, key) < 0) strcpy(key, k);
    }
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[DSA_STAGES];
static int nchosen;
static uint8_t sol[DSA_CELLS];

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
        dsa_make(m->n, m->level, seed, &c->p, sol);
        c->grade = dsa_effort(&c->p, &c->steps);
        if (c->grade != m->level || dsa_count(&c->p, 2, NULL) != 1) continue;
        canon(&c->p, c->key);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (dup) continue;
        c->score = c->steps + 0.01 * (seed % 7);
        k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

int main(void)
{
    for (int ch = 0; ch < DSA_CHAPTERS; ch++) {
        int first = nchosen;
        for (int mi = 0; mi < 2; mi++) {
            const make *m = &CHAPTER[ch][mi];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 2 + mi + 1));
            fprintf(stderr, "chapter %d, make %d (double-%d, level %d): %d candidates, effort %d..%d\n", ch + 1,
                    mi + 1, m->n, m->level, k, k ? pool[0].steps : 0, k ? pool[k - 1].steps : 0);
            for (int i = 0; i < m->count; i++) {
                int lo = m->lo * (k - 1) / 100, at = lo + (int)((i + 0.5f) / m->count * (k - 1 - lo));
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
    printf("/*\n"
           " * Dominosa's levels: our own, made by tools/dominosa_gen.c (the game's maker, the reasoning grader's\n"
           " * grade and effort), six chapters of 25 from the easiest. Each: \"<n>:\" (the set, double-n) and a digit\n"
           " * a cell, row by row, n + 2 columns and n + 1 rows (dominosa.h, dsa_decode); in the comment the reasoning\n"
           " * it needs (0 easy, 1 medium, 2 hard) and the grader's steps weighed by their level.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const dsa_levels[DSA_STAGES] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[CODE];
        dsa_encode(&chosen[i].p, code, sizeof code);
        printf("    \"%s\", /* %d: double-%d, grade %d, effort %d */\n", code, i + 1, chosen[i].p.n, chosen[i].grade,
               chosen[i].steps);
    }
    printf("};\n");
    return nchosen == DSA_STAGES ? 0 : 1;
}
