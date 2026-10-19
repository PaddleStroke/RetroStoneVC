/*
 * Magic Squares' rules: the figures' lines, the 150 designed levels (each read back, magic, one solution by a full
 * count, the figures of its chapter, a gentle start), puzzles made for every variant from many seeds (the same, made
 * fast), a puzzle by hand, the tiles, mistakes, hints and undo, whole games from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/magicsquare.h"
#include "games/magicsquare_levels.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static void test_shapes(void)
{
    msq_puzzle p;
    msq_shape(&p, MSQ_SQ4);
    CHECK(p.np == 16 && p.nl == 10 && p.line[9][0] == 3 && p.line[9][3] == 12 && p.line[4][2] == 8);
    msq_shape(&p, MSQ_TRI4);
    CHECK(p.np == 9 && p.nl == 3 && p.len[0] == 4);
    CHECK(p.line[0][0] == 0 && p.line[0][1] == 3 && p.line[0][2] == 4 && p.line[0][3] == 1);
    CHECK(p.line[2][0] == 2 && p.line[2][3] == 0);
    for (int pl = 0; pl < 9; pl++) {            /* corners on two sides, the others on one */
        int on = 0;
        for (int l = 0; l < 3; l++)
            for (int j = 0; j < 4; j++) on += p.line[l][j] == pl;
        CHECK(on == (pl < 3 ? 2 : 1));
    }
}

static void test_levels(void)
{
    static const char KINDS[MSQ_CHAPTERS][3] = {"ab", "s", "bc", "s", "q", "cq"};
    for (int i = 0; i < MSQ_LEVELS; i++) {
        msq_puzzle p;
        int ok = msq_decode(msq_levels[i], &p);
        CHECK(ok);
        if (!ok) continue;
        char s[160];
        CHECK(msq_encode(&p, s, sizeof s) > 0 && !strcmp(s, msq_levels[i]));
        CHECK(msq_magic(&p) && msq_count(&p, 2, NULL) == 1);
        CHECK(strchr(KINDS[i / 25], msq_levels[i][0]) != NULL);
        int gaps = 0;
        for (int k = 0; k < p.np; k++) gaps += !p.given[k];
        CHECK(gaps >= 2);
        if (i < 3) CHECK(p.kind == MSQ_TRI3 && p.target <= 12);    /* the numbers 1..6, a short start */
        if (i == 2) CHECK(gaps == 3);           /* level 3 a step up from the first two */
        if (i >= 3) CHECK(gaps >= (i < 25 ? 4 : 3));
        for (int j = 0; j < i; j++) CHECK(strcmp(msq_levels[j], msq_levels[i]));
    }
}

static void test_make(void)
{
    clock_t worst = 0;
    for (int v = 0; v < MSQ_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 40; seed++) {
            msq_make m = msq_variant_make(v);
            msq_puzzle p, q;
            clock_t t0 = clock();
            msq_make_puzzle(&m, seed, &p);
            clock_t t = clock() - t0;
            if (t > worst) worst = t;
            msq_make_puzzle(&m, seed, &q);
            CHECK(!memcmp(&p, &q, sizeof p));
            CHECK(p.kind == m.kind && msq_magic(&p) && msq_count(&p, 2, NULL) == 1);
        }
    printf("make: the slowest puzzle %.0f ms\n", worst * 1000.0 / CLOCKS_PER_SEC);
    CHECK(worst * 1000.0 / CLOCKS_PER_SEC < 500);
}

static void test_hand(void)
{
    msq_puzzle p;
    CHECK(msq_decode("s:15:2,7,6,9,5,1,4,?3,?8", &p));
    CHECK(!msq_decode("s:15:2,7,6,9,5,1,4,8,?3", &p));   /* not magic */
    CHECK(msq_decode("s:15:2,7,6,9,5,1,4,?3,?8", &p));
    msq_game g;
    msq_load(&g, &p);
    CHECK(g.ngap == 2 && g.tray[0] == 3 && g.tray[1] == 8 && msq_value(&g, 7) == 0);
    CHECK(msq_script(&g, "h=8"));               /* place h (7): the 8: its column 7 5 8 full and wrong */
    CHECK(g.mistakes == 1 && msq_value(&g, 7) == 8);
    CHECK(msq_script(&g, "i=3"));               /* 4 8 3 = 15, but the column 6 1 3 and a diagonal are wrong */
    CHECK(g.mistakes >= 2 && !msq_solved(&g));
    CHECK(msq_script(&g, "h>i") && msq_solved(&g) && g.moves == 3);
    CHECK(msq_undo(&g) && !msq_solved(&g));
    CHECK(msq_hint(&g) >= 0 && g.hints == 1);
    CHECK(msq_hint(&g) >= 0 && msq_solved(&g));
    while (msq_undo(&g)) {}
    CHECK(g.moves == 0 && g.at[0] == -1 && g.at[1] == -1);
}

static void test_games(void)
{
    for (uint32_t seed = 1; seed <= 30; seed++) {
        msq_game g;
        msq_new(&g, (int)(seed % MSQ_VARIANTS), seed);
        uint32_t s = pr_seed(seed);
        for (int k = 0; k < 3 * g.ngap; k++) {
            int r = pr_below(&s, 4), i = pr_below(&s, g.ngap);
            if (r < 2) msq_put(&g, i, pr_below(&s, g.ntray));
            else if (r == 2) msq_move(&g, i, pr_below(&s, g.ngap));
            else msq_undo(&g);
        }
        int guard = 0;
        while (!msq_solved(&g) && guard++ < 4 * g.ngap) CHECK(msq_hint(&g) >= 0);
        CHECK(msq_solved(&g));
        while (msq_undo(&g)) {}
        CHECK(msq_script(&g, "+99") && msq_solved(&g));
    }
}

int main(void)
{
    test_shapes();
    test_hand();
    test_levels();
    test_make();
    test_games();
    if (fails) {
        printf("magicsquare: %d failures\n", fails);
        return 1;
    }
    printf("magicsquare: ok\n");
    return 0;
}
