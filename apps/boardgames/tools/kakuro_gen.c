/*
 * Kakuro's levels, made by chance and chosen: src/games/kakuro_levels.h. Our own puzzles, from the game's own maker
 * (a pattern of dark squares turned half round itself, digits dealt, digits changed until the reasoning solver
 * finishes it at its level: one solution). Each candidate:
 *   - its grade (the hardest reasoning the solver needed: the ways of each clue; a digit every way needs, pairs; a run
 *     worked out whole) and the solver's passes over the runs (its effort);
 *   - near-duplicates dropped (the same puzzle mirrored on its diagonal, across and down swapped).
 * Each chapter has two makes (a size and a level); a make's candidates are ranked by effort and picked evenly over
 * them, the nearest whose pattern of dark squares is not worn out yet (a pattern used once from 7 x 7, a few times on
 * the small grids, which have few), so the grids vary. Then each chapter goes from its smallest and easiest to its
 * biggest and hardest: 5 x 5 easy first, 12 x 12 hard last; its last level is a showpiece, the hardest of its last
 * make whose pattern is also its own mirror on the diagonal.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/kkrgen apps/boardgames/tools/kakuro_gen.c \
 *       apps/boardgames/src/games/kakuro_rules.c && /tmp/kkrgen > apps/boardgames/src/games/kakuro_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/kakuro.h"

#define CANDS 100                               /* candidates made for each make */

typedef struct {
    kkr_puzzle p;
    int grade;
    long passes;
    double score;
    char key[KKR_CELLS + 8];
    char pattern[KKR_CELLS + 1];                /* its dark squares, whichever way it lies */
    int diagonal;                               /* 1: the pattern is its own mirror on the diagonal too */
} cand;

typedef struct { int n, level, count; } make;
static const make CHAPTER[KKR_CHAPTERS][2] = {
    {{5, KKR_EASY, 10}, {6, KKR_EASY, 15}},
    {{6, KKR_MEDIUM, 10}, {7, KKR_EASY, 15}},
    {{7, KKR_MEDIUM, 12}, {8, KKR_MEDIUM, 13}},
    {{8, KKR_HARD, 12}, {9, KKR_MEDIUM, 13}},
    {{9, KKR_HARD, 12}, {10, KKR_HARD, 13}},
    {{11, KKR_HARD, 12}, {12, KKR_HARD, 13}},
};

/* the puzzle whichever way it lies: the smaller code of it and of it mirrored on its diagonal */
static void canon(const kkr_puzzle *p, char *key)
{
    kkr_puzzle t = *p;
    int n = p->n;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) t.white[x * n + y] = p->white[y * n + x], t.sol[x * n + y] = p->sol[y * n + x];
    char a[KKR_CELLS + 8], b[KKR_CELLS + 8];
    kkr_encode(p, a, sizeof a);
    kkr_encode(&t, b, sizeof b);
    strcpy(key, strcmp(a, b) <= 0 ? a : b);
}

/* the dark squares' pattern, the smaller of it and of it mirrored on its diagonal; 1 when both are the same */
static int pattern_of(const kkr_puzzle *p, char *key)
{
    int n = p->n;
    char a[KKR_CELLS + 1], b[KKR_CELLS + 1];
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            a[y * n + x] = p->white[y * n + x] ? '.' : '#';
            b[x * n + y] = a[y * n + x];
        }
    a[n * n] = b[n * n] = 0;
    strcpy(key, strcmp(a, b) <= 0 ? a : b);
    return !strcmp(a, b);
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[KKR_STAGES];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* how often the levels chosen use a pattern of dark squares */
static int uses(const char *pattern)
{
    int k = 0;
    for (int i = 0; i < nchosen; i++) k += !strcmp(chosen[i].pattern, pattern);
    return k;
}

