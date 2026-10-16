/*
 * Heyawake's rules: the solver and the grader on grids drawn by hand (one room with nothing shaded, three rooms across
 * every row that leave no way, a room too full), the puzzles made from many seeds (one solution, the level they were
 * made for, well under a second each), the player's shading, dots, drags, the faults shown, hints and undo, whole
 * games played from a seed, and every level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/heyawake.h"
#include "games/heyawake_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the player's grid with the shading of sh[] (1: shaded): solved? */
static int keeps_rules(const hyk_puzzle *p, const uint8_t *sh)
{
    static hyk_game g;
    memset(&g, 0, sizeof g);
    g.p = *p;
    for (int c = 0; c < p->n * p->n; c++) g.st[c] = sh[c] ? HYK_SHADE : HYK_EMPTY;
    return hyk_solved(&g);
}

int main(void)
{
    hyk_puzzle p;
    uint8_t sol[HYK_CELLS];

    /* drawn by hand: one room of 3 x 3 with a 0: nothing shaded */
    CHECK(hyk_decode("3:00330", &p) && p.rooms == 1 && hyk_count(&p, 5) == 1);
    CHECK(hyk_grade(&p, sol) == BENCH_EASY && keeps_rules(&p, sol));
    for (int c = 0; c < 9; c++) CHECK(!sol[c]);
    CHECK(!hyk_decode("3:00230", &p));          /* a column left out of every room */
    CHECK(!hyk_decode("3:0033010131", &p));     /* rooms on top of each other */
    /* three columns, a room each with a 1: every row needs a shaded cell, and none keeps the rest joined */
    CHECK(hyk_decode("3:001311013120131", &p) && p.rooms == 3 && hyk_count(&p, 2) == 0);
    CHECK(hyk_grade(&p, NULL) == BENCH_NONE);
    /* a column room of 3 that wants 3 shaded: they would touch */
    CHECK(hyk_decode("3:0023.20133", &p) && hyk_count(&p, 2) == 0);
    /* the same columns with no numbers: a line through three rooms */
    {
        uint8_t sh[9] = {0};
        hyk_decode("3:0013.1013.2013.", &p);
        CHECK(!keeps_rules(&p, sh));
    }

    /* the puzzles from many seeds: one solution, of their level, quick */
    clock_t t0 = clock();
    double slowest[HYK_VARIANTS] = {0};
    int at_level[HYK_VARIANTS] = {0};
    for (int v = 0; v < HYK_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 10; seed++) {
            clock_t t1 = clock();
            hyk_make(hyk_size(v), hyk_level(v), seed * 7919u + (uint32_t)v, &p, sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            CHECK(p.n >= 5 && p.n <= hyk_size(v) && keeps_rules(&p, sol));
            uint8_t found[HYK_CELLS];
            int lv = hyk_grade(&p, found);
            CHECK(lv >= BENCH_EASY && lv <= hyk_level(v));
            at_level[v] += lv == hyk_level(v) && p.n == hyk_size(v);
            CHECK(!memcmp(found, sol, (size_t)p.n * p.n));
            CHECK(hyk_count(&p, 2) == 1);
        }
    printf("heyawake: 10 puzzles a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < HYK_VARIANTS; v++) printf(" %.3f", slowest[v]);
    printf(" s; at their level and size");
    for (int v = 0; v < HYK_VARIANTS; v++) printf(" %d", at_level[v]);
    printf("\n");
    for (int v = 0; v < HYK_VARIANTS; v++) CHECK(slowest[v] < 0.6 && at_level[v] >= 8);

    /* the same seed: the same puzzle */
    static hyk_game a, b;
    hyk_new(&a, 7, 42);
    hyk_new(&b, 7, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's cells: a tap shades or not, a hold a dot; undo */
    static hyk_game G;
    hyk_new(&G, 0, 7);
    int n = G.p.n, f = 0;
    CHECK(hyk_tap_state(&G, f) == HYK_SHADE && hyk_hold_state(&G, f) == HYK_DOT);
    CHECK(hyk_paint(&G, &f, 1, HYK_SHADE) && hyk_tap_state(&G, f) == HYK_EMPTY && hyk_hold_state(&G, f) == HYK_DOT);
    CHECK(hyk_paint(&G, &f, 1, HYK_DOT) && hyk_tap_state(&G, f) == HYK_SHADE && hyk_hold_state(&G, f) == HYK_EMPTY);
    CHECK(G.hist.moves == 2 && hyk_undo(&G) == f && G.st[f] == HYK_SHADE && hyk_undo(&G) == f && !G.st[f]);
    CHECK(hyk_undo(&G) < 0);
    /* the faults: shaded cells touching, a cell cut off, dots through three rooms, a room too full */
    {
        static hyk_game F;
        memset(&F, 0, sizeof F);
        hyk_decode("3:0013.10131201310", &F.p);    /* three columns: no number, a 1, a 0 */
        F.st[0] = F.st[1] = HYK_SHADE;
        CHECK(hyk_touching(&F, 0) && hyk_touching(&F, 1) && hyk_room(&F, 1) == 2 && hyk_room(&F, 0) == 0);
        F.st[4] = HYK_SHADE;
        CHECK(hyk_room(&F, 1) == 1);
        F.st[0] = F.st[1] = F.st[4] = HYK_EMPTY;
        F.st[1] = F.st[3] = HYK_SHADE;          /* a1 cut off */
        CHECK(hyk_apart(&F, 0) && !hyk_apart(&F, 4));
        F.st[6] = F.st[7] = F.st[8] = HYK_DOT;
        CHECK(hyk_long(&F, 7) && !hyk_long(&F, 4));
    }
    /* the notation: a tap dragged down a column shades every empty cell of it */
    hyk_new(&G, 4, 3);
    n = G.p.n;
    CHECK(hyk_script(&G, "a1-a3") && G.st[0] == HYK_SHADE && G.st[n] == HYK_SHADE && G.st[2 * n] == HYK_SHADE);
    CHECK(G.hist.moves == 1 && hyk_undo(&G) >= 0 && !G.st[0] && !G.st[n] && !G.st[2 * n]);
    CHECK(!hyk_script(&G, "z9") && !hyk_script(&G, "a1-c3"));

    /* a hint: a wrong cell put right first, else the next cell found; hints alone solve a puzzle */
    for (int v = 0; v < HYK_VARIANTS; v++) {
        hyk_new(&G, v, 100u + (uint32_t)v);
        n = G.p.n;
        int wrong = -1;
        for (int c = 0; c < n * n && wrong < 0; c++)
            if (!G.sol[c]) wrong = c;
        CHECK(hyk_paint(&G, &wrong, 1, HYK_SHADE) && hyk_hint(&G) == wrong && G.st[wrong] == HYK_DOT);
        int guard = 0;
        while (!hyk_solved(&G) && guard++ < n * n) {
            int c = hyk_hint(&G);
            CHECK(c >= 0 && (G.st[c] == HYK_SHADE) == G.sol[c]);
            if (c < 0) break;
        }
        CHECK(hyk_solved(&G) && G.hist.hints > 0);
    }

    /* whole games from a seed: random taps, holds and drags, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 20; seed++) {
        uint32_t s = pr_seed(seed);
        hyk_new(&G, (int)(seed % HYK_VARIANTS), seed);
        n = G.p.n;
        static hyk_game first;
        first = G;
        for (int i = 0; i < 40; i++) {
            int c = pr_below(&s, n * n), kind = pr_below(&s, 3);
            if (kind < 2) {
                hyk_paint(&G, &c, 1, kind ? hyk_hold_state(&G, c) : hyk_tap_state(&G, c));
            } else {
                int cells[HYK_MAX], k = 0, was = G.st[c];
                for (int x = c; x < (c / n + 1) * n; x++)
                    if (G.st[x] == was) cells[k++] = x;
                hyk_paint(&G, cells, k, hyk_tap_state(&G, c));
            }
        }
        int guard = 0;
        while (!hyk_solved(&G) && guard++ < 2 * n * n) CHECK(hyk_hint(&G) >= 0);
        CHECK(hyk_solved(&G));
        while (hyk_undo(&G) >= 0) {}
        CHECK(!memcmp(G.st, first.st, sizeof G.st) && G.hist.moves == 0);
    }
    /* the stars' mistakes: a wrong cell taken back with undo */
    hyk_new(&G, 0, 11);
    {
        int c = -1;
        for (int i = 0; i < G.p.n * G.p.n && c < 0; i++)
            if (!G.sol[i]) c = i;
        hyk_paint(&G, &c, 1, HYK_SHADE);
        hyk_undo(&G);
        CHECK(G.hist.mistakes == 1 && G.hist.hints == 0);
    }

    /* the levels: each decodes and encodes back, has one solution found by reasoning alone (easy in chapter 1, hard
     * at the end), and the sizes never shrink */
    {
        int last_n = 0, grades[3] = {0, 0, 0};
        clock_t t1 = clock();
        for (int l = 0; l < HYK_LEVELS; l++) {
            hyk_puzzle q;
            static hyk_game L;
            char back[5 * HYK_CELLS + 64];
            int ok = hyk_decode(hyk_levels[l], &q) && hyk_load(&L, &q) && hyk_count(&q, 2) == 1;
            ok = ok && keeps_rules(&q, L.sol);
            if (!ok) printf("heyawake level %d: not one solution\n", l + 1);
            CHECK(ok);
            CHECK(hyk_encode(&q, back, sizeof back) && !strcmp(back, hyk_levels[l]));
            CHECK(q.n >= last_n);
            last_n = q.n;
            if (ok) grades[L.level]++;
            if (l < HYK_LEVELS / HYK_CHAPTERS) CHECK(L.level == BENCH_EASY);
        }
        hyk_puzzle q;
        CHECK(hyk_decode(hyk_levels[HYK_LEVELS - 1], &q) && hyk_grade(&q, NULL) == BENCH_HARD);
        printf("heyawake: %d levels, one solution each (easy %d, medium %d, hard %d) in %.1f s\n", HYK_LEVELS,
               grades[0], grades[1], grades[2], (double)(clock() - t1) / CLOCKS_PER_SEC);
    }

    if (fails) printf("heyawake: %d checks failed\n", fails);
    else printf("heyawake: all checks passed\n");
    return fails != 0;
}
