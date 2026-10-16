/*
 * Number Puzzles' levels, made and chosen: src/games/mathpz_levels.h. Our own levels: no published set is used.
 * 150 a game, in three chapters of 50, each rated by the rules' own solver (npz_rate) and kept in order of difficulty:
 *   Four numbers (levels 1-150): every set of four numbers 1..13 tried.
 *     1: target 24, numbers up to 10, many ways in +, - and x alone, the sums and products of four first (the free
 *        edition plays the first 40);
 *     2: target 24, numbers up to 13, fewer nice ways (a division often needed);
 *     3: other targets (10, 12, 36, 48...) with few nice ways, then every set whose only ways go through a fraction
 *        or below zero (1 5 5 5, 3 3 8 8...).
 *   Target (151-300): six numbers (one or two big ones of 25, 50, 75, 100 and small ones 1..10), a target of three
 *     digits made from them by a chain of operations drawn by chance; rated by the fewest operations reaching it
 *     exactly (chapter 4: two, 5: three, 6: four or five), then by how few ways do it in that many.
 *   All five (301-450): five numbers as a deck of 1..25 deals them, a target 1..25 (chapter 9: up to 50); rated by
 *     the nice ways (chapter 7: many without a division, 8: fewer, 9: few or none: fractions needed).
 * In each chapter the candidates are sorted by difficulty and 50 taken evenly over them, from the easiest; a set of
 * numbers and a target is never used twice. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/npzgen apps/boardgames/tools/mathpz_gen.c -lm && \
 *       /tmp/npzgen > apps/boardgames/src/games/mathpz_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define NPZ_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/mathpz_rules.c"

#define POOL 4000

typedef struct {
    int nums[NPZ_MAX], target;
    double diff;
    npz_rating r;
} cand;

static cand pool[POOL];
static int npool;
static int out_nums[NPZ_LEVELS][NPZ_MAX], out_target[NPZ_LEVELS], nout;
static npz_rating out_r[NPZ_LEVELS];

static int by_diff(const void *a, const void *b)
{
    double x = ((const cand *)a)->diff, y = ((const cand *)b)->diff;
    return x < y ? -1 : x > y;
}

static int by_num(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

static int used(const int *nums, int target)   /* nums: sorted, padded with zeros */
{
    for (int l = 0; l < nout; l++)
        if (out_target[l] == target && !memcmp(out_nums[l], nums, sizeof(int) * NPZ_MAX)) return 1;
    for (int i = 0; i < npool; i++)
        if (pool[i].target == target && !memcmp(pool[i].nums, nums, sizeof(int) * NPZ_MAX)) return 1;
    return 0;
}

static void add(int kind, const int *nums0, int target, double diff, const npz_rating *r)
{
    int n = npz_tiles(kind), nums[NPZ_MAX] = {0};
    memcpy(nums, nums0, sizeof(int) * (size_t)n);
    qsort(nums, (size_t)n, sizeof(int), by_num);
    if (npool >= POOL || used(nums, target)) return;
    cand *c = &pool[npool++];
    memcpy(c->nums, nums, sizeof nums);
    c->target = target;
    c->diff = diff;
    c->r = *r;
}

/* the chapter: `count` of the pool's candidates, sorted by difficulty, taken evenly from the easiest */
static void take(int count)
{
    qsort(pool, (size_t)npool, sizeof(cand), by_diff);
    int n = npool;
    if (n < count) {
        fprintf(stderr, "chapter at level %d: only %d candidates\n", nout + 1, n);
        exit(1);
    }
    for (int k = 0; k < count; k++) {
        const cand *c = &pool[count > 1 ? (int)((long)k * (n - 1) / (count - 1)) : 0];
        memcpy(out_nums[nout], c->nums, sizeof c->nums);
        out_target[nout] = c->target;
        out_r[nout] = c->r;
        nout++;
    }
    npool = 0;
}

/* the nice ways' difficulty: fewer ways, harder; a division needed, harder still */
static double nice_diff(const npz_rating *r)
{
    if (!r->nice) return 20 - log((double)r->ways);
    return 12 - log((double)r->nice) - 0.6 * log(1.0 + r->plain) + (r->plain ? 0 : 1.5);
}

