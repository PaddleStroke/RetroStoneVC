/*
 * Number Puzzles' rules: the fractions and the operations, merges and undo, the solver (the 1362 sets of four
 * numbers 1..13 that make 24, the ones needing a fraction), every designed level (it can be done: "=" plays a way
 * to the end; its game and its rating as its chapter says), the puzzles made at random for each variant over many
 * seeds (possible, played to the end with hints, fast enough), Target's "done" and the stars.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/mathpz.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

int main(void)
{
    npz_q r;
    /* the operations */
    CHECK(npz_apply(NPZ_FOUR, (npz_q){8, 1}, NPZ_DIV, (npz_q){3, 1}, &r) && r.n == 8 && r.d == 3);
    CHECK(npz_apply(NPZ_FOUR, (npz_q){3, 1}, NPZ_SUB, (npz_q){8, 3}, &r) && r.n == 1 && r.d == 3);
    CHECK(npz_apply(NPZ_FOUR, (npz_q){8, 1}, NPZ_DIV, (npz_q){1, 3}, &r) && r.n == 24 && r.d == 1);
    CHECK(npz_apply(NPZ_FOUR, (npz_q){2, 1}, NPZ_SUB, (npz_q){5, 1}, &r) && r.n == -3 && r.d == 1);
    CHECK(!npz_apply(NPZ_FOUR, (npz_q){2, 1}, NPZ_DIV, (npz_q){0, 1}, &r));
    CHECK(!npz_apply(NPZ_TARGET, (npz_q){7, 1}, NPZ_DIV, (npz_q){2, 1}, &r));    /* Target: exact divisions */
    CHECK(!npz_apply(NPZ_TARGET, (npz_q){2, 1}, NPZ_SUB, (npz_q){5, 1}, &r));    /* and above zero */
    CHECK(!npz_apply(NPZ_TARGET, (npz_q){5, 1}, NPZ_SUB, (npz_q){5, 1}, &r));

    /* 3 3 8 8: 8 / (3 - 8 / 3), by hand, then undone all the way */
    npz_game g;
    int n3388[NPZ_MAX] = {3, 3, 8, 8};
    npz_load(&g, NPZ_FOUR, n3388, 24);
    CHECK(npz_script(&g, "c/a"));              /* a: gone, slot a's 3 into c: 8/3 */
    CHECK(g.v[0].n == 8 && g.v[0].d == 3 && !g.live[2]);
    CHECK(npz_script(&g, "b-a d/a"));
    CHECK(npz_solved(&g) && npz_stars(&g) == 3 && g.mistakes == 0);
    CHECK(npz_undo(&g) && npz_undo(&g) && npz_undo(&g) && !npz_undo(&g));
    CHECK(npz_left(&g) == 4 && g.v[0].n == 3 && g.v[2].n == 8 && !g.made[0]);
    CHECK(!npz_merge(&g, 0, NPZ_ADD, 0) && !npz_merge(&g, 0, NPZ_ADD, 6));
    /* a wrong end: a mistake, kept after undo; a hint then undoes */
    CHECK(npz_script(&g, "a+b b+c c+d") && npz_stuck(&g) && g.mistakes == 1);
    CHECK(npz_hint(&g, NULL) == 2 && npz_left(&g) == 2 && g.mistakes == 1);
    for (int k = 0; k < 12 && !npz_solved(&g); k++) npz_hint(&g, NULL);
    CHECK(npz_solved(&g) && npz_stars(&g) == 1);

    /* the solver: every set of four numbers 1..13 */
    int solvable = 0, frac = 0;
    for (int a = 1; a <= 13; a++)
        for (int b = a; b <= 13; b++)
            for (int c = b; c <= 13; c++)
                for (int d = c; d <= 13; d++) {
                    int q[NPZ_MAX] = {a, b, c, d};
                    npz_rating rt;
                    npz_rate(NPZ_FOUR, q, 24, &rt);
                    solvable += rt.ways > 0;
                    frac += rt.ways > 0 && !rt.nice;
                    CHECK(rt.nice <= rt.ways && rt.plain <= rt.nice);
                }
    CHECK(solvable == 1362);
    CHECK(frac == 16);
    int n1111[NPZ_MAX] = {1, 1, 1, 1};
    npz_load(&g, NPZ_FOUR, n1111, 24);
    CHECK(!npz_next(&g, NULL) && !npz_hint(&g, NULL));

    /* every level */
    int bad = 0;
    for (int l = 0; l < NPZ_LEVELS; l++) {
        int nums[NPZ_MAX], t, kind = npz_level_kind(l), ch = l / NPZ_PER_CHAPTER % 3;
        CHECK(npz_level_nums(l, nums, &t) == npz_tiles(kind));
        CHECK(kind == l / 150);
        npz_rating rt;
        npz_rate(kind, nums, t, &rt);
        if (kind == NPZ_TARGET) {
            if (t < 101 || t > 999 || (ch < 2 ? rt.fewest != 2 + ch : rt.fewest < 4))
                bad++, printf("level %d: target %d, fewest %d\n", l + 1, t, rt.fewest);
        } else {
            if (!rt.ways) bad++, printf("level %d: no way\n", l + 1);
            if (ch < 2 && !rt.nice) bad++, printf("level %d: needs a fraction early\n", l + 1);
            if (kind == NPZ_FOUR && ch == 0 && (t != 24 || !rt.plain)) bad++, printf("level %d: not easy\n", l + 1);
        }
        npz_level(&g, l);
        if (!npz_script(&g, "=") || !npz_solved(&g) || npz_stars(&g) != 3) bad++, printf("level %d: '='\n", l + 1);
        for (int k = 0; k < l; k++) {           /* never twice */
            int o[NPZ_MAX], ot;
            npz_level_nums(k, o, &ot);
            if (ot == t && !memcmp(o, nums, sizeof o)) bad++, printf("levels %d and %d are the same\n", k + 1, l + 1);
        }
    }
    CHECK(bad == 0);

    /* random puzzles: possible, solved by hints alone, fast */
    for (int v = 0; v < NPZ_VARIANTS; v++) {
        clock_t t0 = clock();
        double worst = 0;
        for (uint32_t seed = 1; seed <= 30; seed++) {
            clock_t s0 = clock();
            npz_new(&g, v, seed);
            double dt = (double)(clock() - s0) / CLOCKS_PER_SEC;
            if (dt > worst) worst = dt;
            CHECK(g.kind == v / NPZ_TIERS);
            npz_game h = g;
            int steps = 0;
            while (!npz_solved(&h) && steps++ < 20) npz_hint(&h, NULL);
            CHECK(npz_solved(&h));
            CHECK(npz_stars(&h) == (h.hints <= 2 ? 2 : 1));
            if (g.kind == NPZ_TARGET) CHECK(g.target >= 101 && g.target <= 999);
            npz_game k;                         /* the same seed: the same puzzle */
            npz_new(&k, v, seed);
            CHECK(!memcmp(k.v, g.v, sizeof k.v) && k.target == g.target);
        }
        printf("variant %d: 30 puzzles in %.2f s, the slowest %.3f s\n", v, (double)(clock() - t0) / CLOCKS_PER_SEC,
               worst);
        CHECK(worst < 0.5);
    }

    /* Target: near, done, the stars by the distance */
    int tn[NPZ_MAX] = {100, 75, 2, 3, 4, 5};
    npz_load(&g, NPZ_TARGET, tn, 181);
    CHECK(!npz_can_finish(&g));
    CHECK(npz_script(&g, "a+b"));               /* 175: six off */
    CHECK(npz_can_finish(&g) && !npz_solved(&g) && npz_off(&g) == 6);
    CHECK(npz_finish(&g) && npz_solved(&g) && npz_stars(&g) == 1);
    CHECK(npz_undo(&g) && !npz_solved(&g) && npz_left(&g) == 5);
    CHECK(npz_script(&g, "d+b"));               /* 178: three off */
    CHECK(npz_script(&g, "!") && npz_stars(&g) == 2);
    CHECK(npz_script(&g, "u c+b"));             /* 180 */
    CHECK(npz_script(&g, "u u"));
    CHECK(npz_script(&g, "=") && npz_solved(&g) && npz_off(&g) == 0 && npz_stars(&g) == 3);
    CHECK(!npz_script(&g, "a?b") && !npz_script(&g, "zz"));

    if (fails) printf("%d FAILED\n", fails);
    else printf("mathpz: all good\n");
    return fails != 0;
}
