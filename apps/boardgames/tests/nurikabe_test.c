/*
 * Nurikabe's rules: the solver and the grader on grids drawn by hand (one island, two islands that must not touch, a
 * pool, a sea cut in two), the puzzles made from many seeds (one solution, the level they were made for, well under
 * a second each), the player's sea, dots, drags, the faults shown, hints and undo, whole games played from a seed,
 * and every level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/nurikabe.h"
#include "games/nurikabe_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the player's grid with the sea of sea[] (1: sea): solved? */
static int keeps_rules(const nrk_puzzle *p, const uint8_t *sea)
{
    static nrk_game g;
    memset(&g, 0, sizeof g);
    g.p = *p;
    for (int c = 0; c < p->n * p->n; c++) g.st[c] = sea[c] && !p->clue[c] ? NRK_SEA : NRK_EMPTY;
    return nrk_solved(&g);
}

int main(void)
{
    nrk_puzzle p;
    uint8_t sol[NRK_CELLS];

    /* drawn by hand: a 3 x 3 with a 1 in a corner and a 3 opposite: the sea is an L between them */
    CHECK(nrk_decode("3:1.........3", &p) == 0);   /* too long */
    CHECK(nrk_decode("3:1.......3", &p));
    CHECK(nrk_count(&p, 5) == 1);
    {
        int lv = nrk_grade(&p, sol);
        CHECK(lv >= BENCH_EASY && lv <= BENCH_HARD && keeps_rules(&p, sol) && sol[1] && sol[3] && !sol[5]);
    }
    /* a 9 fills the grid: no sea at all */
    CHECK(nrk_decode("3:....9....", &p) && nrk_grade(&p, sol) >= BENCH_EASY && nrk_count(&p, 3) == 1);
    for (int c = 0; c < 9; c++) CHECK(!sol[c]);
    /* two numbers that would have to touch: no solution */
    CHECK(nrk_decode("3:22.......", &p) && nrk_count(&p, 2) == 0 && nrk_grade(&p, NULL) == BENCH_NONE);
    /* too little land: the sea would make a pool */
    CHECK(nrk_decode("3:1........", &p) && nrk_count(&p, 2) == 0);
    /* a sea cut in two by an island across the grid */
    {
        uint8_t cut[9] = {0, 1, 0, 0, 1, 0, 0, 1, 0};   /* the middle column land: the sea in two pieces */
        nrk_decode("3:.3.......", &p);
        uint8_t sea[9];
        for (int c = 0; c < 9; c++) sea[c] = !cut[c];
        CHECK(!keeps_rules(&p, sea));
    }

    /* the puzzles from many seeds: one solution, of their level, quick */
    clock_t t0 = clock();
    double slowest[NRK_VARIANTS] = {0};
    int at_level[NRK_VARIANTS] = {0};
    for (int v = 0; v < NRK_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 10; seed++) {
            clock_t t1 = clock();
            nrk_make(nrk_size(v), nrk_level(v), seed * 7919u + (uint32_t)v, &p, sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            CHECK(p.n >= 5 && p.n <= nrk_size(v) && keeps_rules(&p, sol));
            uint8_t found[NRK_CELLS];
            int lv = nrk_grade(&p, found);
            CHECK(lv >= BENCH_EASY && lv <= nrk_level(v));
            at_level[v] += lv == nrk_level(v) && p.n == nrk_size(v);
            CHECK(!memcmp(found, sol, (size_t)p.n * p.n));
            CHECK(nrk_count(&p, 2) == 1);
        }
    printf("nurikabe: 10 puzzles a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < NRK_VARIANTS; v++) printf(" %.3f", slowest[v]);
    printf(" s; at their level and size");
    for (int v = 0; v < NRK_VARIANTS; v++) printf(" %d", at_level[v]);
    printf("\n");
    for (int v = 0; v < NRK_VARIANTS; v++) CHECK(slowest[v] < 0.6 && at_level[v] >= 8);

    /* the same seed: the same puzzle */
    static nrk_game a, b;
    nrk_new(&a, 7, 42);
    nrk_new(&b, 7, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's cells: a tap sea or none, a hold a dot; a number takes nothing; undo */
    static nrk_game G;
    nrk_new(&G, 0, 7);
    int n = G.p.n, f = -1, num = -1;
    for (int c = 0; c < n * n; c++) {
        if (nrk_free(&G, c) && f < 0) f = c;
        if (!nrk_free(&G, c) && num < 0) num = c;
    }
    CHECK(nrk_tap_state(&G, f) == NRK_SEA && nrk_hold_state(&G, f) == NRK_DOT);
    CHECK(nrk_paint(&G, &f, 1, NRK_SEA) && nrk_tap_state(&G, f) == NRK_EMPTY && nrk_hold_state(&G, f) == NRK_DOT);
    CHECK(nrk_paint(&G, &f, 1, NRK_DOT) && nrk_tap_state(&G, f) == NRK_SEA && nrk_hold_state(&G, f) == NRK_EMPTY);
    CHECK(!nrk_paint(&G, &num, 1, NRK_SEA));
    CHECK(G.hist.moves == 2 && nrk_undo(&G) == f && G.st[f] == NRK_SEA && nrk_undo(&G) == f && !G.st[f]);
    CHECK(nrk_undo(&G) < 0);
    /* the faults: a pool, an island too big, one closed at its size */
    {
        static nrk_game F;
        memset(&F, 0, sizeof F);
        nrk_decode("4:2...............", &F.p);   /* a 2 in the corner a1 */
        F.st[4] = F.st[5] = F.st[8] = F.st[9] = NRK_SEA;
        CHECK(nrk_pool(&F, 4) && nrk_pool(&F, 9) && !nrk_pool(&F, 0));
        F.st[1] = NRK_DOT;
        CHECK(nrk_island(&F, 0) == 0);          /* a1 b1, c1 still open */
        F.st[2] = NRK_DOT;
        CHECK(nrk_island(&F, 0) == 1);          /* three dots and the number: too big */
        F.st[2] = NRK_SEA;
        F.st[1] = NRK_DOT;
        CHECK(nrk_island(&F, 0) == 2);
    }
    /* the notation: a tap dragged down a column lays sea in every empty cell of it, and passes the numbers */
    nrk_new(&G, 4, 3);
    n = G.p.n;
    int col = 0;
    char moves[32];
    snprintf(moves, sizeof moves, "%c1-%c%d", (char)('a' + col), (char)('a' + col), n);
    CHECK(nrk_script(&G, moves) || !nrk_free(&G, col));
    for (int y = 0; y < n && nrk_free(&G, col); y++) {
        int c = y * n + col;
        CHECK(G.p.clue[c] ? G.st[c] == NRK_EMPTY : G.st[c] == NRK_SEA);
    }
    while (nrk_undo(&G) >= 0) {}
    for (int y = 0; y < n; y++) CHECK(G.st[y * n + col] == NRK_EMPTY);
    CHECK(!nrk_script(&G, "z9") && !nrk_script(&G, "a1-c3"));

    /* a hint: a wrong cell put right first, else the next cell found; hints alone solve a puzzle */
    for (int v = 0; v < NRK_VARIANTS; v++) {
        nrk_new(&G, v, 100u + (uint32_t)v);
        n = G.p.n;
        int wrong = -1;
        for (int c = 0; c < n * n && wrong < 0; c++)
            if (nrk_free(&G, c) && !G.sol[c]) wrong = c;
        CHECK(nrk_paint(&G, &wrong, 1, NRK_SEA) && nrk_hint(&G) == wrong && G.st[wrong] == NRK_DOT);
        int guard = 0;
        while (!nrk_solved(&G) && guard++ < n * n) {
            int c = nrk_hint(&G);
            CHECK(c >= 0 && (G.st[c] == NRK_SEA) == G.sol[c]);
            if (c < 0) break;
        }
        CHECK(nrk_solved(&G) && G.hist.hints > 0);
    }

    /* whole games from a seed: random taps, holds and drags, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 20; seed++) {
        uint32_t s = pr_seed(seed);
        nrk_new(&G, (int)(seed % NRK_VARIANTS), seed);
        n = G.p.n;
        static nrk_game first;
        first = G;
        for (int i = 0; i < 40; i++) {
            int c = pr_below(&s, n * n), kind = pr_below(&s, 3);
            if (!nrk_free(&G, c)) continue;
            if (kind < 2) {
                nrk_paint(&G, &c, 1, kind ? nrk_hold_state(&G, c) : nrk_tap_state(&G, c));
            } else {
                int cells[NRK_MAX], k = 0, was = G.st[c];
                for (int x = c; x < (c / n + 1) * n; x++)
                    if (nrk_free(&G, x) && G.st[x] == was) cells[k++] = x;
                nrk_paint(&G, cells, k, nrk_tap_state(&G, c));
            }
        }
        int guard = 0;
        while (!nrk_solved(&G) && guard++ < 2 * n * n) CHECK(nrk_hint(&G) >= 0);
        CHECK(nrk_solved(&G));
        while (nrk_undo(&G) >= 0) {}
        CHECK(!memcmp(G.st, first.st, sizeof G.st) && G.hist.moves == 0);
    }
    /* the stars' mistakes: a wrong cell taken back with undo */
    nrk_new(&G, 0, 11);
    {
        int c = -1;
        for (int i = 0; i < G.p.n * G.p.n && c < 0; i++)
            if (nrk_free(&G, i) && !G.sol[i]) c = i;
        nrk_paint(&G, &c, 1, NRK_SEA);
        nrk_undo(&G);
        CHECK(G.hist.mistakes == 1 && G.hist.hints == 0);
    }

    /* the levels: each decodes and encodes back, has one solution found by reasoning alone (easy in chapter 1, hard
     * at the end), and the sizes never shrink */
    {
        int last_n = 0, grades[3] = {0, 0, 0};
        clock_t t1 = clock();
        for (int l = 0; l < NRK_LEVELS; l++) {
            nrk_puzzle q;
            static nrk_game L;
            char back[NRK_CELLS + 64];
            int ok = nrk_decode(nrk_levels[l], &q) && nrk_load(&L, &q) && nrk_count(&q, 2) == 1;
            ok = ok && keeps_rules(&q, L.sol);
            if (!ok) printf("nurikabe level %d: not one solution\n", l + 1);
            CHECK(ok);
            CHECK(nrk_encode(&q, back, sizeof back) && !strcmp(back, nrk_levels[l]));
            CHECK(q.n >= last_n);
            last_n = q.n;
            if (ok) grades[L.level]++;
            if (l < NRK_LEVELS / NRK_CHAPTERS) CHECK(L.level == BENCH_EASY);
        }
        nrk_puzzle q;
        CHECK(nrk_decode(nrk_levels[NRK_LEVELS - 1], &q) && nrk_grade(&q, NULL) == BENCH_HARD);
        printf("nurikabe: %d levels, one solution each (easy %d, medium %d, hard %d) in %.1f s\n", NRK_LEVELS,
               grades[0], grades[1], grades[2], (double)(clock() - t1) / CLOCKS_PER_SEC);
    }

    if (fails) printf("nurikabe: %d checks failed\n", fails);
    else printf("nurikabe: all checks passed\n");
    return fails != 0;
}
