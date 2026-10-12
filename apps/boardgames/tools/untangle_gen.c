/*
 * Untangle's levels, made by chance and chosen: src/games/untangle_levels.h. Our own levels, from the game's own maker
 * (dots scattered evenly, joined by a thinned triangulation: a layout without crossings), then a few dots moved out
 * of it, each to a free place where its cords cross cords that stayed. Each candidate:
 *   - its places put on the level's grid (64 steps across), still without a crossing, no two dots too close;
 *   - its par: the dots moved (putting them back untangles it), kept when at most one dot fewer might do (ut_cover:
 *     no set of dots smaller than that has one of every crossing's four ends), so the par is tight;
 *   - its difficulty: the dots, the dots moved, the crossings.
 * Each chapter has its dots (6 up to 36) and its moved dots (2 up to 10); 60 candidates a chapter, ranked by
 * difficulty and picked evenly. No two levels alike (each from its own seed; the test compares them).
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/utgen apps/boardgames/tools/untangle_gen.c \
 *       apps/boardgames/src/games/untangle_rules.c -lm && /tmp/utgen > apps/boardgames/src/games/untangle_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/untangle.h"

#define CANDS 60

typedef struct {
    ut_game g;
    int k, crossings, cover;
    double score;
} cand;

/* a chapter's dots (lo..hi) and dots moved (lo..hi) */
static const struct { int v0, v1, k0, k1; } CHAPTER[UT_CHAPTERS] = {
    {6, 8, 2, 3}, {9, 11, 3, 4}, {12, 15, 4, 5}, {16, 20, 5, 6}, {22, 27, 6, 8}, {28, 36, 8, 10},
};

static float snap(float v) { return ut_grid((int)lroundf((v - 0.06f) / 0.88f * 63)); }

/* the layout on the grid: still no crossing, the dots apart, no dot on another's cord */
static int on_grid(ut_game *g)
{
    for (int v = 0; v < g->nv; v++) {
        g->hx[v] = g->x[v] = snap(g->hx[v]);
        g->hy[v] = g->y[v] = snap(g->hy[v]);
    }
    for (int a = 0; a < g->nv; a++)
        for (int b = a + 1; b < g->nv; b++)
            if (hypotf(g->hx[a] - g->hx[b], g->hy[a] - g->hy[b]) < 0.07f) return 0;
    for (int i = 0; i < g->ne; i++)
        for (int v = 0; v < g->nv; v++) {
            int a = g->e[i][0], b = g->e[i][1];
            if (v == a || v == b) continue;
            float dx = g->hx[b] - g->hx[a], dy = g->hy[b] - g->hy[a], l2 = dx * dx + dy * dy;
            float t = ((g->hx[v] - g->hx[a]) * dx + (g->hy[v] - g->hy[a]) * dy) / l2;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            if (hypotf(g->hx[v] - g->hx[a] - t * dx, g->hy[v] - g->hy[a] - t * dy) < 0.025f) return 0;
        }
    return ut_crossings(g) == 0;
}

/* dot v put at (x, y): far enough from every other dot, from its own place, and its cords crossing at least one cord
 * between two dots that stay (moved: the dots moved) */
static int fits(ut_game *g, int v, float x, float y, const uint8_t *moved)
{
    if (hypotf(x - g->hx[v], y - g->hy[v]) < 0.2f) return 0;
    for (int w = 0; w < g->nv; w++)
        if (w != v && hypotf(x - g->x[w], y - g->y[w]) < 0.08f) return 0;
    float ox = g->x[v], oy = g->y[v];
    g->x[v] = x, g->y[v] = y;
    int cross = 0;
    for (int i = 0; i < g->ne && !cross; i++) {
        if (g->e[i][0] != v && g->e[i][1] != v) continue;
        for (int j = 0; j < g->ne && !cross; j++)
            cross = !moved[g->e[j][0]] && !moved[g->e[j][1]] && ut_cords_cross(g, i, j);
    }
    g->x[v] = ox, g->y[v] = oy;
    return cross;
}

static int make(cand *c, int nv, int k, uint32_t seed)
{
    ut_game *g = &c->g;
    int ne = (int)lroundf(nv * (1.4f + 0.01f * nv));
    if (!ut_make(g, nv, ne, seed) || !on_grid(g)) return 0;
    uint32_t r = pr_seed(seed ^ 0x7e57u);
    int dots[UT_MAXV];
    uint8_t moved[UT_MAXV] = {0};
    for (int v = 0; v < nv; v++) dots[v] = v;
    pr_shuffle(&r, dots, nv);
    for (int i = 0; i < k; i++) moved[dots[i]] = 1;
    for (int i = 0; i < k; i++) {               /* each moved dot to a free place where its cords cross */
        int v = dots[i], ok = 0;
        for (int t = 0; t < 300 && !ok; t++) {
            float x = ut_grid(pr_below(&r, 64)), y = ut_grid(pr_below(&r, 64));
            if (fits(g, v, x, y, moved)) g->x[v] = x, g->y[v] = y, ok = 1;
        }
        if (!ok) return 0;
    }
    c->k = k;
    c->crossings = ut_crossings(g);
    if (c->crossings < k || (c->cover = ut_cover(g, k)) < k - 1) return 0;
    c->score = nv + 2.5 * k + 0.05 * c->crossings;
    g->par = k;
    return 1;
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS], chosen[UT_LEVELS];

int main(void)
{
    int nchosen = 0;
    for (int ch = 0; ch < UT_CHAPTERS; ch++) {
        int k = 0;
        uint32_t seed = 1000u * (uint32_t)(ch + 1);
        for (int i = 0; k < CANDS && i < 400 * CANDS; i++, seed++) {
            int nv = CHAPTER[ch].v0 + (int)(k % (CHAPTER[ch].v1 - CHAPTER[ch].v0 + 1));
            int moves = CHAPTER[ch].k0 + (int)(k / 3 % (CHAPTER[ch].k1 - CHAPTER[ch].k0 + 1));
            if (make(&pool[k], nv, moves, seed)) k++;
        }
        fprintf(stderr, "chapter %d: %d candidates\n", ch + 1, k);
        if (k < 25) return 1;
        qsort(pool, (size_t)k, sizeof pool[0], by_score);
        for (int i = 0; i < 25; i++) chosen[nchosen++] = pool[(int)((i + 0.5f) / 25 * k)];
    }
    printf("/*\n * Untangle's levels: our own, made by tools/untangle_gen.c (a layout without crossings by the game's maker,\n"
           " * then a few dots moved out of it, kept when no fewer dots can untangle it), six chapters of 25 from the\n"
           " * easiest. Each: \"<dots>:\", the places, where each dot starts, the cords (untangle.h, ut_encode); in the\n"
           " * comment the dots, the cords, the crossings, the par (the dots moved) and the fewest dots that might do.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n"
           "static const char *const ut_levels[UT_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[8 + 4 * UT_MAXV + 2 * UT_MAXE];
        ut_encode(&chosen[i].g, code, sizeof code);
        printf("    \"%s\", /* %d: %d dots, %d cords, %d crossings, par %d, at least %d */\n", code, i + 1,
               chosen[i].g.nv, chosen[i].g.ne, chosen[i].crossings, chosen[i].k, chosen[i].cover);
    }
    printf("};\n");
    return 0;
}
