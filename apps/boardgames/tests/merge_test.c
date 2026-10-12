/*
 * Merge's rules: the pushes and merges on lines made by hand (each tile merges once a push, the points), every
 * direction against the same push on the grid turned round, the new tiles (only after a push that moved, on an empty
 * square, a 2 or a 4), undo all the way back (the chance taken back too), the target and the end, the hint, whole games
 * of random pushes from a seed until no push moves, the notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/merge.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a push left on one row of a 4 x 4 grid: the row after it, the points */
static int row_left(const char *in, const char *want, int points)
{
    uint8_t c[MRG_CELLS] = {0}, out[MRG_CELLS];
    for (int i = 0; i < 4; i++) c[i] = (uint8_t)(in[i] - '0');
    int gain = -1;
    mrg_slide(c, 4, MRG_LEFT, out, &gain, NULL);
    for (int i = 0; i < 4; i++)
        if (out[i] != want[i] - '0') return 0;
    return gain == points;
}

/* the grid turned a quarter clockwise */
static void turn(const uint8_t *c, int n, uint8_t *o)
{
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) o[(n - 1 - y) + x * n] = c[x + y * n];
}

static mrg_game g, t;

int main(void)
{
    /* lines by hand (powers: 1 is a 2, 2 a 4...) */
    CHECK(row_left("1111", "2200", 8));        /* 2 2 2 2: 4 4 */
    CHECK(row_left("1120", "2200", 4));        /* 2 2 4: 4 4, not 8 (a tile merges once) */
    CHECK(row_left("2110", "2200", 4));        /* 4 2 2: 4 4 */
    CHECK(row_left("0101", "2000", 4));        /* the gap closed, then merged */
    CHECK(row_left("1212", "1212", 0));        /* nothing moves */
    CHECK(row_left("0012", "1200", 0));
    CHECK(row_left("3330", "4300", 16));       /* 8 8 8: 16 8 (the leading pair first) */
    uint8_t row[MRG_CELLS] = {1, 2, 1, 2};
    CHECK(!mrg_slide(row, 4, MRG_LEFT, NULL, NULL, NULL));
    /* where each tile went, and the merges */
    mrg_push how;
    uint8_t a[MRG_CELLS] = {0, 1, 0, 1, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0}, b[MRG_CELLS];
    CHECK(mrg_slide(a, 4, MRG_LEFT, b, NULL, &how));
    CHECK(how.to[1] == 0 && how.to[3] == 0 && how.merged[0] && b[0] == 2);
    CHECK(how.to[4] == 4 && how.to[5] == 4 && how.to[6] == 5 && how.merged[4] && !how.merged[5]);
    CHECK(how.to[0] == -1 && how.gain == 4 + 8);
    /* every direction: the same as a push left on the grid turned round */
    uint32_t s = pr_seed(99);
    for (int trial = 0; trial < 500; trial++) {
        int n = 3 + trial % 3;
        uint8_t c[MRG_CELLS] = {0};
        for (int i = 0; i < n * n; i++) c[i] = (uint8_t)(pr_below(&s, 3) ? pr_below(&s, 4) : 0);
        /* turning q quarters clockwise brings direction d to the left when (d + q) % 4 == 3 */
        for (int d = 0; d < 4; d++) {
            int q = (3 - d + 4) % 4;
            uint8_t r[MRG_CELLS], o1[MRG_CELLS], o2[MRG_CELLS], r2[MRG_CELLS];
            memcpy(r, c, sizeof r);
            for (int k = 0; k < q; k++) { turn(r, n, r2); memcpy(r, r2, sizeof r); }
            int g1, g2, m1 = mrg_slide(c, n, d, o1, &g1, NULL), m2 = mrg_slide(r, n, MRG_LEFT, o2, &g2, NULL);
            for (int k = 0; k < q; k++) { turn(o1, n, r2); memcpy(o1, r2, sizeof o1); }
            CHECK(m1 == m2 && g1 == g2 && !memcmp(o1, o2, (size_t)n * n));
        }
    }

    /* a new game: two tiles, 2s or 4s */
    for (int v = 0; v < MRG_VARIANTS; v++) {
        mrg_new(&g, v, 5);
        int tiles = 0;
        for (int i = 0; i < g.n * g.n; i++)
            if (g.p.c[i]) { tiles++; CHECK(g.p.c[i] == 1 || g.p.c[i] == 2); }
        CHECK(tiles == 2 && g.p.score == 0 && g.moves == 0 && !mrg_over(&g) && !mrg_reached(&g));
    }
    int n0, t0;
    mrg_variant(0, &n0, &t0);
    CHECK(n0 == 4 && t0 == 11);
    mrg_variant(2, &n0, &t0);
    CHECK(n0 == 3 && t0 == 8);

    /* the target: made by the last push, then playing on; the end */
    mrg_new(&g, 0, 1);
    CHECK(mrg_script(&g, "#10,10,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0"));
    CHECK(!mrg_reached(&g) && !mrg_just_reached(&g));
    CHECK(mrg_script(&g, "l") && mrg_top(&g) == 11 && mrg_reached(&g) && mrg_just_reached(&g) && g.p.score == 2048);
    int d = mrg_hint(&g);
    CHECK(d >= 0 && mrg_move(&g, d, NULL) && mrg_reached(&g) && !mrg_just_reached(&g));
    CHECK(mrg_undo(&g) && mrg_just_reached(&g));
    CHECK(mrg_script(&g, "#1,2,1,2, 2,1,2,1, 1,2,1,2, 2,1,2,1") && mrg_over(&g) && mrg_hint(&g) == -1);
    CHECK(!mrg_script(&g, "l"));               /* a push that moves nothing: refused, no new tile */
    CHECK(mrg_script(&g, "#1,2,1,2, 2,1,2,1, 1,2,1,2, 2,1,2,2") && !mrg_over(&g) &&
          mrg_can(&g, MRG_LEFT) && mrg_can(&g, MRG_UP) && mrg_can(&g, MRG_RIGHT) && mrg_can(&g, MRG_DOWN));

    /* whole games of random pushes from a seed, until no push moves */
    long total = 0;
    for (uint32_t seed = 1; seed <= 300; seed++) {
        int v = (int)(seed % MRG_VARIANTS);
        mrg_new(&g, v, seed);
        uint32_t r = pr_seed(seed + 1000);
        int pushes = 0;
        uint32_t score = 0;
        while (!mrg_over(&g) && pushes < 20000) {
            int dir = pr_below(&r, 4), empty0 = 0, empty1 = 0, sum0 = 0, sum1 = 0;
            for (int i = 0; i < g.n * g.n; i++) { empty0 += !g.p.c[i]; sum0 += g.p.c[i] ? 1 << g.p.c[i] : 0; }
            int can = mrg_can(&g, dir);
            mrg_push h;
            int moved = mrg_move(&g, dir, &h);
            CHECK(moved == can);
            if (!moved) continue;
            pushes++;
            for (int i = 0; i < g.n * g.n; i++) { empty1 += !g.p.c[i]; sum1 += g.p.c[i] ? 1 << g.p.c[i] : 0; }
            /* the tiles' sum grows by exactly the new tile; one new tile on a square the push left empty */
            CHECK(h.spawn >= 0 && (sum1 - sum0 == 2 || sum1 - sum0 == 4) && (sum1 - sum0) == 1 << g.p.c[h.spawn]);
            int merges = 0;
            for (int i = 0; i < MRG_CELLS; i++) merges += h.merged[i];
            CHECK(empty1 == empty0 + merges - 1);
            score += (uint32_t)h.gain;
            CHECK(g.p.score == score);
            if (pushes % 50 == 0) {                 /* undo, then the same push again: the same new tile */
                t = g;
                CHECK(mrg_undo(&t) && mrg_move(&t, dir, NULL) && !memcmp(&t.p, &g.p, sizeof g.p));
            }
        }
        CHECK(g.moves == pushes && mrg_over(&g) && mrg_hint(&g) == -1);
        total += pushes;
        while (mrg_undo(&g)) {}
        mrg_new(&t, v, seed);
        CHECK(g.moves == 0 && !memcmp(&g.p, &t.p, sizeof g.p));   /* undone all the way: the start again */
    }
    /* the hint plays better than chance: a few games on 4 x 4 played by the hint alone */
    int best = 0;
    for (uint32_t seed = 1; seed <= 3; seed++) {
        mrg_new(&g, 0, seed);
        for (int k = 0; k < 3000 && !mrg_over(&g); k++) CHECK(mrg_move(&g, mrg_hint(&g), NULL));
        if (mrg_top(&g) > best) best = mrg_top(&g);
    }
    printf("merge: %ld random pushes in 300 games; the hint alone made a %d tile\n", total, 1 << best);
    CHECK(best >= 9);                          /* 512 at least */
    if (fails) { printf("merge: %d failed\n", fails); return 1; }
    printf("merge: all passed\n");
    return 0;
}
