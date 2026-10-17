/*
 * Binairo's levels, made by chance and chosen: src/games/binairo_levels.h. Our own puzzles, from the game's own maker
 * (a full grid by chance, discs taken away while the solver still solves it within its level; binairo_rules.c). Each
 * candidate:
 *   - its grade (bench.h: easy, medium, hard), checked to have one solution;
 *   - the work it asks of the solver: the passes of its reasoning (each a round of the ways allowed; a trial's count
 *     too), and how often the medium way was needed;
 *   - near-duplicates dropped (the same discs turned or mirrored, or with the colours swapped);
 *   - set as by hand: the given discs symmetric (turned half round, one in four mirrored left to right), the four
 *     quarters of the board given about as many (none bare, none crowded).
 * Each chapter has its makes (a size, a level, how many, and the part of the make's candidates, ranked by that work,
 * they are picked from evenly): 6 x 6 at the easy level first, up to 12 x 12 at the hard level. Then each chapter
 * goes from its easiest to its hardest. Its last level is a showpiece: the discs turned a quarter
 * round, among the hardest of its kind.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/bngen apps/boardgames/tools/binairo_gen.c \
 *       apps/boardgames/src/games/binairo_rules.c && /tmp/bngen > apps/boardgames/src/games/binairo_levels.h
 * "/tmp/bngen survey N LEVEL": how the work spreads for that size and level (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/binairo.h"

#define CANDS 100                               /* candidates made for each make */

typedef struct {
    bn_puzzle p;
    int grade, numbers;                         /* numbers: the discs given */
    long passes, mediums;
    double score;
    char key[BN_CELLS + 8];
} cand;

typedef struct { int n, level, count; float lo, hi; } make;
/* a size, a level, how many levels, the part of its candidates (from the least work 0 to the most 1) */
static const make CHAPTER[BN_CHAPTERS][3] = {
    {{6, BENCH_EASY, 25, 0, 0.9f}},
    {{8, BENCH_EASY, 25, 0.05f, 1}},
    {{8, BENCH_MEDIUM, 25, 0, 0.9f}},
    {{10, BENCH_MEDIUM, 25, 0.1f, 1}},
    {{10, BENCH_HARD, 25, 0, 0.75f}},
    {{12, BENCH_HARD, 25, 0.1f, 0.95f}},
};

