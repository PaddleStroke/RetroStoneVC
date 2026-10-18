/*
 * Gears' rules: meshing, hitting, the board's edge; the train's ways round and a jam on an odd loop; targets and
 * shafts; put, take off, move, undo; boards as text both ways; every level's par found again by the search and its
 * solution checked by the rules, with the fewest gears; the hint's steps from the start reaching the end in the par;
 * boards made at random (their par in range, the same from the same seed, the time they take); whole games from a
 * seed (random gears put and taken off, then hints to the end).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/gears.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the hints from where g stands to the end: how many steps (-1 not solved within `most`) */
static int hint_to_end(gea_game *g, int most)
{
    for (int k = 0; k <= most; k++) {
        if (gea_solved(&g->m, &g->s)) return k;
        int peg, size;
        if (!gea_hint(&g->m, &g->s, g->par, &peg, &size) || !gea_put(g, peg, size)) return -1;
    }
    return gea_solved(&g->m, &g->s) ? most : -1;
}

int main(void)
{
    gea_game g;
    gea_map m;
    char text[GEA_TEXT];

    /* meshing and hitting: a motor (medium) at (3, 3); pegs 3 away (a small meshes), 2 away (a clash), 4 and 4
     * away (clear) */
    CHECK(!gea_parse(&m, "mm6Md d2Pgd0Pfd0Phh0Tjd1"));  /* a space: not a board */
    CHECK(!gea_parse(&m, "mm6Mdd2Pgd0Pfd0Phh0Tjd1x"));
    CHECK(gea_parse(&m, "mm6Mdd2Pgd0Pfd0Phh0Tjd1"));
    gea_text(&m, text);
    CHECK(!strcmp(text, "mm6Mdd2Pgd0Pfd0Phh0Tjd1"));
    CHECK(gea_rel(&m, 0, 2, 1, 1) == GEA_MESH);          /* 3 apart: medium + small */
    CHECK(gea_rel(&m, 0, 2, 1, 2) == GEA_CLASH);         /* medium + medium need 4 */
    CHECK(gea_rel(&m, 0, 2, 2, 1) == GEA_CLASH);         /* 2 apart: hit */
    CHECK(gea_rel(&m, 0, 2, 3, 3) == GEA_FREE);          /* 4, 4 apart: 5.66, clear of 5 + 0.4 */
    g.m = m;
    gea_begin(&g);
    CHECK(gea_fits(&g.m, &g.s, 0, 1) == GEA_TAKEN);      /* the motor's peg */
    CHECK(gea_fits(&g.m, &g.s, 2, 1) == GEA_COLLIDES);
    CHECK(gea_fits(&g.m, &g.s, 1, 3) == GEA_OUTSIDE);    /* not in the tray (sizes 1 and 2) */
    CHECK(gea_put(&g, 1, 1));
    gea_eval_t e;
    gea_eval(&g.m, &g.s, &e);
    CHECK(e.train[1] && e.dir[1] == -1 && e.dir[0] == 1 && !e.jam);
    CHECK(!e.solved && e.put == 1 && e.idle == 0);
    CHECK(gea_undo(&g) && !g.s.g[1] && !gea_undo(&g));

    /* an odd loop jams: a medium motor, a small 3 to its right, a large 4 below the small, 5 from the motor */
    CHECK(gea_parse(&g.m, "lm>Mdd2Pgd0Pgh0Tkh1"));
    gea_begin(&g);
    CHECK(gea_put(&g, 1, 1));
    CHECK(gea_put(&g, 2, 3));
    gea_eval(&g.m, &g.s, &e);
    CHECK(e.jam && e.dir[0] == 0 && e.dir[1] == 0 && e.dir[2] == 0 && !e.solved);
    CHECK(gea_put(&g, 1, 0));                             /* the small off: the large is driven by the motor alone */
    gea_eval(&g.m, &g.s, &e);
    CHECK(!e.jam && e.dir[2] == -1);
    /* the target (small, at 10, 7) meshes with the large (3 + 1 = 4 from 7, 7): clockwise, as asked */
    CHECK(e.dir[3] == 1 && e.ok[3] && e.solved);
    CHECK(gea_count(&g.m, &g.s) == 1);
    CHECK(gea_solve(&g.m, NULL, 0, NULL, NULL) == 1);
    CHECK(gea_shift(&g, 2, 1) == 0);                      /* a large at (6, 3): hits the motor */

    /* a shaft: right only with the size its ratio asks for */
    CHECK(gea_parse(&g.m, "kh6Mdd2Sgd1Pgg0"));
    gea_begin(&g);
    CHECK(gea_put(&g, 1, 1));
    gea_eval(&g.m, &g.s, &e);
    CHECK(e.dir[1] == -1 && !e.ok[1]);                   /* turning, the wrong way */
    CHECK(gea_parse(&g.m, "kh6Mdd2sgd1Pgg0"));
    gea_begin(&g);
    CHECK(gea_put(&g, 1, 1));
    CHECK(gea_solved(&g.m, &g.s));
    CHECK(gea_script(&g, "b0 b1") && gea_solved(&g.m, &g.s));
    CHECK(!gea_script(&g, "b2"));                         /* taken already */
    /* an idle gear spoils the win */
    CHECK(gea_parse(&g.m, "kn6Mdd2sgd1Pgk0"));
    gea_begin(&g);
    CHECK(gea_script(&g, "b1 c1"));
    gea_eval(&g.m, &g.s, &e);
    CHECK(e.idle == 1 && !e.solved);

    /* every level: a board, its par found again, the solution the rules accept with that many gears, the hints from
     * the start taking the par exactly */
    int worst_hint = 0;
    for (int l = 0; l < GEA_LEVELS; l++) {
        CHECK(gea_level(&g, l));
        gea_text(&g.m, text);
        CHECK(!strcmp(text, gea_levels[l].text));
        gea_state sol;
        long nodes = 0;
        int p = gea_solve(&g.m, &sol, 0, &nodes, NULL);
        if (p != g.par) printf("level %d: par %d, found %d\n", l + 1, g.par, p);
        CHECK(p == g.par && p >= 1);
        CHECK(gea_solved(&g.m, &sol) && gea_count(&g.m, &sol) == p);
        clock_t t0 = clock();
        int steps = hint_to_end(&g, 3 * g.par + 4);
        int ms = (int)((clock() - t0) * 1000 / CLOCKS_PER_SEC);
        worst_hint = ms > worst_hint ? ms : worst_hint;
        if (steps != g.par) printf("level %d: the hints took %d steps for par %d\n", l + 1, steps, g.par);
        CHECK(steps == g.par);
        CHECK(gea_count(&g.m, &g.s) == g.par);
        for (int k = 0; k < l; k++) CHECK(strcmp(gea_levels[k].text, gea_levels[l].text));
    }
    printf("gears: %d levels solved at par, the hints the slowest %d ms\n", GEA_LEVELS, worst_hint);

    /* boards at random: a par in the variant's range (or near it), the same from the same seed, quick */
    for (int v = 0; v < GEA_VARIANTS; v++) {
        int lo = 99, hi = 0;
        double worst = 0;
        for (uint32_t seed = 1; seed <= 40; seed++) {
            clock_t t0 = clock();
            gea_random(&g, v, seed);
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            worst = ms > worst ? ms : worst;
            CHECK(g.par >= 1);
            CHECK(gea_solve(&g.m, NULL, 0, NULL, NULL) == g.par);
            gea_game t;
            gea_random(&t, v, seed);
            CHECK(!memcmp(&t.m, &g.m, sizeof t.m) && t.par == g.par);
            lo = g.par < lo ? g.par : lo;
            hi = g.par > hi ? g.par : hi;
        }
        printf("gears: random variant %d, pars %d .. %d, the slowest %.0f ms\n", v, lo, hi, worst);
        CHECK(worst < 1500);
    }

    /* whole games from a seed: gears put and taken off at random, then the hints to the end; undo to the start */
    for (uint32_t seed = 1; seed <= 60; seed++) {
        uint32_t r = pr_seed(seed);
        CHECK(gea_level(&g, (int)pr_below(&r, GEA_LEVELS)));
        int moves = 0;
        for (int k = 0; k < 12; k++) {
            int peg = pr_below(&r, g.m.n), size = pr_below(&r, GEA_SIZES + 1);
            moves += gea_put(&g, peg, size);
        }
        int steps = hint_to_end(&g, 40);
        CHECK(steps >= 0);
        CHECK(gea_solved(&g.m, &g.s));
        moves += steps;
        while (gea_undo(&g)) moves--;
        CHECK(moves == 0);
        CHECK(gea_count(&g.m, &g.s) == 0);
    }

    printf(fails ? "gears: %d FAILED\n" : "gears: all passed\n", fails);
    return fails != 0;
}
