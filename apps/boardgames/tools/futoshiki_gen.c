/*
 * Futoshiki's levels, made by chance and chosen: src/games/futoshiki_levels.h. Our own puzzles, from the game's own
 * maker (a Latin square by chance, every digit and sign given, then taken away while the solution stays the only one
 * and the grader still solves it within its level; futoshiki_rules.c). Each candidate:
 *   - its grade (fsk_rate: easy, medium, hard), checked to have one solution;
 *   - the work it asks of the grader: its steps (each the easiest way that works), and the harder ones (a medium
 *     step counts one, a hard one three);
 *   - near-duplicates dropped (the same puzzle turned or mirrored, or with every digit d made n + 1 - d).
 * Each chapter has its makes (a size, a level, how many, and the part of the make's candidates, ranked by that work,
 * they are picked from evenly): 4 x 4 at the easy level first, up to 7 x 7 at the hard level. Then each chapter goes
 * from its easiest to its hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/fskgen apps/boardgames/tools/futoshiki_gen.c \
 *       apps/boardgames/src/games/futoshiki_rules.c && /tmp/fskgen > apps/boardgames/src/games/futoshiki_levels.h
 * "/tmp/fskgen survey N LEVEL": how the work spreads for that size and level (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/futoshiki.h"

#define CANDS 150                               /* candidates made for each make */

typedef struct {
    fsk_puz p;
    int grade, numbers;                         /* numbers: the clues (digits and signs) */
    long passes, mediums;                       /* the grader's steps, the harder ones */
    double score;
    char key[3 * FSK_CELLS + 8];
} cand;

typedef struct { int n, level, count; float lo, hi; } make;
/* a size, a level, how many levels, the part of its candidates (from the least work 0 to the most 1) */
static const make CHAPTER[FSK_CHAPTERS][3] = {
    {{4, FSK_EASY, 12, 0, 1}, {5, FSK_EASY, 13, 0, 0.7f}},
    {{5, FSK_EASY, 8, 0.5f, 1}, {5, FSK_MEDIUM, 17, 0, 0.7f}},
    {{6, FSK_MEDIUM, 25, 0, 0.9f}},
    {{6, FSK_MEDIUM, 8, 0.7f, 1}, {6, FSK_HARD, 17, 0, 0.5f}},
    {{7, FSK_MEDIUM, 8, 0.3f, 1}, {7, FSK_HARD, 17, 0, 0.5f}},
    {{7, FSK_HARD, 25, 0.3f, 1}},
};

/* the puzzle whichever way it lies: the smallest of its sixteen (turned, mirrored, the digits d made n + 1 - d) */
static void canon(const fsk_puz *p, char *key)
{
    int n = p->n;
    char best[3 * FSK_CELLS + 8], k[3 * FSK_CELLS + 8];
    best[0] = 0;
    for (int t = 0; t < 16; t++) {
        fsk_puz q;
        memset(&q, 0, sizeof q);
        q.n = n;
        int to[FSK_CELLS];                      /* where each cell goes */
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int a = t & 1 ? n - 1 - x : x, b = t & 2 ? n - 1 - y : y;
                to[y * n + x] = t & 4 ? a * n + b : b * n + a;
            }
        for (int c = 0; c < n * n; c++) q.g[to[c]] = (uint8_t)(p->g[c] && t & 8 ? n + 1 - p->g[c] : p->g[c]);
        for (int way = 0; way < 2; way++)
            for (int c = 0; c < n * n; c++) {
                if (!p->rel[way][c]) continue;
                int o = way ? c + n : c + 1, a = to[c], b = to[o], less = (p->rel[way][c] == FSK_LT) != !!(t & 8);
                int first = a < b ? a : b, w = abs(a - b) == 1 ? 0 : 1;   /* the pair, now: side by side or not */
                q.rel[w][first] = (uint8_t)((first == a) == less ? FSK_LT : FSK_GT);
            }
        fsk_encode(&q, k, sizeof k);
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
static cand chosen[FSK_NLEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* one candidate from a seed; 1 when it is of the level and has one solution */
static int candidate(cand *c, int n, int level, uint32_t seed)
{
    uint8_t sol[FSK_CELLS];
    fsk_make(n, level, seed, &c->p, sol);
    c->grade = fsk_effort(&c->p, &c->passes, &c->mediums);
    if (c->grade != level || fsk_count(&c->p, 2, NULL) != 1) return 0;
    c->numbers = 0;
    for (int i = 0; i < n * n; i++) c->numbers += (c->p.g[i] != 0) + (c->p.rel[0][i] != 0) + (c->p.rel[1][i] != 0);
    /* the work: the steps, the harder ones weigh more; fewer clues ask a little more */
    c->score = (double)c->passes + 4.0 * c->mediums - 0.2 * c->numbers;
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
    printf("%dx%d level %d: %d candidates; work (steps, harder ones, clues) by tenths:\n", n, n, level, k);
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
    for (int ch = 0; ch < FSK_CHAPTERS; ch++) {
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
                if (a->grade * 1e6 + a->p.n * 1e4 + a->score <= b->grade * 1e6 + b->p.n * 1e4 + b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
    printf("/*\n"
           " * Futoshiki's levels: our own, made by tools/futoshiki_gen.c (the game's maker; each checked to have one\n"
           " * solution), six chapters of 25 from the easiest. Each: \"<n>:\", the digits given, the signs (futoshiki.h,\n"
           " * fsk_decode); in the comment its size, its grade and the work it asks of the grader (its steps, the\n"
           " * harder ones).\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const fsk_levels[FSK_NLEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[3 * FSK_CELLS + 8];
        if (!fsk_encode(&chosen[i].p, code, sizeof code)) return 1;
        printf("    \"%s\", /* %d: %dx%d %s, %ld steps, %ld harder */\n", code, i + 1, chosen[i].p.n, chosen[i].p.n,
               GRADE[chosen[i].grade], chosen[i].passes, chosen[i].mediums);
    }
    printf("};\n");
    return nchosen == FSK_NLEVELS ? 0 : 1;
}
