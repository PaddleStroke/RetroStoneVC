/*
 * Balance Scales' rules: the 150 designed levels (each read back, every scale balanced by its weights, the weights
 * pinned down (the scales' rank) and found by reasoning, the steps of its chapter, a gentle start), puzzles made for
 * every variant from many seeds (the same), a puzzle by hand, the tilts, the guesses, mistakes, hints and undo.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/balance.h"
#include "games/balance_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* every weight checked by brute force: the only weights 1..99 balancing all the scales (up to three shapes) */
static int only_one(const bal_puzzle *p)
{
    if (p->k > 3) return 1;                     /* the rank says it for the bigger ones */
    int16_t w[BAL_SHAPES] = {0};
    int found = 0, top = 40;
    for (w[0] = 1; w[0] <= top; w[0]++)
        for (w[1] = p->k > 1 ? 1 : 0; w[1] <= (p->k > 1 ? top : 0); w[1]++)
            for (w[2] = p->k > 2 ? 1 : 0; w[2] <= (p->k > 2 ? top : 0); w[2]++) {
                int ok = 1;
                for (int i = 0; i < p->m && ok; i++) ok = bal_tilt(&p->sc[i], w) == 0;
                found += ok;
            }
    return found == 1;
}

static void test_levels(void)
{
    for (int i = 0; i < BAL_LEVELS; i++) {
        bal_puzzle p;
        int ok = bal_decode(bal_levels[i], &p), plain, pair;
        CHECK(ok);
        if (!ok) continue;
        char s[256];
        CHECK(bal_encode(&p, s, sizeof s) > 0 && !strcmp(s, bal_levels[i]));
        CHECK(bal_check(&p) && bal_rank(&p) == p.k && only_one(&p));
        CHECK(bal_reason(&p, &plain, &pair));
        int ch = i / 25;
        if (ch < 3) CHECK(pair == 0);           /* the first chapters: one scale at a time */
        if (ch == 3) CHECK(pair >= 1);
        if (i < 2) CHECK(p.k == 1 && p.m == 1 && bal_most(&p) <= 5);    /* a short start */
        if (i >= 2) CHECK(p.k >= 2);           /* level 3 a step up: two shapes */
        if (i >= 3 && i < 25) CHECK(p.k >= 3);  /* then not trivial: three shapes */
        if (i < 40) CHECK(bal_most(&p) <= 9);
        for (int j = 0; j < i; j++) CHECK(strcmp(bal_levels[j], bal_levels[i]));
    }
}

static void test_make(void)
{
    for (int v = 0; v < BAL_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 100; seed++) {
            bal_make m = bal_variant_make(v);
            bal_puzzle p, q;
            bal_make_puzzle(&m, seed, &p);
            bal_make_puzzle(&m, seed, &q);
            CHECK(!memcmp(&p, &q, sizeof p));
            int plain, pair;
            CHECK(p.k == m.k && bal_check(&p) && bal_rank(&p) == p.k && only_one(&p));
            CHECK(bal_reason(&p, &plain, &pair) && (m.pair ? pair >= 1 : pair == 0));
        }
}

static void test_hand(void)
{
    bal_puzzle p;
    CHECK(bal_decode("3,5;a,b/8|a,a/6", &p));  /* a + b = 8, a + a = 6 */
    CHECK(p.k == 2 && p.m == 2 && p.sc[0].n[0] == 2 && p.sc[0].w[1] == 8 && p.sc[1].n[0] == 2);
    CHECK(!bal_decode("3,6;a,b/8|a,a/6", &p)); /* not balanced */
    CHECK(bal_decode("3,5;a,b/8|a,a/6", &p));
    bal_game g;
    bal_load(&g, &p);
    CHECK(bal_tilt(&p.sc[0], g.guess) == 2);
    CHECK(bal_script(&g, "a=4"));
    CHECK(bal_tilt(&p.sc[1], g.guess) == -1);  /* 4 + 4 against 6: the left goes down */
    CHECK(bal_script(&g, "b=4") && g.mistakes == 1 && !bal_solved(&g));
    CHECK(bal_tilt(&p.sc[0], g.guess) == 0);   /* 4 + 4 = 8 balances, but the other does not */
    CHECK(bal_undo(&g) && g.guess[1] == 0 && g.moves == 1);
    CHECK(bal_hint(&g) == 0 && g.guess[0] == 3 && g.hints == 1);   /* the wrong 4 put right */
    CHECK(bal_hint(&g) == 1 && bal_solved(&g) && g.mistakes == 1);
    while (bal_undo(&g)) {}
    CHECK(g.guess[0] == 0 && g.moves == 0);
    CHECK(bal_script(&g, "+9") && bal_solved(&g));
    int plain, pair;
    CHECK(bal_decode("2,7;a,b/9|a,a,b/11", &p) && bal_reason(&p, &plain, &pair) && pair == 1 && plain == 1);
}

int main(void)
{
    test_hand();
    test_levels();
    test_make();
    if (fails) {
        printf("balance: %d failures\n", fails);
        return 1;
    }
    printf("balance: ok\n");
    return 0;
}
