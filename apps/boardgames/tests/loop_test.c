/*
 * Loop's rules: the sides and their dots, a loop checked (one loop; two loops, a branch and a touch refused), the
 * puzzles made from many seeds for every variant (the solution one loop, the numbers its own, one solution by a plain
 * search, the level they were made for, made fast), the player's marks, hints and undo, whole games from a seed, and
 * every designed level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/loop.h"
#include "games/loop_levels.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static void square(uint8_t *on, int n, int x0, int y0, int x1, int y1)   /* the border of cells x0..x1, y0..y1 */
{
    for (int x = x0; x <= x1; x++) on[slk_across(n, x, y0)] = on[slk_across(n, x, y1 + 1)] = 1;
    for (int y = y0; y <= y1; y++) on[slk_down(n, x0, y)] = on[slk_down(n, x1 + 1, y)] = 1;
}

int main(void)
{
    int n = 5;
    CHECK(slk_sides(n) == 60);
    int x0, y0, x1, y1;
    slk_ends(n, slk_across(n, 2, 3), &x0, &y0, &x1, &y1);
    CHECK(x0 == 2 && y0 == 3 && x1 == 3 && y1 == 3);
    slk_ends(n, slk_down(n, 5, 4), &x0, &y0, &x1, &y1);
    CHECK(x0 == 5 && y0 == 4 && x1 == 5 && y1 == 5);
    uint8_t on[SLK_SIDES] = {0};
    square(on, n, 0, 0, 1, 1);
    CHECK(slk_loop_ok(n, on));
    square(on, n, 3, 3, 4, 4);
    CHECK(!slk_loop_ok(n, on));                  /* two loops */
    memset(on, 0, sizeof on);
    square(on, n, 0, 0, 1, 1);
    on[slk_across(n, 2, 0)] = 1;                 /* a branch */
    CHECK(!slk_loop_ok(n, on));
    memset(on, 0, sizeof on);
    square(on, n, 0, 0, 0, 0);
    square(on, n, 1, 1, 1, 1);                   /* touching at a dot: four lines there */
    CHECK(!slk_loop_ok(n, on));

    /* a puzzle by hand: the numbers of one 2 x 2 square's loop in a 3 x 3 grid, every one given */
    slk_puzzle h;
    memset(&h, 0, sizeof h);
    h.n = 3;
    square(h.sol, 3, 0, 0, 1, 1);
    const int8_t nums[9] = {2, 2, 1, 2, 2, 1, 1, 1, 0};
    memcpy(h.clue, nums, 9);
    CHECK(slk_count(&h, 2) == 1);
    CHECK(slk_rate(&h) >= 0 && slk_rate(&h) < BENCH_GUESS);
    for (int c = 0; c < 9; c++) h.clue[c] = -1;  /* no numbers: many loops */
    CHECK(slk_count(&h, 2) == 2);
    CHECK(slk_rate(&h) == BENCH_GUESS);

    for (int var = 0; var < SLK_VARIANTS; var++) {
        n = slk_size_of(var);
        int level = slk_level_of(var), exact = 0, clues = 0;
        double slowest = 0, total = 0;
        int tries = n >= 10 ? 20 : 40;
        for (int s = 1; s <= tries; s++) {
            slk_puzzle p;
            clock_t t0 = clock();
            slk_make(n, level, (uint32_t)s * 7919u + (uint32_t)var, &p);
            double took = (double)(clock() - t0) / CLOCKS_PER_SEC;
            total += took;
            if (took > slowest) slowest = took;
            CHECK(p.n == n);
            CHECK(slk_loop_ok(n, p.sol));
            for (int c = 0; c < n * n; c++) {
                if (p.clue[c] < 0) continue;
                clues++;
                int x = c % n, y = c / n;
                CHECK(p.clue[c] == p.sol[slk_across(n, x, y)] + p.sol[slk_across(n, x, y + 1)] +
                                       p.sol[slk_down(n, x, y)] + p.sol[slk_down(n, x + 1, y)]);
            }
            int r = slk_rate(&p);
            CHECK(r >= 0 && r <= level);
            exact += r == level;
            if (s <= (n >= 10 ? 4 : 10)) CHECK(slk_count(&p, 2) == 1);
        }
        printf("loop %dx%d level %d: %d/%d of the level, %.1f numbers, %.1f ms on average, slowest %.1f ms\n", n, n,
               level, exact, tries, (double)clues / tries, 1000 * total / tries, 1000 * slowest);
        CHECK(exact >= tries * 9 / 10);
        CHECK(slowest < 0.35);
    }

    /* the player's marks */
    slk_game g;
    slk_new(&g, 0, 5);
    n = g.p.n;
    int s = slk_across(n, 0, 0);
    CHECK(slk_cycle(&g, s) && g.side[s] == SLK_LINE);
    CHECK(slk_cycle(&g, s) && g.side[s] == SLK_CROSS);
    CHECK(slk_cycle(&g, s) && g.side[s] == SLK_EMPTY);
    CHECK(slk_script(&g, "a1-b1=1") && g.side[s] == SLK_LINE && slk_dot_lines(&g, 0, 0) == 1 && slk_lines(&g, 0) == 1);
    CHECK(slk_script(&g, "a1-a2") && g.side[slk_down(n, 0, 0)] == SLK_LINE && slk_dot_lines(&g, 0, 0) == 2);
    CHECK(!slk_script(&g, "a1-b2"));             /* not a side */
    CHECK(slk_undo(&g) >= 0 && slk_undo(&g) == s && g.side[s] == SLK_EMPTY);
    while (slk_undo(&g) >= 0) {}
    CHECK(g.hist.moves == 0);
    s = -1;                                      /* a line off the loop: the hint takes it first */
    for (int e = 0; e < slk_sides(n) && s < 0; e++)
        if (!g.p.sol[e]) s = e;
    CHECK(slk_set(&g, s, SLK_LINE) && slk_hint(&g) == s && g.side[s] == SLK_CROSS);

    for (uint32_t seed = 1; seed <= 24; seed++) {
        uint32_t rng = pr_seed(seed);
        slk_new(&g, (int)(seed % SLK_VARIANTS), seed);
        n = g.p.n;
        int steps = 0;
        while (!slk_solved(&g) && steps < 5000) {
            int e = pr_below(&rng, slk_sides(n));
            if (steps < slk_sides(n) && pr_below(&rng, 3)) slk_cycle(&g, e);
            else CHECK(slk_hint(&g) >= 0);
            steps++;
        }
        CHECK(slk_solved(&g));
        while (slk_undo(&g) >= 0) {}
        for (int e = 0; e < slk_sides(n); e++) CHECK(!g.side[e]);
    }

    /* every designed level (loop_levels.h): read back the same, its loop one loop and its numbers the loop's (the
     * decoder checks), one solution (the grader proves it; up to 10 x 10 a plain search finds no other), the first
     * chapter with the plainest reasoning, the last one 13 x 13 or bigger, no two the same; hints alone solve it */
    {
        int bad = 0, per = SLK_NLEVELS / SLK_CHAPTERS;
        clock_t t0 = clock();
        for (int i = 0; i < SLK_NLEVELS; i++) {
            static slk_game L;
            char back[SLK_CODE];
            long passes;
            int ok = slk_decode(&L, slk_levels[i]), r = -1;
            if (ok) slk_encode(&L.p, back), r = slk_rate_work(&L.p, &passes);
            ok = ok && !strcmp(back, slk_levels[i]) && r >= BENCH_EASY && r <= BENCH_HARD;
            ok = ok && (L.p.n > 10 || slk_count(&L.p, 2) == 1);
            ok = ok && (i >= per || r == BENCH_EASY) && (i < 5 * per || L.p.n >= 13);
            for (int j = 0; ok && j < i; j++) ok = strcmp(slk_levels[i], slk_levels[j]) != 0;
            for (int guard = 0; ok && !slk_solved(&L) && guard < SLK_SIDES; guard++) ok = slk_hint(&L) >= 0;
            ok = ok && slk_solved(&L);
            if (!ok) printf("loop level %d: bad (grade %d)\n", i + 1, r);
            bad += !ok;
        }
        CHECK(bad == 0);
        printf("loop: %d levels checked in %.1f s\n", SLK_NLEVELS, (double)(clock() - t0) / CLOCKS_PER_SEC);
        static slk_game L;
        CHECK(!slk_decode(&L, "5:..") && !slk_decode(&L, "x"));
    }

    printf(fails ? "loop: %d FAILED\n" : "loop: all passed\n", fails);
    return fails != 0;
}
