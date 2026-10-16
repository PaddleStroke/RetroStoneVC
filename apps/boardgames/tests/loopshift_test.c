/*
 * Row Shift's rules: a slide then its inverse changes nothing, slides of one line merge, undo goes back to the start,
 * the notation; every level read, solved by playing its way (par moves), and its par checked by the search (and, on
 * 3 x 3, against a plain search over every move); random puzzles from many seeds, made in good time; hints followed
 * to the end, after random detours too.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/loopshift.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a plain search over every move (no order, no table): a way of at most d moves exists */
static int plain(const int8_t *at, int n, int colour, int d)
{
    if (lsh_solved_at(at, n, colour)) return 1;
    if (!d) return 0;
    for (int line = 0; line < 2 * n; line++)
        for (int k = 1; k < n; k++) {
            int8_t t[LSH_CELLS];
            memcpy(t, at, sizeof t);
            lsh_apply(t, n, lsh_move(line, k));
            if (plain(t, n, colour, d - 1)) return 1;
        }
    return 0;
}

/* hints followed from where the game is: solved in at most `most` moves */
static int follow(lsh_game *g, int most)
{
    for (int i = 0; i < most && !lsh_solved(g); i++) {
        int m = lsh_hint(g);
        if (m < 0 || !lsh_play(g, m)) return 0;
    }
    return lsh_solved(g);
}

