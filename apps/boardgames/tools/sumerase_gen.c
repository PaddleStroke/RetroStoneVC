/*
 * Sum Erase's levels, made by chance and chosen: src/games/sumerase_levels.h. Our own puzzles, from the game's own
 * maker (digits and the cells erased dealt by chance, then changed a little at a time until the solver solves the
 * grid at its level; sumerase_rules.c). Each candidate:
 *   - its grade (bench.h: easy, medium, hard), checked to have one solution by a plain search;
 *   - the work it asks of the solver: the passes of its reasoning, and how often the medium ways were needed;
 *   - near-duplicates dropped (the same grid turned or mirrored).
 * Each chapter has its makes (a size, the digits used, a level, how many, and the part of the make's candidates,
 * ranked by that work, they are picked from evenly): 3 x 3 with small digits at the easy level first, up to 9 x 9 at
 * the hard level. Then each chapter goes from its smallest and easiest to its biggest and hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/smegen apps/boardgames/tools/sumerase_gen.c \
 *       apps/boardgames/src/games/sumerase_rules.c && /tmp/smegen > apps/boardgames/src/games/sumerase_levels.h
 * "/tmp/smegen survey N MAXD LEVEL": how the work spreads for that make (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/sumerase.h"

#define CANDS 120                               /* candidates made for each make */
#define BUDGET 200000L                          /* the solver's passes a candidate may cost */

typedef struct {
    sme_puzzle p;
    uint8_t sol[SME_CELLS];
    int grade;
    long passes, mediums;
    double score;
    char key[2 * SME_CELLS + 8];
} cand;

typedef struct { int n, maxd, level, count; float lo, hi; } make;
static const make CHAPTER[SME_CHAPTERS][2] = {
    {{3, 5, BENCH_EASY, 10, 0, 1}, {4, 6, BENCH_EASY, 15, 0, 1}},
    {{4, 9, BENCH_EASY, 10, 0.2f, 1}, {5, 9, BENCH_EASY, 15, 0, 1}},
    {{5, 9, BENCH_MEDIUM, 12, 0, 0.8f}, {6, 9, BENCH_EASY, 13, 0.3f, 1}},
    {{6, 9, BENCH_MEDIUM, 15, 0.1f, 1}, {7, 9, BENCH_MEDIUM, 10, 0, 0.8f}},
    {{7, 9, BENCH_HARD, 10, 0, 0.7f}, {8, 9, BENCH_MEDIUM, 15, 0.2f, 1}},
    {{8, 9, BENCH_HARD, 15, 0.1f, 1}, {9, 9, BENCH_HARD, 10, 0.2f, 1}},
};

/* the grid whichever way it lies: the smallest of its eight (turned, mirrored), digits and erasures */
static void canon(const sme_puzzle *p, const uint8_t *sol, char *key)
{
    int n = p->n;
    char best[2 * SME_CELLS + 8], k[2 * SME_CELLS + 8];
    best[0] = 0;
    for (int t = 0; t < 8; t++) {
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int a = t & 1 ? n - 1 - x : x, b = t & 2 ? n - 1 - y : y, sx = t & 4 ? b : a, sy = t & 4 ? a : b;
                int c = sy * n + sx;
                k[y * n + x] = (char)(sol[c] ? 'a' + p->num[c] - 1 : '0' + p->num[c]);
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
static cand chosen[SME_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* one candidate from a seed; 1 when it is of the size and the level and has one solution */
static int candidate(cand *c, const make *m, uint32_t seed)
{
    sme_make(m->n, m->maxd, m->level, seed, BUDGET, &c->p, c->sol);
    if (c->p.n != m->n) return 0;
    c->grade = sme_effort(&c->p, &c->passes, &c->mediums);
    if (c->grade != m->level || sme_count(&c->p, 2) != 1) return 0;
    if (m->level > BENCH_EASY && sme_grade(&c->p, m->level - 1, NULL) != BENCH_GUESS) return 0;
    c->score = (double)c->passes + 4.0 * c->mediums;
    canon(&c->p, c->sol, c->key);
    return 1;
}

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 20 * CANDS; seed++) {
        cand *c = &pool[k];
        if (!candidate(c, m, seed)) continue;
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (!dup) k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

static void survey(int n, int maxd, int level)
{
    make m = {n, maxd, level, 0, 0, 1};
    int k = make_pool(&m, 7);
    printf("%dx%d digits 1..%d level %d: %d candidates; work (passes, medium finds) by tenths:\n", n, n, maxd, level,
           k);
    for (int i = 0; i < 10 && k; i++) {
        const cand *c = &pool[i * (k - 1) / 9];
        printf("  %ld %ld\n", c->passes, c->mediums);
    }
}

static const char *const GRADE[] = {"easy", "medium", "hard"};

int main(int argc, char **argv)
{
    if (argc == 5 && !strcmp(argv[1], "survey")) {
        survey(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
        return 0;
    }
    for (int ch = 0; ch < SME_CHAPTERS; ch++) {
        int first = nchosen;
        for (int p = 0; p < 2 && CHAPTER[ch][p].count; p++) {
            const make *m = &CHAPTER[ch][p];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 2 + p + 1));
            fprintf(stderr, "chapter %d, make %d (%dx%d 1..%d %s): %d candidates\n", ch + 1, p + 1, m->n, m->n, m->maxd,
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
           " * Sum Erase's levels: our own, made by tools/sumerase_gen.c (the game's maker; each checked to have one\n"
           " * solution), six chapters of 25 from the easiest. Each: \"<n>:\" and a character a cell, row by row: '1'..'9'\n"
           " * a digit kept, 'a'..'i' a digit 1..9 erased (sumerase.h, sme_decode; the targets are the sums); in the\n"
           " * comment the reasoning it needs (0 easy, 1 medium, 2 hard) and the solver's passes.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "static const char *const sme_levels[SME_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char s[SME_CELLS + 8];
        sme_encode(&chosen[i].p, chosen[i].sol, s, sizeof s);
        printf("    \"%s\", /* %d: %dx%d, grade %d, %ld passes */\n", s, i + 1, chosen[i].p.n, chosen[i].p.n,
               chosen[i].grade, chosen[i].passes);
    }
    printf("};\n");
    return 0;
}
