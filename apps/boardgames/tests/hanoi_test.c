/*
 * The Tower of Hanoi's rules: the start, legal and illegal moves, undo, the shortest way for every size (2^n - 1
 * moves, each legal, ending solved), whole games of random legal moves from a seed, the notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/hanoi.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static int well_stacked(const hn_game *g)
{
    int total = 0;
    for (int p = 0; p < 3; p++) {
        total += g->h[p];
        for (int i = 1; i < g->h[p]; i++)
            if (g->disc[p][i] >= g->disc[p][i - 1]) return 0;
    }
    return total == g->n;
}

int main(void)
{
    hn_game g;
    for (int n = 4; n <= 7; n++) {
        hn_new(&g, n);
        CHECK(g.h[0] == n && g.h[1] == 0 && g.h[2] == 0 && hn_top(&g, 0) == 1 && g.disc[0][0] == n);
        CHECK(!hn_solved(&g) && hn_optimum(n) == (1 << n) - 1);
    }

    hn_new(&g, 4);
    CHECK(!hn_move(&g, 1, 2) && !hn_move(&g, 0, 0));   /* an empty peg; the same peg */
    CHECK(hn_move(&g, 0, 1) && hn_top(&g, 1) == 1 && g.moves == 1);
    CHECK(!hn_can(&g, 0, 1));                   /* 2 on 1: never */
    CHECK(hn_move(&g, 0, 2) && hn_top(&g, 2) == 2);
    CHECK(hn_move(&g, 1, 2) && hn_top(&g, 2) == 1 && g.h[2] == 2);
    CHECK(hn_undo(&g) && hn_top(&g, 1) == 1 && g.moves == 2);
    CHECK(hn_undo(&g) && hn_undo(&g) && g.moves == 0 && g.h[0] == 4 && !hn_undo(&g));

    /* the shortest way: every move legal, solved after exactly 2^n - 1, to the right peg or the middle one */
    for (int n = 4; n <= 7; n++)
        for (int target = 1; target <= 2; target++) {
            hn_new(&g, n);
            int ok = 1;
            for (int k = 0; k < hn_optimum(n); k++) {
                int f, t;
                hn_way(n, 0, target, k, &f, &t);
                if (hn_solved(&g) || !hn_move(&g, f, t)) ok = 0;
            }
            CHECK(ok && hn_solved(&g) && g.h[target] == n && g.moves == hn_optimum(n));
        }

    /* random legal moves from seeds: the discs always well stacked; undone back to the start */
    for (uint32_t seed = 1; seed <= 300; seed++) {
        uint32_t s = pr_seed(seed);
        int n = 4 + seed % 4;
        hn_new(&g, n);
        int ok = 1, made = 0;
        for (int k = 0; k < 500; k++) {
            int f = pr_below(&s, 3), t = pr_below(&s, 3);
            int can = hn_can(&g, f, t);
            if (hn_move(&g, f, t) != can) ok = 0;
            made += can;
            if (!well_stacked(&g)) ok = 0;
        }
        CHECK(ok && g.moves == made);
        while (hn_undo(&g)) {}
        CHECK(g.h[0] == n && g.moves == 0);
    }

    hn_new(&g, 5);
    CHECK(hn_script(&g, "ac ab cb") && hn_top(&g, 1) == 1 && g.h[1] == 2 && g.moves == 3);
    CHECK(!hn_script(&g, "ab"));                /* 3 on 1 */
    hn_new(&g, 6);
    CHECK(hn_script(&g, "=62") && !hn_solved(&g) && hn_script(&g, "=1") && hn_solved(&g) && g.h[2] == 6);
    CHECK(!hn_script(&g, "=1"));

    if (fails) {
        printf("hanoi: %d failed\n", fails);
        return 1;
    }
    printf("hanoi: all passed\n");
    return 0;
}
