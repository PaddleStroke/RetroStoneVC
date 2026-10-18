/*
 * Yin-Yang's levels, made by chance and chosen for their look: src/games/yinyang_levels.h. Our own puzzles, from the
 * game's own maker (an ebony group grown by chance, every stone given, then stones taken away while the solver still
 * finishes it at its level; yinyang_rules.c). Each candidate:
 *   - its grade (bench.h: easy, medium, hard), checked to have one solution;
 *   - the work it asks of the solver: the passes of its reasoning, and the cells found by trials;
 *   - its look (taste): given stones about symmetric, spread over the quarters, both colours alike, not too many;
 *   - near-duplicates dropped (the same stones turned or mirrored).
 * Each chapter has its makes (a size, a level, how many); a make's candidates are ranked by work, cut in as many
 * slices as picks, and each pick takes the best-looking of its slice; each chapter ends on its make's best-looking
 * hard tenth. Then each chapter goes from its smallest and easiest to its biggest and hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/yyggen apps/boardgames/tools/yinyang_gen.c \
 *       apps/boardgames/src/games/yinyang_rules.c && /tmp/yyggen > apps/boardgames/src/games/yinyang_levels.h
 * "/tmp/yyggen survey N LEVEL": how many candidates come out and how the work spreads (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "games/yinyang.h"

#define CANDS 100                               /* candidates made for each make */
#define BUDGET 200000L                          /* the solver's passes a candidate may cost */
#define KEY (YYG_CELLS + 8)

typedef struct {
    yyg_puzzle p;
    uint8_t sol[YYG_CELLS];
    int grade;
    long passes, mediums;
    double score, taste;
    char key[KEY];
} cand;

typedef struct { int n, level, count; } make;
static const make CHAPTER[YYG_CHAPTERS][2] = {
    {{6, BENCH_EASY, 25}},
    {{6, BENCH_MEDIUM, 10}, {7, BENCH_EASY, 15}},
    {{7, BENCH_MEDIUM, 10}, {8, BENCH_EASY, 15}},
    {{8, BENCH_MEDIUM, 15}, {8, BENCH_HARD, 10}},
    {{9, BENCH_MEDIUM, 12}, {9, BENCH_HARD, 13}},
    {{10, BENCH_MEDIUM, 10}, {10, BENCH_HARD, 15}},
};

/* the stones whichever way they lie: the smallest of its eight readings (a character a cell) */
static void canon(int n, const uint8_t *sol, char *key)
{
    key[0] = 0;
    for (int t = 0; t < 8; t++) {
        char k[KEY];
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int sx = t & 1 ? n - 1 - x : x, sy = t & 2 ? n - 1 - y : y;
                if (t & 4) { int s = sx; sx = sy; sy = s; }
                k[y * n + x] = (char)('0' + sol[sy * n + sx]);
            }
        k[n * n] = 0;
        if (!key[0] || strcmp(k, key) < 0) strcpy(key, k);
    }
}

/* the look, lower the better: the given stones placed about symmetrically (a half turn or a mirror: the part with
 * no partner), spread over the four quarters alike, both colours about as many, not too many */
