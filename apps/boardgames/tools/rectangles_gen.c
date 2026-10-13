/*
 * Rectangles' levels, made by chance and chosen: src/games/rectangles_levels.h. Our own puzzles, from the game's own
 * maker (the grid cut into rectangles by chance, a number in each, numbers moved within their rectangles until the
 * reasoning grader finishes it at its level: one solution). Each candidate:
 *   - its grade (the hardest reasoning the grader needed: the cells every rectangle of a number covers; a cell only
 *     one number reaches, a rectangle leaving another number no room; a rectangle followed to a contradiction) and
 *     its steps, weighed by their level (its effort);
 *   - near-duplicates dropped (the same numbers turned or mirrored).
 * Each chapter has two makes (a size and a level); a make's candidates are ranked by effort and picked evenly over
 * them (the last two chapters over its harder part). Then each chapter goes from its smallest and easiest to its
 * biggest and hardest: 5 x 5 easy first, 14 x 14 hard last.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/rctgen apps/boardgames/tools/rectangles_gen.c \
 *       apps/boardgames/src/games/rectangles_rules.c && /tmp/rctgen > apps/boardgames/src/games/rectangles_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/rectangles.h"

#define CANDS 60                                /* candidates made for each make */
#define CODE (RCT_CELLS + 8)

typedef struct {
    rct_puz p;
    int grade, steps, clues;
    double score;
    char key[CODE];
} cand;

typedef struct { int n, level, count, lo; } make;   /* lo: the picks from that part (in a hundred) of the pool up */
static const make CHAPTER[RCT_CHAPTERS][2] = {
    {{5, RCT_EASY, 12, 0}, {6, RCT_EASY, 13, 0}},
    {{7, RCT_EASY, 10, 0}, {7, RCT_MEDIUM, 15, 0}},
    {{8, RCT_MEDIUM, 12, 0}, {9, RCT_MEDIUM, 13, 0}},
    {{9, RCT_HARD, 12, 0}, {10, RCT_HARD, 13, 0}},
    {{11, RCT_HARD, 12, 25}, {12, RCT_HARD, 13, 25}},
    {{13, RCT_HARD, 12, 50}, {14, RCT_HARD, 13, 50}},
};

/* the numbers whichever way they lie: the smallest code of their eight (turned, mirrored) */
static void canon(const rct_puz *p, char *key)
{
    int n = p->n;
    key[0] = 0;
    for (int t = 0; t < 8; t++) {
        rct_puz q = *p;
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int sx = t & 1 ? n - 1 - x : x, sy = t & 2 ? n - 1 - y : y;
                if (t & 4) { int s = sx; sx = sy; sy = s; }
                q.clue[y * n + x] = p->clue[sy * n + sx];
            }
        char k[CODE];
        rct_encode(&q, k, sizeof k);
        if (!key[0] || strcmp(k, key) < 0) strcpy(key, k);
    }
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[RCT_STAGES];
static int nchosen;
static uint32_t sol[RCT_CELLS];

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 40 * CANDS; seed++) {
        cand *c = &pool[k];
        rct_make(m->n, m->level, seed, &c->p, sol);
        c->grade = rct_effort(&c->p, &c->steps);
        if (c->grade != m->level || rct_count(&c->p, 2, NULL) != 1) continue;
        canon(&c->p, c->key);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (dup) continue;
        c->clues = 0;
        for (int i = 0; i < m->n * m->n; i++) c->clues += c->p.clue[i] != 0;
        c->score = c->steps + 0.01 * (seed % 7);
        k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

int main(void)
{
    for (int ch = 0; ch < RCT_CHAPTERS; ch++) {
        int first = nchosen;
        for (int mi = 0; mi < 2; mi++) {
            const make *m = &CHAPTER[ch][mi];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 2 + mi + 1));
            fprintf(stderr, "chapter %d, make %d (%dx%d level %d): %d candidates, effort %d..%d\n", ch + 1, mi + 1,
                    m->n, m->n, m->level, k, k ? pool[0].steps : 0, k ? pool[k - 1].steps : 0);
            for (int i = 0; i < m->count; i++) {
                int lo = m->lo * (k - 1) / 100, at = lo + (int)((i + 0.5f) / m->count * (k - 1 - lo));
                while (at < k && seen(pool[at].key)) at++;
                if (at >= k) {
                    fprintf(stderr, "chapter %d: not enough of make %d\n", ch + 1, mi + 1);
                    return 1;
                }
                chosen[nchosen++] = pool[at];
            }
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
    }
    printf("/*\n"
           " * Rectangles' levels: our own, made by tools/rectangles_gen.c (the game's maker, the reasoning grader's\n"
           " * grade and effort), six chapters of 25 from the easiest. Each: \"<n>:\" and a character a cell, row by\n"
           " * row: '.' none, '1'..'9', 'a' 10, 'b' 11... (rectangles.h, rct_decode); in the comment the reasoning it\n"
           " * needs (0 easy, 1 medium, 2 hard), the grader's steps weighed by their level, and the numbers.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const rct_levels[RCT_STAGES] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[CODE];
        rct_encode(&chosen[i].p, code, sizeof code);
        printf("    \"%s\", /* %d: %dx%d, grade %d, effort %d, %d numbers */\n", code, i + 1, chosen[i].p.n,
               chosen[i].p.n, chosen[i].grade, chosen[i].steps, chosen[i].clues);
    }
    printf("};\n");
    return nchosen == RCT_STAGES ? 0 : 1;
}
