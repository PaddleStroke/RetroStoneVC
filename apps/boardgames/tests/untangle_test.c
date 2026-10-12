/*
 * Untangle's rules: the crossings (proper, touching, a dot on a cord, cords sharing a dot, running along each
 * other), the solved check, the puzzles made from many seeds (the maker's layout has no crossing, the circle has many,
 * every dot keeps two cords, all joined, the counts, made fast, the same from the same seed), moves kept on the board,
 * hints to the end and undo all the way back.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/untangle.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a small graph: dots at the given places, cords between them */
static void load(ut_game *g, int nv, const float *xy, int ne, const int *e)
{
    memset(g, 0, sizeof *g);
    g->nv = nv;
    g->ne = ne;
    for (int i = 0; i < nv; i++) g->x[i] = g->hx[i] = xy[2 * i], g->y[i] = g->hy[i] = xy[2 * i + 1];
    for (int i = 0; i < ne; i++) g->e[i][0] = (uint8_t)e[2 * i], g->e[i][1] = (uint8_t)e[2 * i + 1];
}

static int joined(const ut_game *g)
{
    int seen[UT_MAXV] = {0}, grew = 1;
    seen[0] = 1;
    while (grew) {
        grew = 0;
        for (int i = 0; i < g->ne; i++)
            if (seen[g->e[i][0]] != seen[g->e[i][1]]) seen[g->e[i][0]] = seen[g->e[i][1]] = 1, grew = 1;
    }
    for (int v = 0; v < g->nv; v++)
        if (!seen[v]) return 0;
    return 1;
}

