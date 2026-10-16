/*
 * Spot the Difference's rules: the palette (its colours pairwise apart, for every eye where a change rests on colour
 * alone), every level (exactly its differences, on distinct cells, each a fair one; no two levels alike), the random
 * puzzles of every variant from many seeds (and how long the biggest takes), the levels' text, and the play: finds,
 * mistakes, hints, undo, the notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/spotdiff.h"
#include "games/spotdiff_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the cells where the two grids differ, counted from what each grid shows */
static int differing(const sdf_puzzle *p, int *distinct_ok)
{
    int n = 0;
    *distinct_ok = 1;
    for (int i = 0; i < p->w * p->h; i++) {
        int c0, s0, c1, s1, k = 0;
        sdf_cell(p, 0, i, &c0, &s0);
        sdf_cell(p, 1, i, &c1, &s1);
        if (c0 != c1 || s0 != s1) n++;
        for (int j = 0; j < p->n; j++) k += p->d[j].at == i;
        if (k > 1) *distinct_ok = 0;
        if ((k == 1) != (c0 != c1 || s0 != s1)) *distinct_ok = 0;
    }
    return n;
}

/* a puzzle every way right: what sdf_check says, and the grids' own count */
static int sound(const sdf_puzzle *p)
{
    int ok;
    if (!sdf_check(p) || differing(p, &ok) != p->n || !ok) return 0;
    for (int k = 0; k < p->n; k++) {
        int b = p->base[p->d[k].at], c = p->d[k].c;
        if (sdf_kind(p, k) == SDF_COLOUR && sdf_cvd_dist(b, c) < 22) return 0;
        if (sdf_kind(p, k) == SDF_SYMBOL && (p->d[k].s == b || !b)) return 0;
    }
    return 1;
}

