/*
 * Paths' levels, made by chance and chosen: src/games/paths_levels.h. Our own levels, from the game's own maker
 * (paths grown by chance over the grid, joined while the deductions alone still solve the puzzle: one solution).
 * Each candidate:
 *   - its grade (nk_grade: the fewest kinds of deduction that solve it without a guess: the links each square needs
 *     and a path's one colour; each pair's dots joined through the squares that may take its colour; the squares
 *     every such way passes through) and the passes of deduction it took;
 *   - its pairs (fewer, longer paths are harder);
 *   - near-duplicates dropped (the same dots turned or mirrored, whatever their colours);
 *   - taste: none with a pair side by side (a path of two squares is no puzzle) or one path over two fifths of the
 *     grid, none with its dots heaped in one quarter (the fullest at most three times the emptiest) or more than
 *     three in five on the edges; and how symmetric its dots are (the share with a dot at their half-turn image).
 * Each chapter ends on a showpiece: of its hardest candidates, the one with the most symmetric dots.
 * Each chapter has its sizes (5 x 5 up to 12 x 12); a size's candidates are ranked by difficulty and picked evenly
 * over a band of that ranking that moves up with the chapters. Then each chapter goes from its easiest to its hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/nkgen apps/boardgames/tools/paths_gen.c \
 *       apps/boardgames/src/games/paths_rules.c && /tmp/nkgen > apps/boardgames/src/games/paths_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/paths.h"
#include "games/puzrand.h"

#define CANDS 120                               /* candidates made for each size of each chapter */

typedef struct {
    int n, pairs, grade, rounds;
    double sym;                                 /* the share of dots with a dot at their half-turn image */
    double score;
    uint8_t colour[NK_CELLS], end[NK_COLOURS][2];
    char key[NK_CELLS + 1];
} cand;

typedef struct { int n, count; } part;
static const struct { part p[2]; float lo, hi; } CHAPTER[NK_CHAPTERS] = {
    {{{5, 12}, {6, 13}}, 0.0f, 0.5f},
    {{{6, 8}, {7, 17}}, 0.2f, 0.75f},
    {{{7, 8}, {8, 17}}, 0.3f, 0.85f},
    {{{8, 5}, {9, 20}}, 0.35f, 0.95f},
    {{{9, 5}, {10, 20}}, 0.4f, 1.0f},
    {{{11, 13}, {12, 12}}, 0.45f, 1.0f},
};

/* the pairs a size may have */
static void pairs_of(int n, int *lo, int *hi)
{
    *lo = n - 2;
    *hi = n + 1;
    if (*hi > NK_COLOURS) *hi = NK_COLOURS;
}

/* the dots' picture whichever way it lies: the smallest of its eight (turned, mirrored), each colour named by the
 * order its first dot comes in */