int main(void)
{
    static ut_game g, h;

    /* segments: an X crosses, parallels and far ones do not, an end on the other one touches, T shapes, collinear */
    CHECK(ut_segments_cross(0, 0, 1, 1, 0, 1, 1, 0));
    CHECK(!ut_segments_cross(0, 0, 1, 0, 0, 0.1f, 1, 0.1f));
    CHECK(!ut_segments_cross(0, 0, 0.4f, 0.4f, 0.6f, 0.6f, 1, 1));   /* on one line, apart */
    CHECK(ut_segments_cross(0, 0, 0.6f, 0, 0.4f, 0, 1, 0));          /* on one line, overlapping */
    CHECK(ut_segments_cross(0, 0, 1, 0, 0.5f, 0, 0.5f, 1));          /* an end on the other: touching */
    CHECK(!ut_segments_cross(0, 0, 1, 0, 0.5f, 0.01f, 0.5f, 1));     /* just above it */
    CHECK(!ut_segments_cross(0, 0, 1, 0, 1.2f, -1, 1.2f, 1));

    /* a square with its two diagonals: they cross; moving one corner inside untangles it */
    static const float sq[] = {0.2f, 0.2f, 0.8f, 0.2f, 0.8f, 0.8f, 0.2f, 0.8f};
    static const int se[] = {0, 1, 1, 2, 2, 3, 3, 0, 0, 2, 1, 3};
    load(&g, 4, sq, 6, se);
    CHECK(ut_cords_cross(&g, 4, 5) && ut_crossings(&g) == 1 && ut_crossed(&g, 4) && !ut_crossed(&g, 0));
    CHECK(!ut_cords_cross(&g, 0, 1));           /* sharing a dot at a corner */
    CHECK(!ut_solved(&g));
    CHECK(ut_move(&g, 3, 0.6f, 0.4f) && ut_solved(&g) && g.moves == 1);     /* inside the triangle 0 1 2 */
    CHECK(ut_undo(&g) == 3 && g.x[3] == 0.2f && g.y[3] == 0.8f && g.moves == 0 && ut_undo(&g) < 0);
    /* a dot on a cord it does not belong to: touching counts */
    CHECK(ut_move(&g, 3, 0.5f, 0.2f) && ut_crossings(&g) > 0);
    /* two cords from one dot, one along the other */
    static const float ln[] = {0.1f, 0.5f, 0.5f, 0.5f, 0.9f, 0.5f};
    static const int le[] = {0, 1, 0, 2};
    load(&g, 3, ln, 2, le);
    CHECK(ut_cords_cross(&g, 0, 1) && !ut_solved(&g));
    g.y[2] = 0.6f;
    CHECK(!ut_cords_cross(&g, 0, 1) && ut_solved(&g));
    /* moves stay on the board; no move, no change */
    CHECK(ut_move(&g, 2, 1.5f, -3) && g.x[2] == 1 - UT_MIN && g.y[2] == UT_MIN);
    CHECK(!ut_move(&g, 2, 2, -1) && !ut_move(&g, 7, 0.5f, 0.5f));
    CHECK(ut_script(&g, "0:0.3,0.3 1:0.6,0.7") && g.moves == 3 && fabsf(g.x[1] - 0.6f) < 1e-6f);
    CHECK(!ut_script(&g, "1:0.6") && !ut_script(&g, "x") && !ut_script(&g, "1:0.6,0.7"));

    /* the puzzles from many seeds */
    static const int dots[UT_VARIANTS] = {10, 20, 30, 45};
    for (int v = 0; v < UT_VARIANTS; v++) {
        double worst = 0, hint_worst = 0;
        int seeds = v == 3 ? 60 : 150, crossings = 0, hints_max = 0;
        for (int seed = 1; seed <= seeds; seed++) {
            clock_t t0 = clock();
            ut_new(&g, v, (uint32_t)seed * 4099u + (uint32_t)v);
            double ms = 1000.0 * (clock() - t0) / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            CHECK(g.nv == dots[v] && g.ne >= g.nv && g.ne <= 3 * g.nv - 6);
            int deg[UT_MAXV] = {0}, dup = 0;
            for (int i = 0; i < g.ne; i++) {
                deg[g.e[i][0]]++, deg[g.e[i][1]]++;
                CHECK(g.e[i][0] != g.e[i][1] && g.e[i][0] < g.nv && g.e[i][1] < g.nv);
                for (int j = 0; j < i; j++)
                    dup += (g.e[i][0] == g.e[j][0] && g.e[i][1] == g.e[j][1]) ||
                           (g.e[i][0] == g.e[j][1] && g.e[i][1] == g.e[j][0]);
            }
            CHECK(!dup && joined(&g));
            for (int d = 0; d < g.nv; d++) CHECK(deg[d] >= 2);
            int k = ut_crossings(&g);
            CHECK(k >= g.nv && !ut_solved(&g));
            crossings += k;
            h = g;                              /* the maker's layout: no crossing */
            memcpy(h.x, g.hx, sizeof h.x);
            memcpy(h.y, g.hy, sizeof h.y);
            CHECK(ut_solved(&h));
            ut_new(&h, v, (uint32_t)seed * 4099u + (uint32_t)v);
            CHECK(!memcmp(h.x, g.x, sizeof g.x) && !memcmp(h.e, g.e, sizeof g.e) && h.ne == g.ne);
            /* played: a few moves, then the hints to the end (each one never leaves more crossings for long), then
             * undo all the way back */
            if (v >= 2 && seed % 10) continue;      /* the hints to the end: a few of the big ones */
            static ut_game start;
            start = g;
            for (int i = 0; i < 3; i++) ut_move(&g, (seed + 5 * i) % g.nv, 0.1f + 0.3f * i, 0.5f);
            int hints = 0, cap = ut_crossings(&g) + 2 * g.nv;
            while (!ut_solved(&g) && hints < cap) {
                t0 = clock();
                int d = ut_hint(&g);
                ms = 1000.0 * (clock() - t0) / CLOCKS_PER_SEC;
                if (ms > hint_worst) hint_worst = ms;
                if (d < 0) break;
                hints++;
            }
            CHECK(ut_solved(&g));
            if (hints > hints_max) hints_max = hints;
            CHECK(ut_hint(&g) < 0);
            while (ut_undo(&g) >= 0) {}
            CHECK(g.moves == 0 && !memcmp(g.x, start.x, sizeof g.x) && !memcmp(g.y, start.y, sizeof g.y));
        }
        printf("untangle %d dots: %d cords, crossings at the start %.1f on average, hints to the end %d at most; "
               "slowest %.1f ms to make, %.1f ms a hint\n",
               dots[v], g.ne, (double)crossings / seeds, hints_max, worst, hint_worst);
        CHECK(worst < 300 && hint_worst < 300);
    }

    if (fails) printf("untangle: %d failures\n", fails);
    else printf("untangle: all passed\n");
    return fails != 0;
}
