/*
 * Colour Mixer's rules: the paints mix as pigments (red and yellow orange, yellow and blue green, red and blue
 * violet, white lighter, black darker, only the ratios counting); drops, tubes running dry, undo, a drop taken back;
 * boards as text both ways; every level's par found again by the search, its recipe exact, no other recipe of as few
 * drops near it; the hints from the start reaching the end in the par; boards made at random (their par, the same
 * from the same seed, the time they take); whole games from a seed (random drops, then hints to the end).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/colourmix.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static void mix(float r, float y, float b, float w, float k, float c[3])
{
    const float a[CMX_BASE] = {r, y, b, w, k};
    cmx_colour(a, c);
}

static float lum(const float c[3]) { return 0.3f * c[0] + 0.59f * c[1] + 0.11f * c[2]; }

static int hint_to_end(cmx_game *g, int most)
{
    for (int k = 0; k <= most; k++) {
        if (cmx_solved(&g->m, &g->s)) return k;
        int t, take;
        if (!cmx_hint(&g->m, &g->s, &t, &take)) return -1;
        if (take ? !cmx_take(g, t) : !cmx_drop(g, t)) return -1;
    }
    return cmx_solved(&g->m, &g->s) ? most : -1;
}

int main(void)
{
    float c[3], d[3];
    /* pigments, not light */
    mix(1, 1, 0, 0, 0, c);
    CHECK(c[0] > 0.8f && c[1] > 0.35f && c[1] < 0.7f && c[2] < 0.2f);   /* orange */
    mix(0, 1, 1, 0, 0, c);
    CHECK(c[1] > c[0] + 0.2f && c[1] > c[2] + 0.2f);                    /* green */
    mix(1, 0, 1, 0, 0, c);
    CHECK(c[0] > c[1] + 0.2f && c[2] > c[1] + 0.2f);                    /* violet */
    mix(1, 1, 1, 0, 0, c);
    CHECK(lum(c) < 0.3f);                                               /* all three: dark */
    mix(1, 0, 0, 0, 0, d);
    mix(1, 0, 0, 1, 0, c);
    CHECK(lum(c) > lum(d) + 0.15f && c[0] > c[1]);                      /* pink */
    mix(1, 0, 0, 0, 1, c);
    CHECK(lum(c) < 0.6f * lum(d));                                      /* a little black goes far */
    mix(2, 2, 0, 0, 0, c);
    mix(1, 1, 0, 0, 0, d);
    CHECK(cmx_de(c, d) < 1e-3f);                                        /* the ratios only */
    const float none[CMX_BASE] = {0};
    CHECK(!cmx_colour(none, c));

    /* a board: red and yellow to orange, red holding one drop */
    cmx_game g;
    char text[CMX_TEXT];
    CHECK(!cmx_parse(&g.m, "ry/--/00"));
    CHECK(!cmx_parse(&g.m, "rx/--/00/11"));
    CHECK(cmx_parse(&g.m, "ry/1-/00/11"));
    cmx_text(&g.m, text);
    CHECK(!strcmp(text, "ry/1-/00/11"));
    cmx_begin(&g);
    CHECK(cmx_dist(&g.m, g.s.add) >= 1000);                             /* an empty pot */
    CHECK(cmx_drop(&g, 0) && !cmx_drop(&g, 0));                         /* the red tube is dry */
    CHECK(cmx_left(&g.m, &g.s, 0) == 0 && cmx_left(&g.m, &g.s, 1) == CMX_ANY);
    CHECK(!cmx_solved(&g.m, &g.s));
    CHECK(cmx_drop(&g, 1) && cmx_solved(&g.m, &g.s) && cmx_count(&g.s) == 2);
    CHECK(cmx_drop(&g, 1) && !cmx_solved(&g.m, &g.s));                  /* too yellow */
    int t, take;
    CHECK(cmx_hint(&g.m, &g.s, &t, &take) && take && t == 1);           /* take the yellow back */
    CHECK(cmx_take(&g, 1) && cmx_solved(&g.m, &g.s));
    CHECK(cmx_undo(&g) && cmx_undo(&g) && !cmx_undo(&g) && cmx_count(&g.s) == 0);
    CHECK(cmx_script(&g, "ab") && cmx_solved(&g.m, &g.s));
    CHECK(!cmx_script(&g, "a"));                                        /* dry */
    /* a pot holding blue: yellow makes it green */
    CHECK(cmx_parse(&g.m, "yb/--/01/10"));
    cmx_begin(&g);
    CHECK(cmx_dist(&g.m, g.s.add) > 10);
    CHECK(cmx_solve(&g.m, g.s.add, 4, NULL) == 1);

    /* every level */
    int worst = 0;
    for (int l = 0; l < CMX_LEVELS; l++) {
        CHECK(cmx_level(&g, l));
        cmx_text(&g.m, text);
        CHECK(!strcmp(text, cmx_levels[l].text));
        int total = 0;
        for (int i = 0; i < g.m.n; i++) total += g.m.recipe[i];
        CHECK(total == g.par && g.par >= 1);
        CHECK(cmx_dist(&g.m, g.m.recipe) < 0.05f);                     /* the recipe is the colour */
        int p = cmx_solve(&g.m, g.s.add, g.par + 1, NULL);
        if (p != g.par) printf("level %d: par %d, found %d\n", l + 1, g.par, p);
        CHECK(p == g.par);
        CHECK(cmx_rivals(&g.m, g.par, 2.2f * CMX_TOL) == 0);
        clock_t t0 = clock();
        int steps = hint_to_end(&g, 3 * g.par);
        int ms = (int)((clock() - t0) * 1000 / CLOCKS_PER_SEC);
        worst = ms > worst ? ms : worst;
        if (steps != g.par) printf("level %d: the hints took %d steps for par %d\n", l + 1, steps, g.par);
        CHECK(steps == g.par);
        for (int k = 0; k < l; k++) CHECK(strcmp(cmx_levels[k].text, cmx_levels[l].text));
    }
    printf("colourmix: %d levels at par, the hints the slowest %d ms\n", CMX_LEVELS, worst);

    /* boards at random */
    for (int v = 0; v < CMX_VARIANTS; v++) {
        int lo = 99, hi = 0;
        double slow = 0;
        for (uint32_t seed = 1; seed <= 30; seed++) {
            clock_t t0 = clock();
            cmx_random(&g, v, seed);
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            slow = ms > slow ? ms : slow;
            CHECK(g.par >= 1 && cmx_solve(&g.m, g.s.add, g.par, NULL) == g.par);
            cmx_game h;
            cmx_random(&h, v, seed);
            CHECK(!memcmp(&h.m, &g.m, sizeof h.m) && h.par == g.par);
            lo = g.par < lo ? g.par : lo;
            hi = g.par > hi ? g.par : hi;
        }
        printf("colourmix: random variant %d, pars %d .. %d, the slowest %.0f ms\n", v, lo, hi, slow);
        CHECK(slow < 1500);
    }

    /* whole games from a seed: drops at random, then the hints to the end; undo to the start */
    for (uint32_t seed = 1; seed <= 60; seed++) {
        uint32_t r = pr_seed(seed);
        CHECK(cmx_level(&g, (int)pr_below(&r, CMX_LEVELS)));
        int n = 1 + pr_below(&r, 6);
        for (int k = 0; k < n; k++) cmx_drop(&g, pr_below(&r, g.m.n));
        CHECK(hint_to_end(&g, 40) >= 0);
        CHECK(cmx_solved(&g.m, &g.s));
        while (cmx_undo(&g)) {}
        CHECK(cmx_count(&g.s) == 0);
        for (int i = 0; i < g.m.n; i++) CHECK(g.s.add[i] == 0);
    }

    printf(fails ? "colourmix: %d FAILED\n" : "colourmix: all passed\n", fails);
    return fails != 0;
}
