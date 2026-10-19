/*
 * Cross Math's rules: the arithmetic (the usual order, exact divisions, nothing below zero), the 150 designed levels
 * (each read back, every equation true, solved by reasoning alone, one solution by a full count, the operations of
 * its chapter, past its opening a step past the plain ones), puzzles made from many seeds for every variant (the
 * same, made fast), a puzzle by hand, the player's tiles, the mistakes, hints and undo, and whole games from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/mathcross.h"
#include "games/mathcross_levels.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static void test_eval(void)
{
    int r, n[3], op[2];
    n[0] = 2, n[1] = 3, n[2] = 4, op[0] = MCR_ADD, op[1] = MCR_MUL;
    CHECK(mcr_eval(n, op, 3, &r) && r == 14);  /* 2 + 3 x 4: times first */
    op[0] = MCR_MUL, op[1] = MCR_ADD;
    CHECK(mcr_eval(n, op, 3, &r) && r == 10);
    n[0] = 12, n[1] = 6, n[2] = 2, op[0] = MCR_DIV, op[1] = MCR_MUL;
    CHECK(mcr_eval(n, op, 3, &r) && r == 4);   /* 12 / 6 x 2: from the left */
    n[0] = 10, n[1] = 4, n[2] = 3, op[0] = MCR_SUB, op[1] = MCR_SUB;
    CHECK(mcr_eval(n, op, 3, &r) && r == 3);   /* 10 - 4 - 3 */
    n[0] = 3, n[1] = 4, op[0] = MCR_SUB;
    CHECK(!mcr_eval(n, op, 2, &r));             /* below zero */
    n[0] = 7, n[1] = 2, op[0] = MCR_DIV;
    CHECK(!mcr_eval(n, op, 2, &r));             /* not exact */
    n[0] = 9, n[1] = 4, n[2] = 2, op[0] = MCR_SUB, op[1] = MCR_DIV;
    CHECK(mcr_eval(n, op, 3, &r) && r == 7);   /* 9 - 4 / 2 */
}

static int all_true(const mcr_puzzle *p)
{
    mcr_puzzle q = *p;
    mcr_eq eq[MCR_EQS];
    int n = mcr_parse(&q, eq);
    for (int e = 0; e < n; e++) {
        int nums[4], k = 0;
        for (int i = 0; i < eq[e].len; i += 2) nums[k++] = p->val[eq[e].cell[i]];
        if (!mcr_true(p, &eq[e], nums)) return 0;
    }
    return n > 0;
}

static unsigned ops_of(const mcr_puzzle *p, int *three)
{
    unsigned m = 0;
    mcr_puzzle q = *p;
    mcr_eq eq[MCR_EQS];
    int n = mcr_parse(&q, eq);
    *three = 0;
    for (int e = 0; e < n; e++) *three += eq[e].len == 7;
    for (int c = 0; c < p->w * p->h; c++)
        if (p->kind[c] >= MCR_ADD && p->kind[c] <= MCR_DIV) m |= 1u << p->kind[c];
    return m;
}

static void test_levels(void)
{
    int mediums = 0, open[3] = {0};
    for (int i = 0; i < MCR_LEVELS; i++) {
        mcr_puzzle p;
        int ok = mcr_decode(mcr_levels[i], &p), medium = 0, three;
        CHECK(ok);
        if (!ok) continue;
        char s[1024];
        CHECK(mcr_encode(&p, s, sizeof s) > 0 && !strcmp(s, mcr_levels[i]));
        CHECK(all_true(&p));
        CHECK(mcr_reason(&p, &medium));
        CHECK(mcr_count(&p, 2) == 1);
        mediums += medium > 0;
        if (i % 25 >= (i < 25 ? 3 : 2)) CHECK(medium >= 1);   /* past a chapter's opening: more than plain sums */
        unsigned ops = ops_of(&p, &three);
        int ch = i / 25;
        if (ch == 0) CHECK(ops == 1u << MCR_ADD);   /* plus alone first, sums to ten */
        if (ch == 0)
            for (int c = 0; c < p.w * p.h; c++) CHECK(p.kind[c] != MCR_NUM || p.val[c] <= 10);
        if (ch <= 1) CHECK(!(ops & (1u << MCR_MUL | 1u << MCR_DIV)));
        if (ch <= 2) CHECK(!(ops & 1u << MCR_DIV));
        if (ch < 5) CHECK(three == 0);
        else CHECK(three > 0);
        mcr_game g;
        CHECK(mcr_load(&g, &p) && g.ngap >= 2);
        if (i < 3) open[i] = g.ngap;
        for (int j = 0; j < i; j++) CHECK(strcmp(mcr_levels[j], mcr_levels[i]));
    }
    CHECK(open[2] > open[0] && open[2] > open[1]);   /* level 3 a step up from the first two */
    CHECK(mediums > 30);
    printf("levels: %d need more than the plain steps\n", mediums);
}

