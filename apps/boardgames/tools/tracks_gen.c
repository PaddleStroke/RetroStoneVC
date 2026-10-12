/*
 * Tracks' levels, made by chance and chosen: src/games/tracks_levels.h. Our own levels, from the game's own maker (a
 * line drawn by chance from edge to edge, pieces given where the reasoning solver gets stuck, then the ones not needed
 * taken away; kept when the solver lays the whole track: one solution). Each candidate:
 *   - its grade (the highest reasoning the solver used: a cell's sides and the counts; no loop; the cuts' crossings;
 *     a side tried and followed to a contradiction), the line's length and the pieces given;
 *   - near-duplicates dropped (the same line turned or mirrored).
 * Each chapter has its makes (5 x 5 with the plainest reasoning up to 10 x 10 needing trials); a make's candidates
 * are ranked by difficulty and picked evenly over them. Then each chapter goes from its easiest to its hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/tkgen apps/boardgames/tools/tracks_gen.c \
 *       apps/boardgames/src/games/tracks_rules.c && /tmp/tkgen > apps/boardgames/src/games/tracks_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/tracks.h"

#define CANDS 70                                /* candidates made for each make of each chapter */

typedef struct {
    tk_game g;
    int len, givens;
    double score;
    char key[TK_CELLS + 1];
} cand;

typedef struct { tk_config cf; int count; } part;
/* n, the line's length (lmin, lmax), the reasoning allowed, the least needed */
static const struct { part p[2]; } CHAPTER[TK_CHAPTERS] = {
    {{{{5, 8, 15, 0, 0}, 12}, {{6, 12, 20, 0, 0}, 13}}},
    {{{{6, 13, 22, 1, 0}, 10}, {{7, 18, 30, 1, 0}, 15}}},
    {{{{7, 18, 30, 1, 1}, 10}, {{8, 24, 40, 1, 1}, 15}}},
    {{{{8, 24, 40, 2, 1}, 12}, {{9, 30, 50, 2, 1}, 13}}},
    {{{{9, 30, 50, 2, 2}, 12}, {{10, 38, 62, 2, 2}, 13}}},
    {{{{10, 38, 62, 3, 2}, 25}, {{0}, 0}}},
};

/* the line's picture whichever way it lies: the smallest of its eight (turned, mirrored) */
static void canon(const tk_game *g, char *key)
{
    int n = g->n;
    char best[TK_CELLS + 1], k[TK_CELLS + 1];
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
                    int v = g->sol[sy * n + sx];
                    v = ((v << r) | (v >> (4 - r))) & 15;   /* N E S W: a quarter turn clockwise moves each on */
                    if (m) v = (v & (TK_N | TK_S)) | (v & TK_E ? TK_W : 0) | (v & TK_W ? TK_E : 0);
                    k[y * n + x] = "0123456789abcdef"[v];
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
static cand chosen[TK_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

static int make_pool(const tk_config *cf, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 20 * CANDS; seed++) {
        cand *c = &pool[k];
        if (tk_make(&c->g, cf, seed, 400, 20000) != 2) continue;
        uint8_t out[TK_CELLS];
        int grade;
        if (!tk_deduce(&c->g, cf->level, &grade, out) || memcmp(out, c->g.sol, (size_t)(cf->n * cf->n))) continue;
        c->len = c->givens = 0;
        for (int i = 0; i < cf->n * cf->n; i++) c->len += c->g.sol[i] != 0, c->givens += c->g.given[i];
        canon(&c->g, c->key);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (dup) continue;
        c->score = 2.0 * c->g.grade + 0.04 * c->len - 0.15 * c->givens + 0.01 * (seed % 7);
        k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

int main(void)
{
    for (int ch = 0; ch < TK_CHAPTERS; ch++) {
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
                cand *a = &chosen[j - 1], *b = &chosen[j];
                if (a->g.n * 3 + a->score <= b->g.n * 3 + b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
    printf("/*\n * Tracks' levels: our own, made by tools/tracks_gen.c (a line drawn by chance, pieces given where the\n"
           " * reasoning solver is stuck, kept when it lays the whole track), six chapters of 25 from the easiest.\n"
           " * Each: \"<n>:\" and a character a cell (tracks.h, tk_encode); in the comment the reasoning it needs (its\n"
           " * grade), the line's length and the pieces given.\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const tk_levels[TK_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[TK_CELLS + 8];
        tk_encode(&chosen[i].g, code, sizeof code);
        printf("    \"%s\", /* %d: %dx%d, grade %d, %d cells, %d given */\n", code, i + 1, chosen[i].g.n,
               chosen[i].g.n, chosen[i].g.grade, chosen[i].len, chosen[i].givens);
    }
    printf("};\n");
    return nchosen == TK_LEVELS ? 0 : 1;
}
