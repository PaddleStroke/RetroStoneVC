/*
 * Pipes' rules: the shapes and their turns, the maker's trees (every tile used, no cross, one network), the puzzles
 * from many seeds (found by deduction alone, so one solution, checked by the counter), the solver on known cases, the
 * water's way, taps, hints, undo, and whole games played from a seed; the maker's time on the biggest boards; every
 * level (one solution found by deduction alone, its par reached by turning each tile the fewest times, by the hints
 * and by "=" alike, and no way to any solved board, found by search, shorter than par).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/pipes.h"
#include "games/pipes_levels.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static int same_shape(uint8_t a, uint8_t b)
{
    for (int k = 0; k < 4; k++)
        if (pp_turn(a, k) == b) return 1;
    return 0;
}

/* the fewest quarter turns from the start to ANY solved board, by search: every tile tried in each of its turns, in
 * reading order, a turn kept only when it meets its placed neighbours (and the edge, unwrapped), the cost so far
 * pruned against the best found; a full board counts when pp_solved says so. It never looks at the maker's solution,
 * so a second solution or a cheaper way would show. */
static pp_game srch;
static uint8_t srch_start[PP_TILES];
static int srch_best, srch_nodes;

static int srch_fits(int t)
{
    int n = srch.n;
    uint8_t s = srch.now[t];
    for (int d = 0; d < 4; d++) {
        int u = pp_next(n, srch.wrap, t, d), pipe = s >> d & 1;
        if (u < 0) {
            if (pipe) return 0;
            continue;
        }
        if (u > t) continue;                    /* not placed yet */
        if (pipe != (srch.now[u] >> ((d + 2) % 4) & 1)) return 0;
    }
    return 1;
}

static void srch_go(int t, int cost)
{
    srch_nodes++;
    if (cost >= srch_best) return;
    if (t == srch.n * srch.n) {
        if (pp_solved(&srch)) srch_best = cost;
        return;
    }
    for (int k = 0; k < 4; k++) {
        uint8_t s = pp_turn(srch_start[t], k);
        int dup = 0;
        for (int j = 0; j < k; j++) dup |= pp_turn(srch_start[t], j) == s;   /* a straight: two turns alike */
        if (dup) continue;
        srch.now[t] = s;
        if (srch_fits(t)) srch_go(t + 1, cost + k);
    }
    srch.now[t] = srch_start[t];
}

static int fewest_turns(const pp_game *g)
{
    srch = *g;
    memcpy(srch_start, g->now, (size_t)g->n * g->n);
    srch_best = 4 * PP_TILES;
    srch_nodes = 0;
    srch_go(0, 0);
    return srch_best;
}

