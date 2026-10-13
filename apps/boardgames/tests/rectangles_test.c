/*
 * Rectangles' rules: the solver and the grader on puzzles by hand, the puzzles made from many seeds at every size and
 * level (one solution, counted apart from the grader; the level they were made for; each number in its rectangle,
 * its area), the time the biggest take, the 150 designed levels (one solution each, the chapters growing), the
 * player's rectangles, hints and undo, and whole games from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/rectangles.h"
#include "games/rectangles_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a solution's rectangles tile the grid, each holding one number, its area */
static int tiling_ok(const rct_puz *p, const uint32_t *own)
{
    int n = p->n;
    for (int c = 0; c < n * n; c++) {
        if (!own[c]) return 0;
        int x, y, w, h, clues = 0, area = 0;
        rct_unpack(own[c], &x, &y, &w, &h);
        if (c % n < x || c % n >= x + w || c / n < y || c / n >= y + h || x + w > n || y + h > n) return 0;
        for (int j = y; j < y + h; j++)
            for (int i = x; i < x + w; i++) {
                if (own[j * n + i] != own[c]) return 0;
                if (p->clue[j * n + i]) clues++, area = p->clue[j * n + i];
            }
        if (clues != 1 || area != w * h) return 0;
    }
    return 1;
}

int main(void)
{
    /* a 7 x 7 by hand: seven rows of 7 */
    rct_puz p;
    memset(&p, 0, sizeof p);
    p.n = 7;
    for (int y = 0; y < 7; y++) p.clue[y * 7 + (y * 3) % 7] = 7;
    uint32_t sol[RCT_CELLS];
    int k = rct_count(&p, 10, sol);
    CHECK(k >= 1);
    CHECK(tiling_ok(&p, sol));
    /* two 2s side by side in a 2 x 2 corner can lie either way: not one solution */
    rct_puz q;
    memset(&q, 0, sizeof q);
    q.n = 7;
    q.clue[0] = 2, q.clue[8] = 2;              /* a1 and b2: the 2 x 2 corner cut either way */
    q.clue[2] = 35;                             /* the rest: c1.. is not a rectangle, so no solution */
    CHECK(rct_count(&q, 2, NULL) == 0);
    CHECK(rct_rate(&q) == RCT_BEYOND);

    /* the puzzles from many seeds */
    int seeds = 40;
    double total = 0;
    for (int v = 0; v < RCT_VARIANTS; v++) {
        int n = rct_size_of(v), level = rct_level_of(v), rated[4] = {0}, nums[2] = {999, 0};
        double slowest = 0;
        for (int seed = 1; seed <= seeds; seed++) {
            rct_puz r;
            static uint32_t s[RCT_CELLS], found[RCT_CELLS];
            clock_t t0 = clock();
            rct_make(n, level, (uint32_t)seed * 7919u + (uint32_t)v, &r, s);
            double took = (double)(clock() - t0) / CLOCKS_PER_SEC;
            total += took;
            if (took > slowest) slowest = took;
            CHECK(r.n == n);
            CHECK(tiling_ok(&r, s));
            CHECK(rct_count(&r, 2, found) == 1);
            CHECK(!memcmp(found, s, sizeof(uint32_t) * (size_t)(n * n)));
            int g = rct_rate(&r), cnt = 0;
            rated[g]++;
            for (int c = 0; c < n * n; c++) cnt += r.clue[c] != 0;
            if (cnt < nums[0]) nums[0] = cnt;
            if (cnt > nums[1]) nums[1] = cnt;
        }
        printf("rectangles %dx%d level %d: graded easy %d medium %d hard %d beyond %d; numbers %d-%d; slowest %.3f s\n",
               n, n, level, rated[0], rated[1], rated[2], rated[3], nums[0], nums[1], slowest);
        CHECK(rated[RCT_BEYOND] == 0);
        CHECK(rated[level] >= seeds * 9 / 10);
        for (int l = level + 1; l < 4; l++) CHECK(rated[l] == 0);
        CHECK(slowest < 0.5);
    }
    printf("rectangles: %d puzzles in %.1f s\n", seeds * RCT_VARIANTS, total);

    /* the same seed: the same puzzle */
    static rct_game a, b;
    rct_new(&a, 4, 42);
    rct_new(&b, 4, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the designed levels: each decodes (its code again the same), has one solution the grader finds; the chapters
     * grow (their sizes and grades) */
    {
        int last_n = 0, weight[RCT_CHAPTERS] = {0};
        for (int i = 0; i < RCT_STAGES; i++) {
            rct_puz p;
            char again[RCT_CELLS + 8];
            int ok = rct_decode(rct_levels[i], &p);
            CHECK(ok);
            if (!ok) continue;
            CHECK(rct_count(&p, 2, NULL) == 1);
            int r = rct_rate(&p);
            CHECK(r >= RCT_EASY && r <= RCT_HARD);
            weight[i / (RCT_STAGES / RCT_CHAPTERS)] += p.n + 2 * r;
            CHECK(rct_encode(&p, again, sizeof again) && !strcmp(again, rct_levels[i]));
            if (i % (RCT_STAGES / RCT_CHAPTERS) == 0) {
                CHECK(p.n >= last_n);
                last_n = p.n;
            }
        }
        for (int c = 1; c < RCT_CHAPTERS; c++) CHECK(weight[c] > weight[c - 1]);
        static rct_game lv;
        CHECK(rct_open(&lv, rct_levels[0]) && lv.n == 5 && !rct_solved(&lv));
        CHECK(rct_script(&lv, "+30") && rct_solved(&lv));
        CHECK(!rct_open(&lv, "3:33.......") && !rct_open(&lv, "2:22.") && !rct_open(&lv, "2:2..2"));
    }

    /* the player's rectangles: drawn over others, taken away, undone */
    static rct_game G;
    rct_new(&G, 0, 7);
    int n = G.n;
    CHECK(rct_draw(&G, 0, n + 1) && G.own[0] == rct_pack(0, 0, 2, 2) && G.own[n + 1] == G.own[0]);
    CHECK(!rct_draw(&G, n + 1, 0));             /* the same one again: nothing */
    CHECK(rct_draw(&G, 1, 2) && G.own[0] == 0 && G.own[n] == 0 && G.own[1] == rct_pack(1, 0, 2, 1));
    CHECK(G.hist.moves == 2);
    CHECK(rct_undo(&G) >= 0 && G.own[0] == rct_pack(0, 0, 2, 2) && G.own[2] == 0);
    CHECK(rct_erase(&G, n) && G.own[0] == 0 && G.own[n + 1] == 0);
    CHECK(!rct_erase(&G, n));
    CHECK(rct_undo(&G) >= 0 && rct_undo(&G) >= 0 && G.hist.moves == 0);
    for (int c = 0; c < n * n; c++) CHECK(G.own[c] == 0);
    /* good and bad rectangles */
    CHECK(rct_good(&G, G.sol[0]) && !rct_good(&G, rct_pack(0, 0, n, n)));
    /* a hint takes a wrong rectangle away first, else draws a right one */
    int wrong = G.sol[0] == rct_pack(0, 0, 1, 1) ? 1 : 0;
    (void)wrong;
    CHECK(rct_draw(&G, 0, 0));
    if (G.sol[0] != rct_pack(0, 0, 1, 1)) CHECK(rct_hint(&G) == 0 && G.own[0] == 0);
    int h = rct_hint(&G);
    CHECK(h >= 0 && G.own[h] == G.sol[h]);

    /* whole games from seeds: random rectangles, erasures, undo and hints until solved */
    for (int v = 0; v < RCT_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 4; seed++) {
            static rct_game g;
            rct_new(&g, v, seed * 131u + (uint32_t)v);
            int m = g.n, guard = 0;
            uint32_t r = seed;
            while (!rct_solved(&g) && guard++ < 300) {
                r = r * 1103515245u + 12345u;
                int c = (int)(r >> 8) % (m * m), d = (int)(r >> 16) % (m * m);
                switch ((r >> 4) % 5) {
                case 0: rct_draw(&g, c, c % m + (c / m + (d % 3)) % m * m); break;
                case 1: rct_erase(&g, c); break;
                case 2: rct_undo(&g); break;
                default: CHECK(rct_hint(&g) >= 0); break;
                }
            }
            for (guard = 0; !rct_solved(&g) && guard < 1000; guard++) CHECK(rct_hint(&g) >= 0);
            CHECK(rct_solved(&g));
            for (int c = 0; c < m * m; c++) CHECK(g.own[c] == g.sol[c]);
        }

    /* the notation */
    rct_new(&G, 0, 5);
    CHECK(rct_script(&G, "a1-b2 xa1") && G.own[0] == 0);
    CHECK(!rct_script(&G, "xa1"));
    CHECK(!rct_script(&G, "z9-a1"));
    CHECK(rct_script(&G, "+999") && rct_solved(&G));

    if (fails) printf("rectangles: %d checks failed\n", fails);
    else printf("rectangles: all checks passed\n");
    return fails != 0;
}