int main(void)
{
    uint32_t s = pr_seed(42);

    /* a slide then its inverse; n slides of one step; merging and undo */
    for (int n = LSH_MIN; n <= LSH_MAX; n++) {
        lsh_game g;
        lsh_init(&g, n, 0);
        CHECK(lsh_solved(&g));
        for (int i = 0; i < 40; i++) lsh_apply(g.at, n, lsh_move(pr_below(&s, 2 * n), 1 + pr_below(&s, n - 1)));
        for (int line = 0; line < 2 * n; line++)
            for (int k = 1; k < n; k++) {
                int8_t t[LSH_CELLS];
                memcpy(t, g.at, sizeof t);
                lsh_apply(t, n, lsh_move(line, k));
                CHECK(memcmp(t, g.at, (size_t)(n * n)));
                lsh_apply(t, n, lsh_inverse(n, lsh_move(line, k)));
                CHECK(!memcmp(t, g.at, (size_t)(n * n)));
                for (int r = 0; r < n; r++) lsh_apply(t, n, lsh_move(line, 1));
                CHECK(!memcmp(t, g.at, (size_t)(n * n)));
            }
        lsh_init(&g, n, 0);
        CHECK(lsh_play(&g, lsh_move(0, 1)) && lsh_moves(&g) == 1);
        CHECK(lsh_play(&g, lsh_move(0, 1)) && lsh_moves(&g) == (n == 2 ? 0 : 1));   /* merged */
        CHECK(lsh_play(&g, lsh_move(n, 1)) && lsh_moves(&g) == 2);
        CHECK(lsh_play(&g, lsh_move(n, n - 1)) && lsh_moves(&g) == 1);   /* slid back: no move */
        CHECK(lsh_play(&g, lsh_move(0, n - 2)) && lsh_moves(&g) == 0 && lsh_solved(&g));
        CHECK(!lsh_play(&g, lsh_move(2 * n, 1)) && !lsh_play(&g, lsh_move(0, 0)) && !lsh_play(&g, lsh_move(0, n)));
        for (int i = 0; i < 60; i++) lsh_play(&g, lsh_move(pr_below(&s, 2 * n), 1 + pr_below(&s, n - 1)));
        while (lsh_undo(&g) >= 0) {}
        CHECK(lsh_solved(&g) && lsh_moves(&g) == 0);
        /* the notation */
        lsh_init(&g, n, 0);
        CHECK(lsh_script(&g, "R1+1 C2-1"));
        CHECK(g.at[0] == n - 1 && lsh_moves(&g) == 2);
        CHECK(lsh_script(&g, "C2+1 R1-1") && lsh_solved(&g));
        CHECK(!lsh_script(&g, "R0+1") && !lsh_script(&g, "X1+1") && !lsh_script(&g, "R1+0"));
        /* colour bands: a row slid changes nothing, a column does */
        lsh_init(&g, n, 1);
        lsh_play(&g, lsh_move(1, 1));
        CHECK(lsh_solved(&g));
        lsh_play(&g, lsh_move(n + 1, 1));
        CHECK(!lsh_solved(&g));
    }

    /* the levels */
    int checked = 0, plain_checked = 0;
    clock_t t0 = clock();
    for (int l = 0; l < LSH_LEVELS; l++) {
        lsh_game g;
        if (!lsh_level(&g, l)) { printf("FAIL level %d: not read\n", l + 1); fails++; continue; }
        int n = g.n, ok = g.par > 0 && !lsh_solved(&g) && lsh_level_size(l) == n;
        for (int i = 0; i < g.nsol; i++) ok &= lsh_play(&g, g.sol[i]);
        ok &= lsh_solved(&g) && lsh_moves(&g) == g.par;
        if (l > 0) ok &= lsh_level_size(l) >= lsh_level_size(l - 1);   /* the sizes never go down */
        for (int k = 0; k < l; k++) ok &= strcmp(lsh_levels[k], lsh_levels[l]) != 0;
        int a, b;
        lsh_depths(n, &a, &b);
        if (g.par <= a + b) {                   /* the par is the fewest */
            int len = lsh_solve(g.start, n, g.colour, a, b, 0, NULL, 0);
            ok &= len == g.par && g.exact;
            checked++;
        }
        if (n == 3) {
            ok &= plain(g.start, n, g.colour, g.par) && !plain(g.start, n, g.colour, g.par - 1);
            plain_checked++;
        }
        if (!ok) { printf("FAIL level %d (%s)\n", l + 1, lsh_levels[l]); fails++; }
    }
    CHECK(lsh_level_size(0) == 3 && lsh_level_size(LSH_LEVELS - 1) == 6);
    printf("levels: %d read and solved, %d pars proven by the search, %d by the plain one (%.1f s)\n", LSH_LEVELS,
           checked, plain_checked, (double)(clock() - t0) / CLOCKS_PER_SEC);

    /* random puzzles: made, solvable in par moves, in good time; hints followed */
    for (int v = 0; v < LSH_VARIANTS; v++) {
        double most = 0;
        int exact = 0, seeds = v % 4 == 3 ? 12 : 40;
        for (int k = 0; k < seeds; k++) {
            lsh_game g;
            clock_t c = clock();
            lsh_new(&g, v, 1000u + (uint32_t)k * 7919u);
            double t = (double)(clock() - c) / CLOCKS_PER_SEC;
            if (t > most) most = t;
            exact += g.exact;
            CHECK(!lsh_solved(&g) && g.par > 0 && g.par == g.nsol);
            CHECK(g.n == LSH_MIN + v % 4 && g.colour == (v >= 4));
            lsh_game h = g;
            for (int i = 0; i < h.nsol; i++) lsh_play(&h, h.sol[i]);
            CHECK(lsh_solved(&h) && lsh_moves(&h) == g.par);
            h = g;                              /* the hints from the start: par moves when the par is proven */
            CHECK(follow(&h, 3 * g.par + 8));
            if (g.exact) CHECK(lsh_moves(&h) == g.par);
            h = g;                              /* a detour first, then the hints */
            uint32_t r = pr_seed(k + 77u);
            for (int i = 0; i < 3; i++) lsh_play(&h, lsh_move(pr_below(&r, 2 * h.n), 1 + pr_below(&r, h.n - 1)));
            CHECK(follow(&h, 200));
        }
        printf("variant %d: %d puzzles, %d pars proven, the slowest made in %.3f s\n", v, seeds, exact, most);
        CHECK(most < 1.0);
    }

    /* the levels' hints: from the start, the par */
    for (int l = 0; l < LSH_LEVELS; l += 7) {
        lsh_game g;
        lsh_level(&g, l);
        CHECK(follow(&g, 100) && (!g.exact || lsh_moves(&g) <= g.par));
    }
    lsh_solve_free();
    printf("%s\n", fails ? "loopshift: FAILED" : "loopshift: all good");
    return fails != 0;
}