int main(void)
{
    /* the shapes and their turns */
    CHECK(pp_turn(PP_UP, 1) == PP_RIGHT && pp_turn(PP_UP, 3) == PP_LEFT && pp_turn(PP_LEFT, 1) == PP_UP);
    CHECK(pp_turn(PP_UP | PP_RIGHT, 2) == (PP_DOWN | PP_LEFT));
    int k;
    CHECK(pp_shape(PP_LEFT, &k) == PP_END && k == 3);
    CHECK(pp_shape(PP_LEFT | PP_RIGHT, &k) == PP_STRAIGHT && k == 1);
    CHECK(pp_shape(PP_DOWN | PP_LEFT, &k) == PP_BEND && k == 2);
    CHECK(pp_shape(PP_LEFT | PP_UP | PP_RIGHT, &k) == PP_TEE && k == 3);
    CHECK(pp_next(5, 0, 0, 0) < 0 && pp_next(5, 0, 0, 3) < 0 && pp_next(5, 0, 0, 1) == 1 && pp_next(5, 0, 0, 2) == 5);
    CHECK(pp_next(5, 1, 0, 0) == 20 && pp_next(5, 1, 0, 3) == 4 && pp_next(5, 1, 24, 1) == 20);

    /* the solver on a known case: a 2 x 2 ring can't be (a loop); a line of three: one solution */
    {
        uint8_t line[9] = {PP_RIGHT, PP_LEFT | PP_RIGHT, PP_LEFT | PP_DOWN,
                           PP_RIGHT, PP_LEFT | PP_RIGHT, PP_LEFT | PP_UP | PP_DOWN,
                           PP_RIGHT, PP_LEFT | PP_RIGHT, PP_LEFT | PP_UP};
        uint8_t out[9];
        CHECK(pp_deduce(3, 0, line, out) == 9);
        CHECK(pp_count(3, 0, line, 5) == 1);
        int ok = 1;
        for (int t = 0; t < 9; t++) ok &= out[t] == line[t];
        CHECK(ok);
        /* every tile a bend: no tree can be made of them (4 bends of a 2 x 2 close a loop) */
        uint8_t ring[4] = {PP_RIGHT | PP_DOWN, PP_LEFT | PP_DOWN, PP_UP | PP_RIGHT, PP_UP | PP_LEFT};
        CHECK(pp_count(2, 0, ring, 5) == 0);
        CHECK(pp_deduce(2, 0, ring, NULL) < 0);
        /* two ends and two straights on a 2 x 2: the ends can't face each other */
        uint8_t two[4] = {PP_RIGHT, PP_LEFT | PP_DOWN, PP_RIGHT, PP_LEFT | PP_UP};
        CHECK(pp_count(2, 0, two, 5) == 1);   /* the ends side by side, not joined */
    }

    /* the puzzles from many seeds: a tree over every tile, no cross, solved by its own sides; found by deduction
     * alone (so one solution); the tiles turned from it; the same from the same seed */
    int not_deduced[PP_VARIANTS] = {0}, tries[PP_VARIANTS] = {0}, ends[PP_VARIANTS] = {0};
    for (int v = 0; v < PP_VARIANTS; v++) {
        int n, wrap;
        pp_variant(v, &n, &wrap);
        int nt = n * n, seeds = v >= 3 ? 150 : 300;
        for (uint32_t seed = 1; seed <= (uint32_t)seeds; seed++) {
            uint8_t sol[PP_TILES], sol2[PP_TILES];
            int src, src2;
            int tr = pp_make(n, wrap, seed * 2654435761u, sol, &src);
            pp_make(n, wrap, seed * 2654435761u, sol2, &src2);
            CHECK(!memcmp(sol, sol2, (size_t)nt) && src == src2);
            CHECK(src == n / 2 * n + n / 2);
            tries[v] += tr;
            int edges = 0, cross = 0, open = 0;
            for (int t = 0; t < nt; t++) {
                int b = __builtin_popcount(sol[t]);
                edges += b;
                cross += b == 4 || b == 0;
                ends[v] += b == 1;
            }
            CHECK(edges == 2 * (nt - 1) && !cross);
            pp_game g;
            pp_load(&g, n, wrap, src, sol, sol);
            for (int t = 0; t < nt; t++)
                for (int d = 0; d < 4; d++) open += pp_open(&g, t, d);
            CHECK(!open && pp_solved(&g));
            if (tr > PP_TRIES) {
                not_deduced[v]++;
                CHECK(pp_count(n, wrap, sol, 2) >= 1);
                continue;
            }
            uint8_t out[PP_TILES];
            CHECK(pp_deduce(n, wrap, sol, out) == nt);
            CHECK(!memcmp(out, sol, (size_t)nt));
            if (seed <= 40) CHECK(pp_count(n, wrap, sol, 2) == 1);
            pp_new(&g, v, seed);
            CHECK(g.n == n && g.wrap == wrap && !pp_solved(&g) && g.moves == 0);
            int shapes = 1;
            for (int t = 0; t < nt; t++) shapes &= same_shape(g.now[t], g.sol[t]);
            CHECK(shapes);
        }
        printf("pipes %dx%d%s: %d of %d not found by deduction alone; %.1f tries a puzzle; %.0f%% ends\n", n, n,
               wrap ? " wrapped" : "", not_deduced[v], seeds, tries[v] / (float)seeds,
               100.0f * ends[v] / (seeds * nt));
        CHECK(not_deduced[v] * 50 <= seeds);   /* at most 2% */
    }

    /* the maker's time on the biggest boards */
    for (int v = 3; v < PP_VARIANTS; v++) {
        clock_t c0 = clock();
        pp_game g;
        int worst = 0;
        for (uint32_t seed = 1; seed <= 50; seed++) {
            clock_t a = clock();
            pp_new(&g, v, seed * 7919u);
            int ms = (int)((clock() - a) * 1000 / CLOCKS_PER_SEC);
            if (ms > worst) worst = ms;
        }
        float avg = (clock() - c0) * 1000.0f / CLOCKS_PER_SEC / 50;
        printf("pipes variant %d: %.1f ms a puzzle, %d ms at worst\n", v, avg, worst);
        CHECK(avg < 60 && worst < 300);
    }

    /* taps, the water, hints, undo */
    pp_game g;
    pp_new(&g, 0, 7);
    int dist[PP_TILES], from[PP_TILES];
    CHECK(pp_flow(&g, dist, from) >= 1 && dist[g.source] == 0 && from[g.source] == -1);
    uint8_t before = g.now[0];
    CHECK(pp_script(&g, "a1") && g.now[0] == pp_turn(before, 1) && g.moves == 1);
    CHECK(pp_script(&g, "a1*3") && g.now[0] == before && g.moves == 4);
    CHECK(pp_undo(&g) == 0 && g.now[0] == pp_turn(before, 3) && g.moves == 3);
    CHECK(!pp_script(&g, "f1") && !pp_script(&g, "a6") && !pp_script(&g, "x"));
    while (pp_undo(&g) >= 0) {}
    CHECK(g.moves == 0 && g.now[0] == before);
    int wrong = pp_wrong(&g), h = pp_hint(&g);
    CHECK(h >= 0 && g.now[h] == g.sol[h] && pp_wrong(&g) == wrong - 1 && g.moves == 1);
    CHECK(pp_undo(&g) == h && pp_wrong(&g) == wrong);
    {   /* the water's way: every wet tile's pipe from its source tile meets it */
        int wet = pp_flow(&g, dist, from), ok = 1;
        for (int t = 0; t < g.n * g.n; t++)
            if (dist[t] > 0) ok &= from[t] >= 0 && dist[from[t]] == dist[t] - 1;
        CHECK(ok && wet >= 1);
    }

    /* whole games from seeds: taps by chance, then each wrong tile turned until right (solved at the last one only),
     * hints to the end, undo all the way back */
    for (uint32_t seed = 1; seed <= 60; seed++)
        for (int v = 0; v < PP_VARIANTS; v++) {
            pp_new(&g, v, seed);
            uint8_t start[PP_TILES];
            memcpy(start, g.now, sizeof start);
            int nt = g.n * g.n;
            uint32_t r = pr_seed(seed + 99);
            for (int i = 0; i < 40 && !pp_solved(&g); i++) pp_tap(&g, pr_below(&r, nt));
            int early = 0;
            for (int t = 0; t < nt; t++) {
                while (g.now[t] != g.sol[t]) {
                    early |= pp_solved(&g);
                    pp_tap(&g, t);
                }
            }
            CHECK(!early && pp_solved(&g) && pp_wrong(&g) == 0);
            CHECK(!pp_tap(&g, 0));             /* solved: the tiles stay */
            while (pp_undo(&g) >= 0) {}
            CHECK(g.moves == 0 && !memcmp(start, g.now, (size_t)nt));
            int hints = 0;
            while (!pp_solved(&g) && pp_hint(&g) >= 0) hints++;
            CHECK(pp_solved(&g) && g.hints == hints && g.moves == g.par);   /* the hints' turns: par's */
            CHECK(pp_hint(&g) < 0);
        }
    pp_new(&g, 3, 5);
    CHECK(pp_tile(&g, "k11") == 120 && pp_tile(&g, "a10") == 99 && pp_tile(&g, "l1") < 0 && pp_tile(&g, "a12") < 0);
    CHECK(pp_script(&g, "k11 a10*2 =5") && g.moves >= 5);

    /* par and hints */
    pp_new(&g, 1, 11);
    CHECK(g.par == pp_par(g.n, g.sol, g.now) && g.par >= 1 && g.hints == 0);
    pp_hint(&g);
    pp_undo(&g);
    CHECK(g.hints == 1 && g.moves == 0);

    /* every level: it reads back, has one solution that the deductions alone find, is not solved at the start, and is
     * solved in exactly par turns (each tile the fewest times); its text again from the game; no two alike */
    {
        int grades[4] = {0}, last_n = 0, sizes_ok = 1;
        long nodes = 0;
        for (int i = 0; i < PP_LEVELS; i++) {
            CHECK(pp_decode(&g, pp_levels[i]));
            int nt = g.n * g.n, rounds;
            CHECK(g.source == g.n / 2 * g.n + g.n / 2);
            CHECK(pp_count(g.n, g.wrap, g.sol, 2) == 1);
            int grade = pp_grade(g.n, g.wrap, g.sol, &rounds);
            CHECK(grade >= 0);
            if (grade >= 0) grades[grade]++;
            if (i < 25) CHECK(g.n <= 5 && !g.wrap);   /* the first chapter: small */
            CHECK(!pp_solved(&g) && g.par >= 1);
            char code[PP_TILES + 8];
            CHECK(pp_encode(&g, code, sizeof code) && !strcmp(code, pp_levels[i]));
            sizes_ok &= g.n >= last_n - 4;
            last_n = g.n;
            for (int t = 0; t < nt; t++)
                while (g.now[t] != g.sol[t] && g.moves <= 4 * nt) pp_tap(&g, t);
            CHECK(pp_solved(&g) && g.moves == g.par);
            /* the same unit whatever solves it: the hints, "=" (one action a tile, its turns counted), and no way at
             * all, by search over every board, takes fewer turns than par */
            pp_game h;
            CHECK(pp_decode(&h, pp_levels[i]));
            while (!pp_solved(&h) && pp_hint(&h) >= 0) {}
            CHECK(pp_solved(&h) && h.moves == h.par);
            CHECK(pp_decode(&h, pp_levels[i]) && pp_script(&h, "=999") && pp_solved(&h) && h.moves == h.par);
            CHECK(pp_decode(&h, pp_levels[i]));
            int few = fewest_turns(&h);
            CHECK(few == h.par);
            if (few != h.par) printf("level %d: par %d, a way in %d\n", i + 1, h.par, few);
            nodes += srch_nodes;
            for (int j = 0; j < i; j++) CHECK(strcmp(pp_levels[i], pp_levels[j]));
        }
        CHECK(sizes_ok);
        printf("pipes levels: grades %d %d %d %d; the search for the fewest turns: %ld boards\n", grades[0], grades[1],
               grades[2], grades[3], nodes);
    }
    CHECK(!pp_decode(&g, "5:00") && !pp_decode(&g, "x") && !pp_decode(&g, "13:0"));

    if (fails) {
        printf("pipes: %d failed\n", fails);
        return 1;
    }
    printf("pipes: all passed\n");
    return 0;
}
