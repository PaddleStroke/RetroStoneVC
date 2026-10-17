/*
 * Light Up's levels, made by chance and chosen: src/games/lightup_levels.h. Our own puzzles, from the game's own maker
 * (walls by chance, turned half round, lamps where it is still dark, numbers taken away while the solver still
 * solves it within its level; lightup_rules.c). Each candidate:
 *   - its grade (bench.h: easy, medium, hard), checked to have one solution;
 *   - the work it asks of the solver: the passes of its reasoning (each a round of the ways allowed; a trial's count
 *     too), and how often the medium way was needed;
 *   - near-duplicates dropped (the same walls turned or mirrored).
 * Taste, so the grids look made rather than drawn: the walls already lie turned half round; the numbers are made to
 * follow (a numbered wall's twin numbered too: the missing numbers put back, or else the lone ones taken away, when
 * the grid keeps its one solution and its grade). The look counts too: numbers in twins, no lumps of walls (2 x 2),
 * the walls spread over the four quarters, and the numbers neither too few nor too many. Each pick takes, among the
 * few candidates nearest its place in the ranking, the one that looks best.
 * Each chapter has its makes (a size, a level, how many, and the part of the make's candidates, ranked by that work,
 * they are picked from evenly): 6 x 6 and 7 x 7 at the easy level first, up to 14 x 14 at the hard level. Then each
 * chapter goes from its easiest to its hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/lugen apps/boardgames/tools/lightup_gen.c \
 *       apps/boardgames/src/games/lightup_rules.c && /tmp/lugen > apps/boardgames/src/games/lightup_levels.h
 * "/tmp/lugen survey N LEVEL": how the work spreads for that size and level (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/lightup.h"

#define CANDS 150                               /* candidates made for each make */

typedef struct {
    lu_puzzle p;
    int grade, numbers, twins;
    double look;
    long passes, mediums;
    double score;
    char key[LU_CELLS + 8];
} cand;

typedef struct { int n, level, count; float lo, hi; } make;
/* a size, a level, how many levels, the part of its candidates (from the least work 0 to the most 1) */
static const make CHAPTER[LU_CHAPTERS][3] = {
    {{6, BENCH_EASY, 12, 0, 0.6f}, {7, BENCH_EASY, 13, 0, 0.7f}},
    {{8, BENCH_EASY, 10, 0.2f, 1}, {8, BENCH_MEDIUM, 15, 0, 0.6f}},
    {{9, BENCH_MEDIUM, 12, 0.2f, 1}, {10, BENCH_MEDIUM, 13, 0.2f, 1}},
    {{10, BENCH_HARD, 25, 0, 0.6f}},
    {{12, BENCH_HARD, 25, 0.1f, 0.9f}},
    {{14, BENCH_HARD, 25, 0.2f, 1}},
};