int main(void)
{
    /* the palette: every pair apart for normal eyes, and apart for every eye at least a little (the symbols do the
     * rest); colour-only changes allowed only between pairs far apart for every eye, and some exist */
    float least = 1e9f, least_normal = 1e9f;
    int fair = 0;
    for (int a = 1; a < SDF_COLOURS; a++)
        for (int b = a + 1; b < SDF_COLOURS; b++) {
            float d = sdf_cvd_dist(a, b);
            if (d < least) least = d;
            CHECK(memcmp(sdf_rgb[a], sdf_rgb[b], 3) != 0);
            CHECK(sdf_pair_ok(a, b) == sdf_pair_ok(b, a));
            fair += sdf_pair_ok(a, b);
            int dr = sdf_rgb[a][0] - sdf_rgb[b][0], dg = sdf_rgb[a][1] - sdf_rgb[b][1], db = sdf_rgb[a][2] - sdf_rgb[b][2];
            float rgb = (float)(dr * dr + dg * dg + db * db);
            if (rgb < least_normal) least_normal = rgb;
        }
    CHECK(least > 5);
    CHECK(least_normal > 60 * 60);
    CHECK(fair >= 12);
    CHECK(!sdf_pair_ok(3, 3) && !sdf_pair_ok(0, 3));
    printf("palette: least distance for every eye %.1f, %d fair colour pairs of 36\n", least, fair);

    /* every level */
    int kinds[SDF_KINDS] = {0};
    for (int l = 0; l < SDF_LEVELS; l++) {
        sdf_puzzle p, q;
        int ok = sdf_decode(sdf_levels[l], &p);
        CHECK(ok);
        if (!ok) {
            printf("  level %d does not decode\n", l + 1);
            continue;
        }
        CHECK(sound(&p));
        CHECK(p.n >= 3 && p.n <= SDF_MAXD);
        for (int k = 0; k < p.n; k++) kinds[sdf_kind(&p, k)]++;
        char s[SDF_CELLS + 80];
        CHECK(sdf_encode(&p, s, sizeof s) > 0 && !strcmp(s, sdf_levels[l]));
        CHECK(sdf_decode(s, &q) && !memcmp(&p, &q, sizeof p));
        if (l) {                                /* sizes and counts never go down */
            sdf_puzzle o;
            sdf_decode(sdf_levels[l - 1], &o);
            CHECK(p.w * p.h >= o.w * o.h);
            CHECK(p.n >= o.n);
        }
        for (int m = 0; m < l; m++) {           /* no two alike */
            sdf_puzzle o;
            sdf_decode(sdf_levels[m], &o);
            CHECK(!(o.w == p.w && o.h == p.h && !memcmp(o.base, p.base, (size_t)p.w * p.h)));
        }
    }
    CHECK(kinds[SDF_SYMBOL] > 50 && kinds[SDF_COLOUR] > 30);
    printf("levels: %d differences changing both, %d the symbol, %d the colour\n", kinds[SDF_BOTH], kinds[SDF_SYMBOL],
           kinds[SDF_COLOUR]);
    {                                           /* the first chapter: plain changes only, each a drawing of its own */
        sdf_puzzle p;
        for (int l = 0; l < SDF_LEVELS / SDF_CHAPTERS; l++) {
            sdf_decode(sdf_levels[l], &p);
            for (int k = 0; k < p.n; k++) CHECK(sdf_kind(&p, k) == SDF_BOTH);
        }
    }

    /* random puzzles: hundreds of seeds each variant, the biggest timed */
    for (int v = 0; v < SDF_VARIANTS; v++) {
        clock_t t0 = clock();
        int bad = 0;
        for (uint32_t seed = 1; seed <= 300; seed++) {
            sdf_puzzle p, q;
            sdf_make_variant(&p, v, seed);
            sdf_make_variant(&q, v, seed);
            int w, h;
            sdf_variant_size(v, &w, &h);
            if (!sound(&p) || p.w != w || p.h != h || memcmp(&p, &q, sizeof p)) bad++;
        }
        double ms = (double)(clock() - t0) * 1000 / CLOCKS_PER_SEC / 300;
        CHECK(bad == 0);
        CHECK(ms < 50);
        printf("variant %d: 300 puzzles, %d bad, %.2f ms each\n", v, bad, ms);
    }

    /* the text refuses what is wrong */
    {
        sdf_puzzle p;
        CHECK(!sdf_decode("", &p));
        CHECK(!sdf_decode("4x4:0000000000000000:", &p));          /* no difference */
        CHECK(!sdf_decode("4x4:0000000000000000:000100", &p));    /* no change */
        CHECK(!sdf_decode("4x4:0000000000000000:000002", &p));    /* the wood with a symbol */
        CHECK(!sdf_decode("4x4:0000000000000000:000020", &p));    /* a colour without one */
        CHECK(sdf_decode("4x4:1000000000000000:000012", &p) && sdf_kind(&p, 0) == SDF_SYMBOL);
        CHECK(sdf_decode("4x4:1000000000000000:000199", &p) && sdf_kind(&p, 0) == SDF_BOTH);
        CHECK(!sdf_decode("4x4:1000000000000000:000012000113", &p));   /* two on one cell */
        CHECK(sdf_decode("4x4:0000000000000000:000033", &p) && sdf_kind(&p, 0) == SDF_BOTH);   /* a cell painted */
        int ua = 0, ub = 0;                     /* a colour-only change some eyes cannot see: refused */
        for (int a = 1; a < SDF_COLOURS && !ua; a++)
            for (int b = 1; b < SDF_COLOURS && !ua; b++)
                if (a != b && !sdf_pair_ok(a, b)) ua = a, ub = b;
        if (ua) {
            char s[64];
            snprintf(s, sizeof s, "4x4:%d000000000000000:0000%d%d", ua, ub, ua);
            CHECK(!sdf_decode(s, &p));
            int fa = 0, fb = 0;
            for (int a = 1; a < SDF_COLOURS && !fa; a++)
                for (int b = 1; b < SDF_COLOURS && !fa; b++)
                    if (sdf_pair_ok(a, b)) fa = a, fb = b;
            snprintf(s, sizeof s, "4x4:%d000000000000000:0000%d%d", fa, fb, fa);
            CHECK(sdf_decode(s, &p) && sdf_kind(&p, 0) == SDF_COLOUR);
        }
    }

    /* the play */
    {
        sdf_puzzle p;
        sdf_game g;
        CHECK(sdf_decode(sdf_levels[0], &p));
        sdf_load(&g, &p);
        int wrong = -1;
        for (int i = 0; i < p.w * p.h && wrong < 0; i++)
            if (sdf_diff_at(&p, i) < 0) wrong = i;
        CHECK(sdf_tap(&g, wrong) == -1 && g.mistakes == 1);
        CHECK(sdf_tap(&g, p.d[1].at) == 1 && g.nfound == 1);
        CHECK(sdf_tap(&g, p.d[1].at) == -2 && g.nfound == 1 && g.mistakes == 1);
        CHECK(sdf_hint(&g) == 0 && g.hints == 1 && g.found[0] == 2);
        CHECK(sdf_undo(&g) == 0 && !g.found[0] && g.hints == 1);
        CHECK(sdf_undo(&g) == 1 && sdf_undo(&g) == -1);
        CHECK(!sdf_solved(&g));
        for (int k = 0; k < p.n; k++) CHECK(sdf_tap(&g, p.d[k].at) == k);
        CHECK(sdf_solved(&g) && g.mistakes == 1);
        CHECK(sdf_tap(&g, wrong) == -2 && g.mistakes == 1);   /* solved: taps do nothing */

        sdf_load(&g, &p);                       /* the notation */
        char cell[8];
        snprintf(cell, sizeof cell, "%c%d", 'a' + p.d[0].at % p.w, p.d[0].at / p.w + 1);
        CHECK(sdf_script(&g, cell) && g.found[0] == 1);
        CHECK(sdf_script(&g, "h +5") && sdf_solved(&g) && g.hints == 1 && g.mistakes == 0);
        CHECK(!sdf_script(&g, "z9"));

        for (uint32_t seed = 1; seed <= 200; seed++) {   /* whole games from a seed: by chance taps to the end */
            sdf_make_variant(&p, (int)(seed % SDF_VARIANTS), seed);
            sdf_load(&g, &p);
            uint32_t r = seed * 2654435761u;
            int taps = 0;
            while (!sdf_solved(&g) && taps < 100000) {
                r = r * 1664525u + 1013904223u;
                int i = (int)(r >> 8) % (p.w * p.h), k = sdf_diff_at(&p, i), was = g.mistakes;
                int res = sdf_tap(&g, i);
                CHECK(k < 0 ? res == -1 && g.mistakes == was + 1 : res == k || res == -2);
                if ((r >> 4) % 401 == 0 && g.nfound) sdf_undo(&g);
                taps++;
            }
            CHECK(sdf_solved(&g));
        }
    }

    if (fails) printf("spotdiff: %d FAILED\n", fails);
    else printf("spotdiff: all passed\n");
    return fails != 0;
}
