/*
 * Masyu's rules: the solver and the grader on grids drawn by hand (two white pearls that leave one loop, a black
 * pearl in a corner that leaves two, pearls that leave none), the puzzles made from many seeds (one solution, the
 * level they were made for, well under a second each), the player's lines, crosses and drags, the faults shown,
 * hints and undo, whole games played from a seed, and every level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/masyu.h"
#include "games/masyu_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the player's links with the lines of lines[] (1: a line): solved? */
static int keeps_rules(const msy_puzzle *p, const uint8_t *lines)
{
    static msy_game g;
    memset(&g, 0, sizeof g);
    g.p = *p;
    for (int e = 0; e < msy_links(p->n); e++) g.st[e] = lines[e] ? MSY_LINE : MSY_EMPTY;
    return msy_solved(&g);
}

int main(void)
{
    msy_puzzle p;
    uint8_t sol[MSY_LINKS];

    /* drawn by hand: a 3 x 3 with white pearls in the middle of the top and bottom rows: the loop round the border */
    CHECK(msy_decode("3:.w.....w..", &p) == 0);   /* too long */
    CHECK(msy_decode("3:.w.....w.", &p));
    CHECK(msy_count(&p, 5) == 1);
    {
        int lv = msy_grade(&p, sol);
        CHECK(lv >= BENCH_EASY && lv <= BENCH_HARD && keeps_rules(&p, sol));
        int lines = 0;
        for (int e = 0; e < msy_links(3); e++) lines += sol[e];
        CHECK(lines == 8 && !sol[msy_link(3, 1, 4)] && !sol[msy_link(3, 3, 4)] && sol[msy_link(3, 0, 1)]);
    }
    /* a black pearl in a corner: its legs run along two sides, then two ways to close the loop */
    CHECK(msy_decode("3:b........", &p) && msy_count(&p, 5) == 2 && msy_grade(&p, NULL) == BENCH_GUESS);
    /* two black pearls side by side in a corner, a white one in a corner: no loop at all */
    CHECK(msy_decode("3:bb.......", &p) && msy_count(&p, 2) == 0 && msy_grade(&p, NULL) == BENCH_NONE);
    CHECK(msy_decode("4:w...............", &p) && msy_count(&p, 2) == 0);
    /* a white pearl the loop turns on: broken */
    {
        static msy_game F;
        memset(&F, 0, sizeof F);
        msy_decode("3:.w.....w.", &F.p);
        F.st[msy_link(3, 0, 1)] = F.st[msy_link(3, 1, 4)] = MSY_LINE;
        CHECK(msy_broken(&F, 1) && !msy_broken(&F, 7) && msy_lines(&F, 1) == 2);
        F.st[msy_link(3, 1, 4)] = MSY_EMPTY;
        F.st[msy_link(3, 1, 2)] = MSY_LINE;
        CHECK(!msy_broken(&F, 1) && !msy_solved(&F));
    }

    /* the puzzles from many seeds: one solution, of their level, quick */
    clock_t t0 = clock();
    double slowest[MSY_VARIANTS] = {0};
    int at_level[MSY_VARIANTS] = {0};
    for (int v = 0; v < MSY_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 10; seed++) {
            clock_t t1 = clock();
            msy_make(msy_size(v), msy_level(v), seed * 7919u + (uint32_t)v, &p, sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            CHECK(p.n >= 5 && p.n <= msy_size(v) && keeps_rules(&p, sol));
            uint8_t found[MSY_LINKS];
            int lv = msy_grade(&p, found);
            CHECK(lv >= BENCH_EASY && lv <= msy_level(v));
            at_level[v] += lv == msy_level(v) && p.n == msy_size(v);
            CHECK(!memcmp(found, sol, (size_t)msy_links(p.n)));
            CHECK(msy_count(&p, 2) == 1);
        }
    printf("masyu: 10 puzzles a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < MSY_VARIANTS; v++) printf(" %.3f", slowest[v]);
    printf(" s; at their level and size");
    for (int v = 0; v < MSY_VARIANTS; v++) printf(" %d", at_level[v]);
    printf("\n");
    for (int v = 0; v < MSY_VARIANTS; v++) CHECK(slowest[v] < 0.6 && at_level[v] >= 8);

    /* the same seed: the same puzzle */
    static msy_game a, b;
    msy_new(&a, 7, 42);
    msy_new(&b, 7, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's links: a tap a line or none, a hold a cross or none; undo */
    static msy_game G;
    msy_new(&G, 0, 7);
    int n = G.p.n, e = 0;
    CHECK(msy_tap_state(&G, e) == MSY_LINE && msy_hold_state(&G, e) == MSY_CROSS);
    CHECK(msy_set(&G, e, MSY_LINE) && msy_tap_state(&G, e) == MSY_EMPTY && msy_hold_state(&G, e) == MSY_CROSS);
    CHECK(msy_set(&G, e, MSY_CROSS) && msy_tap_state(&G, e) == MSY_LINE && msy_hold_state(&G, e) == MSY_EMPTY);
    CHECK(!msy_set(&G, e, MSY_CROSS) && !msy_set(&G, -1, MSY_LINE) && !msy_set(&G, msy_links(n), MSY_LINE));
    CHECK(G.hist.moves == 2 && msy_undo(&G) == e && G.st[e] == MSY_LINE && msy_undo(&G) == e && !G.st[e]);
    CHECK(msy_undo(&G) < 0);
    /* the notation: a drag along the top row draws every link of it in one action; undo takes it all back */
    {
        char moves[16];
        snprintf(moves, sizeof moves, "a1-%c1", (char)('a' + n - 1));
        CHECK(msy_script(&G, moves) && G.hist.moves == 1);
        for (int x = 0; x + 1 < n; x++) CHECK(G.st[msy_across(n, x, 0)] == MSY_LINE);
        CHECK(msy_lines(&G, 1) == 2 && msy_lines(&G, 0) == 1);
        CHECK(msy_script(&G, "a1-b1") && G.st[msy_across(n, 0, 0)] == MSY_EMPTY && G.hist.moves == 2);   /* erased */
        CHECK(msy_undo(&G) >= 0 && msy_undo(&G) >= 0 && G.hist.moves == 0);
        for (int x = 0; x + 1 < n; x++) CHECK(G.st[msy_across(n, x, 0)] == MSY_EMPTY);
        CHECK(msy_script(&G, "b2/b3") && G.st[msy_link(n, n + 1, 2 * n + 1)] == MSY_LINE);
        CHECK(msy_script(&G, ".b2/b3") && G.st[msy_link(n, n + 1, 2 * n + 1)] == MSY_CROSS);
        CHECK(!msy_script(&G, "z9-a1") && !msy_script(&G, "a1-c3") && !msy_script(&G, "a1/c3"));
        while (msy_undo(&G) >= 0) {}
    }

    /* a hint: a wrong mark put right first, else the next link found; hints alone solve a puzzle */
    for (int v = 0; v < MSY_VARIANTS; v++) {
        msy_new(&G, v, 100u + (uint32_t)v);
        n = G.p.n;
        int wrong = -1;
        for (int i = 0; i < msy_links(n) && wrong < 0; i++)
            if (!G.sol[i]) wrong = i;
        CHECK(msy_set(&G, wrong, MSY_LINE) && msy_hint(&G) == wrong && G.st[wrong] == MSY_CROSS);
        int guard = 0;
        while (!msy_solved(&G) && guard++ < 2 * msy_links(n)) {
            int h = msy_hint(&G);
            CHECK(h >= 0 && (G.st[h] == MSY_LINE) == G.sol[h]);
            if (h < 0) break;
        }
        CHECK(msy_solved(&G) && G.hist.hints > 0);
    }

    /* whole games from a seed: random taps, holds and drags, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 20; seed++) {
        uint32_t s = pr_seed(seed);
        msy_new(&G, (int)(seed % MSY_VARIANTS), seed);
        n = G.p.n;
        static msy_game first;
        first = G;
        for (int i = 0; i < 40; i++) {
            int l = pr_below(&s, msy_links(n)), kind = pr_below(&s, 3);
            if (kind < 2) {
                msy_set(&G, l, kind ? msy_hold_state(&G, l) : msy_tap_state(&G, l));
            } else {
                char moves[48];
                int y = pr_below(&s, n), x0 = pr_below(&s, n), x1 = pr_below(&s, n);
                snprintf(moves, sizeof moves, "%c%d-%c%d", 'a' + x0, y + 1, 'a' + x1, y + 1);
                msy_script(&G, moves);
            }
        }
        int guard = 0;
        while (!msy_solved(&G) && guard++ < 3 * msy_links(n)) CHECK(msy_hint(&G) >= 0);
        CHECK(msy_solved(&G));
        while (msy_undo(&G) >= 0) {}
        CHECK(!memcmp(G.st, first.st, sizeof G.st) && G.hist.moves == 0);
    }
    /* the stars' mistakes: a wrong line taken back with undo */
    msy_new(&G, 0, 11);
    {
        int l = -1;
        for (int i = 0; i < msy_links(G.p.n) && l < 0; i++)
            if (!G.sol[i]) l = i;
        msy_set(&G, l, MSY_LINE);
        msy_undo(&G);
        CHECK(G.hist.mistakes == 1 && G.hist.hints == 0);
    }

    /* the levels: each decodes and encodes back, has one solution found by reasoning alone (easy in chapter 1, hard
     * at the end), and the sizes never shrink */
    {
        int last_n = 0, grades[3] = {0, 0, 0};
        clock_t t1 = clock();
        for (int l = 0; l < MSY_LEVELS; l++) {
            msy_puzzle q;
            static msy_game L;
            char back[MSY_CELLS + 64];
            int ok = msy_decode(msy_levels[l], &q) && msy_load(&L, &q) && msy_count(&q, 2) == 1;
            ok = ok && keeps_rules(&q, L.sol);
            if (!ok) printf("masyu level %d: not one solution\n", l + 1);
            CHECK(ok);
            CHECK(msy_encode(&q, back, sizeof back) && !strcmp(back, msy_levels[l]));
            CHECK(q.n >= last_n);
            last_n = q.n;
            if (ok) grades[L.level]++;
            if (l < MSY_LEVELS / MSY_CHAPTERS) CHECK(L.level == BENCH_EASY);
        }
        msy_puzzle q;
        CHECK(msy_decode(msy_levels[MSY_LEVELS - 1], &q) && msy_grade(&q, NULL) == BENCH_HARD);
        printf("masyu: %d levels, one solution each (easy %d, medium %d, hard %d) in %.1f s\n", MSY_LEVELS, grades[0],
               grades[1], grades[2], (double)(clock() - t1) / CLOCKS_PER_SEC);
    }

    if (fails) printf("masyu: %d checks failed\n", fails);
    else printf("masyu: all checks passed\n");
    return fails != 0;
}
