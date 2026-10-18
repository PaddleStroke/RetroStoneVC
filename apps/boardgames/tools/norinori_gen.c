/*
 * Norinori's levels, made by chance and chosen for their look: src/games/norinori_levels.h. Our own puzzles, from the
 * game's own maker (regions grown by chance, a shading found for them, unshaded cells passed between regions until
 * the solver finishes it at its level; norinori_rules.c). Each candidate:
 *   - its grade (bench.h: easy, medium, hard), checked to have one solution;
 *   - the work it asks of the solver: the passes of its reasoning, and the cells found by trials;
 *   - its look (taste): region sizes near each other, compact shapes (no snakes), regions answering each other
 *     across the centre (a half turn or a mirror);
 *   - near-duplicates dropped (the same regions turned or mirrored).
 * Each chapter has its makes (a size, a level, how many); a make's candidates are ranked by work, cut in as many
 * slices as picks, and each pick takes the best-looking of its slice; each chapter ends on its make's best-looking
 * hard tenth. Then each chapter goes from its smallest and easiest to its biggest and hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/nrngen apps/boardgames/tools/norinori_gen.c \
 *       apps/boardgames/src/games/norinori_rules.c && /tmp/nrngen > apps/boardgames/src/games/norinori_levels.h
 * "/tmp/nrngen survey N LEVEL": how many candidates come out and how the work spreads (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/norinori.h"

#define CANDS 40                                /* candidates made for each make */
#define BUDGET 200000L                          /* the solver's passes a candidate may cost */
#define KEY (NRN_CELLS + 8)

typedef struct {
    nrn_puzzle p;
    int grade;
    long passes, mediums;
    double score, taste;
    char key[KEY];
} cand;

typedef struct { int n, level, count; } make;
static const make CHAPTER[NRN_CHAPTERS][2] = {
    {{6, BENCH_EASY, 25}},
    {{6, BENCH_MEDIUM, 10}, {7, BENCH_EASY, 15}},
    {{7, BENCH_MEDIUM, 10}, {8, BENCH_EASY, 15}},
    {{8, BENCH_MEDIUM, 15}, {8, BENCH_HARD, 10}},
    {{9, BENCH_MEDIUM, 12}, {9, BENCH_HARD, 13}},
    {{10, BENCH_MEDIUM, 10}, {10, BENCH_HARD, 15}},
};

/* the regions read after a turn or a mirror (t: 0..7) */
static void turned(const nrn_puzzle *p, int t, uint8_t *out)
{
    int n = p->n;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int sx = t & 1 ? n - 1 - x : x, sy = t & 2 ? n - 1 - y : y;
            if (t & 4) { int s = sx; sx = sy; sy = s; }
            out[y * n + x] = p->region[sy * n + sx];
        }
}

/* the regions whichever way they lie: the smallest code of their eight readings */
static void canon(const nrn_puzzle *p, char *key)
{
    key[0] = 0;
    for (int t = 0; t < 8; t++) {
        nrn_puzzle q = *p;
        turned(p, t, q.region);
        int name[256], next = 0;
        memset(name, 0xff, sizeof name);
        for (int c = 0; c < p->n * p->n; c++) {
            if (name[q.region[c]] < 0) name[q.region[c]] = next++;
            q.region[c] = (uint8_t)name[q.region[c]];
        }
        char k[KEY];
        nrn_encode(&q, k, sizeof k);
        if (!key[0] || strcmp(k, key) < 0) strcpy(key, k);
    }
}

/* the walls of a reading: bit 0 the wall right of a cell, bit 1 under it */
static void walls(int n, const uint8_t *reg, uint8_t *w)
{
    for (int c = 0; c < n * n; c++)
        w[c] = (uint8_t)((c % n < n - 1 && reg[c] != reg[c + 1]) | (c / n < n - 1 && reg[c] != reg[c + n]) << 1);
}

/* the look, lower the better: sizes near each other, compact shapes, the walls answering each other across the centre
 * (the best of a half turn and the two mirrors: the part of walls with no partner) */
static double taste(const nrn_puzzle *p)
{
    int n = p->n, size[64] = {0}, edge[64] = {0}, R = p->regions;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int r = p->region[y * n + x];
            size[r]++;
            edge[r] += (x == 0 || p->region[y * n + x - 1] != r) + (x == n - 1 || p->region[y * n + x + 1] != r) +
                       (y == 0 || p->region[(y - 1) * n + x] != r) + (y == n - 1 || p->region[(y + 1) * n + x] != r);
        }
    double mean = (double)n * n / R, spread = 0, shape = 0;
    for (int r = 0; r < R; r++) {
        double d = (size[r] - mean) / mean;
        spread += d * d;
        shape += (double)edge[r] * edge[r] / (16.0 * size[r]);
    }
    uint8_t w0[NRN_CELLS], w1[NRN_CELLS], reg[NRN_CELLS];
    walls(n, p->region, w0);
    double asym = 1;
    const int T[3] = {3, 1, 2};                 /* a half turn, a mirror left to right, top to bottom */
    for (int i = 0; i < 3; i++) {
        turned(p, T[i], reg);
        walls(n, reg, w1);
        int all = 0, odd = 0;
        for (int c = 0; c < n * n; c++) {
            all += __builtin_popcount(w0[c]);
            odd += __builtin_popcount(w0[c] ^ w1[c]);
        }
        double a = all ? odd / (2.0 * all) : 0;
        if (a < asym) asym = a;
    }
    return 2.0 * spread / R + 1.5 * (shape / R - 1) + 3.0 * asym;
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[NRN_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

static int candidate(cand *c, int n, int level, uint32_t seed)
{
    uint8_t sol[NRN_CELLS];
    nrn_make_ex(n, level, seed, BUDGET, &c->p, sol);
    if (c->p.n != n) return 0;
    c->grade = nrn_effort(&c->p, &c->passes, &c->mediums);
    if (c->grade != level || nrn_count(&c->p, 2) != 1) return 0;
    c->score = (double)c->passes + 4.0 * c->mediums + 0.01 * (seed % 7);
    c->taste = taste(&c->p);
    canon(&c->p, c->key);
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
    for (int ch = 0; ch < NRN_CHAPTERS; ch++) {
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
           " * Norinori's levels: our own, made by tools/norinori_gen.c (the game's maker; each checked to have one\n"
           " * solution), six chapters of 25 from the easiest. Each: \"<n>:\" and a character a cell, its region\n"
           " * (norinori.h, nrn_decode); in the comment its size, its grade and the work it asks of the solver (its\n"
           " * passes, the cells found by trials).\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const nrn_levels[NRN_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[KEY];
        if (!nrn_encode(&chosen[i].p, code, sizeof code)) return 1;
        printf("    \"%s\", /* %d: %dx%d %s, %ld passes, %ld by trials */\n", code, i + 1, chosen[i].p.n, chosen[i].p.n,
               GRADE[chosen[i].grade], chosen[i].passes, chosen[i].mediums);
    }
    printf("};\n");
    return nchosen == NRN_LEVELS ? 0 : 1;
}
