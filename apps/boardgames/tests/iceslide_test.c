/*
 * Ice Slide's rules: the slides (stopped by a rock or the edge, caught by the hole on the way), the search against a
 * plain search of every way of a given length, the generator over hundreds of seeds of every variant (the par in the
 * variant's range, the shortest way really reaching the hole in par pushes and none shorter, the time it takes), the
 * hint followed from anywhere, whole games of random pushes from a seed, undo all the way back, the notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/iceslide.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* is there a way of k pushes or fewer from `from` into the hole? every sequence tried, depth first (a place already
 * tried with as many pushes left or more is not tried again) */
static int left_at[ICS_CELLS];

static int within(const ics_board *b, int from, int k)
{
    if (from == b->goal) return 1;
    if (!k || left_at[from] >= k) return 0;
    left_at[from] = k;
    for (int d = 0; d < 4; d++) {
        int len, to = ics_slide(b, from, d, &len);
        if (len && within(b, to, k - 1)) return 1;
    }
    return 0;
}

static int way_within(const ics_board *b, int from, int k)
{
    for (int i = 0; i < ICS_CELLS; i++) left_at[i] = -1;
    return within(b, from, k);
}

int main(void)
{
    ics_game g;
    /* the slides on a small board */
    CHECK(ics_load(&g, 5, 4,
                   "P..#."
                   "....."
                   ".#..O"
                   "....."));
    CHECK(g.puck == 0 && g.b.goal == 14 && !ics_solved(&g));
    int len;
    CHECK(ics_slide(&g.b, 0, ICS_RIGHT, &len) == 2 && len == 2);   /* stopped by the rock */
    CHECK(ics_slide(&g.b, 0, ICS_DOWN, &len) == 15 && len == 3);    /* the bottom edge */
    CHECK(ics_slide(&g.b, 0, ICS_UP, &len) == 0 && len == 0);       /* against the edge: no move */
    CHECK(ics_slide(&g.b, 0, ICS_LEFT, &len) == 0 && len == 0);
    CHECK(ics_slide(&g.b, 4, ICS_DOWN, &len) == 14 && len == 2);    /* down column 4: caught by the hole on the way */
    CHECK(ics_slide(&g.b, 10, ICS_RIGHT, &len) == 10 && len == 0);  /* (10 is ice, the rock at 11) */
    CHECK(!ics_can(&g, ICS_UP) && ics_can(&g, ICS_RIGHT) && ics_can(&g, ICS_DOWN));
    /* the shortest way, against the plain search */
    int par = ics_solve(&g.b, 0, NULL);
    CHECK(par > 0 && par == g.par);
    CHECK(way_within(&g.b, 0, par) && !way_within(&g.b, 0, par - 1));
    /* a push, undo, the counter */
    CHECK(ics_move(&g, ICS_RIGHT) == 2 && g.puck == 2 && g.moves == 1);
    CHECK(!ics_move(&g, ICS_RIGHT) && g.moves == 1);                /* against the rock: nothing */
    CHECK(ics_undo(&g) && g.puck == 0 && g.moves == 0 && !ics_undo(&g));
    /* the notation: "=" plays a shortest way */
    CHECK(ics_script(&g, "=") && ics_solved(&g) && g.moves == par);
    CHECK(!ics_move(&g, ICS_LEFT) && !ics_can(&g, ICS_DOWN));       /* in the hole: nothing moves any more */
    CHECK(ics_load(&g, 5, 4, "P..#./...../.#..O/.....") && ics_script(&g, "r d") && g.moves == 2);
    CHECK(!ics_script(&g, "x"));
    /* a hole that cannot be reached */
    CHECK(ics_load(&g, 3, 3, "P#./##./..O") && g.par == -1 && ics_hint(&g) == -1);

    /* the generator, every variant, many seeds */
    double slowest = 0;
    for (int v = 0; v < ICS_VARIANTS; v++) {
        int w, h, lo, hi, rocks, spread[ICS_LONGEST] = {0};
        ics_variant(v, &w, &h, &lo, &hi, &rocks);
        for (uint32_t seed = 1; seed <= 300; seed++) {
            clock_t t0 = clock();
            ics_new(&g, v, seed);
            double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
            if (ms > slowest) slowest = ms;
            CHECK(g.b.w == w && g.b.h == h && g.variant == v);
            CHECK(g.par >= lo && g.par <= hi);
            CHECK(g.start != g.b.goal && !g.b.rock[g.start] && !g.b.rock[g.b.goal] && g.puck == g.start);
            CHECK(ics_solve(&g.b, g.start, NULL) == g.par);
            ics_board open = g.b;
            open.goal = -1;
            CHECK(ics_reach(&open, g.start) >= lo + 3);
            CHECK(!way_within(&g.b, g.start, g.par - 1));   /* none shorter */
            if (g.par < ICS_LONGEST) spread[g.par]++;
            /* the shortest way played: in the hole in par pushes */
            ics_game t = g;
            CHECK(ics_script(&t, "=") && ics_solved(&t) && t.moves == g.par);
            /* the same seed: the same puzzle */
            ics_game again;
            ics_new(&again, v, seed);
            CHECK(!memcmp(&again.b, &g.b, sizeof g.b) && again.start == g.start);
        }
        printf("iceslide: variant %d (%dx%d, %d..%d): pars", v, w, h, lo, hi);
        for (int k = lo; k <= hi; k++) printf(" %d:%d", k, spread[k]);
        printf("\n");
    }
    printf("iceslide: the slowest puzzle made in %.1f ms\n", slowest);
    CHECK(slowest < 250);

    /* whole games of random pushes from a seed: the hint from wherever the puck rests, undo all the way back */
    for (uint32_t seed = 1; seed <= 200; seed++) {
        uint32_t s = pr_seed(seed * 7 + 3);
        ics_new(&g, (int)(seed % ICS_VARIANTS), seed);
        int pushes = 0;
        while (!ics_solved(&g) && pushes < 120) {
            int d = pr_below(&s, 4), can = ics_can(&g, d), before = g.puck;
            int moved = ics_move(&g, d);
            CHECK(moved == can);
            if (moved) pushes++;
            else CHECK(g.puck == before);
            if (!ics_solved(&g) && pushes % 9 == 0) {   /* the hint followed from here */
                ics_game t = g;
                int left = ics_solve(&t.b, t.puck, NULL), k = 0;
                while (!ics_solved(&t) && k < 80) {
                    int hd = ics_hint(&t);
                    if (hd < 0) break;
                    CHECK(ics_move(&t, hd));
                    k++;
                }
                if (left >= 0) CHECK(ics_solved(&t) && k == left);
                else CHECK(ics_hint(&t) == -1 && k == 0);
            }
        }
        CHECK(g.moves == pushes);
        while (ics_undo(&g)) {}
        CHECK(g.puck == g.start && g.moves == 0);
    }
    /* the levels: each one loads, its par is the one kept, no way is shorter, the shortest way reaches the hole; the
     * ice never shrinks from one chapter to the next; no two alike */
    int last_cells = 0;
    for (int l = 0; l < ICS_LEVELS; l++) {
        CHECK(ics_level(&g, l));
        CHECK(g.par > 0 && g.par == ics_level_par[l] && g.puck == g.start && g.moves == 0);
        CHECK(!way_within(&g.b, g.start, g.par - 1));
        ics_game t = g;
        CHECK(ics_script(&t, "=") && ics_solved(&t) && t.moves == g.par);
        if (l % (ICS_LEVELS / ICS_CHAPTERS) == 0) {
            CHECK(g.b.w * g.b.h >= last_cells);
            last_cells = g.b.w * g.b.h;
        }
        for (int k = 0; k < l; k++) CHECK(strcmp(ics_levels[k], ics_levels[l]) != 0);
    }
    CHECK(!ics_level(&g, ICS_LEVELS) && !ics_level(&g, -1));

    if (fails) { printf("iceslide: %d failed\n", fails); return 1; }
    printf("iceslide: all passed\n");
    return 0;
}
