/*
 * Pipes' levels, made by chance and chosen: src/games/pipes_levels.h. Our own levels, from the game's own maker (a
 * tree grown by chance from the source, kept when the deductions alone find it: one solution). Each candidate:
 *   - its grade (pp_grade: the fewest kinds of deduction that solve it: a tile's own sides, no loop, a way out for
 *     every group, two groups never shut off together) and the passes of deduction it took;
 *   - its start: every tile turned by chance, most of them wrong; its par, the fewest quarter turns to the solution;
 *   - near-duplicates dropped (the same tree turned or mirrored).
 * Each chapter has its sizes (4 x 4 up to 11 x 11, the wrapped boards in the last two); a size's candidates are
 * ranked by difficulty (the grade, then the passes per tile) and picked evenly over a band of that ranking that moves
 * up with the chapters, the easiest band in chapter 1. Then each chapter goes from its easiest to its hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/ppgen apps/boardgames/tools/pipes_gen.c \
 *       apps/boardgames/src/games/pipes_rules.c && /tmp/ppgen > apps/boardgames/src/games/pipes_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/pipes.h"
#include "games/puzrand.h"

#define CANDS 160                               /* candidates made for each size of each chapter */

typedef struct {
    int n, wrap, grade, rounds, par;
    double score;
    uint8_t sol[PP_TILES], now[PP_TILES];
    char key[PP_TILES + 1];
} cand;

/* a chapter's sizes: n, wrapped, how many levels; and the band of the ranking they come from (0 easiest .. 1) */
typedef struct { int n, wrap, count; } part;
static const struct { part p[3]; float lo, hi; } CHAPTER[PP_CHAPTERS] = {
    {{{4, 0, 9}, {5, 0, 16}}, 0.0f, 0.45f},
    {{{5, 0, 8}, {6, 0, 17}}, 0.2f, 0.7f},
    {{{6, 0, 7}, {7, 0, 18}}, 0.3f, 0.85f},
    {{{7, 0, 5}, {8, 0, 20}}, 0.35f, 0.95f},
    {{{9, 0, 15}, {7, 1, 10}}, 0.4f, 1.0f},
    {{{10, 0, 9}, {11, 0, 8}, {9, 1, 8}}, 0.5f, 1.0f},
};

/* the sides of a tile seen in the board turned (r quarter turns clockwise) then mirrored (left-right) */
static uint8_t sides_of(uint8_t s, int r, int m)
{
    s = pp_turn(s, r);
    if (m) s = (uint8_t)((s & (PP_UP | PP_DOWN)) | (s & PP_RIGHT ? PP_LEFT : 0) | (s & PP_LEFT ? PP_RIGHT : 0));
    return s;
}

/* the smallest of the tree's eight pictures (turned, mirrored): the same tree whichever way it lies */
static void canon(const cand *c, char *key)
{
    int n = c->n;
    char best[PP_TILES + 1], k[PP_TILES + 1];
    best[0] = 0;
    for (int r = 0; r < 4; r++)
        for (int m = 0; m < 2; m++) {
            for (int y = 0; y < n; y++)
                for (int x = 0; x < n; x++) {
                    int sx = x, sy = y;         /* the cell that lands at (x, y) */
                    if (m) sx = n - 1 - sx;
                    for (int i = 0; i < r; i++) {   /* undo r clockwise turns: (x, y) <- (y, n-1-x) */
                        int tx = sy, ty = n - 1 - sx;
                        sx = tx, sy = ty;
                    }
                    k[y * n + x] = PP_CODE[sides_of(c->sol[sy * n + sx], r, m)];
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
static cand chosen[PP_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* candidates of one size: made, graded, ranked */
static int make_pool(int n, int wrap, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 50 * CANDS; seed++) {
        cand *c = &pool[k];
        int source;
        memset(c, 0, sizeof *c);
        c->n = n;
        c->wrap = wrap;
        if (pp_make(n, wrap, seed, c->sol, &source) > PP_TRIES) continue;
        c->grade = pp_grade(n, wrap, c->sol, &c->rounds);
        if (c->grade < 0 || pp_count(n, wrap, c->sol, 2) != 1) continue;
        uint32_t r = pr_seed(seed ^ 0x2b3c4d5eu);
        int wrong;
        do {                                    /* scrambled: at least two tiles in three wrong */
            wrong = 0;
            for (int t = 0; t < n * n; t++) {
                c->now[t] = pp_turn(c->sol[t], pr_below(&r, 4));
                wrong += c->now[t] != c->sol[t];
            }
        } while (3 * wrong < 2 * n * n);
        c->par = pp_par(n, c->sol, c->now);
        canon(c, c->key);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (dup) continue;
        c->score = c->grade + (double)c->rounds / (n * n) * 4 + 0.01 * (seed % 7);
        k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

int main(void)
{
    for (int ch = 0; ch < PP_CHAPTERS; ch++) {
        int first = nchosen;
        for (int p = 0; p < 3 && CHAPTER[ch].p[p].n; p++) {
            const part *pt = &CHAPTER[ch].p[p];
            int k = make_pool(pt->n, pt->wrap, 1000u * (uint32_t)(ch * 3 + p + 1));
            float lo = CHAPTER[ch].lo, hi = CHAPTER[ch].hi;
            for (int i = 0; i < pt->count; i++) {   /* evenly over the band, the next unseen one */
                int at = (int)((lo + (hi - lo) * (i + 0.5f) / pt->count) * (k - 1));
                while (at < k && seen(pool[at].key)) at++;
                if (at >= k) {
                    fprintf(stderr, "chapter %d: not enough %dx%d\n", ch + 1, pt->n, pt->n);
                    return 1;
                }
                chosen[nchosen++] = pool[at];
            }
        }
        /* the chapter from its easiest to its hardest: the size first, then the score */
        for (int i = first + 1; i < nchosen; i++)
            for (int j = i; j > first; j--) {
                cand *a = &chosen[j - 1], *b = &chosen[j];
                double ka = a->n * a->n * (1 + 0.6 * a->wrap) + 8 * a->score;
                double kb = b->n * b->n * (1 + 0.6 * b->wrap) + 8 * b->score;
                if (ka <= kb) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
    if (nchosen != PP_LEVELS) {
        fprintf(stderr, "%d levels, not %d\n", nchosen, PP_LEVELS);
        return 1;
    }
    printf("/*\n * Pipes' levels: our own, made by tools/pipes_gen.c (trees grown by chance from the source, kept when the\n"
           " * deductions alone solve them, ranked by how hard those deductions are), six chapters of 25 from the easiest.\n"
           " * Each: \"<n>[w]:\" and a character a tile (pipes.h, PP_CODE); in the comment its grade, its passes of\n"
           " * deduction and its par (the fewest quarter turns).\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const pp_levels[PP_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        pp_game g;
        char code[PP_TILES + 8];
        pp_load(&g, chosen[i].n, chosen[i].wrap, chosen[i].n / 2 * chosen[i].n + chosen[i].n / 2, chosen[i].sol,
                chosen[i].now);
        pp_encode(&g, code, sizeof code);
        printf("    \"%s\", /* %d: %dx%d%s, grade %d, %d passes, par %d */\n", code, i + 1, chosen[i].n, chosen[i].n,
               chosen[i].wrap ? " wrapped" : "", chosen[i].grade, chosen[i].rounds, chosen[i].par);
    }
    printf("};\n");
    return 0;
}