/* the walls whichever way they lie: the smallest of their eight (turned, mirrored) */
static void canon(const lu_puzzle *p, char *key)
{
    int n = p->n;
    char best[LU_CELLS + 8], k[LU_CELLS + 8];
    best[0] = 0;
    for (int t = 0; t < 8; t++) {
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int a = t & 1 ? n - 1 - x : x, b = t & 2 ? n - 1 - y : y, sx = t & 4 ? b : a, sy = t & 4 ? a : b;
                int w = p->wall[sy * n + sx];
                k[y * n + x] = w == LU_WHITE ? '.' : w == LU_WALL ? '#' : (char)('0' + w);
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
static cand chosen[LU_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* the number a wall has in the solution: the lamps beside it */
static int lamps_by(int n, const uint8_t *sol, int c)
{
    int x = c % n, y = c / n;
    return (x > 0 && sol[c - 1]) + (x < n - 1 && sol[c + 1]) + (y > 0 && sol[c - n]) + (y < n - 1 && sol[c + n]);
}

/* the numbered walls whose twin (turned half round) is a wall without a number */
static int lone(const lu_puzzle *p)
{
    int n = p->n, k = 0;
    for (int c = 0; c < n * n; c++)
        if (p->wall[c] >= 0 && p->wall[n * n - 1 - c] == LU_WALL) k++;
    return k;
}

/* the numbers in twins: put the missing ones back, or else take the lone ones away, when the grid keeps its grade
 * and its one solution */
static void twin_numbers(lu_puzzle *p, const uint8_t *sol, int level)
{
    int n = p->n, N = n * n;
    if (!lone(p)) return;
    lu_puzzle q = *p;
    long a, b;
    for (int c = 0; c < N; c++)
        if (q.wall[c] == LU_WALL && p->wall[N - 1 - c] >= 0) q.wall[c] = (int8_t)lamps_by(n, sol, c);
    if (lu_effort(&q, &a, &b) == level && lu_count(&q, 2) == 1) {
        *p = q;
        return;
    }
    q = *p;
    for (int c = 0; c < N; c++)
        if (q.wall[c] >= 0 && p->wall[N - 1 - c] == LU_WALL) q.wall[c] = LU_WALL;
    if (lu_effort(&q, &a, &b) == level && lu_count(&q, 2) == 1) *p = q;
}

/* how good it looks (more is better): numbers in twins, no 2 x 2 lumps of walls, walls in all four quarters, a
 * fair share of numbered walls */
static double looks(const lu_puzzle *p)
{
    int n = p->n, walls = 0, nums = 0, lumps = 0, q[4] = {0};
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int c = x + y * n;
            if (p->wall[c] == LU_WHITE) continue;
            walls++, nums += p->wall[c] >= 0;
            if (2 * x != n - 1 && 2 * y != n - 1) q[(2 * x > n - 1) + 2 * (2 * y > n - 1)]++;
            if (x < n - 1 && y < n - 1 && p->wall[c + 1] != LU_WHITE && p->wall[c + n] != LU_WHITE &&
                p->wall[c + n + 1] != LU_WHITE)
                lumps++;
        }
    int lo = q[0], hi = q[0];
    for (int i = 1; i < 4; i++) lo = q[i] < lo ? q[i] : lo, hi = q[i] > hi ? q[i] : hi;
    double share = walls ? (double)nums / walls : 0;
    return -2.0 * lone(p) - 3.0 * lumps - 1.0 * (hi - lo) - 6.0 * (share < 0.35 ? 0.35 - share : 0) -
           6.0 * (share > 0.8 ? share - 0.8 : 0);
}

/* one candidate from a seed; 1 when it is of the level and has one solution */
static int candidate(cand *c, int n, int level, uint32_t seed)
{
    uint8_t sol[LU_CELLS];
    lu_make(n, level, seed, &c->p, sol);
    if (lu_count(&c->p, 2) == 1) twin_numbers(&c->p, sol, level);
    c->grade = lu_effort(&c->p, &c->passes, &c->mediums);
    if (c->grade != level || lu_count(&c->p, 2) != 1) return 0;
    c->numbers = 0;
    for (int i = 0; i < n * n; i++) c->numbers += c->p.wall[i] >= 0;
    /* the work: the passes, the medium finds weigh more; fewer numbers given ask a little more */
    c->score = (double)c->passes + 4.0 * c->mediums - 0.2 * c->numbers;
    c->twins = !lone(&c->p);
    c->look = looks(&c->p);
    canon(&c->p, c->key);
    return 1;
}

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 40 * CANDS; seed++) {
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
    for (int ch = 0; ch < LU_CHAPTERS; ch++) {
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
                /* the best looking of the few nearest that place in the ranking */
                int span = k / (2 * m->count) > 1 ? k / (2 * m->count) : 1, best = at;
                for (int j = at - span; j <= at + span; j++)
                    if (j >= 0 && j < k && !seen(pool[j].key) && pool[j].look > pool[best].look) best = j;
                chosen[nchosen++] = pool[best];
            }
        }
        for (int i = first + 1; i < nchosen; i++)   /* the chapter from its easiest to its hardest */
            for (int j = i; j > first; j--) {
                cand *a = &chosen[j - 1], *b = &chosen[j];
                if (a->grade * 1e6 + a->p.n * 1e4 + a->score <= b->grade * 1e6 + b->p.n * 1e4 + b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
    printf("/*\n"
           " * Light Up's levels: our own, made by tools/lightup_gen.c (the game's maker; each checked to have one\n"
           " * solution), six chapters of 25 from the easiest. Each: \"<n>:\" and a character a cell, row by row\n"
           " * (lightup.h, lu_decode); in the comment its size, its grade and the work it asks of the solver (its\n"
           " * passes, the medium finds).\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const lu_levels[LU_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[LU_CELLS + 8];
        if (!lu_encode(&chosen[i].p, code, sizeof code)) return 1;
        printf("    \"%s\", /* %d: %dx%d %s, %ld passes, %ld medium%s */\n", code, i + 1, chosen[i].p.n,
               chosen[i].p.n, GRADE[chosen[i].grade], chosen[i].passes, chosen[i].mediums,
               chosen[i].twins ? "" : ", lone numbers");
    }
    printf("};\n");
    return nchosen == LU_LEVELS ? 0 : 1;
}
