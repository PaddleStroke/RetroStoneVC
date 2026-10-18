/*
 * The LITS rules: the shapes of four cells, the solver and the grader on grids drawn by hand (rows of four with no
 * answer, regions in two pieces or too small refused), the puzzles made from seeds (one solution, and how long they
 * take), the player's shading, dots, drags, the faults shown, hints and undo, whole games played from a seed, and
 * every level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/lits.h"
#include "games/lits_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the player's grid with the shading of sh[] (1: shaded): solved? */
static int keeps_rules(const lts_puzzle *p, const uint8_t *sh)
{
    static lts_game g;
    memset(&g, 0, sizeof g);
    g.p = *p;
    for (int c = 0; c < p->n * p->n; c++) g.st[c] = sh[c] ? LTS_SHADE : LTS_EMPTY;
    return lts_solved(&g);
}

int main(void)
{
    lts_puzzle p;
    uint8_t sol[LTS_CELLS];

    /* the shapes (on a grid 4 wide): turned and mirrored alike; the square and pieces apart are none */
    {
        static const int L[4] = {0, 4, 8, 9}, J[4] = {1, 5, 8, 9}, I[4] = {0, 1, 2, 3}, T[4] = {0, 1, 2, 5};
        static const int S[4] = {1, 2, 4, 5}, Z[4] = {0, 1, 5, 6}, O[4] = {0, 1, 4, 5}, A[4] = {0, 1, 2, 8};
        CHECK(lts_shape(L, 4) == LTS_L && lts_shape(J, 4) == LTS_L && lts_shape(I, 4) == LTS_I);
        CHECK(lts_shape(T, 4) == LTS_T && lts_shape(S, 4) == LTS_S && lts_shape(Z, 4) == LTS_S);
        CHECK(lts_shape(O, 4) < 0 && lts_shape(A, 4) < 0);
    }
    /* drawn by hand: four rows of four have only I pieces, side by side (none); four squares of 2 x 2 have none */
    CHECK(lts_decode("4:aaaabbbbccccdddd", &p) && p.regions == 4 && lts_count(&p, 9) == 0);
    CHECK(lts_grade(&p, NULL) == BENCH_NONE);
    CHECK(lts_decode("4:aabbaabbccddccdd", &p) && lts_count(&p, 9) == 0);
    CHECK(lts_decode("4:aabbaabbccccdddd", &p) && p.regions == 4);
    CHECK(!lts_decode("4:abababbbbbbbbbbb", &p));   /* a region in two pieces */
    CHECK(!lts_decode("4:abbbbbbbbbbbbbbb", &p));   /* a region under four cells */
    CHECK(!lts_decode("4:aaaa", &p));               /* too short */

    /* the puzzles from seeds (made, or a level turned when the maker finds none in time): one solution, quick */
    clock_t t0 = clock();
    double slowest[LTS_VARIANTS] = {0};
    for (int v = 0; v < LTS_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 3; seed++) {
            clock_t t1 = clock();
            static lts_game R;
            lts_new(&R, v, seed * 7919u + (uint32_t)v);
            p = R.p;
            memcpy(sol, R.sol, sizeof sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            CHECK(p.n == lts_size(v) && keeps_rules(&p, sol));
            uint8_t found[LTS_CELLS];
            int lv = lts_grade(&p, found);
            CHECK(lv >= BENCH_EASY && lv <= BENCH_HARD);
            CHECK(!memcmp(found, sol, (size_t)p.n * p.n));
            CHECK(lts_count(&p, 2) == 1);
        }
    printf("lits: 3 puzzles a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < LTS_VARIANTS; v++) printf(" %.2f", slowest[v]);
    printf(" s\n");
    for (int v = 0; v < LTS_VARIANTS; v++) CHECK(slowest[v] < 1.5);

    /* the same seed: the same puzzle */
    static lts_game a, b;
    lts_new(&a, 4, 42);
    lts_new(&b, 4, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's cells: a tap shades or not, a hold a dot; undo */
    static lts_game G;
    lts_new(&G, 0, 7);
    int n = G.p.n, f = 0;
    CHECK(lts_tap_state(&G, f) == LTS_SHADE && lts_hold_state(&G, f) == LTS_DOT);
    CHECK(lts_paint(&G, &f, 1, LTS_SHADE) && lts_tap_state(&G, f) == LTS_EMPTY && lts_hold_state(&G, f) == LTS_DOT);
    CHECK(lts_paint(&G, &f, 1, LTS_DOT) && lts_tap_state(&G, f) == LTS_SHADE && lts_hold_state(&G, f) == LTS_EMPTY);
    CHECK(G.hist.moves == 2 && lts_undo(&G) == f && G.st[f] == LTS_SHADE && lts_undo(&G) == f && !G.st[f]);
    CHECK(lts_undo(&G) < 0);
    /* the faults: two I pieces side by side (and their 2 x 2s), a region with five */
    {
        static lts_game F;
        memset(&F, 0, sizeof F);
        lts_decode("4:aaaabbbbccccdddd", &F.p);   /* four rows */
        for (int c = 0; c < 8; c++) F.st[c] = LTS_SHADE;
        CHECK(lts_piece(&F, 0) == LTS_I && lts_piece(&F, 1) == LTS_I && lts_piece(&F, 2) < 0);
        CHECK(lts_twins(&F, 0) && lts_twins(&F, 1) && !lts_twins(&F, 2));
        CHECK(lts_square(&F, 0) && lts_square(&F, 5) && !lts_over(&F, 0));
        F.st[4] = F.st[5] = F.st[6] = F.st[7] = LTS_EMPTY;
        CHECK(!lts_twins(&F, 0) && !lts_square(&F, 0) && !lts_solved(&F));
        memset(&F, 0, sizeof F);
        lts_decode("4:aaaaaaaabbbbbbbb", &F.p);   /* two halves */
        for (int c = 0; c < 5; c++) F.st[c] = LTS_SHADE;
        CHECK(lts_over(&F, 0) && !lts_over(&F, 1) && lts_piece(&F, 0) < 0);
    }
    /* the notation: a tap dragged down a column shades every empty cell of it */
    lts_new(&G, 3, 3);
    n = G.p.n;
    CHECK(lts_script(&G, "a1-a3") && G.st[0] == LTS_SHADE && G.st[n] == LTS_SHADE && G.st[2 * n] == LTS_SHADE);
    CHECK(G.hist.moves == 1 && lts_undo(&G) >= 0 && !G.st[0] && !G.st[n] && !G.st[2 * n]);
    CHECK(!lts_script(&G, "z9") && !lts_script(&G, "a1-c3"));

    /* a hint: a wrong cell put right first, else the next cell found; hints alone solve a puzzle */
    for (int l = 0; l < LTS_LEVELS; l += 37) {
        CHECK(lts_decode(lts_levels[l], &p) && lts_load(&G, &p));
        n = G.p.n;
        int wrong = -1;
        for (int c = 0; c < n * n && wrong < 0; c++)
            if (!G.sol[c]) wrong = c;
        CHECK(lts_paint(&G, &wrong, 1, LTS_SHADE) && lts_hint(&G) == wrong && G.st[wrong] == LTS_DOT);
        int guard = 0;
        while (!lts_solved(&G) && guard++ < n * n) {
            int c = lts_hint(&G);
            CHECK(c >= 0 && (G.st[c] == LTS_SHADE) == G.sol[c]);
            if (c < 0) break;
        }
        CHECK(lts_solved(&G) && G.hist.hints > 0);
    }

    /* whole games from a seed: random taps, holds and drags, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 12; seed++) {
        uint32_t s = pr_seed(seed);
        CHECK(lts_decode(lts_levels[seed * 11 % LTS_LEVELS], &p) && lts_load(&G, &p));
        n = G.p.n;
        static lts_game first;
        first = G;
        for (int i = 0; i < 40; i++) {
            int c = pr_below(&s, n * n), kind = pr_below(&s, 3);
            if (kind < 2) {
                lts_paint(&G, &c, 1, kind ? lts_hold_state(&G, c) : lts_tap_state(&G, c));
            } else {
                int cells[LTS_MAX], k = 0, was = G.st[c];
                for (int x = c; x < (c / n + 1) * n; x++)
                    if (G.st[x] == was) cells[k++] = x;
                lts_paint(&G, cells, k, lts_tap_state(&G, c));
            }
        }
        int guard = 0;
        while (!lts_solved(&G) && guard++ < 2 * n * n) CHECK(lts_hint(&G) >= 0);
        CHECK(lts_solved(&G));
        while (lts_undo(&G) >= 0) {}
        CHECK(!memcmp(G.st, first.st, sizeof G.st) && G.hist.moves == 0);
    }
    /* the stars' mistakes: a wrong cell taken back with undo */
    lts_decode(lts_levels[0], &p);
    lts_load(&G, &p);
    {
        int c = -1;
        for (int i = 0; i < G.p.n * G.p.n && c < 0; i++)
            if (!G.sol[i]) c = i;
        lts_paint(&G, &c, 1, LTS_SHADE);
        lts_undo(&G);
        CHECK(G.hist.mistakes == 1 && G.hist.hints == 0);
    }

    /* the levels: each decodes and encodes back, has one solution found by reasoning alone (easy in chapter 1, hard
     * at the end), and the sizes never shrink */
    {
        int last_n = 0, grades[3] = {0, 0, 0};
        clock_t t1 = clock();
        for (int l = 0; l < LTS_LEVELS; l++) {
            lts_puzzle q;
            static lts_game L;
            char back[LTS_CELLS + 8];
            int ok = lts_decode(lts_levels[l], &q) && lts_load(&L, &q) && lts_count(&q, 2) == 1;
            ok = ok && keeps_rules(&q, L.sol);
            if (!ok) printf("lits level %d: not one solution\n", l + 1);
            CHECK(ok);
            CHECK(lts_encode(&q, back, sizeof back) && !strcmp(back, lts_levels[l]));
            CHECK(q.n >= last_n);
            last_n = q.n;
            if (ok) grades[L.level]++;
            if (l < LTS_LEVELS / LTS_CHAPTERS) CHECK(L.level == BENCH_EASY);
        }
        lts_puzzle q;
        CHECK(lts_decode(lts_levels[LTS_LEVELS - 1], &q) && lts_grade(&q, NULL) == BENCH_HARD);
        printf("lits: %d levels, one solution each (easy %d, medium %d, hard %d) in %.1f s\n", LTS_LEVELS,
               grades[0], grades[1], grades[2], (double)(clock() - t1) / CLOCKS_PER_SEC);
    }

    if (fails) printf("lits: %d checks failed\n", fails);
    else printf("lits: all checks passed\n");
    return fails != 0;
}
