/*
 * Dominosa's levels, made by chance and chosen with taste: src/games/dominosa_levels.h. Our own puzzles, from the
 * game's own maker (the grid tiled with dominoes by chance, the set dealt onto them, dominoes swapped or turned round
 * until the reasoning grader finishes it at its level: one solution). Each candidate:
 *   - its grade (the hardest reasoning the grader needed: a cell with one neighbour left, a domino with one place
 *     left; a cell whose pairs are all one domino, a domino whose places share a cell, a pair leaving a cell alone; a
 *     pair followed to a contradiction) and its steps, weighed by their level (its effort);
 *   - its look: the numbers (runs of three alike in a row or column, three alike in a 2 x 2 square, count against
 *     it) and the solution's dominoes, seen when solved (lying and standing ones mixed, few pairs side by side like
 *     bricks, the most dominoes whose half turn is a domino too);
 *   - near-duplicates dropped (the same numbers mirrored or turned half round).
 * Each chapter has two makes (a set and a level, the level never falling as the set grows); a make's candidates are
 * ranked by effort and picked evenly over them (the last chapter over its harder part), each pick the best looking of
 * its neighbours. Each chapter goes from its smallest and easiest to its biggest and hardest, and ends on a showpiece:
 * the best looking of its last make's harder half. Double-3 (5 x 4 cells) easy first, double-8 (10 x 9) hard last.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/dsagen apps/boardgames/tools/dominosa_gen.c \
 *       apps/boardgames/src/games/dominosa_rules.c -lm && /tmp/dsagen > apps/boardgames/src/games/dominosa_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/dominosa.h"

#define CANDS 300                               /* candidates made for each make */
#define CODE (DSA_CELLS + 8)
#define WINDOW 8                                /* a pick: the best looking within this many of its place */

typedef struct {
    dsa_puz p;
    int grade, steps;
    double score, look;
    char key[CODE];
} cand;

typedef struct { int n, level, count, lo; } make;   /* lo: the picks from that part (in a hundred) of the pool up */
static const make CHAPTER[DSA_CHAPTERS][2] = {
    {{3, DSA_EASY, 12, 0}, {4, DSA_EASY, 13, 0}},
    {{4, DSA_MEDIUM, 10, 0}, {5, DSA_MEDIUM, 15, 0}},
    {{5, DSA_MEDIUM, 8, 0}, {6, DSA_MEDIUM, 17, 0}},
    {{5, DSA_HARD, 10, 0}, {6, DSA_HARD, 15, 0}},
    {{7, DSA_MEDIUM, 10, 0}, {7, DSA_HARD, 15, 0}},
    {{8, DSA_HARD, 12, 25}, {8, DSA_HARD, 13, 50}},
};

/* the numbers whichever way they lie: the smallest code of their four (mirrored across, down, both) */
static void canon(const dsa_puz *p, char *key)
{
    int w = p->w, h = p->h;
    key[0] = 0;
    for (int t = 0; t < 4; t++) {
        dsa_puz q = *p;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                q.num[y * w + x] = p->num[(t & 2 ? h - 1 - y : y) * w + (t & 1 ? w - 1 - x : x)];
        char k[CODE];
        dsa_encode(&q, k, sizeof k);
        if (!key[0] || strcmp(k, key) < 0) strcpy(key, k);
    }
}

