/*
 * Close the Loops' rules: a quarter turn, taps that change nothing refused, undo, a closed board (no end open, none at
 * the edge); boards as text both ways; the fewest taps by the rules' search against the test's own search over every
 * way of turning the tiles (small boards); every level: its par the search's, its closing way closed in exactly par
 * taps, not closed at the start, its design symmetric, the chapters' features, no two alike; the boards made at random
 * (closable, the same from the same seed, the time they take); whole games from a seed (random taps, hints, the rest
 * by "=", undo back to the start).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/loopclose.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* ---- the test's own search: every tile in each of its turns, tile by tile, the fewest taps closing it ---------- */
static int ow, oh, obest;
static uint8_t otry[LCL_CELLS];

static int fits(int c)                          /* tile c agrees with its left and upper neighbours and the edges */
{
    int x = c % ow, y = c / ow, m = otry[c];
    if (x == 0 && (m & LCL_LEFT)) return 0;
    if (y == 0 && (m & LCL_UP)) return 0;
    if (x == ow - 1 && (m & LCL_RIGHT)) return 0;
    if (y == oh - 1 && (m & LCL_DOWN)) return 0;
    if (x > 0 && !!(m & LCL_LEFT) != !!(otry[c - 1] & LCL_RIGHT)) return 0;
    if (y > 0 && !!(m & LCL_UP) != !!(otry[c - ow] & LCL_DOWN)) return 0;
    return 1;
}

static void own(const uint8_t *start, int c, int taps)
{
    if (taps >= obest) return;
    if (c == ow * oh) { obest = taps; return; }
    int m = start[c];
    for (int r = 0; r < 4; r++, m = lcl_rot(m)) {
        if (r && m == start[c]) break;          /* its turns repeat */
        otry[c] = (uint8_t)m;
        if (fits(c)) own(start, c + 1, taps + r);
    }
}

static int own_par(int w, int h, const uint8_t *start)
{
    ow = w;
    oh = h;
    obest = 1 << 20;
    own(start, 0, 0);
    return obest == 1 << 20 ? -1 : obest;
}

static int kind(int m)
{
    int k = m;
    for (int r = 0; r < 4; r++, m = lcl_rot(m))
        if (m < k) k = m;
    return k;
}

static int degree(int m) { return (m & 1) + (m >> 1 & 1) + (m >> 2 & 1) + (m >> 3 & 1); }

/* the design is the same mirrored across, turned half round, or turned a quarter: compared by the closing way's tiles
 * (any closing way of a symmetric design need not be symmetric: the tiles' kinds are compared instead) */
static int symmetric(const lcl_map *m)
{
    int a = 1, b = 1, c = m->w == m->h;
    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < m->w; x++) {
            int k = kind(m->start[x + y * m->w]);
            a &= k == kind(m->start[m->w - 1 - x + y * m->w]);
            b &= k == kind(m->start[m->w - 1 - x + (m->h - 1 - y) * m->w]);
            if (c) c &= k == kind(m->start[m->w - 1 - y + x * m->w]);
        }
    return a || b || c;
}

