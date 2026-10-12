/*
 * Eight Queens' rules: the solutions of each board (their known counts, each one checked, none twice), the attacks,
 * putting queens on and off and undo, the fixed queen drawn from hundreds of seeds (always in a solution, one not found
 * yet when there is one), whole games solved by following the hint alone from the start and from random boards, the
 * hint leading to the solutions not found yet, holes, fixed queens and mistakes, every level (one solution, solved by
 * the hint alone), and the time it takes.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/queens.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* follows the hint until solved; the steps it took, or -1 */
static int by_hint(qn_game *g, const uint64_t *found, int nfound, int limit)
{
    for (int steps = 0; steps <= limit; steps++) {
        if (qn_solved(g)) return steps;
        int rm, s = qn_hint(g, found, nfound, &rm);
        if (s < 0 || s == g->given) return -1;
        if (rm != g->q[s]) return -1;           /* puts on an empty square, takes off a queen */
        qn_toggle(g, s);
    }
    return -1;
}

int main(void)
{
    static const int counts[QN_VARIANTS] = {10, 4, 92, 724};
    qn_game g;

    /* the attacks, toggling, undo, the fixed queen */
    CHECK(qn_attacks(8, 0, 7) && qn_attacks(8, 0, 56) && qn_attacks(8, 0, 63) && qn_attacks(8, 7, 56));
    CHECK(!qn_attacks(8, 0, 10) && !qn_attacks(8, 0, 17) && !qn_attacks(8, 5, 5));
    qn_empty(&g, 8, 0);
    CHECK(g.count == 1 && g.q[0] && !qn_toggle(&g, 0) && !qn_toggle(&g, 64));
    CHECK(qn_toggle(&g, 9) && qn_clash(&g, 9) && qn_clash(&g, 0) && qn_attacked(&g, 18) == 2);
    CHECK(qn_toggle(&g, 9) && !g.q[9] && g.count == 1 && g.moves == 2);
    CHECK(qn_undo(&g) && g.q[9] && qn_undo(&g) && !g.q[9] && !qn_undo(&g) && g.moves == 0);
    CHECK(qn_script(&g, "b3 c5") && g.q[17] && g.q[34] && !qn_script(&g, "a1") && !qn_script(&g, "z9"));
    /* a solution put on the board by script: solved */
    qn_empty(&g, 8, -1);
    CHECK(qn_script(&g, "a1 e2 h3 f4 c5 g6 b7 d8") && qn_solved(&g));
    CHECK(qn_toggle(&g, 1) && !qn_solved(&g) && qn_toggle(&g, 1) && qn_solved(&g));
    qn_empty(&g, 10, 0);
    CHECK(qn_script(&g, "? ? ? ? ? ? ? ? ?") && qn_solved(&g) && !qn_script(&g, "?"));

    for (int v = 0; v < QN_VARIANTS; v++) {
        int n = qn_size(v), total = qn_solutions(n);
        CHECK(total == counts[v]);
        /* each solution checked; none twice: found by solving with growing found lists */
        static uint64_t found[1024];
        int nfound = 0, starts_fresh = 1;
        clock_t t0 = clock();
        double worst = 0;
        for (uint32_t seed = 1; seed <= (uint32_t)(total < 300 ? 300 : total + 40); seed++) {
            clock_t a = clock();
            qn_new(&g, v, seed, found, nfound);
            CHECK(g.n == n && g.count == 1 && g.q[g.given]);
            int steps = by_hint(&g, found, nfound, 3 * n);
            double dt = (double)(clock() - a) / CLOCKS_PER_SEC;
            if (dt > worst) worst = dt;
            CHECK(steps >= 0 && steps <= n - 1); /* from the start, the hint only puts queens on */
            CHECK(qn_solved(&g));
            uint64_t k = qn_key(&g);
            int seen = 0;
            for (int j = 0; j < nfound; j++) seen |= found[j] == k;
            if (nfound < total && seen) starts_fresh = 0;   /* a new one each game until all are found */
            if (!seen) found[nfound++] = k;
            /* the solution itself: n queens, one a rank, none attacked */
            for (int s = 0; s < n * n; s++) CHECK(!g.q[s] || qn_attacked(&g, s) == 0);
        }
        CHECK(nfound == total && starts_fresh);
        /* from random boards: the hint takes queens off, then puts them on, until solved */
        for (uint32_t seed = 1; seed <= 200; seed++) {
            uint32_t rs = pr_seed(seed * 7 + (uint32_t)v);
            qn_new(&g, v, seed, NULL, 0);
            int taps = pr_below(&rs, 2 * n);
            for (int i = 0; i < taps; i++) qn_toggle(&g, pr_below(&rs, n * n));
            CHECK(by_hint(&g, NULL, 0, 4 * n + 4) >= 0);
            /* undo all the way back: only the fixed queen */
            while (qn_undo(&g)) {}
            CHECK(g.count == 1 && g.q[g.given] && g.moves == 0);
        }
        printf("%d x %d: %d solutions, all found by new games and hints; the slowest game %.4f s, all %.2f s\n", n, n,
               total, worst, (double)(clock() - t0) / CLOCKS_PER_SEC);
        CHECK(worst < 0.1);
    }

    /* holes and fixed queens, mistakes, the stars */
    CHECK(qn_load(&g, "Q...|..#.|....|....") && g.n == 4 && g.fixed[12] && g.q[12] && g.count == 1 && g.hole[10]);
    CHECK(!qn_toggle(&g, 12) && !qn_toggle(&g, 10) && g.moves == 0);
    CHECK(!qn_load(&g, "Q...|....|....") && !qn_load(&g, "Q..|...|...") && !qn_load(&g, "Q...|..x.|....|...."));
    qn_load(&g, "Q...|..#.|....|....");
    CHECK(qn_toggle(&g, 13) && g.mistakes == 1 && qn_undo(&g) && g.mistakes == 1);   /* b4: in the fixed queen's row */
    CHECK(qn_stars(&g) == 2);
    qn_load(&g, "Q...|..#.|....|....");
    CHECK(qn_toggle(&g, 1) && g.mistakes == 0 && qn_stars(&g) == 3);   /* b1: not attacked */
    g.hints = 2;
    CHECK(qn_stars(&g) == 2);
    g.hints = 3;
    CHECK(qn_stars(&g) == 1);

    /* every level: its size in its chapter's, one solution only, solved by the hint alone (queens put on only), no
     * level twice */
    {
        static const int lo[QN_CHAPTERS] = {4, 6, 7, 8, 9, 10}, hi[QN_CHAPTERS] = {5, 7, 8, 8, 9, 10};
        int holes = 0, fixed = 0;
        for (int l = 0; l < QN_LEVELS; l++) {
            if (!qn_level(&g, l)) {
                printf("FAIL level %d: not loaded\n", l + 1);
                fails++;
                continue;
            }
            int c = l / (QN_LEVELS / QN_CHAPTERS), given = g.count;
            CHECK(g.n >= lo[c] && g.n <= hi[c]);
            uint64_t key = 0;
            int k = qn_count(&g, &key);
            CHECK(k == 1);
            if (k != 1) printf("FAIL level %d: %d solutions\n", l + 1, k);
            for (int s = 0; s < g.n * g.n; s++) holes += g.hole[s], fixed += g.fixed[s];
            for (int m = 0; m < l; m++) CHECK(!qn_levels[m] || strcmp(qn_levels[m], qn_levels[l]) != 0);
            CHECK(by_hint(&g, NULL, 0, g.n) == g.n - given);
            CHECK(qn_solved(&g) && qn_key(&g) == key && g.mistakes == 0);
        }
        printf("%d levels: %d holes, %d fixed queens, each with one solution\n", QN_LEVELS, holes, fixed);
    }

    if (fails) {
        printf("queens: %d failures\n", fails);
        return 1;
    }
    printf("queens: ok\n");
    return 0;
}