static void four(void)
{
    static const int OTHER[] = {10, 12, 15, 18, 20, 30, 36, 40, 48, 60};
    npz_rating r;
    int q[NPZ_MAX] = {0};
    for (int pass = 0; pass < 3; pass++) {
        for (int a = 1; a <= 13; a++)
            for (int b = a; b <= 13; b++)
                for (int c = b; c <= 13; c++)
                    for (int d = c; d <= 13; d++) {
                        q[0] = a, q[1] = b, q[2] = c, q[3] = d;
                        if (pass == 0) {
                            if (d > 10) continue;
                            npz_rate(NPZ_FOUR, q, 24, &r);
                            /* the sums and the products of four first */
                            double easy = 2.5 * (a + b + c + d == 24) + 2.0 * (a * b * c * d == 24);
                            if (r.plain >= 10) add(NPZ_FOUR, q, 24, nice_diff(&r) - easy, &r);
                        } else if (pass == 1) {
                            npz_rate(NPZ_FOUR, q, 24, &r);
                            if (r.nice >= 1 && r.plain < 10 && r.nice <= 40) add(NPZ_FOUR, q, 24, nice_diff(&r), &r);
                        } else {
                            npz_rate(NPZ_FOUR, q, 24, &r);    /* the fraction ones: last, all of them */
                            if (r.ways && !r.nice) add(NPZ_FOUR, q, 24, 100 - log((double)r.ways), &r);
                            if ((a * 7 + b * 5 + c * 3 + d) % 3) continue;   /* a third of the sets, for the rest */
                            int t = OTHER[(a + b * 3 + c * 5 + d * 7) % 10];
                            npz_rate(NPZ_FOUR, q, t, &r);
                            if (r.nice >= 1 && r.nice <= 12 && r.plain <= 6)
                                add(NPZ_FOUR, q, t, nice_diff(&r), &r);
                        }
                    }
        if (pass < 2) {
            take(NPZ_PER_CHAPTER);
            continue;
        }
        /* chapter 3: 34 of the other targets evenly, then the fraction ones (sorted last by their diff) */
        int nfrac = 0;
        for (int i = 0; i < npool; i++) nfrac += pool[i].diff > 50;
        qsort(pool, (size_t)npool, sizeof(cand), by_diff);
        int others = NPZ_PER_CHAPTER - nfrac, n = npool - nfrac;
        for (int k = 0; k < others; k++) {
            const cand *c = &pool[(int)((long)k * (n - 1) / (others - 1))];
            memcpy(out_nums[nout], c->nums, sizeof c->nums);
            out_target[nout] = c->target;
            out_r[nout++] = c->r;
        }
        for (int i = n; i < npool; i++) {
            memcpy(out_nums[nout], pool[i].nums, sizeof pool[i].nums);
            out_target[nout] = pool[i].target;
            out_r[nout++] = pool[i].r;
        }
        npool = 0;
    }
}

static void target(void)
{
    for (int ch = 0; ch < 3; ch++) {
        uint32_t rs = pr_seed(9100u + (uint32_t)ch);
        for (int tries = 0; npool < 700 && tries < 40000; tries++) {
            int k = ch == 0 ? 2 : ch == 1 ? 3 : 4 + (tries & 1), nums[NPZ_MAX];
            target_nums(&rs, ch == 2 ? pr_below(&rs, 3) : 1 + pr_below(&rs, 2), nums);
            int t = target_build(&rs, nums, k);
            if (t < 101 || t > 999) continue;
            npz_rating r;
            npz_rate(NPZ_TARGET, nums, t, &r);
            if (r.fewest != k) continue;
            add(NPZ_TARGET, nums, t, 3.0 * k - log((double)r.ways_fewest) + (t % 10 ? 0.3 : 0), &r);
        }
        take(NPZ_PER_CHAPTER);
    }
}

static void five(void)
{
    for (int ch = 0; ch < 3; ch++) {
        uint32_t rs = pr_seed(9200u + (uint32_t)ch);
        int top = ch == 0 ? 12 : ch == 1 ? 17 : 25, ttop = ch == 2 ? 50 : 25;
        for (int tries = 0; npool < (ch == 2 ? 300 : 400) && tries < 30000; tries++) {
            int nums[NPZ_MAX] = {0};
            for (int i = 0; i < 5; i++) nums[i] = five_card(&rs, top);
            int t = ch == 2 && pr_below(&rs, 2) ? 26 + pr_below(&rs, 25) : five_card(&rs, ttop);
            npz_rating r;
            long nice = ways_in(NPZ_FIVE, nums, t, M_NICE);     /* the cheap count first */
            if (ch == 0 && nice < 300) continue;
            if (ch == 1 && (nice < 10 || nice > 300)) continue;
            if (ch == 2 && nice > 24) continue;
            npz_rate(NPZ_FIVE, nums, t, &r);
            if (!r.ways || (ch == 0 && r.plain < 300)) continue;
            add(NPZ_FIVE, nums, t, nice_diff(&r), &r);
        }
        take(NPZ_PER_CHAPTER);
    }
}

int main(void)
{
    four();
    target();
    five();
    if (nout != NPZ_LEVELS) {
        fprintf(stderr, "%d levels, not %d\n", nout, NPZ_LEVELS);
        return 1;
    }
    printf("/*\n * Number Puzzles' levels: our own, made by tools/mathpz_gen.c (rated by the rules' solver; 150 a game,"
           " in\n * chapters of 50 from the easiest). Each: its numbers, and its target in npz_lv_target; the comment: "
           "the ways\n * (all, nice, without a division), Target's fewest operations and their ways. The tests check "
           "them.\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("static const uint8_t npz_lv_nums[NPZ_LEVELS][NPZ_MAX] = {\n");
    for (int l = 0; l < NPZ_LEVELS; l++) {
        printf("    {");
        for (int i = 0; i < NPZ_MAX; i++) printf(i ? ", %d" : "%d", out_nums[l][i]);
        const npz_rating *r = &out_r[l];
        if (npz_level_kind(l) == NPZ_TARGET)
            printf("}, /* %d: %d, %d operations, %ld ways */\n", l + 1, out_target[l], r->fewest, r->ways_fewest);
        else
            printf("}, /* %d: %d, %ld ways, %ld nice, %ld plain */\n", l + 1, out_target[l], r->ways, r->nice,
                   r->plain);
    }
    printf("};\n\nstatic const uint16_t npz_lv_target[NPZ_LEVELS] = {");
    for (int l = 0; l < NPZ_LEVELS; l++) printf("%s%d,", l % 16 ? " " : "\n    ", out_target[l]);
    printf("\n};\n");
    return 0;
}
