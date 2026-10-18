/*
 * Fillomino's levels, made by chance and chosen: src/games/fillomino_levels.h. Our own puzzles, from the game's own
 * maker (the grid cut into regions by chance, every number given, then numbers taken away while the solver finishes it
 * at its level; fillomino_rules.c). Each candidate:
 *   - its grade (bench.h: easy, medium, hard), checked to have one solution;
 *   - the work it asks of the solver: the passes of its reasoning (each a round of the ways allowed; a trial's count
 *     too), and how often the medium ways were needed;
 *   - near-duplicates dropped (the same numbers turned or mirrored).
 * Each chapter has its makes (a size, a level, how many, and the part of the make's candidates, ranked by that work,
 * they are picked from evenly): 6 x 6 at the easy level first, up to 12 x 12 at the hard level. Then each chapter goes
 * from its smallest and easiest to its biggest and hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/flmgen apps/boardgames/tools/fillomino_gen.c \
 *       apps/boardgames/src/games/fillomino_rules.c && /tmp/flmgen > apps/boardgames/src/games/fillomino_levels.h
 * "/tmp/flmgen survey N LEVEL": how the work spreads for that size and level (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/fillomino.h"

#define CANDS 80                                /* candidates made for each make */
#define BUDGET 200000L                          /* the solver's passes a candidate may cost */

typedef struct {
    flm_puzzle p;
    int grade, numbers;
    long passes, mediums;
    double score;
    char key[FLM_CELLS + 8];
} cand;

typedef struct { int n, level, count; float lo, hi; } make;
/* a size, a level, how many levels, the part of its candidates (from the least work 0 to the most 1) */
static const make CHAPTER[FLM_CHAPTERS][3] = {
    {{6, BENCH_EASY, 15, 0, 0.8f}, {7, BENCH_EASY, 10, 0, 1}},
    {{8, BENCH_EASY, 10, 0.2f, 1}, {8, BENCH_MEDIUM, 15, 0, 0.7f}},
    {{9, BENCH_MEDIUM, 25, 0.1f, 1}},
    {{9, BENCH_HARD, 10, 0, 0.6f}, {10, BENCH_MEDIUM, 15, 0.2f, 1}},
    {{10, BENCH_HARD, 25, 0.1f, 0.9f}},
    {{12, BENCH_MEDIUM, 10, 0.4f, 1}, {12, BENCH_HARD, 15, 0.2f, 1}},
};