/* the look: the higher the better */
static double look(const dsa_puz *p, const uint8_t *mate)
{
    int w = p->w, h = p->h, cells = w * h, runs = 0, clumps = 0, across = 0, bricks = 0, sym = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int c = y * w + x, v = p->num[c];
            if (x + 2 < w && p->num[c + 1] == v && p->num[c + 2] == v) runs++;
            if (y + 2 < h && p->num[c + w] == v && p->num[c + 2 * w] == v) runs++;
            if (x + 1 < w && y + 1 < h) {
                int q[4] = {p->num[c], p->num[c + 1], p->num[c + w], p->num[c + w + 1]};
                for (int i = 0; i < 4; i++) {
                    int same = 0;
                    for (int j = 0; j < 4; j++) same += q[j] == q[i];
                    if (same >= 3) { clumps++; break; }
                }
                /* two dominoes side by side filling the square, like bricks */
                if ((mate[c] == c + 1 && mate[c + w] == c + w + 1) || (mate[c] == c + w && mate[c + 1] == c + w + 1))
                    bricks++;
            }
            if (mate[c] == c + 1) across++;
            int r = cells - 1 - c, rm = cells - 1 - mate[c];   /* its half turn */
            if (mate[c] > c && mate[r] == rm) sym++;
        }
    double doms = cells / 2.0, f = across / doms;
    return -2.0 * runs - 1.5 * clumps - 6.0 * fabs(f - 0.5) - 8.0 * bricks / doms + 6.0 * sym / doms;
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[DSA_STAGES];
static int nchosen;
static uint8_t sol[DSA_CELLS];

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
        dsa_make(m->n, m->level, seed, &c->p, sol);
        c->grade = dsa_effort(&c->p, &c->steps);
        if (c->grade != m->level || dsa_count(&c->p, 2, NULL) != 1) continue;
        canon(&c->p, c->key);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (dup) continue;
        c->look = look(&c->p, sol);
        c->score = c->steps + 0.01 * (seed % 7);
        k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

int main(void)
{
    for (int ch = 0; ch < DSA_CHAPTERS; ch++) {
        int first = nchosen;
        cand show;
        memset(&show, 0, sizeof show);
        for (int mi = 0; mi < 2; mi++) {
            const make *m = &CHAPTER[ch][mi];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 2 + mi + 1));
            fprintf(stderr, "chapter %d, make %d (double-%d, level %d): %d candidates, effort %d..%d\n", ch + 1,
                    mi + 1, m->n, m->level, k, k ? pool[0].steps : 0, k ? pool[k - 1].steps : 0);
            int count = m->count - (mi == 1);
            for (int i = 0; i < count; i++) {
                int lo = m->lo * (k - 1) / 100, at = lo + (int)((i + 0.5f) / count * (k - 1 - lo)), best = -1;
                for (int j = at - WINDOW; j <= at + WINDOW; j++)
                    if (j >= lo && j < k && !seen(pool[j].key) && (best < 0 || pool[j].look > pool[best].look))
                        best = j;
                if (best < 0) {
                    fprintf(stderr, "chapter %d: not enough of make %d\n", ch + 1, mi + 1);
                    return 1;
                }
                chosen[nchosen++] = pool[best];
            }
            if (mi == 1) {                      /* the showpiece: the best looking of the harder half */
                int best = -1;
                for (int i = k / 2; i < k; i++)
                    if (!seen(pool[i].key) && (best < 0 || pool[i].look > pool[best].look)) best = i;
                if (best < 0) {
                    fprintf(stderr, "chapter %d: no showpiece\n", ch + 1);
                    return 1;
                }
                show = pool[best];
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
        chosen[nchosen++] = show;
    }
    printf("/*\n"
           " * Dominosa's levels: our own, made by tools/dominosa_gen.c (the game's maker, the reasoning grader's\n"
           " * grade and effort, chosen by their look), six chapters of 25 from the easiest, each ending on a\n"
           " * showpiece. Each: \"<n>:\" (the set, double-n) and a digit a cell, row by row, n + 2 columns and n + 1\n"
           " * rows (dominosa.h, dsa_decode); in the comment the reasoning it needs (0 easy, 1 medium, 2 hard) and the\n"
           " * grader's steps weighed by their level.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const dsa_levels[DSA_STAGES] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[CODE];
        dsa_encode(&chosen[i].p, code, sizeof code);
        printf("    \"%s\", /* %d: double-%d, grade %d, effort %d */\n", code, i + 1, chosen[i].p.n, chosen[i].grade,
               chosen[i].steps);
    }
    printf("};\n");
    return nchosen == DSA_STAGES ? 0 : 1;
}
