/*
 * Yin-Yang's rules: the code of the levels, the solver and the grader on grids drawn by hand (a 2 x 2 and its twelve
 * answers, a rim of four stretches refused), the puzzles made from seeds (one solution, and how long they take), the
 * player's stones, drags, the faults shown, hints and undo, whole games played from a seed, and every level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/yinyang.h"
#include "games/yinyang_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the player's grid with the stones of st[] (YYG_IVORY, YYG_EBONY): solved? */
static int keeps_rules(const yyg_puzzle *p, const uint8_t *st)
{
    static yyg_game g;
    memset(&g, 0, sizeof g);
    g.p = *p;
    for (int c = 0; c < p->n * p->n; c++) g.st[c] = st[c];
    return yyg_solved(&g);
}

int main(void)
{
    yyg_puzzle p;
    uint8_t sol[YYG_CELLS];

    /* the code back and forth; bad ones refused */
    {
        char back[64];
        CHECK(yyg_decode("3:o.x...x.o", &p) && p.n == 3 && p.given[0] == YYG_IVORY && p.given[2] == YYG_EBONY);
        CHECK(yyg_encode(&p, back, sizeof back) && !strcmp(back, "3:o.x...x.o"));
        CHECK(!yyg_decode("3:o.x...x.", &p) && !yyg_decode("3:o.x...x.z", &p) && !yyg_decode("1:o", &p));
    }
    /* drawn by hand: a 2 x 2 with nothing given has twelve answers (not one colour, not crosswise) */
    CHECK(yyg_decode("2:....", &p) && yyg_count(&p, 20) == 12 && yyg_grade(&p, NULL) == BENCH_GUESS);
    /* a crosswise 2 x 2 given: none; three of a colour given: the fourth the other */
    CHECK(yyg_decode("2:oxxo", &p) && yyg_count(&p, 5) == 0 && yyg_grade(&p, NULL) == BENCH_NONE);
    CHECK(yyg_decode("2:ooo.", &p) && yyg_grade(&p, sol) == BENCH_EASY && sol[3] == YYG_EBONY);
    /* round the rim of a 3 x 3: ivory, ebony, ivory, ebony on the corners is four stretches: none */
    CHECK(yyg_decode("3:o.x.....o", &p) && yyg_count(&p, 50) > 0);
    CHECK(yyg_decode("3:o.x...x.o", &p) && yyg_count(&p, 5) == 0 && yyg_grade(&p, NULL) == BENCH_NONE);
    {
        uint8_t st[9] = {YYG_IVORY, YYG_IVORY, YYG_IVORY, YYG_EBONY, YYG_EBONY, YYG_IVORY,
                         YYG_EBONY, YYG_IVORY, YYG_IVORY};   /* ivory all joined, ebony too, no square */
        CHECK(yyg_decode("3:.........", &p) && keeps_rules(&p, st));
        st[8] = YYG_EBONY;                      /* the ebony now in two groups */
        CHECK(!keeps_rules(&p, st));
    }

    /* the puzzles from seeds (made, or a level turned when the maker finds none in time): one solution, quick */
    clock_t t0 = clock();
    double slowest[YYG_VARIANTS] = {0};
    for (int v = 0; v < YYG_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 3; seed++) {
            clock_t t1 = clock();
            static yyg_game R;
            yyg_new(&R, v, seed * 7919u + (uint32_t)v);
            p = R.p;
            memcpy(sol, R.sol, sizeof sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            CHECK(p.n == yyg_size(v) && keeps_rules(&p, sol));
            uint8_t found[YYG_CELLS];
            int lv = yyg_grade(&p, found);
            CHECK(lv >= BENCH_EASY && lv <= BENCH_HARD);
            CHECK(!memcmp(found, sol, (size_t)p.n * p.n));
            CHECK(yyg_count(&p, 2) == 1);
        }
    printf("yinyang: 3 puzzles a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < YYG_VARIANTS; v++) printf(" %.2f", slowest[v]);
    printf(" s\n");
    for (int v = 0; v < YYG_VARIANTS; v++) CHECK(slowest[v] < 1.5);

    /* the same seed: the same puzzle */
    static yyg_game a, b;
    yyg_new(&a, 4, 42);
    yyg_new(&b, 4, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's cells: a tap ebony, ivory, empty; a hold ivory in or out; the given stones stay; undo */
    static yyg_game G;
    CHECK(yyg_decode(yyg_levels[0], &p) && yyg_load(&G, &p));
    int n = G.p.n, f = 0, given = 0;
    while (!yyg_free(&G, f)) f++;
    while (yyg_free(&G, given)) given++;
    CHECK(G.st[given] == G.p.given[given] && !yyg_paint(&G, &given, 1, YYG_EMPTY));
    CHECK(yyg_tap_state(&G, f) == YYG_EBONY && yyg_hold_state(&G, f) == YYG_IVORY);
    CHECK(yyg_paint(&G, &f, 1, YYG_EBONY) && yyg_tap_state(&G, f) == YYG_IVORY && yyg_hold_state(&G, f) == YYG_IVORY);
    CHECK(yyg_paint(&G, &f, 1, YYG_IVORY) && yyg_tap_state(&G, f) == YYG_EMPTY && yyg_hold_state(&G, f) == YYG_EMPTY);
    CHECK(G.hist.moves == 2 && yyg_undo(&G) == f && G.st[f] == YYG_EBONY && yyg_undo(&G) == f && !G.st[f]);
    CHECK(yyg_undo(&G) < 0);
    /* the faults: four of a colour in a square; a group shut in by the other colour */
    {
        static yyg_game F;
        memset(&F, 0, sizeof F);
        yyg_decode("3:.........", &F.p);
        F.st[0] = F.st[1] = F.st[3] = F.st[4] = YYG_EBONY;
        CHECK(yyg_square(&F, 0) && yyg_square(&F, 4) && !yyg_square(&F, 2) && !yyg_apart(&F, 0));
        memset(F.st, 0, sizeof F.st);
        F.st[0] = YYG_EBONY;
        F.st[1] = F.st[3] = YYG_IVORY;
        F.st[8] = YYG_EBONY;
        CHECK(yyg_apart(&F, 0) && !yyg_apart(&F, 8) && !yyg_apart(&F, 1));
    }
    /* the notation: a tap dragged down a column puts ebony in every empty cell of it; a hold ivory */
    yyg_decode("3:.........", &p);
    yyg_load(&G, &p);
    n = G.p.n;
    CHECK(yyg_script(&G, "a1-a3") && G.st[0] == YYG_EBONY && G.st[n] == YYG_EBONY && G.st[2 * n] == YYG_EBONY);
    CHECK(G.hist.moves == 1 && yyg_undo(&G) >= 0 && !G.st[0] && !G.st[n] && !G.st[2 * n]);
    CHECK(yyg_script(&G, ".b2") && G.st[4] == YYG_IVORY);
    CHECK(!yyg_script(&G, "z9") && !yyg_script(&G, "a1-c3"));

    /* a hint: a wrong stone put right first, else the next stone found; hints alone solve a puzzle */
    for (int l = 0; l < YYG_LEVELS; l += 37) {
        CHECK(yyg_decode(yyg_levels[l], &p) && yyg_load(&G, &p));
        n = G.p.n;
        int wrong = 0;
        while (!yyg_free(&G, wrong)) wrong++;
        int bad = G.sol[wrong] == YYG_EBONY ? YYG_IVORY : YYG_EBONY;
        CHECK(yyg_paint(&G, &wrong, 1, bad) && yyg_hint(&G) == wrong && G.st[wrong] == G.sol[wrong]);
        int guard = 0;
        while (!yyg_solved(&G) && guard++ < n * n) {
            int c = yyg_hint(&G);
            CHECK(c >= 0 && G.st[c] == G.sol[c]);
            if (c < 0) break;
        }
        CHECK(yyg_solved(&G) && G.hist.hints > 0);
    }

    /* whole games from a seed: random taps, holds and drags, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 12; seed++) {
        uint32_t s = pr_seed(seed);
        CHECK(yyg_decode(yyg_levels[seed * 11 % YYG_LEVELS], &p) && yyg_load(&G, &p));
        n = G.p.n;
        static yyg_game first;
        first = G;
        for (int i = 0; i < 40; i++) {
            int c = pr_below(&s, n * n), kind = pr_below(&s, 3);
            if (kind < 2) {
                yyg_paint(&G, &c, 1, kind ? yyg_hold_state(&G, c) : yyg_tap_state(&G, c));
            } else {
                int cells[YYG_MAX], k = 0, was = G.st[c];
                for (int x = c; x < (c / n + 1) * n; x++)
                    if (G.st[x] == was && yyg_free(&G, x)) cells[k++] = x;
                yyg_paint(&G, cells, k, yyg_tap_state(&G, c));
            }
        }
        int guard = 0;
        while (!yyg_solved(&G) && guard++ < 2 * n * n) CHECK(yyg_hint(&G) >= 0);
        CHECK(yyg_solved(&G));
        while (yyg_undo(&G) >= 0) {}
        CHECK(!memcmp(G.st, first.st, sizeof G.st) && G.hist.moves == 0);
    }
    /* the stars' mistakes: a wrong stone taken back with undo */
    yyg_decode(yyg_levels[0], &p);
    yyg_load(&G, &p);
    {
        int c = 0;
        while (!yyg_free(&G, c)) c++;
        yyg_paint(&G, &c, 1, G.sol[c] == YYG_EBONY ? YYG_IVORY : YYG_EBONY);
        yyg_undo(&G);
        CHECK(G.hist.mistakes == 1 && G.hist.hints == 0);
    }

    /* the levels: each decodes and encodes back, has one solution found by reasoning alone (easy in chapter 1, hard
     * at the end), and the sizes never shrink */
    {
        int last_n = 0, grades[3] = {0, 0, 0};
        clock_t t1 = clock();
        for (int l = 0; l < YYG_LEVELS; l++) {
            yyg_puzzle q;
            static yyg_game L;
            char back[YYG_CELLS + 8];
            int ok = yyg_decode(yyg_levels[l], &q) && yyg_load(&L, &q) && yyg_count(&q, 2) == 1;
            ok = ok && keeps_rules(&q, L.sol);
            if (!ok) printf("yinyang level %d: not one solution\n", l + 1);
            CHECK(ok);
            CHECK(yyg_encode(&q, back, sizeof back) && !strcmp(back, yyg_levels[l]));
            CHECK(q.n >= last_n);
            last_n = q.n;
            if (ok) grades[L.level]++;
            if (l < YYG_LEVELS / YYG_CHAPTERS) CHECK(L.level == BENCH_EASY);
        }
        yyg_puzzle q;
        CHECK(yyg_decode(yyg_levels[YYG_LEVELS - 1], &q) && yyg_grade(&q, NULL) == BENCH_HARD);
        printf("yinyang: %d levels, one solution each (easy %d, medium %d, hard %d) in %.1f s\n", YYG_LEVELS,
               grades[0], grades[1], grades[2], (double)(clock() - t1) / CLOCKS_PER_SEC);
    }

    if (fails) printf("yinyang: %d checks failed\n", fails);
    else printf("yinyang: all checks passed\n");
    return fails != 0;
}