static void canon(const cand *c, char *key)
{
    int n = c->n, dot[NK_CELLS];
    char best[NK_CELLS + 1], k[NK_CELLS + 1];
    best[0] = 0;
    for (int i = 0; i < n * n; i++) dot[i] = -1;
    for (int p = 0; p < c->pairs; p++) dot[c->end[p][0]] = dot[c->end[p][1]] = p;
    for (int r = 0; r < 4; r++)
        for (int m = 0; m < 2; m++) {
            int name[NK_COLOURS], named = 0;
            for (int i = 0; i < NK_COLOURS; i++) name[i] = -1;
            for (int y = 0; y < n; y++)
                for (int x = 0; x < n; x++) {
                    int sx = m ? n - 1 - x : x, sy = y;
                    for (int i = 0; i < r; i++) {
                        int tx = sy, ty = n - 1 - sx;
                        sx = tx, sy = ty;
                    }
                    int d = dot[sy * n + sx];
                    if (d >= 0 && name[d] < 0) name[d] = named++;
                    k[y * n + x] = d < 0 ? '.' : (char)('A' + name[d]);
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
static cand chosen[NK_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* taste: 0 when the dots look made by chance (a pair side by side, a path that fills much of the grid, dots heaped
 * in one quarter or along the edges); else 1, and *sym: the share of dots whose half-turn image is a dot too */
static int tasteful(const cand *c, double *sym)
{
    int n = c->n, len[NK_COLOURS] = {0}, dot[NK_CELLS] = {0}, q[4] = {0}, edge = 0, mirrored = 0;
    for (int i = 0; i < n * n; i++) len[c->colour[i]]++;
    for (int p = 0; p < c->pairs; p++) {
        if (len[p] < 3 || len[p] * 5 > n * n * 2) return 0;
        for (int e = 0; e < 2; e++) {
            int s = c->end[p][e], x = s % n, y = s / n;
            dot[s] = 1;
            edge += x == 0 || y == 0 || x == n - 1 || y == n - 1;
            if (2 * x + 1 != n && 2 * y + 1 != n) q[(2 * x >= n) + 2 * (2 * y >= n)]++;
        }
    }
    int lo = q[0], hi = q[0], dots = 2 * c->pairs;
    for (int i = 1; i < 4; i++) lo = q[i] < lo ? q[i] : lo, hi = q[i] > hi ? q[i] : hi;
    if (!lo || hi > 3 * lo || edge * 5 > dots * 3) return 0;
    for (int s = 0; s < n * n; s++) mirrored += dot[s] && dot[n * n - 1 - s];
    *sym = (double)mirrored / dots;
    return 1;
}

static int make_pool(int n, uint32_t seed0)
{
    int k = 0, lo, hi;
    pairs_of(n, &lo, &hi);
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 40 * CANDS; seed++) {
        cand *c = &pool[k];
        memset(c, 0, sizeof *c);
        c->n = n;
        nk_make_n(n, lo, hi, seed, c->colour, c->end, &c->pairs);
        if (c->pairs < lo || c->pairs > hi || !tasteful(c, &c->sym)) continue;
        c->grade = nk_grade(n, c->pairs, (const uint8_t (*)[2])c->end, &c->rounds);
        if (c->grade < 0 || nk_count(n, c->pairs, (const uint8_t (*)[2])c->end, 2, 2000000) != 1) continue;
        canon(c, c->key);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (dup) continue;
        c->score = c->grade + 0.04 * c->rounds + 1.5 * (double)n / c->pairs + 0.01 * (seed % 7);
        k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

int main(void)
{
    for (int ch = 0; ch < NK_CHAPTERS; ch++) {
        int first = nchosen;
        for (int p = 0; p < 2; p++) {
            const part *pt = &CHAPTER[ch].p[p];
            int k = make_pool(pt->n, 1000u * (uint32_t)(ch * 2 + p + 1));
            float lo = CHAPTER[ch].lo, hi = CHAPTER[ch].hi;
            for (int i = 0; i < pt->count; i++) {
                int at = (int)((lo + (hi - lo) * (i + 0.5f) / pt->count) * (k - 1));
                while (at < k && seen(pool[at].key)) at++;
                if (at >= k) {
                    fprintf(stderr, "chapter %d: not enough %dx%d\n", ch + 1, pt->n, pt->n);
                    return 1;
                }
                if (p == 1 && i == pt->count - 1)   /* the showpiece: the most symmetric dots of the hardest */
                    for (int j = at + 1; j < k; j++)
                        if (!seen(pool[j].key) && pool[j].grade <= pool[at].grade && pool[j].sym > pool[at].sym)
                            at = j;
                chosen[nchosen++] = pool[at];
            }
        }
        cand show = chosen[nchosen - 1];
        for (int i = first + 1; i < nchosen; i++)   /* the chapter from its easiest to its hardest */
            for (int j = i; j > first; j--) {
                cand *a = &chosen[j - 1], *b = &chosen[j];
                if (a->n * a->n + 10 * a->score <= b->n * b->n + 10 * b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
        for (int i = first; i < nchosen; i++)   /* the showpiece last */
            if (!strcmp(chosen[i].key, show.key)) {
                memmove(&chosen[i], &chosen[i + 1], (size_t)(nchosen - 1 - i) * sizeof show);
                chosen[nchosen - 1] = show;
                break;
            }
        fprintf(stderr, "chapter %d: showpiece symmetry %.2f\n", ch + 1, show.sym);
    }
    printf("/*\n * Paths' levels: our own, made by tools/paths_gen.c (paths grown by chance over the grid, kept when the\n"
           " * deductions alone solve them, ranked by how hard those deductions are), six chapters of 25 from the easiest.\n"
           " * Each: \"<n>:\" and a letter a square, its colour in the one solution (the dots in capitals); in the\n"
           " * comment its pairs, its grade (paths.h, nk_grade) and its passes of deduction.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n"
           "static const char *const nk_levels[NK_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[NK_CELLS + 8];
        nk_encode(chosen[i].n, chosen[i].colour, (const uint8_t (*)[2])chosen[i].end, chosen[i].pairs, code,
                  sizeof code);
        printf("    \"%s\", /* %d: %dx%d, %d pairs, grade %d, %d passes */\n", code, i + 1, chosen[i].n, chosen[i].n,
               chosen[i].pairs, chosen[i].grade, chosen[i].rounds);
    }
    printf("};\n");
    return 0;
}