static double taste(const yyg_puzzle *p, const uint8_t *sol)
{
    (void)sol;
    int n = p->n, quarter[4] = {0}, all = 0, ivory = 0;
    for (int c = 0; c < n * n; c++) {
        if (!p->given[c]) continue;
        int x = c % n, y = c / n;
        all++;
        ivory += p->given[c] == YYG_IVORY;
        quarter[(2 * x >= n) + 2 * (2 * y >= n)]++;
    }
    double asym = 1;
    for (int t = 0; t < 3; t++) {               /* a half turn, a mirror left to right, top to bottom */
        int odd = 0;
        for (int c = 0; c < n * n; c++) {
            int x = c % n, y = c / n, mx = t == 2 ? x : n - 1 - x, my = t == 1 ? y : n - 1 - y;
            odd += !p->given[c] != !p->given[my * n + mx];
        }
        double a = all ? odd / (2.0 * all) : 0;
        if (a < asym) asym = a;
    }
    double spread = 0, mean = all / 4.0, colours = all ? fabs(2.0 * ivory - all) / all : 0;
    for (int q = 0; q < 4; q++) spread += (quarter[q] - mean) * (quarter[q] - mean);
    return 3.0 * asym + spread / (mean * mean + 1) + 0.04 * all + colours;
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[YYG_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

static int candidate(cand *c, int n, int level, uint32_t seed)
{
    yyg_make_ex(n, level, seed, BUDGET, &c->p, c->sol);
    if (c->p.n != n) return 0;
    c->grade = yyg_effort(&c->p, &c->passes, &c->mediums);
    if (c->grade != level || yyg_count(&c->p, 2) != 1) return 0;
    c->score = (double)c->passes + 4.0 * c->mediums + 0.01 * (seed % 7);
    c->taste = taste(&c->p, c->sol);
    canon(n, c->sol, c->key);
    return 1;
}

static int make_pool(const make *m, uint32_t seed0, int want)
{
    int k = 0;
    for (uint32_t seed = seed0; k < want && seed < seed0 + 12 * (uint32_t)want; seed++) {
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
    make m = {n, level, 0};
    clock_t t0 = clock();
    int k = make_pool(&m, 7, 12);
    printf("%dx%d level %d: %d candidates of 12 wanted in %.1f s; work (passes, trial finds, taste) by tenths:\n", n,
           n, level, k, (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int i = 0; i < 10 && k; i++) {
        const cand *c = &pool[i * (k - 1) / 9];
        printf("  %ld %ld %.2f\n", c->passes, c->mediums, c->taste);
    }
}

static const char *const GRADE[] = {"easy", "medium", "hard"};

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "survey")) {
        survey(atoi(argv[2]), atoi(argv[3]));
        return 0;
    }
    for (int ch = 0; ch < YYG_CHAPTERS; ch++) {
        int first = nchosen;
        for (int mi = 0; mi < 2 && CHAPTER[ch][mi].count; mi++) {
            const make *m = &CHAPTER[ch][mi];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 2 + mi + 1), CANDS);
            fprintf(stderr, "chapter %d, make %d (%dx%d %s): %d candidates\n", ch + 1, mi + 1, m->n, m->n,
                    GRADE[m->level], k);
            for (int i = 0; i < m->count; i++) {
                int at = -1, a = i * (k - 1) / m->count, b = (i + 1) * (k - 1) / m->count;
                if (i == m->count - 1 && mi == 1) a = (k - 1) * 9 / 10;   /* the showpiece */
                for (int j = a; j <= b && j < k; j++)
                    if (!seen(pool[j].key) && (at < 0 || pool[j].taste < pool[at].taste)) at = j;
                for (int j = 0; at < 0 && j < k; j++)
                    if (!seen(pool[j].key)) at = j;
                if (at < 0) {
                    fprintf(stderr, "chapter %d: not enough of make %d\n", ch + 1, mi + 1);
                    return 1;
                }
                chosen[nchosen++] = pool[at];
            }
        }
        for (int i = first + 1; i < nchosen; i++)   /* the chapter from its easiest to its hardest */
            for (int j = i; j > first; j--) {
                cand *a = &chosen[j - 1], *b = &chosen[j];
                if (a->p.n * 1e7 + a->grade * 1e6 + a->score <= b->p.n * 1e7 + b->grade * 1e6 + b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
    printf("/*\n"
           " * Yin-Yang's levels: our own, made by tools/yinyang_gen.c (the game's maker; each checked to have one\n"
           " * solution), six chapters of 25 from the easiest. Each: \"<n>:\", a character a cell row by row (\".\"\n"
           " * none, \"o\" ivory, \"x\" ebony: yinyang.h, yyg_decode); in the comment its size, its grade and the\n"
           " * work it asks of the solver (its passes, the cells found by trials).\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const yyg_levels[YYG_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[512];
        if (!yyg_encode(&chosen[i].p, code, sizeof code)) return 1;
        printf("    \"%s\", /* %d: %dx%d %s, %ld passes, %ld by trials */\n", code, i + 1, chosen[i].p.n, chosen[i].p.n,
               GRADE[chosen[i].grade], chosen[i].passes, chosen[i].mediums);
    }
    printf("};\n");
    return nchosen == YYG_LEVELS ? 0 : 1;
}
