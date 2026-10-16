/*
 * Thermometers' rules: the thermometers read from text (a bad one refused), the solver and the grader on grids drawn
 * by hand, the puzzles made from many seeds (one solution, the level they were made for, well under a second each),
 * the player's filling, crosses, drags, hints and undo, whole games played from a seed, and every level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/thermometers.h"
#include "games/thermometers_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the player's grid with the filling of full[]: solved? */
static int keeps_rules(const thm_puzzle *p, const uint8_t *full)
{
    static thm_game g;
    memset(&g, 0, sizeof g);
    g.p = *p;
    memcpy(g.sol, full, (size_t)p->n * p->n);
    for (int c = 0; c < p->n * p->n; c++) g.st[c] = full[c] ? THM_FULL : THM_EMPTY;
    return thm_solved(&g);
}

int main(void)
{
    static thm_puzzle p;
    uint8_t sol[THM_CELLS];

    /* drawn by hand: three columns of three, each a thermometer from the bottom (bulbs on row 3) */
    CHECK(thm_decode("3:123123ddddddooo", &p) && p.thermos == 3);
    CHECK(p.of[6] == p.of[0] && p.at[6] == 0 && p.at[0] == 2 && thm_prev(&p, 0) == 3 && thm_prev(&p, 6) < 0);
    /* rows 1 2 3 from the top, columns 1 2 3: the first column filled one, the second two, the third three */
    CHECK(thm_count(&p, 3) == 1 && thm_grade(&p, sol) == BENCH_EASY);
    CHECK(sol[6] && !sol[3] && sol[7] && sol[4] && !sol[1] && sol[2] && sol[5] && sol[8] && keeps_rules(&p, sol));
    /* refused: a tube off the grid, two tubes from one cell, a loop with no bulb, a count too big */
    CHECK(!thm_decode("3:000000uoooooooo", &p));
    CHECK(!thm_decode("3:000000roloooooo", &p));
    CHECK(!thm_decode("3:000000rdouloooo", &p));
    CHECK(!thm_decode("3:900000ddddddooo", &p));
    /* a gap is no way to fill: the bulb empty and the cell above it filled */
    CHECK(thm_decode("3:123123ddddddooo", &p));
    {
        uint8_t gap[9] = {0, 0, 1, 1, 1, 1, 0, 1, 1};
        CHECK(!keeps_rules(&p, gap));
    }

    /* the puzzles from many seeds: one solution, of their level, quick */
    clock_t t0 = clock();
    double slowest[THM_VARIANTS] = {0};
    int at_level[THM_VARIANTS] = {0};
    for (int v = 0; v < THM_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 15; seed++) {
            clock_t t1 = clock();
            thm_make(thm_size(v), thm_level(v), seed * 7919u + (uint32_t)v, &p, sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            CHECK(p.n >= 4 && p.n <= thm_size(v) && keeps_rules(&p, sol));
            uint8_t found[THM_CELLS];
            int lv = thm_grade(&p, found);
            CHECK(lv >= BENCH_EASY && lv <= thm_level(v));
            at_level[v] += lv == thm_level(v) && p.n == thm_size(v);
            CHECK(!memcmp(found, sol, (size_t)p.n * p.n));
            CHECK(thm_count(&p, 2) == 1);
            char text[2 * THM_CELLS];
            static thm_puzzle back;
            CHECK(thm_encode(&p, text, sizeof text) && thm_decode(text, &back) && back.thermos == p.thermos);
        }
    printf("thermometers: 15 puzzles a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < THM_VARIANTS; v++) printf(" %.3f", slowest[v]);
    printf(" s; at their level and size");
    for (int v = 0; v < THM_VARIANTS; v++) printf(" %d", at_level[v]);
    printf("\n");
    for (int v = 0; v < THM_VARIANTS; v++) CHECK(slowest[v] < 0.5 && at_level[v] >= 13);

    /* the same seed: the same puzzle */
    static thm_game a, b;
    thm_new(&a, 7, 42);
    thm_new(&b, 7, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's cells: a tap fills up to a cell (from the bulb), a tap on a filled cell empties it and those above;
     * a hold crosses it and those above; undo */
    static thm_game G;
    memset(&G, 0, sizeof G);
    CHECK(thm_decode("3:123123ddddddooo", &G.p));
    CHECK(thm_tap_state(&G, 0) == THM_FULL && thm_act(&G, 0, 0, THM_FULL));
    CHECK(G.st[0] == THM_FULL && G.st[3] == THM_FULL && G.st[6] == THM_FULL && !G.st[1]);
    CHECK(thm_tap_state(&G, 3) == THM_EMPTY && thm_act(&G, 3, 3, THM_EMPTY));
    CHECK(G.st[6] == THM_FULL && !G.st[3] && !G.st[0]);
    CHECK(thm_act(&G, 4, 4, THM_CROSS) && G.st[4] == THM_CROSS && G.st[1] == THM_CROSS && !G.st[7]);
    CHECK(thm_act(&G, 7, 7, THM_FULL) && G.st[7] == THM_FULL && G.st[4] == THM_CROSS);
    CHECK(!thm_act(&G, 7, 7, THM_FULL));        /* nothing changed: no move */
    CHECK(G.hist.moves == 4 && thm_undo(&G) >= 0 && !G.st[7] && thm_undo(&G) >= 0 && !G.st[4] && !G.st[1]);
    while (thm_undo(&G) >= 0) {}
    for (int c = 0; c < 9; c++) CHECK(!G.st[c]);
    /* the notation: a drag crosses a row's cells as its first was; a hint finishes it */
    CHECK(thm_script(&G, "a1-c1") && G.st[0] == THM_CROSS && G.st[1] == THM_CROSS && G.st[2] == THM_CROSS);
    CHECK(G.hist.moves == 1 && thm_line_full(&G, 0, 0) == 0);
    CHECK(!thm_script(&G, "z9") && !thm_script(&G, "a1-c3"));

    /* a hint: a wrong cell put right first, else the next cell found; hints alone solve a puzzle */
    for (int v = 0; v < THM_VARIANTS; v++) {
        thm_new(&G, v, 100u + (uint32_t)v);
        int n = G.p.n, wrong = -1;
        for (int c = 0; c < n * n && wrong < 0; c++)
            if (!G.sol[c] && G.p.at[c] == 0) wrong = c;   /* an empty thermometer's bulb */
        if (wrong >= 0) {
            CHECK(thm_act(&G, wrong, wrong, THM_FULL) && thm_hint(&G) == wrong && G.st[wrong] == THM_CROSS);
        }
        int guard = 0;
        while (!thm_solved(&G) && guard++ < n * n) {
            int c = thm_hint(&G);
            CHECK(c >= 0 && (G.st[c] == THM_FULL) == G.sol[c]);
            if (c < 0) break;
        }
        CHECK(thm_solved(&G) && G.hist.hints > 0);
    }

    /* whole games from a seed: random taps, holds and drags, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 20; seed++) {
        uint32_t s = pr_seed(seed);
        thm_new(&G, (int)(seed % THM_VARIANTS), seed);
        int n = G.p.n;
        static thm_game first;
        first = G;
        for (int i = 0; i < 40; i++) {
            int c = pr_below(&s, n * n), kind = pr_below(&s, 3);
            if (kind < 2) thm_act(&G, c, c, kind ? thm_hold_state(&G, c) : thm_tap_state(&G, c));
            else thm_act(&G, c, (c / n + 1) * n - 1, thm_hold_state(&G, c));
        }
        int guard = 0;
        while (!thm_solved(&G) && guard++ < 2 * n * n) CHECK(thm_hint(&G) >= 0);
        CHECK(thm_solved(&G));
        while (thm_undo(&G) >= 0) {}
        CHECK(!memcmp(G.st, first.st, sizeof G.st) && G.hist.moves == 0);
    }
    /* the stars' mistakes: a wrong cell taken back with undo */
    thm_new(&G, 0, 11);
    {
        int c = -1;
        for (int i = 0; i < G.p.n * G.p.n && c < 0; i++)
            if (!G.sol[i] && G.p.at[i] == 0) c = i;
        if (c >= 0) {
            thm_act(&G, c, c, THM_FULL);
            thm_undo(&G);
            CHECK(G.hist.mistakes >= 1 && G.hist.hints == 0);
        }
    }

    /* the levels: each decodes and encodes back, has one solution found by reasoning alone (easy in chapter 1, hard
     * at the end), and the sizes never shrink */
    {
        int last_n = 0, grades[3] = {0, 0, 0};
        clock_t t1 = clock();
        for (int l = 0; l < THM_LEVELS; l++) {
            static thm_puzzle q;
            static thm_game L;
            char back[2 * THM_CELLS + 64];
            int ok = thm_decode(thm_levels[l], &q) && thm_load(&L, &q) && thm_count(&q, 2) == 1;
            ok = ok && keeps_rules(&q, L.sol);
            if (!ok) printf("thermometers level %d: not one solution\n", l + 1);
            CHECK(ok);
            CHECK(thm_encode(&q, back, sizeof back) && !strcmp(back, thm_levels[l]));
            CHECK(q.n >= last_n);
            last_n = q.n;
            if (ok) grades[L.level]++;
            if (l < THM_LEVELS / THM_CHAPTERS) CHECK(L.level == BENCH_EASY);
        }
        static thm_puzzle q;
        CHECK(thm_decode(thm_levels[THM_LEVELS - 1], &q) && thm_grade(&q, NULL) == BENCH_HARD);
        printf("thermometers: %d levels, one solution each (easy %d, medium %d, hard %d) in %.1f s\n", THM_LEVELS,
               grades[0], grades[1], grades[2], (double)(clock() - t1) / CLOCKS_PER_SEC);
    }

    if (fails) printf("thermometers: %d checks failed\n", fails);
    else printf("thermometers: all checks passed\n");
    return fails != 0;
}
