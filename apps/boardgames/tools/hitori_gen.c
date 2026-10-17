/*
 * Hitori's levels, made by chance and chosen: src/games/hitori_levels.h. Our own puzzles, from the game's own
 * maker (a Latin square and shaded cells by chance, then changed a little at a time until the solver solves the grid at
 * its level; hitori_rules.c). Each candidate:
 *   - its grade (bench.h: easy, medium, hard), checked to have one solution;
 *   - the work it asks of the solver: the passes of its reasoning (each a round of the ways allowed; a trial's count
 *     too), and how often the medium ways were needed;
 *   - near-duplicates dropped (the same numbers turned or mirrored);
 *   - how set by hand its shading looks: symmetric (turned half round or mirrored), the board's quarters shaded
 *     alike, a fifth to a third of it shaded; the better looking two in five of the candidates are kept.
 * Each chapter has its makes (a size, a level, how many, and the part of the make's candidates, ranked by that work,
 * they are picked from evenly): 5 x 5 at the easy level first, up to 12 x 12 at the hard level. Then each chapter
 * goes from its smallest and easiest to its biggest and hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/htogen apps/boardgames/tools/hitori_gen.c \
 *       apps/boardgames/src/games/hitori_rules.c && /tmp/htogen > apps/boardgames/src/games/hitori_levels.h
 * "/tmp/htogen survey N LEVEL": how the work spreads for that size and level (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/hitori.h"

#define CANDS 360                               /* candidates made for each make */
#define BUDGET 200000L                          /* the solver's passes a candidate may cost */

typedef struct {
    hto_puzzle p;
    int grade, numbers;                         /* numbers: the cells left to decide at the start */
    long passes, mediums;
    double score, taste;
    char key[HTO_CELLS + 8];
} cand;

typedef struct { int n, level, count; float lo, hi; } make;
/* a size, a level, how many levels, the part of its candidates (from the least work 0 to the most 1) */
static const make CHAPTER[HTO_CHAPTERS][3] = {
    {{5, BENCH_EASY, 10, 0.1f, 0.9f}, {6, BENCH_EASY, 15, 0, 1}},
    {{7, BENCH_EASY, 10, 0.2f, 1}, {7, BENCH_MEDIUM, 15, 0, 0.8f}},
    {{8, BENCH_MEDIUM, 15, 0.1f, 1}, {9, BENCH_MEDIUM, 10, 0.2f, 1}},
    {{9, BENCH_HARD, 10, 0, 0.7f}, {10, BENCH_MEDIUM, 15, 0.2f, 1}},
    {{10, BENCH_HARD, 25, 0.1f, 1}},
    {{11, BENCH_HARD, 10, 0.2f, 1}, {12, BENCH_HARD, 15, 0.3f, 1}},
};

/* the numbers whichever way they lie: the smallest of their eight (turned, mirrored) */
static void canon(const hto_puzzle *p, char *key)
{
    int n = p->n;
    char best[HTO_CELLS + 8], k[HTO_CELLS + 8];
    best[0] = 0;
    for (int t = 0; t < 8; t++) {
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int a = t & 1 ? n - 1 - x : x, b = t & 2 ? n - 1 - y : y, sx = t & 4 ? b : a, sy = t & 4 ? a : b;
                k[y * n + x] = (char)('0' + p->num[sy * n + sx]);
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
static cand chosen[HTO_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* how set by hand its shading looks (the solution's shaded cells), higher better: as symmetric as can be (turned half
 * round, or mirrored either way: the part of the shaded cells whose partner is shaded too), the four quarters of the
 * board shaded about alike, and about a fifth to a third of the cells shaded */
static double taste(const uint8_t *sol, int n)
{
    int all = 0, q[4] = {0, 0, 0, 0}, pair[3] = {0, 0, 0}, most = 0;
    for (int c = 0; c < n * n; c++) {
        if (!sol[c]) continue;
        int x = c % n, y = c / n;
        all++;
        q[(x >= (n + 1) / 2) + 2 * (y >= (n + 1) / 2)] += !(n % 2 && (x == n / 2 || y == n / 2));
        pair[0] += sol[(n - 1 - y) * n + n - 1 - x] != 0;
        pair[1] += sol[y * n + n - 1 - x] != 0;
        pair[2] += sol[(n - 1 - y) * n + x] != 0;
    }
    if (!all) return -9;
    int best = pair[0] > pair[1] ? pair[0] : pair[1];
    best = best > pair[2] ? best : pair[2];
    int inq = q[0] + q[1] + q[2] + q[3];
    for (int i = 0; i < 4; i++) most = q[i] > most ? q[i] : most;
    double share = (double)all / (n * n), off = share < 0.2 ? 0.2 - share : share > 0.34 ? share - 0.34 : 0;
    return (double)best / all - (inq ? (most - inq / 4.0) / (inq * 0.75) : 1) - 4 * off;
}

static int by_taste(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->taste > y->taste ? -1 : x->taste < y->taste;
}

/* one candidate from a seed; 1 when it is of the size and the level and has one solution */
static int candidate(cand *c, int n, int level, uint32_t seed)
{
    uint8_t sol[HTO_CELLS];
    hto_make_ex(n, level, seed, BUDGET, &c->p, sol);
    if (c->p.n != n) return 0;
    c->grade = hto_effort(&c->p, &c->passes, &c->mediums);
    if (c->grade != level || hto_count(&c->p, 2) != 1) return 0;
    c->taste = taste(sol, n);
    c->numbers = 0;
    for (int i = 0; i < n * n; i++)
        for (int j = 0; j < n; j++) {
            int x = i % n, y = i / n;
            if ((j != x && c->p.num[y * n + j] == c->p.num[i]) || (j != y && c->p.num[j * n + x] == c->p.num[i])) {
                c->numbers++;
                break;
            }
        }
    /* the work: the passes, the medium finds weigh more; the cells left to decide, a little more */
    c->score = (double)c->passes + 4.0 * c->mediums + 0.1 * c->numbers;
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
    qsort(pool, (size_t)k, sizeof pool[0], by_taste);   /* the better looking two in five, then by their work */
    if (k > 60) k = k * 2 / 5;
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

static void survey(int n, int level)
{
    make m = {n, level, 0, 0, 1};
    int k = make_pool(&m, 7);
    printf("%dx%d level %d: %d candidates; work (passes, medium finds, cells to decide) by tenths:\n", n, n, level, k);
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
    for (int ch = 0; ch < HTO_CHAPTERS; ch++) {
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
           " * Hitori's levels: our own, made by tools/hitori_gen.c (the game's maker; each checked to have one\n"
           " * solution), six chapters of 25 from the easiest. Each: \"<n>:\" and a character a cell, row by row\n"
           " * (hitori.h, hto_decode); in the comment its size, its grade and the work it asks of the solver (its\n"
           " * passes, the medium finds).\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const hto_levels[HTO_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[HTO_CELLS + 64];
        if (!hto_encode(&chosen[i].p, code, sizeof code)) return 1;
        printf("    \"%s\", /* %d: %dx%d %s, %ld passes, %ld medium */\n", code, i + 1, chosen[i].p.n, chosen[i].p.n,
               GRADE[chosen[i].grade], chosen[i].passes, chosen[i].mediums);
    }
    printf("};\n");
    return nchosen == HTO_LEVELS ? 0 : 1;
}
