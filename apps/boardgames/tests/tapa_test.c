/*
 * Tapa's rules: the runs round a cell, the solver and the grader on grids drawn by hand, the code of the levels,
 * the puzzles made from seeds (one solution, and how long they take), the player's shading, dots, drags, the faults
 * shown, hints and undo, whole games played from a seed, and every level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/tapa.h"
#include "games/tapa_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the player's grid with the shading of sh[] (1: shaded): solved? */
static int keeps_rules(const tpa_puzzle *p, const uint8_t *sh)
{
    static tpa_game g;
    memset(&g, 0, sizeof g);
    g.p = *p;
    for (int c = 0; c < p->n * p->n; c++) g.st[c] = sh[c] ? TPA_SHADE : TPA_EMPTY;
    return tpa_solved(&g);
}

int main(void)
{
    tpa_puzzle p;
    uint8_t sol[TPA_CELLS];

    /* the runs round a cell: one of seven, four of one, none; a ring wrapping past the north is one run */
    {
        uint8_t len[4], sh[9] = {1, 1, 1, 1, 0, 1, 1, 1, 0};
        CHECK(tpa_runs(sh, 3, 4, len) == 1 && len[0] == 7);
        uint8_t alt[9] = {1, 0, 1, 0, 0, 0, 1, 0, 1};
        CHECK(tpa_runs(alt, 3, 4, len) == 4 && len[0] == 1 && len[3] == 1);
        uint8_t none[9] = {0};
        CHECK(tpa_runs(none, 3, 4, len) == 1 && len[0] == 0);
        uint8_t wrap[9] = {1, 1, 0, 1, 0, 0, 0, 0, 0};   /* north-west, north and west: one run of 3 */
        CHECK(tpa_runs(wrap, 3, 4, len) == 1 && len[0] == 3);
        uint8_t two[9] = {0, 1, 1, 0, 0, 0, 1, 1, 0};
        CHECK(tpa_runs(two, 3, 4, len) == 2 && len[0] == 2 && len[1] == 2);
    }
    /* drawn by hand: a clue 3 in the middle of a 3 x 3 has eight answers; 8 one; the code back and forth */
    CHECK(tpa_decode("3:....3,....", &p) && p.count[4] == 1 && p.num[4][0] == 3 && tpa_count(&p, 20) == 8);
    CHECK(tpa_grade(&p, NULL) == BENCH_GUESS);
    CHECK(tpa_decode("3:....8,....", &p) && tpa_count(&p, 20) == 1 && tpa_grade(&p, sol) == BENCH_EASY);
    CHECK(keeps_rules(&p, sol) && sol[0] && !sol[4]);
    {
        char back[64];
        CHECK(tpa_decode("4:31,......11,.....2,..", &p) && p.count[0] == 2 && p.num[7][1] == 1);
        CHECK(tpa_encode(&p, back, sizeof back) && !strcmp(back, "4:31,......11,.....2,.."));
        CHECK(!tpa_decode("3:....13,....", &p) && !tpa_decode("3:....3....", &p) && !tpa_decode("3:...", &p));
    }

    /* the puzzles from seeds (made, or a level turned when the maker finds none in time): one solution, quick */
    clock_t t0 = clock();
    double slowest[TPA_VARIANTS] = {0};
    for (int v = 0; v < TPA_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 3; seed++) {
            clock_t t1 = clock();
            static tpa_game R;
            tpa_new(&R, v, seed * 7919u + (uint32_t)v);
            p = R.p;
            memcpy(sol, R.sol, sizeof sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            CHECK(p.n == tpa_size(v) && keeps_rules(&p, sol));
            uint8_t found[TPA_CELLS];
            int lv = tpa_grade(&p, found);
            CHECK(lv >= BENCH_EASY && lv <= BENCH_HARD);
            CHECK(!memcmp(found, sol, (size_t)p.n * p.n));
            CHECK(tpa_count(&p, 2) == 1);
        }
    printf("tapa: 3 puzzles a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < TPA_VARIANTS; v++) printf(" %.2f", slowest[v]);
    printf(" s\n");
    for (int v = 0; v < TPA_VARIANTS; v++) CHECK(slowest[v] < 1.5);

    /* the same seed: the same puzzle */
    static tpa_game a, b;
    tpa_new(&a, 4, 42);
    tpa_new(&b, 4, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's cells: a tap shades or not, a hold a dot; undo */
    static tpa_game G;
    tpa_new(&G, 0, 7);
    int n = G.p.n, f = 0;
    while (!tpa_free(&G, f)) f++;
    CHECK(tpa_tap_state(&G, f) == TPA_SHADE && tpa_hold_state(&G, f) == TPA_DOT);
    CHECK(tpa_paint(&G, &f, 1, TPA_SHADE) && tpa_tap_state(&G, f) == TPA_EMPTY && tpa_hold_state(&G, f) == TPA_DOT);
    CHECK(tpa_paint(&G, &f, 1, TPA_DOT) && tpa_tap_state(&G, f) == TPA_SHADE && tpa_hold_state(&G, f) == TPA_EMPTY);
    CHECK(G.hist.moves == 2 && tpa_undo(&G) == f && G.st[f] == TPA_SHADE && tpa_undo(&G) == f && !G.st[f]);
    CHECK(tpa_undo(&G) < 0);
    /* the faults: a clue kept, still open, broken; an all shaded 2 x 2 */
    {
        static tpa_game F;
        memset(&F, 0, sizeof F);
        tpa_decode("3:....3,....", &F.p);      /* a clue 3 in the middle */
        CHECK(tpa_clue(&F, 4) == 0 && !tpa_free(&F, 4));
        F.st[0] = F.st[1] = F.st[2] = TPA_SHADE;
        CHECK(tpa_clue(&F, 4) == 2 && tpa_solved(&F));
        F.st[6] = TPA_SHADE;
        CHECK(tpa_clue(&F, 4) == 1 && !tpa_solved(&F));
        F.st[6] = TPA_EMPTY;
        F.st[0] = TPA_DOT;
        CHECK(tpa_clue(&F, 4) == 0);
        F.st[3] = F.st[5] = F.st[6] = F.st[7] = F.st[8] = TPA_DOT;
        CHECK(tpa_clue(&F, 4) == 1);
        memset(&F, 0, sizeof F);
        tpa_decode("3:.........", &F.p);
        F.st[0] = F.st[1] = F.st[3] = F.st[4] = TPA_SHADE;
        CHECK(tpa_square(&F, 0) && tpa_square(&F, 4) && !tpa_square(&F, 2) && !tpa_solved(&F));
    }
    /* the notation: a tap dragged down a column shades every empty cell of it */
    tpa_decode("3:....3,....", &p);
    tpa_load(&G, &p);
    n = G.p.n;
    CHECK(tpa_script(&G, "a1-a3") && G.st[0] == TPA_SHADE && G.st[n] == TPA_SHADE && G.st[2 * n] == TPA_SHADE);
    CHECK(G.hist.moves == 1 && tpa_undo(&G) >= 0 && !G.st[0] && !G.st[n] && !G.st[2 * n]);
    CHECK(!tpa_script(&G, "z9") && !tpa_script(&G, "a1-c3"));

    /* a hint: a wrong cell put right first, else the next cell found; hints alone solve a puzzle */
    for (int l = 0; l < TPA_LEVELS; l += 37) {
        CHECK(tpa_decode(tpa_levels[l], &p) && tpa_load(&G, &p));
        n = G.p.n;
        int wrong = -1;
        for (int c = 0; c < n * n && wrong < 0; c++)
            if (!G.sol[c] && tpa_free(&G, c)) wrong = c;
        CHECK(tpa_paint(&G, &wrong, 1, TPA_SHADE) && tpa_hint(&G) == wrong && G.st[wrong] == TPA_DOT);
        int guard = 0;
        while (!tpa_solved(&G) && guard++ < n * n) {
            int c = tpa_hint(&G);
            CHECK(c >= 0 && (G.st[c] == TPA_SHADE) == G.sol[c]);
            if (c < 0) break;
        }
        CHECK(tpa_solved(&G) && G.hist.hints > 0);
    }

    /* whole games from a seed: random taps, holds and drags, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 12; seed++) {
        uint32_t s = pr_seed(seed);
        CHECK(tpa_decode(tpa_levels[seed * 11 % TPA_LEVELS], &p) && tpa_load(&G, &p));
        n = G.p.n;
        static tpa_game first;
        first = G;
        for (int i = 0; i < 40; i++) {
            int c = pr_below(&s, n * n), kind = pr_below(&s, 3);
            if (kind < 2) {
                tpa_paint(&G, &c, 1, kind ? tpa_hold_state(&G, c) : tpa_tap_state(&G, c));
            } else {
                int cells[TPA_MAX], k = 0, was = G.st[c];
                for (int x = c; x < (c / n + 1) * n; x++)
                    if (G.st[x] == was) cells[k++] = x;
                tpa_paint(&G, cells, k, tpa_tap_state(&G, c));
            }
        }
        int guard = 0;
        while (!tpa_solved(&G) && guard++ < 2 * n * n) CHECK(tpa_hint(&G) >= 0);
        CHECK(tpa_solved(&G));
        while (tpa_undo(&G) >= 0) {}
        CHECK(!memcmp(G.st, first.st, sizeof G.st) && G.hist.moves == 0);
    }
    /* the stars' mistakes: a wrong cell taken back with undo */
    tpa_decode(tpa_levels[0], &p);
    tpa_load(&G, &p);
    {
        int c = -1;
        for (int i = 0; i < G.p.n * G.p.n && c < 0; i++)
            if (!G.sol[i] && tpa_free(&G, i)) c = i;
        tpa_paint(&G, &c, 1, TPA_SHADE);
        tpa_undo(&G);
        CHECK(G.hist.mistakes == 1 && G.hist.hints == 0);
    }

    /* the levels: each decodes and encodes back, has one solution found by reasoning alone (easy in chapter 1, hard
     * at the end), and the sizes never shrink */
    {
        int last_n = 0, grades[3] = {0, 0, 0};
        clock_t t1 = clock();
        for (int l = 0; l < TPA_LEVELS; l++) {
            tpa_puzzle q;
            static tpa_game L;
            char back[512];
            int ok = tpa_decode(tpa_levels[l], &q) && tpa_load(&L, &q) && tpa_count(&q, 2) == 1;
            ok = ok && keeps_rules(&q, L.sol);
            if (!ok) printf("tapa level %d: not one solution\n", l + 1);
            CHECK(ok);
            CHECK(tpa_encode(&q, back, sizeof back) && !strcmp(back, tpa_levels[l]));
            CHECK(q.n >= last_n);
            last_n = q.n;
            if (ok) grades[L.level]++;
            if (l < TPA_LEVELS / TPA_CHAPTERS) CHECK(L.level == BENCH_EASY);
        }
        tpa_puzzle q;
        CHECK(tpa_decode(tpa_levels[TPA_LEVELS - 1], &q) && tpa_grade(&q, NULL) == BENCH_HARD);
        printf("tapa: %d levels, one solution each (easy %d, medium %d, hard %d) in %.1f s\n", TPA_LEVELS,
               grades[0], grades[1], grades[2], (double)(clock() - t1) / CLOCKS_PER_SEC);
    }

    if (fails) printf("tapa: %d checks failed\n", fails);
    else printf("tapa: all checks passed\n");
    return fails != 0;
}