static void test_make(void)
{
    clock_t worst = 0;
    for (int v = 0; v < MCR_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 25; seed++) {
            mcr_puzzle p;
            mcr_make m = mcr_variant_make(v);
            clock_t t0 = clock();
            mcr_make_puzzle(&m, seed, &p);
            clock_t t = clock() - t0;
            if (t > worst) worst = t;
            CHECK(all_true(&p));
            CHECK(mcr_reason(&p, NULL));
            CHECK(mcr_count(&p, 2) == 1);
            mcr_game g;
            CHECK(mcr_load(&g, &p) && g.ngap >= 2);
            CHECK(g.neq >= m.eqs - 1);
        }
    printf("make: the slowest puzzle %.0f ms\n", worst * 1000.0 / CLOCKS_PER_SEC);
    CHECK(worst * 1000.0 / CLOCKS_PER_SEC < 500);
}

/* by hand: 3 + 4 = 7 across, 7 - 2 = 5 down from the 7; the 4 and the 2 missing */
static void test_hand(void)
{
    mcr_puzzle p;
    CHECK(mcr_decode("5x5:#+#=#....-....#....=....#:3,?4,7,?2,5", &p));
    CHECK(p.w == 5 && p.h == 5 && p.kind[1] == MCR_ADD && p.kind[9] == MCR_SUB && p.val[14] == 2);
    CHECK(all_true(&p) && mcr_reason(&p, NULL) && mcr_count(&p, 9) == 1);
    mcr_game g;
    CHECK(mcr_load(&g, &p));
    CHECK(g.neq == 2 && g.ngap == 2 && g.ntray == 2 && g.tray[0] == 2 && g.tray[1] == 4);
    CHECK(mcr_value(&g, 2) == 0 && mcr_value(&g, 0) == 3);
    CHECK(mcr_script(&g, "c1=2"));              /* 3 + 2 = 7: wrong */
    CHECK(mcr_eq_state(&g, 0) == -1 && g.mistakes == 1);
    CHECK(mcr_script(&g, "e3=4"));              /* 7 - 4 = 5: wrong too */
    CHECK(g.mistakes == 2 && !mcr_solved(&g));
    CHECK(mcr_script(&g, "c1>e3"));             /* swapped: both right */
    CHECK(mcr_solved(&g) && g.moves == 3 && g.mistakes == 2);
    CHECK(mcr_undo(&g) && !mcr_solved(&g) && g.moves == 2);
    CHECK(mcr_script(&g, "e3"));                /* the 4 back to the tray */
    CHECK(mcr_value(&g, 14) == 0 && mcr_tile_free(&g, 1));
    CHECK(mcr_hint(&g) == 0 && g.hints == 1);   /* the wrong 2 in c1 put right first: the 4 */
    CHECK(mcr_value(&g, 2) == 4);
    CHECK(mcr_hint(&g) == 1 && mcr_solved(&g));
    while (mcr_undo(&g)) {}
    CHECK(g.moves == 0 && mcr_value(&g, 2) == 0 && mcr_value(&g, 14) == 0);
    CHECK(!mcr_decode("5x5:#+#=#....-....#....=....#:3,?4,7,?2", &p));   /* a number short */
    CHECK(mcr_decode("5x2:#+#=##....:1,1,2,3", &p));
    mcr_eq eq[MCR_EQS];
    CHECK(mcr_parse(&p, eq) == 0);              /* two cells side by side down: not an equation */
}

static void test_games(void)
{
    for (uint32_t seed = 1; seed <= 30; seed++) {
        mcr_game g;
        mcr_new(&g, (int)(seed % MCR_VARIANTS), seed);
        uint32_t s = pr_seed(seed);
        for (int k = 0; k < 3 * g.ngap; k++) {
            int r = pr_below(&s, 4), i = pr_below(&s, g.ngap);
            if (r < 2) mcr_put(&g, i, pr_below(&s, g.ntray));
            else if (r == 2) mcr_move(&g, i, pr_below(&s, g.ngap));
            else mcr_undo(&g);
        }
        int guard = 0;
        while (!mcr_solved(&g) && guard++ < 4 * g.ngap) CHECK(mcr_hint(&g) >= 0);
        CHECK(mcr_solved(&g));
        for (int t = 0; t < g.ntray; t++) CHECK(!mcr_tile_free(&g, t));
        while (mcr_undo(&g)) {}
        for (int i = 0; i < g.ngap; i++) CHECK(g.at[i] == -1);
        CHECK(mcr_script(&g, "+99") && mcr_solved(&g));
    }
}

int main(void)
{
    test_eval();
    test_hand();
    test_levels();
    test_make();
    test_games();
    if (fails) {
        printf("mathcross: %d failures\n", fails);
        return 1;
    }
    printf("mathcross: ok\n");
    return 0;
}