/* the numbers whichever way they lie: the smallest of their eight (turned, mirrored) */
static void canon(const flm_puzzle *p, char *key)
{
    int n = p->n;
    char best[FLM_CELLS + 8], k[FLM_CELLS + 8];
    best[0] = 0;
    for (int t = 0; t < 8; t++) {
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int a = t & 1 ? n - 1 - x : x, b = t & 2 ? n - 1 - y : y, sx = t & 4 ? b : a, sy = t & 4 ? a : b;
                k[y * n + x] = (char)('.' + p->given[sy * n + sx]);
            }
        k[n * n] = 0;
        if (!best[0] || strcmp(k, best) < 0) strcpy(best, k);
    }
    strcpy(key, best);
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[FLM_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* the numbers taken away again two at a time, a cell and its partner turned half round the centre, while the solver
 * still finishes it at its level: the numbers left lie the way a setter lays them (turned half round, the same).
 * Several orders tried, the one leaving the fewest numbers kept */
typedef struct { flm_puzzle *p; const uint8_t *full; } halves;

static int half_grade(void *u, int cap)
{
    (void)cap;
    return flm_grade(((halves *)u)->p, NULL);
}

static void half_clue(void *u, int i, int on)
{
    halves *h = u;
    int m = h->p->n * h->p->n - 1 - i;
    h->p->given[i] = on ? h->full[i] : 0;
    h->p->given[m] = on ? h->full[m] : 0;
}

#define ORDERS 8                                /* the orders tried: the sparsest kept (pairs go less easily) */

static int symmetric(flm_puzzle *p, const uint8_t *sol, int level, uint32_t seed)
{
    int n = p->n, order[FLM_CELLS], k = 0, best = FLM_CELLS + 1;
    flm_puzzle q = *p, keep = *p;
    for (int c = 0; c <= n * n - 1 - c; c++) order[k++] = c;
    uint32_t s = seed * 2654435761u + 12345u;
    halves h = {&q, sol};
    bench_maker mk = {&h, half_grade, half_clue};
    for (int t = 0; t < ORDERS; t++) {
        for (int i = k - 1; i > 0; i--) {
            s = s * 1103515245u + 12345u;
            int j = (int)((s >> 8) % (uint32_t)(i + 1)), x = order[i];
            order[i] = order[j];
            order[j] = x;
        }
        memcpy(q.given, sol, sizeof q.given);
        if (bench_reduce(&mk, order, k, level, NULL, 0) != level) continue;
        int numbers = 0;
        for (int c = 0; c < n * n; c++) numbers += q.given[c] > 0;
        if (numbers < best) best = numbers, keep = q;
    }
    if (best > FLM_CELLS) return 0;             /* none at the level */
    *p = keep;
    return 1;
}

/* one candidate from a seed; 1 when it is of the size and the level and has one solution */
static int candidate(cand *c, int n, int level, uint32_t seed)
{
    uint8_t sol[FLM_CELLS];
    flm_make_ex(n, level, seed, BUDGET, &c->p, sol);
    if (c->p.n != n) return 0;
    if (!symmetric(&c->p, sol, level, seed)) return 0;
    c->numbers = 0;
    for (int i = 0; i < n * n; i++) c->numbers += c->p.given[i] > 0;
    if (c->numbers * 100 > n * n * (level == BENCH_EASY ? 62 : 50)) return 0;   /* too full: little left to do */
    c->grade = flm_effort(&c->p, &c->passes, &c->mediums);
    if (c->grade != level || flm_count(&c->p, 2) != 1) return 0;
    /* the work: the passes, the medium finds weigh more; fewer numbers, a little more */
    c->score = (double)c->passes + 4.0 * c->mediums - 0.3 * c->numbers;
    canon(&c->p, c->key);
    return 1;
}

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 20 * CANDS; seed++) {
        cand *c = &pool[k];
        if (!candidate(c, m->n, m->level, seed)) continue;
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (!dup) k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

static void survey(int n, int level)
{
    make m = {n, level, 0, 0, 1};
    int k = make_pool(&m, 7);
    printf("%dx%d level %d: %d candidates; work (passes, medium finds, numbers) by tenths:\n", n, n, level, k);
    for (int i = 0; i < 10 && k; i++) {
        const cand *c = &pool[i * (k - 1) / 9];
        printf("  %ld %ld %d\n", c->passes, c->mediums, c->numbers);
    }
}

static const char *const GRADE[] = {"easy", "medium", "hard"};

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "survey")) {
        survey(atoi(argv[2]), atoi(argv[3]));
        return 0;
    }
    for (int ch = 0; ch < FLM_CHAPTERS; ch++) {
        int first = nchosen;
        for (int p = 0; p < 3 && CHAPTER[ch][p].count; p++) {
            const make *m = &CHAPTER[ch][p];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 3 + p + 1));
            fprintf(stderr, "chapter %d, make %d (%dx%d %s): %d candidates\n", ch + 1, p + 1, m->n, m->n,
                    GRADE[m->level], k);
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
                if (a->p.n * 1e6 + a->grade * 1e4 + a->score <= b->p.n * 1e6 + b->grade * 1e4 + b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
    printf("/*\n"
           " * Fillomino's levels: our own, made by tools/fillomino_gen.c (the game's maker; each checked to have one\n"
           " * solution), six chapters of 25 from the easiest. Each: \"<n>:\" and a character a cell, row by row\n"
           " * (fillomino.h, flm_decode); in the comment its size, its grade and the work it asks of the solver (its\n"
           " * passes, the medium finds).\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const flm_levels[FLM_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[FLM_CELLS + 64];
        if (!flm_encode(&chosen[i].p, code, sizeof code)) return 1;
        printf("    \"%s\", /* %d: %dx%d %s, %ld passes, %ld medium */\n", code, i + 1, chosen[i].p.n, chosen[i].p.n,
               GRADE[chosen[i].grade], chosen[i].passes, chosen[i].mediums);
    }
    printf("};\n");
    return nchosen == FLM_LEVELS ? 0 : 1;
}