int main(void)
{
    static lcl_game g, t, first;
    char text[LCL_TEXT];
    uint8_t way[LCL_CELLS];
    long ways;

    /* turns */
    CHECK(lcl_rot(LCL_UP) == LCL_RIGHT && lcl_rot(LCL_LEFT) == LCL_UP && lcl_rot(LCL_UP | LCL_RIGHT) == (LCL_RIGHT | LCL_DOWN));
    CHECK(lcl_taps(LCL_UP, LCL_LEFT) == 3 && lcl_taps(5, 10) == 1 && lcl_taps(5, 5) == 0 && lcl_taps(1, 3) == -1);
    /* a ring of four corners: 6 c | 3 9 closed */
    CHECK(lcl_parse(&g.m, "6c|39") && g.m.w == 2 && g.m.h == 2);
    lcl_begin(&g);
    CHECK(lcl_solved(&g) && g.par == 0);
    CHECK(lcl_parse(&g.m, "c6|93"));            /* each corner turned away */
    lcl_begin(&g);
    CHECK(!lcl_solved(&g) && g.par == 8 && own_par(2, 2, g.m.start) == 8);
    CHECK(lcl_tap(&g, 0) && g.cur[0] == 9 && g.moves == 1 && lcl_undo(&g) && g.cur[0] == 0xc && !g.moves);
    CHECK(!lcl_undo(&g));
    CHECK(lcl_script(&g, "=") && lcl_solved(&g) && g.moves == 8 && g.hints == 0);
    /* an end at the edge stays open; a cross and an empty square cannot be turned */
    CHECK(lcl_parse(&g.m, "f0|00"));
    lcl_begin(&g);
    CHECK(!lcl_tap(&g, 0) && !lcl_tap(&g, 1) && g.par == -1 && !lcl_solved(&g));
    CHECK(lcl_parse(&g.m, "24|00") && lcl_solve(2, 2, g.m.start, way, &ways) == 1 && ways == 1 && way[0] == 2 && way[1] == 8);
    lcl_text(&g.m, text);
    CHECK(!strcmp(text, "24|00"));
    CHECK(!lcl_parse(&g.m, "2g|00") && !lcl_parse(&g.m, "24|0") && !lcl_script(&g, "c1"));
    /* the search against the test's own on small random boards */
    {
        int ok = 1;
        for (uint32_t seed = 1; seed <= 300; seed++) {
            uint32_t r = pr_seed(seed);
            lcl_map m = {2 + pr_below(&r, 3), 2 + pr_below(&r, 3), {0}};
            for (int c = 0; c < m.w * m.h; c++) m.start[c] = (uint8_t)pr_below(&r, 16);
            int p = lcl_solve(m.w, m.h, m.start, way, NULL), q = own_par(m.w, m.h, m.start);
            if (p != q) ok = 0;
            if (p >= 0) {
                int sum = 0;
                for (int c = 0; c < m.w * m.h; c++) sum += lcl_taps(m.start[c], way[c]);
                if (sum != p || !lcl_closed(m.w, m.h, way)) ok = 0;
            }
        }
        CHECK(ok);
    }

    /* every level */
    int per = LCL_LEVELS / LCL_CHAPTERS;
    for (int lv = 0; lv < LCL_LEVELS; lv++) {
        CHECK(lcl_level(&g, lv));
        lcl_text(&g.m, text);
        CHECK(!strcmp(text, lcl_levels[lv].board));
        CHECK(!lcl_solved(&g) && g.par == lcl_levels[lv].par && g.par > 0);
        CHECK(lcl_solve(g.m.w, g.m.h, g.m.start, way, NULL) == g.par && lcl_closed(g.m.w, g.m.h, way));
        int sum = 0;
        for (int c = 0; c < g.m.w * g.m.h; c++) sum += lcl_taps(g.m.start[c], way[c]);
        CHECK(sum == g.par);
        if (g.m.w * g.m.h <= 16) CHECK(own_par(g.m.w, g.m.h, g.m.start) == g.par);
        t = g;
        CHECK(lcl_script(&t, "=") && lcl_solved(&t) && t.moves == g.par);
        CHECK(symmetric(&g.m));
        if (!symmetric(&g.m)) printf("loopclose: level %d not symmetric\n", lv + 1);
        int n[5] = {0};
        for (int c = 0; c < g.m.w * g.m.h; c++) n[degree(g.m.start[c])]++;
        int ch = lv / per;
        if (ch == 0) CHECK(!n[1] && !n[3] && !n[4]);
        if (ch == 1) CHECK(n[3] + n[4] >= 2 && !n[1]);
        if (ch == 2) CHECK(n[1] >= 2);
        if (ch == 4) CHECK(g.m.w == g.m.h);
        for (int k = 0; k < lv; k++) CHECK(strcmp(lcl_levels[k].board, lcl_levels[lv].board) != 0);
    }
    CHECK(lcl_levels[0].par <= 5);
    CHECK(!lcl_level(&g, LCL_LEVELS) && !lcl_level(&g, -1));

    /* the boards made at random */
    for (int v = 0; v < LCL_VARIANTS; v++) {
        double worst = 0;
        int lo = 9999, hi = 0;
        for (uint32_t seed = 1; seed <= 20; seed++) {
            clock_t t0 = clock();
            lcl_random(&g, v, seed);
            double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
            if (ms > worst) worst = ms;
            if (g.par < lo) lo = g.par;
            if (g.par > hi) hi = g.par;
            CHECK(g.par > 0 && !lcl_solved(&g) && symmetric(&g.m));
            lcl_random(&t, v, seed);
            CHECK(!memcmp(&t.m, &g.m, sizeof g.m));
        }
        CHECK(worst < 300);
        printf("loopclose: random variant %d, pars %d .. %d, the slowest %.1f ms\n", v, lo, hi, worst);
    }

    /* whole games: random taps, a hint now and then, the rest by "=", undo back to the start */
    int ok = 1;
    for (uint32_t seed = 1; seed <= 60; seed++) {
        uint32_t r = pr_seed(seed);
        lcl_level(&g, (int)(seed * 7 % LCL_LEVELS));
        first = g;
        for (int k = 0; k < 40; k++) {
            if (pr_below(&r, 8) == 0) {
                int c, tp, before = g.moves;
                if (lcl_hint(&g, &c, &tp)) {
                    int left = lcl_solve(g.m.w, g.m.h, g.cur, NULL, NULL);
                    while (tp-- > 0) lcl_tap(&g, c);
                    if (lcl_solve(g.m.w, g.m.h, g.cur, NULL, NULL) != left - (g.moves - before)) ok = 0;
                }
            } else lcl_tap(&g, pr_below(&r, g.m.w * g.m.h));
        }
        int left = lcl_solve(g.m.w, g.m.h, g.cur, NULL, NULL), before = g.moves;
        if (!lcl_script(&g, "=") || !lcl_solved(&g) || g.moves - before != left) ok = 0;
        while (lcl_undo(&g)) {}
        if (memcmp(first.cur, g.cur, sizeof g.cur) || g.moves) ok = 0;
    }
    CHECK(ok);

    if (fails) { printf("loopclose: %d failures\n", fails); return 1; }
    printf("loopclose: all passed\n");
    return 0;
}
