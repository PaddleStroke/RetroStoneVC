/*
 * Mirrors' levels, made by chance and chosen: src/games/mirrors_levels.h. Our own levels, from the game's own maker
 * (a beam's path drawn by chance, its mirrors, gems along it, bombs beside it, decoys; kept when the search finds one
 * way only to solve it). Each candidate:
 *   - the search's work (the choices it tried), the loose mirrors, the glass, its par (the fewest actions);
 *   - near-duplicates dropped (the same board turned or mirrored, whichever way its pivots start).
 * Each chapter has its makes (5 x 5 with pivots alone, then a loose mirror and a bomb, then glass, up to 8 x 8 with
 * three loose mirrors); a make's candidates are ranked by difficulty and picked evenly over them. Then each chapter
 * goes from its easiest to its hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/mrgen apps/boardgames/tools/mirrors_gen.c \
 *       apps/boardgames/src/games/mirrors_rules.c -lm && /tmp/mrgen > apps/boardgames/src/games/mirrors_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/mirrors.h"

#define CANDS 60                                /* candidates made for each make of each chapter */

typedef struct {
    mr_game g;
    long work;
    double score;
    char key[MR_CELLS + 4];
} cand;

typedef struct { mr_config cf; int count; } part;
/* n, pivots, loose, glass, gems, bombs, walls, decoys, work min, work max, steps from the solution */
static const struct { part p[2]; } CHAPTER[MR_CHAPTERS] = {
    {{{{5, 3, 0, 0, 2, 0, 1, 1, 2, 200, 2}, 13}, {{5, 4, 0, 0, 2, 1, 1, 1, 4, 400, 2}, 12}}},
    {{{{5, 3, 1, 0, 2, 1, 1, 1, 4, 2000, 2}, 10}, {{6, 3, 1, 0, 3, 1, 2, 1, 10, 4000, 2}, 15}}},
    {{{{6, 4, 1, 0, 3, 2, 2, 2, 20, 8000, 3}, 12}, {{7, 3, 2, 0, 3, 2, 3, 2, 30, 20000, 3}, 13}}},
    {{{{7, 3, 1, 1, 3, 2, 3, 2, 30, 20000, 3}, 12}, {{7, 3, 2, 1, 3, 2, 3, 2, 40, 40000, 3}, 13}}},
    {{{{8, 4, 2, 1, 4, 2, 3, 2, 100, 100000, 3}, 12}, {{8, 3, 3, 0, 4, 3, 3, 2, 150, 200000, 3}, 13}}},
    {{{{8, 3, 3, 1, 4, 3, 3, 2, 200, 300000, 3}, 25}, {{0}, 0}}},
};

/* a cell's character seen in the board turned r quarter turns clockwise, then mirrored left-right (m); a pivot's way
 * left out (it starts by chance) */
static char cell_seen(const mr_game *g, int c, int r, int m)
{
    int k = g->kind[c], w = g->way[c];
    if (k == MR_PIVOT) return 'p';
    if (k == MR_GLASS) {
        w ^= (r & 1) ^ m;                       /* a quarter turn or a mirror swaps '/' and '\' */
        return w == MR_SLASH ? 'g' : 'h';
    }
    if (k == MR_LAMP) {
        for (int i = 0; i < r; i++) w = (w + 3) & 3;   /* east turns to south... */
        if (m && (w == 0 || w == 2)) w ^= 2;
        return "ENWS"[w];
    }
    return k == MR_WALL ? '#' : k == MR_GEM ? '*' : k == MR_BOMB ? 'x' : '.';
}

static void canon(const mr_game *g, char *key)
{
    int n = g->n;
    char best[MR_CELLS + 1], k[MR_CELLS + 1];
    best[0] = 0;
    for (int r = 0; r < 4; r++)
        for (int m = 0; m < 2; m++) {
            for (int y = 0; y < n; y++)
                for (int x = 0; x < n; x++) {
                    int sx = m ? n - 1 - x : x, sy = y;
                    for (int i = 0; i < r; i++) {
                        int tx = sy, ty = n - 1 - sx;
                        sx = tx, sy = ty;
                    }
                    k[y * n + x] = cell_seen(g, sy * n + sx, r, m);
                }
            k[n * n] = 0;
            if (!best[0] || strcmp(k, best) < 0) strcpy(best, k);
        }
    sprintf(key, "%d%s", g->loose, best);
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[MR_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

static int make_pool(const mr_config *cf, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 20 * CANDS; seed++) {
        cand *c = &pool[k];
        if (!mr_make(&c->g, cf, seed, 4000)) continue;
        if (mr_solve(&c->g, 2, &c->work) != 1 || c->work < 0 || c->g.par < 2) continue;
        canon(&c->g, c->key);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (dup) continue;
        c->score = log2((double)c->work + 1) + 1.5 * c->g.loose + 1.5 * cf->glass + 0.25 * c->g.par;
        k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

int main(void)
{
    for (int ch = 0; ch < MR_CHAPTERS; ch++) {
        int first = nchosen;
        for (int p = 0; p < 2 && CHAPTER[ch].p[p].count; p++) {
            const part *pt = &CHAPTER[ch].p[p];
            int k = make_pool(&pt->cf, 1000u * (uint32_t)(ch * 2 + p + 1));
            fprintf(stderr, "chapter %d, make %d: %d candidates\n", ch + 1, p + 1, k);
            for (int i = 0; i < pt->count; i++) {
                int at = (int)((i + 0.5f) / pt->count * (k - 1));
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
                if (chosen[j - 1].score <= chosen[j].score) break;
                cand t = chosen[j - 1];
                chosen[j - 1] = chosen[j];
                chosen[j] = t;
            }
    }
    printf("/*\n * Mirrors' levels: our own, made by tools/mirrors_gen.c (a beam's way drawn by chance, kept when the search\n"
           " * finds one way only to solve it, ranked by the search's work), six chapters of 25 from the easiest.\n"
           " * Each: \"<n><loose mirrors>:\" and a character a cell (mirrors.h, mr_encode); in the comment the search's\n"
           " * work and the par (the fewest actions).\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n"
           "static const char *const mr_levels[MR_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[MR_CELLS + 8];
        mr_encode(&chosen[i].g, code, sizeof code);
        printf("    \"%s\", /* %d: %dx%d, %d loose, work %ld, par %d */\n", code, i + 1, chosen[i].g.n, chosen[i].g.n,
               chosen[i].g.loose, chosen[i].work, chosen[i].g.par);
    }
    printf("};\n");
    return nchosen == MR_LEVELS ? 0 : 1;
}
