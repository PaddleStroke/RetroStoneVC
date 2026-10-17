/*
 * Thermometers' levels, made by chance and chosen: src/games/thermometers_levels.h. Our own puzzles, from the game's
 * own maker (thermometers laid by chance, their levels then changed a little at a time until the solver solves the grid
 * at its level; thermometers_rules.c). Each candidate:
 *   - its taste: at most one row or column left empty, and few lone bulbs (thermometers of one cell);
 *   - its grade (bench.h: easy, medium, hard), checked to have one solution;
 *   - the work it asks of the solver: the passes of its reasoning (each a round of the ways allowed; a trial's count
 *     too), and how often the medium ways were needed;
 *   - near-duplicates dropped (the same thermometers and filling turned or mirrored).
 * Each chapter has its makes (a size, a level, how many, and the part of the make's candidates, ranked by that work,
 * they are picked from evenly): 5 x 5 at the easy level first, up to 10 x 10 at the hard level. Then each chapter
 * goes from its smallest and easiest to its biggest and hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/thmgen apps/boardgames/tools/thermometers_gen.c \
 *       apps/boardgames/src/games/thermometers_rules.c && /tmp/thmgen > apps/boardgames/src/games/thermometers_levels.h
 * "/tmp/thmgen survey N LEVEL": how the work spreads for that size and level (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/thermometers.h"

#define CANDS 90                                /* candidates made for each make */
#define BUDGET 200000L                          /* the solver's passes a candidate may cost */

typedef struct {
    thm_puzzle p;
    int grade, numbers;                         /* numbers: the thermometers */
    long passes, mediums;
    double score;
    char key[2 * THM_CELLS + 8];
} cand;

typedef struct { int n, level, count; float lo, hi; } make;
/* a size, a level, how many levels, the part of its candidates (from the least work 0 to the most 1) */
static const make CHAPTER[THM_CHAPTERS][3] = {
    {{5, BENCH_EASY, 10, 0, 0.8f}, {6, BENCH_EASY, 15, 0, 1}},
    {{6, BENCH_MEDIUM, 10, 0, 0.8f}, {7, BENCH_MEDIUM, 15, 0.1f, 1}},
    {{7, BENCH_HARD, 10, 0, 0.6f}, {8, BENCH_MEDIUM, 15, 0.2f, 1}},
    {{8, BENCH_HARD, 25, 0, 0.9f}},
    {{9, BENCH_MEDIUM, 10, 0.3f, 1}, {9, BENCH_HARD, 15, 0.1f, 1}},
    {{10, BENCH_HARD, 25, 0.1f, 1}},
};

/* the thermometers and their filling whichever way they lie: the smallest of their eight (turned, mirrored), each cell
 * the way its tube comes from (turned with it) and whether it is filled */
static void canon(const thm_puzzle *p, const uint8_t *sol, char *key)
{
    int n = p->n;
    char best[2 * THM_CELLS + 8], k[2 * THM_CELLS + 8];
    best[0] = 0;
    for (int t = 0; t < 8; t++) {
        for (int c = 0; c < n * n; c++) {
            int x = c % n, y = c / n, a = t & 1 ? n - 1 - x : x, b = t & 2 ? n - 1 - y : y;
            int tx = t & 4 ? b : a, ty = t & 4 ? a : b, d = 'o', pc = thm_prev(p, c);
            if (pc >= 0) {
                int px = pc % n, py = pc / n, pa = t & 1 ? n - 1 - px : px, pb = t & 2 ? n - 1 - py : py;
                int qx = t & 4 ? pb : pa, qy = t & 4 ? pa : pb;
                d = qy < ty ? 'u' : qy > ty ? 'd' : qx < tx ? 'l' : 'r';
            }
            k[2 * (ty * n + tx)] = (char)d;
            k[2 * (ty * n + tx) + 1] = (char)('0' + sol[c]);
        }
        k[2 * n * n] = 0;
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
static cand chosen[THM_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* one candidate from a seed; 1 when it is of the size and the level and has one solution */
static int candidate(cand *c, int n, int level, uint32_t seed)
{
    uint8_t sol[THM_CELLS];
    thm_make_ex(n, level, seed, BUDGET, &c->p, sol);
    if (c->p.n != n) return 0;
    /* the taste: a lone bulb (a thermometer of one cell) or an empty row or column now and then, not often */
    int lone = 0, empty = 0;
    for (int t = 0; t < c->p.thermos; t++) lone += c->p.len[t] == 1;
    for (int w = 0; w < 2; w++)
        for (int i = 0; i < n; i++) empty += c->p.count[w][i] == 0;
    if (lone > (n >= 9 ? 2 : 1) || empty > 1) return 0;
    c->grade = thm_effort(&c->p, &c->passes, &c->mediums);
    if (c->grade != level || thm_count(&c->p, 2) != 1) return 0;
    c->numbers = 0;
    c->numbers = c->p.thermos;
    /* the work: the passes, the medium finds weigh more; fewer thermometers, a little more */
    c->score = (double)c->passes + 4.0 * c->mediums - 0.2 * c->numbers;
    canon(&c->p, sol, c->key);
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
    printf("%dx%d level %d: %d candidates; work (passes, medium finds, thermometers) by tenths:\n", n, n, level, k);
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
    for (int ch = 0; ch < THM_CHAPTERS; ch++) {
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
           " * Thermometers' levels: our own, made by tools/thermometers_gen.c (the game's maker; each checked\n"
           " * to have one solution), six chapters of 25 from the easiest. Each: \"<n>:\", the counts, and a\n"
           " * character a cell, row by row (thermometers.h, thm_decode); in the comment its size, its grade and\n"
           " * the work it asks of the solver (its passes, the medium finds).\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const thm_levels[THM_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[THM_CELLS + 64];
        if (!thm_encode(&chosen[i].p, code, sizeof code)) return 1;
        printf("    \"%s\", /* %d: %dx%d %s, %ld passes, %ld medium */\n", code, i + 1, chosen[i].p.n, chosen[i].p.n,
               GRADE[chosen[i].grade], chosen[i].passes, chosen[i].mediums);
    }
    printf("};\n");
    return nchosen == THM_LEVELS ? 0 : 1;
}
