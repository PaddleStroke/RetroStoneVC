/*
 * Bridges' rules: the links and their crossings on a small puzzle by hand, the solver on it, the puzzles made from
 * many seeds for every variant (no island beside another, numbers 1..8, the solution's bridges crossing nothing and
 * joining every island, one solution by a plain search, the level they were made for, made fast), the player's
 * bridges, the crossing refused, hints and undo, whole games from a seed, and every designed level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/hashi.h"
#include "games/hashi_levels.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static void island(hsh_puzzle *p, int x, int y, int need)
{
    int i = p->nisl++;
    p->ix[i] = (uint8_t)x;
    p->iy[i] = (uint8_t)y;
    p->need[i] = (uint8_t)need;
    p->at[y * p->n + x] = (int16_t)i;
}

static int puzzle_ok(const hsh_puzzle *p)
{
    int n = p->n, has[HSH_ISLANDS] = {0}, root[HSH_ISLANDS];
    for (int i = 0; i < p->nisl; i++) {
        root[i] = i;
        if (p->need[i] < 1 || p->need[i] > 8) return 0;
        for (int j = 0; j < p->nisl; j++)
            if (j != i && abs(p->ix[i] - p->ix[j]) + abs(p->iy[i] - p->iy[j]) == 1) return 0;
        if (p->at[p->iy[i] * n + p->ix[i]] != i) return 0;
    }
    for (int l = 0; l < p->nlink; l++) {
        if (p->sol[l] > 2) return 0;
        has[p->la[l]] += p->sol[l];
        has[p->lb[l]] += p->sol[l];
        for (int k = 0; k < p->nlink; k++)
            if (p->sol[l] && p->sol[k] && hsh_cross(p, l, k)) return 0;
        if (p->sol[l]) {
            int a = p->la[l], b = p->lb[l];
            while (root[a] != a) a = root[a];
            while (root[b] != b) b = root[b];
            root[a] = b;
        }
    }
    int groups = 0;
    for (int i = 0; i < p->nisl; i++) {
        if (has[i] != p->need[i]) return 0;
        groups += root[i] == i;
    }
    return groups == 1;
}

int main(void)
{
    /* by hand on 5 x 5:   2 . 3 . 1      the 2 and the 1 in the corners, a 3 between them on the top row,
     *                     . . . . .      a 2 below the left corner; the 3 and the 2 join to the 2 at the
     *                     2 . 3 . .      bottom middle.
     *                     . . . . .
     *                     . . 2 . .                                                                          */
    hsh_puzzle p;
    memset(&p, 0, sizeof p);
    p.n = 5;
    for (int c = 0; c < 25; c++) p.at[c] = -1;
    island(&p, 0, 0, 2);
    island(&p, 2, 0, 3);
    island(&p, 4, 0, 1);
    island(&p, 0, 2, 2);
    island(&p, 2, 2, 3);
    island(&p, 2, 4, 1);
    hsh_links(&p);
    CHECK(p.nlink == 6);                         /* a-c, a-down, c-e, c-down, row 3: a-c, c-down */
    CHECK(hsh_link(&p, 0, 1) >= 0 && hsh_link(&p, 1, 2) >= 0 && hsh_link(&p, 0, 3) >= 0);
    CHECK(hsh_link(&p, 0, 2) < 0);               /* the 3 is between them */
    CHECK(hsh_link(&p, 4, 5) >= 0 && hsh_link(&p, 3, 4) >= 0 && hsh_link(&p, 1, 4) >= 0);
    /* solution: a1-c1 1, c1-e1 1, a1-a3 1, a3-c3 1, c1-c3 1, c3-c5 1: numbers 2 3 1 2 3 1 */
    CHECK(hsh_count(&p, 3) >= 1);
    hsh_puzzle q = p;
    q.need[0] = 7;                               /* two links can take four at most */
    CHECK(hsh_count(&q, 2) == 0 && hsh_rate(&q) == BENCH_NONE);
    /* crossing: an across link over a down one */
    hsh_puzzle x;
    memset(&x, 0, sizeof x);
    x.n = 5;
    for (int c = 0; c < 25; c++) x.at[c] = -1;
    island(&x, 0, 2, 1);
    island(&x, 4, 2, 1);
    island(&x, 2, 0, 1);
    island(&x, 2, 4, 1);
    hsh_links(&x);
    CHECK(x.nlink == 2 && hsh_cross(&x, 0, 1));
    CHECK(hsh_count(&x, 2) == 0);                /* two pairs that can't both be bridged, and can't be joined */

    for (int var = 0; var < HSH_VARIANTS; var++) {
        int n = hsh_size_of(var), level = hsh_level_of(var), exact = 0, islands = 0;
        double slowest = 0, total = 0;
        int tries = n >= 13 ? 30 : 50;
        for (int s = 1; s <= tries; s++) {
            hsh_puzzle m;
            clock_t t0 = clock();
            hsh_make(n, level, (uint32_t)s * 7919u + (uint32_t)var, &m);
            double took = (double)(clock() - t0) / CLOCKS_PER_SEC;
            total += took;
            if (took > slowest) slowest = took;
            CHECK(m.n == n);
            CHECK(puzzle_ok(&m));
            int r = hsh_rate(&m);
            CHECK(r >= 0 && r <= level);
            exact += r == level;
            islands += m.nisl;
            if (s <= 10) CHECK(hsh_count(&m, 2) == 1);
        }
        printf("bridges %dx%d level %d: %d/%d of the level, %.1f islands, %.1f ms on average, slowest %.1f ms\n", n, n,
               level, exact, tries, (double)islands / tries, 1000 * total / tries, 1000 * slowest);
        CHECK(exact >= tries * 9 / 10);
        CHECK(slowest < 0.35);
    }

    /* the player's bridges */
    hsh_game g;
    hsh_new(&g, 0, 77);
    int l = 0;
    CHECK(hsh_cycle(&g, l) && g.br[l] == 1);
    CHECK(hsh_cycle(&g, l) && g.br[l] == 2);
    CHECK(hsh_cycle(&g, l) && g.br[l] == 0);
    CHECK(g.hist.moves == 3);
    CHECK(hsh_undo(&g) == l && g.br[l] == 2);
    while (hsh_undo(&g) >= 0) {}
    CHECK(g.br[l] == 0 && g.hist.moves == 0);
    for (int a = 0; a < g.p.nlink; a++)          /* a bridge across another is refused */
        for (int b = 0; b < g.p.nlink; b++)
            if (hsh_cross(&g.p, a, b)) {
                CHECK(hsh_set(&g, a, 1));
                CHECK(hsh_blocked(&g, b) == a && !hsh_cycle(&g, b));
                hsh_undo(&g);
                a = b = g.p.nlink;
            }
    CHECK(hsh_hint(&g) >= 0);
    char mv[16];
    int a = g.p.la[1], b = g.p.lb[1];
    snprintf(mv, sizeof mv, "%c%d-%c%d=2", 'a' + g.p.ix[a], g.p.iy[a] + 1, 'a' + g.p.ix[b], g.p.iy[b] + 1);
    if (hsh_blocked(&g, 1) < 0) CHECK(hsh_script(&g, mv) && g.br[1] == 2);
    CHECK(!hsh_script(&g, "a1-a1"));

    for (uint32_t seed = 1; seed <= 30; seed++) {
        uint32_t rng = pr_seed(seed);
        hsh_new(&g, (int)(seed % HSH_VARIANTS), seed);
        int steps = 0;
        while (!hsh_solved(&g) && steps < 3000) {
            int k = pr_below(&rng, g.p.nlink);
            if (steps < 2 * g.p.nlink && pr_below(&rng, 3)) hsh_cycle(&g, k);
            else CHECK(hsh_hint(&g) >= 0);
            steps++;
        }
        CHECK(hsh_solved(&g));
        CHECK(hsh_hint(&g) < 0);
        for (int i = 0; i < g.p.nisl; i++) CHECK(hsh_has(&g, i) == g.p.need[i]);
        while (hsh_undo(&g) >= 0) {}
        for (int k = 0; k < g.p.nlink; k++) CHECK(!g.br[k]);
    }

    /* every designed level (hashi_levels.h): read back the same, one solution (the grader proves it, a plain search
     * finds no other), the first chapter counting alone, the last one 13 x 13 or bigger, no two the
     * same; hints alone solve it */
    {
        int bad = 0, per = HSH_NLEVELS / HSH_CHAPTERS;
        for (int i = 0; i < HSH_NLEVELS; i++) {
            static hsh_game L;
            char back[HSH_CELLS + 8];
            int ok = hsh_decode(&L, hsh_levels[i]);
            if (ok) hsh_encode(&L.p, back);
            ok = ok && !strcmp(back, hsh_levels[i]) && hsh_count(&L.p, 2) == 1 && hsh_rate(&L.p) == L.level;
            ok = ok && (i >= per || L.level == HSH_EASY) && (i < 5 * per || L.p.n >= 13);
            for (int j = 0; ok && j < i; j++) ok = strcmp(hsh_levels[i], hsh_levels[j]) != 0;
            for (int guard = 0; ok && !hsh_solved(&L) && guard < 400; guard++) ok = hsh_hint(&L) >= 0;
            ok = ok && hsh_solved(&L);
            if (!ok) printf("bridges level %d: bad\n", i + 1);
            bad += !ok;
        }
        CHECK(bad == 0);
        static hsh_game L;
        CHECK(!hsh_decode(&L, "5:..") && !hsh_decode(&L, "5:1...2....................") && !hsh_decode(&L, "x"));
    }

    printf(fails ? "bridges: %d FAILED\n" : "bridges: all passed\n", fails);
    return fails != 0;
}
