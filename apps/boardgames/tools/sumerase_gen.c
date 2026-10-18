/*
 * Sum Erase's levels, made by chance and chosen: src/games/sumerase_levels.h. Our own puzzles, from the game's own
 * maker (digits and the cells erased dealt by chance, then changed a little at a time until the solver solves the
 * grid at its level; sumerase_rules.c). Each candidate:
 *   - its grade (bench.h: easy, medium, hard), checked to have one solution by a plain search;
 *   - the work it asks of the solver: the passes of its reasoning, and how often the medium ways were needed;
 *   - near-duplicates dropped (the same grid turned or mirrored).
 * Each chapter has three makes (a size, the digits used, a level, how many, and the part of the make's candidates,
 * ranked by that work, they are picked from evenly): its opening (three 3 x 3 grids of small digits first, then two
 * of the chapter's easiest), then two makes of its real puzzles: 5 x 5 and 6 x 6 at the easy level in the first
 * chapter, the medium ways from the second, the hard ones from the third, up to 9 x 9. Then each chapter goes from its
 * smallest and easiest to its biggest and hardest. The per-level table and the chapters' summary (sizes, grades, the
 * solver's passes and medium finds, the trivial ones: easy and 4 x 4 or less) go to stderr (-DREPORT: the same for the
 * levels in sumerase_levels.h).
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
#define MAKES 3                                 /* a chapter: its opening (its easiest), then two makes */
#define E_ BENCH_EASY
#define M_ BENCH_MEDIUM
#define H_ BENCH_HARD
static const make CHAPTER[SME_CHAPTERS][MAKES] = {
    {{3, 5, E_, 3, 0, 0.2f}, {5, 9, E_, 10, 0.3f, 1}, {6, 9, E_, 12, 0.3f, 1}},
    {{5, 9, M_, 2, 0, 0.3f}, {6, 9, M_, 11, 0, 1}, {7, 9, M_, 12, 0, 0.8f}},
    {{7, 9, M_, 2, 0.3f, 0.6f}, {7, 9, M_, 11, 0.5f, 1}, {7, 9, H_, 12, 0, 0.7f}},
    {{8, 9, M_, 2, 0.3f, 0.6f}, {8, 9, M_, 11, 0.5f, 1}, {8, 9, H_, 12, 0, 0.8f}},
    {{8, 9, H_, 2, 0, 0.4f}, {9, 9, M_, 11, 0.5f, 1}, {9, 9, H_, 12, 0, 0.6f}},
    {{9, 9, H_, 2, 0.3f, 0.5f}, {9, 9, H_, 11, 0.4f, 0.8f}, {9, 9, H_, 12, 0.6f, 1}},
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

/* ---- the difficulty metrics: the size, the grade, the solver's passes (rounds of its reasoning over every line) and
 * its medium finds. Trivial: the easy ways on a grid of 4 x 4 or less, or in two passes. ------------------------- */
static int trivial(const cand *c) { return c->grade == BENCH_EASY && (c->p.n <= 4 || c->passes <= 2); }

static void table(const cand *c, int n)
{
    fprintf(stderr, "lvl size grade passes mediums\n");
    for (int i = 0; i < n; i++)
        fprintf(stderr, "%4d %4d %5d %6ld %7ld\n", i + 1, c[i].p.n, c[i].grade, c[i].passes, c[i].mediums);
    fprintf(stderr, "chapter: size mean (min..max), grades easy/medium/hard, passes mean, medium finds mean; trivial "
                    "(easy, and 4 x 4 or less or two passes)\n");
    for (int ch = 0; ch * 25 < n; ch++) {
        double s = 0, ps = 0, md = 0;
        int lo = 99, hi = 0, g[3] = {0}, t = 0, k = 0;
        for (int i = ch * 25; i < n && i < ch * 25 + 25; i++, k++) {
            s += c[i].p.n, ps += c[i].passes, md += c[i].mediums, g[c[i].grade < 3 ? c[i].grade : 2]++;
            t += trivial(&c[i]);
            lo = c[i].p.n < lo ? c[i].p.n : lo;
            hi = c[i].p.n > hi ? c[i].p.n : hi;
        }
        fprintf(stderr, "  %d: size %.1f (%d..%d)  grades %d/%d/%d  passes %.1f  mediums %.1f  trivial %d/%d\n", ch + 1,
                s / k, lo, hi, g[0], g[1], g[2], ps / k, md / k, t, k);
    }
}

#ifdef REPORT                                   /* -DREPORT: the metrics of the levels in sumerase_levels.h */
#include "games/sumerase_levels.h"
static int report(void)
{
    static cand lv[SME_LEVELS];
    for (int i = 0; i < SME_LEVELS; i++) {
        sme_decode(sme_levels[i], &lv[i].p, lv[i].sol);
        lv[i].grade = sme_effort(&lv[i].p, &lv[i].passes, &lv[i].mediums);
    }
    table(lv, SME_LEVELS);
    return 0;
}
#endif

static const char *const GRADE[] = {"easy", "medium", "hard"};

int main(int argc, char **argv)
{
#ifdef REPORT
    (void)argc, (void)argv;
    return report();
#endif
    if (argc == 5 && !strcmp(argv[1], "survey")) {
        survey(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
        return 0;
    }
    for (int ch = 0; ch < SME_CHAPTERS; ch++) {
        int first = nchosen;
        for (int p = 0; p < MAKES && CHAPTER[ch][p].count; p++) {
            const make *m = &CHAPTER[ch][p];
            int k = make_pool(m, 1000u * (uint32_t)(ch * MAKES + p + 1));
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
    table(chosen, nchosen);
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