/* a candidate for a pick: not chosen yet, its pattern not worn out (the small grids have few patterns; at last any) */
static int fresh(const cand *c, int slack)
{
    int n = c->p.n, most = (n <= 5 ? 3 : n <= 6 ? 2 : 1) + slack;
    return !seen(c->key) && (slack >= 3 || uses(c->pattern) < most);
}

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 20 * CANDS; seed++) {
        cand *c = &pool[k];
        kkr_make(m->n, m->level, seed, &c->p);
        c->grade = kkr_effort(&c->p, &c->passes);
        if (c->grade != m->level || kkr_count(&c->p, 2) != 1) continue;
        canon(&c->p, c->key);
        c->diagonal = pattern_of(&c->p, c->pattern);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (dup) continue;
        c->score = (double)c->passes + 0.01 * (seed % 7);
        k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

int main(void)
{
    for (int ch = 0; ch < KKR_CHAPTERS; ch++) {
        int first = nchosen, k = 0;
        for (int mi = 0; mi < 2; mi++) {
            const make *m = &CHAPTER[ch][mi];
            k = make_pool(m, 1000u * (uint32_t)(ch * 2 + mi + 1));
            fprintf(stderr, "chapter %d, make %d (%dx%d level %d): %d candidates, effort %ld..%ld\n", ch + 1, mi + 1,
                    m->n, m->n, m->level, k, k ? pool[0].passes : 0, k ? pool[k - 1].passes : 0);
            for (int i = 0; i < m->count; i++) {
                int at0 = (int)((i + 0.5f) / m->count * (k - 1)), at = k;
                for (int slack = 0; slack < 4 && at >= k; slack++)   /* the nearest fresh one, after it or before */
                    for (int d = 0; d < k && at >= k; d++) {
                        if (at0 + d < k && fresh(&pool[at0 + d], slack)) at = at0 + d;
                        else if (at0 - d >= 0 && d && fresh(&pool[at0 - d], slack)) at = at0 - d;
                    }
                if (at >= k) {
                    fprintf(stderr, "chapter %d: not enough of make %d\n", ch + 1, mi + 1);
                    return 1;
                }
                chosen[nchosen++] = pool[at];
            }
        }
        /* the chapter's showpiece, its last level: the hardest of its last make whose pattern is also its own mirror
         * on the diagonal, in place of that make's hardest pick */
        int last = -1;
        for (int i = k - 1; i >= k / 2 && last < 0; i--)
            if (pool[i].diagonal && !seen(pool[i].key) && !uses(pool[i].pattern)) last = i;
        if (last >= 0) {
            int worst = -1;
            for (int i = first; i < nchosen; i++)
                if (chosen[i].p.n == pool[last].p.n && (worst < 0 || chosen[i].score >= chosen[worst].score)) worst = i;
            chosen[worst] = pool[last];
        }
        for (int i = first + 1; i < nchosen; i++)   /* the chapter from its easiest to its hardest */
            for (int j = i; j > first; j--) {
                cand *a = &chosen[j - 1], *b = &chosen[j];
                double ka = a->p.n * 1e6 + a->grade * 1e5 + a->score, kb = b->p.n * 1e6 + b->grade * 1e5 + b->score;
                if (ka <= kb) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
        for (int i = first; last >= 0 && i < nchosen - 1; i++)   /* the showpiece last */
            if (!strcmp(chosen[i].key, pool[last].key)) {
                cand t = chosen[i];
                memmove(&chosen[i], &chosen[i + 1], (size_t)(nchosen - 1 - i) * sizeof t);
                chosen[nchosen - 1] = t;
                break;
            }
    }
    printf("/*\n * Kakuro's levels: our own, made by tools/kakuro_gen.c (the game's maker, the reasoning solver's grade and\n"
           " * effort), six chapters of 25 from the easiest. Each: \"<n>:\" and a character a square, row by row, '#' dark\n"
           " * or the solution's digit (kakuro.h, kkr_decode; the clues are its sums); in the comment the reasoning it\n"
           " * needs (0 easy, 1 medium, 2 hard) and the solver's passes.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const kkr_levels[KKR_STAGES] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[KKR_CELLS + 8];
        kkr_encode(&chosen[i].p, code, sizeof code);
        printf("    \"%s\", /* %d: %dx%d, grade %d, %ld passes */\n", code, i + 1, chosen[i].p.n, chosen[i].p.n,
               chosen[i].grade, chosen[i].passes);
    }
    printf("};\n");
    return nchosen == KKR_STAGES ? 0 : 1;
}