/* the discs whichever way they lie: the smallest of their sixteen (turned, mirrored, the colours swapped) */
static void canon(const bn_puzzle *p, char *key)
{
    int n = p->n;
    char best[BN_CELLS + 8], k[BN_CELLS + 8];
    best[0] = 0;
    for (int t = 0; t < 16; t++) {
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int a = t & 1 ? n - 1 - x : x, b = t & 2 ? n - 1 - y : y, sx = t & 4 ? b : a, sy = t & 4 ? a : b;
                int d = p->given[sy * n + sx];
                if (d && t & 8) d = 3 - d;
                k[y * n + x] = ".01"[d % 3];
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
static cand chosen[BN_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* the cells a given disc brings along, as a person setting the puzzle by hand would: SYM_HALF turned half round
 * (most levels), SYM_MIRROR mirrored left to right (a few), SYM_QUARTER turned a quarter round (the showpieces) */
enum { SYM_HALF, SYM_MIRROR, SYM_QUARTER };

static int orbit(int n, int sym, int c, int *out)
{
    int x = c % n, y = c / n, k = 0, pts[4][2] = {{x, y}, {n - 1 - x, n - 1 - y}, {n - 1 - y, x}, {y, n - 1 - x}};
    if (sym == SYM_MIRROR) pts[1][0] = n - 1 - x, pts[1][1] = y;
    for (int i = 0; i < (sym == SYM_QUARTER ? 4 : 2); i++) {
        int d = pts[i][1] * n + pts[i][0], dup = 0;
        for (int j = 0; j < k; j++) dup |= out[j] == d;
        if (!dup) out[k++] = d;
    }
    return k;
}

/* the maker's puzzle made symmetric: every given disc's partners given too (its full grid has them), then the
 * partners taken away together while the solver still solves it within its level (the easy ways first, as bench.h) */
static void symmetric(bn_puzzle *p, const uint8_t *sol, int level, int sym, uint32_t seed)
{
    int n = p->n, cells = n * n, part[4], order[BN_CELLS], k = 0;
    for (int c = 0; c < cells; c++)
        if (p->given[c])
            for (int i = 0, m = orbit(n, sym, c, part); i < m; i++) p->given[part[i]] = sol[part[i]];
    for (int c = 0; c < cells; c++) {           /* each orbit once, by its smallest cell */
        int m = orbit(n, sym, c, part), low = 1;
        for (int i = 0; i < m; i++) low &= part[i] >= c;
        if (low && p->given[c]) order[k++] = c;
    }
    for (int i = k - 1; i > 0; i--) {           /* in an order by chance (a fixed one: the seed's) */
        seed = seed * 1103515245u + 12345u;
        int j = (int)((seed >> 8) % (uint32_t)(i + 1)), t = order[i];
        order[i] = order[j];
        order[j] = t;
    }
    for (int cap = BENCH_EASY; cap <= level; cap = cap < level ? level : cap + 1)
        for (int i = 0; i < k; i++) {
            int m = orbit(n, sym, order[i], part);
            if (!p->given[order[i]]) continue;
            for (int j = 0; j < m; j++) p->given[part[j]] = BN_EMPTY;
            int g = bn_grade(p, NULL);
            if (g < BENCH_EASY || g > cap)
                for (int j = 0; j < m; j++) p->given[part[j]] = sol[part[j]];
        }
}

/* the givens' balance: 0 when the four quarters of the board hold as many, up to 1 when one holds them all */
static double lopsided(const bn_puzzle *p)
{
    int n = p->n, q[4] = {0, 0, 0, 0}, all = 0, most = 0;
    for (int c = 0; c < n * n; c++)
        if (p->given[c]) q[(c % n >= n / 2) + 2 * (c / n >= n / 2)]++, all++;
    for (int i = 0; i < 4; i++) most = q[i] > most ? q[i] : most;
    return all ? (most - all / 4.0) / (all * 0.75) : 1;
}

/* one candidate from a seed; 1 when it is of the level and has one solution */
static int candidate(cand *c, int n, int level, uint32_t seed, int sym)
{
    uint8_t sol[BN_CELLS];
    bn_make(n, level, seed, &c->p, sol);
    symmetric(&c->p, sol, level, sym, seed);
    if (lopsided(&c->p) > 0.2) return 0;        /* a corner left bare, or crowded */
    c->grade = bn_effort(&c->p, &c->passes, &c->mediums);
    if (c->grade != level || bn_count(&c->p, 2) != 1) return 0;
    c->numbers = 0;
    for (int i = 0; i < n * n; i++) c->numbers += c->p.given[i] != BN_EMPTY;
    /* the work: the passes, the medium finds weigh more; fewer discs given ask a little more */
    c->score = (double)c->passes + 4.0 * c->mediums - 0.2 * c->numbers;
    canon(&c->p, c->key);
    return 1;
}

/* sym -1: half turns, one in four mirrored (a mix); else that symmetry */
static int make_pool(const make *m, uint32_t seed0, int sym, int want)
{
    int k = 0;
    for (uint32_t seed = seed0; k < want && seed < seed0 + 40 * CANDS; seed++) {
        cand *c = &pool[k];
        if (!candidate(c, m->n, m->level, seed, sym >= 0 ? sym : seed % 4 ? SYM_HALF : SYM_MIRROR)) continue;
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
    int k = make_pool(&m, 7, -1, CANDS);
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
    for (int ch = 0; ch < BN_CHAPTERS; ch++) {
        int first = nchosen;
        for (int p = 0; p < 3 && CHAPTER[ch][p].count; p++) {
            const make *m = &CHAPTER[ch][p];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 3 + p + 1), -1, CANDS);
            fprintf(stderr, "chapter %d, make %d (%dx%d %s): %d candidates\n", ch + 1, p + 1, m->n, m->n,
                    GRADE[m->level], k);
            int count = m->count - (p == 0);    /* the last level of a chapter: a showpiece, below */
            for (int i = 0; i < count; i++) {
                int at = (int)((m->lo + (m->hi - m->lo) * (i + 0.5f) / count) * (k - 1));
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
                if (a->grade * 1e6 + a->p.n * 1e4 + a->score <= b->grade * 1e6 + b->p.n * 1e4 + b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
        /* the chapter's showpiece, last: its last make's size and level, the discs turned a quarter round, among
         * the hardest of its kind */
        const make *m = &CHAPTER[ch][0];
        for (int p = 1; p < 3 && CHAPTER[ch][p].count; p++) m = &CHAPTER[ch][p];
        int k = make_pool(m, 500000u + 1000u * (uint32_t)ch, SYM_QUARTER, CANDS / 4), at = k * 3 / 4;
        fprintf(stderr, "chapter %d, showpiece: %d candidates\n", ch + 1, k);
        while (at < k && seen(pool[at].key)) at++;
        if (at >= k) {
            fprintf(stderr, "chapter %d: no showpiece\n", ch + 1);
            return 1;
        }
        chosen[nchosen++] = pool[at];
    }
    printf("/*\n"
           " * Binairo's levels: our own, made by tools/binairo_gen.c (the game's maker; each checked to have one\n"
           " * solution), six chapters of 25 from the easiest. Each: \"<n>:\" and a character a cell, row by row\n"
           " * (binairo.h, bn_decode); in the comment its size, its grade and the work it asks of the solver (its\n"
           " * passes, the medium finds).\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const bn_levels[BN_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[BN_CELLS + 8];
        if (!bn_encode(&chosen[i].p, code, sizeof code)) return 1;
        printf("    \"%s\", /* %d: %dx%d %s, %ld passes, %ld medium */\n", code, i + 1, chosen[i].p.n, chosen[i].p.n,
               GRADE[chosen[i].grade], chosen[i].passes, chosen[i].mediums);
    }
    printf("};\n");
    return nchosen == BN_LEVELS ? 0 : 1;
}
