/*
 * The 15 Puzzle's rules: the parity rule checked against every position of the 3 x 3 tray (a search from the solved
 * tray reaches exactly the orders it calls solvable) and against random slides on the larger trays, the shuffles from
 * many seeds, the slides of a row or column, undo, and whole games solved from a seed on the 3 x 3 tray.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/taquin.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the 3 x 3 trays numbered 0..9!-1 (the order's rank) */
static int rank9(const int8_t *at)
{
    int r = 0, used = 0;
    for (int i = 0; i < 9; i++) {
        int less = 0;
        for (int v = 0; v < at[i]; v++) less += !(used >> v & 1);
        used |= 1 << at[i];
        r = r * (9 - i) + less;
    }
    return r;
}

static void unrank9(int r, int8_t *at)
{
    int digits[9], used = 0;
    for (int i = 8; i >= 0; i--) {
        digits[i] = r % (9 - i);
        r /= 9 - i;
    }
    for (int i = 0; i < 9; i++) {
        int k = digits[i];
        for (int v = 0; v < 9; v++)
            if (!(used >> v & 1) && k-- == 0) { at[i] = (int8_t)v; used |= 1 << v; break; }
    }
}

int main(void)
{
    /* every 3 x 3 order: a search from the solved tray reaches it exactly when the parity rule says so; its distance
     * to the solved tray, for the whole games below */
    enum { N9 = 362880 };
    int16_t *dist = malloc(N9 * sizeof *dist);
    int *queue = malloc(N9 * sizeof *queue);
    if (!dist || !queue) return 1;
    for (int i = 0; i < N9; i++) dist[i] = -1;
    int8_t at[TQ_PLACES] = {1, 2, 3, 4, 5, 6, 7, 8, 0};
    int head = 0, tail = 0, start = rank9(at);
    dist[start] = 0;
    queue[tail++] = start;
    while (head < tail) {
        int r = queue[head++];
        unrank9(r, at);
        tq_game g;
        g.picture = 0;
        tq_load(&g, 3, at);
        for (int p = 0; p < 9; p++) {
            if (tq_line(&g, p) != 1) continue;
            tq_game h = g;
            tq_tap(&h, p);
            int k = rank9(h.at);
            if (dist[k] < 0) { dist[k] = (int16_t)(dist[r] + 1); queue[tail++] = k; }
        }
    }
    CHECK(tail == N9 / 2);
    int agree = 1;
    for (int r = 0; r < N9; r++) {
        unrank9(r, at);
        if ((dist[r] >= 0) != tq_solvable(at, 3)) agree = 0;
    }
    CHECK(agree);

    /* the larger trays: random slides from the solved tray keep it solvable; a tray with two tiles swapped stays
     * unsolvable */
    uint32_t s = 12345;
    for (int n = 3; n <= 5; n++)
        for (int swap = 0; swap < 2; swap++) {
            int8_t t[TQ_PLACES];
            for (int i = 0; i < n * n; i++) t[i] = (int8_t)((i + 1) % (n * n));
            if (swap) { int8_t x = t[0]; t[0] = t[1]; t[1] = x; }
            tq_game g;
            g.picture = 0;
            tq_load(&g, n, t);
            int ok = 1;
            for (int k = 0; k < 4000; k++) {
                int p = pr_below(&s, n * n);
                if (tq_line(&g, p)) tq_tap(&g, p);
                if (tq_solvable(g.at, n) != !swap) ok = 0;
            }
            CHECK(ok);
        }

    /* the shuffles from many seeds: every tile once, solvable, not already solved, the same from the same seed */
    for (int v = 0; v < TQ_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 400; seed++) {
            tq_game g, h;
            tq_new(&g, v, seed);
            tq_new(&h, v, seed);
            CHECK(g.n == 3 + v % 3 && g.picture == (v >= 3));
            CHECK(!memcmp(g.at, h.at, sizeof g.at));
            int seen = 0;
            for (int i = 0; i < g.n * g.n; i++) seen |= 1 << g.at[i];
            CHECK(seen == (1 << (g.n * g.n)) - 1);
            CHECK(tq_solvable(g.at, g.n) && !tq_solved(&g));
            CHECK(g.at[g.gap] == 0 && g.moves == 0);
        }

    /* slides: a tile beside the gap, a whole row, a column; not off the gap's lines; undo */
    int8_t t4[TQ_PLACES] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0};
    tq_game g;
    g.picture = 0;
    tq_load(&g, 4, t4);
    CHECK(tq_solved(&g));
    CHECK(tq_tap(&g, 12) == 3 && g.gap == 12 && g.at[13] == 13 && g.at[14] == 14 && g.at[15] == 15 && g.moves == 3);
    CHECK(tq_tap(&g, 0) == 3 && g.gap == 0 && g.at[4] == 1 && g.at[12] == 9 && g.moves == 6);
    CHECK(!tq_tap(&g, 5) && !tq_tap(&g, 0));
    CHECK(tq_undo(&g) == 12 && g.moves == 3 && g.at[0] == 1);
    CHECK(tq_undo(&g) == 15 && g.moves == 0 && tq_solved(&g));
    CHECK(tq_undo(&g) < 0);
    CHECK(tq_script(&g, "15 11 7") && g.gap == 6 && g.moves == 3);
    CHECK(!tq_script(&g, "1"));                 /* not in line with the gap */
    CHECK(tq_script(&g, "@") && !tq_solved(&g) && tq_script(&g, "15") && tq_solved(&g));

    /* whole games: 3 x 3 trays from seeds solved by the search's shortest way, then undone back */
    for (uint32_t seed = 1; seed <= 200; seed++) {
        tq_new(&g, 0, seed);
        tq_game first = g;
        int steps = 0;
        while (!tq_solved(&g) && steps < 40) {
            int best = -1, bd = 1 << 20;
            for (int p = 0; p < 9; p++) {
                if (tq_line(&g, p) != 1) continue;
                tq_game h = g;
                tq_tap(&h, p);
                int d = dist[rank9(h.at)];
                if (d >= 0 && d < bd) { bd = d; best = p; }
            }
            CHECK(best >= 0);
            if (best < 0) break;
            tq_tap(&g, best);
            steps++;
        }
        CHECK(tq_solved(&g) && steps == dist[rank9(first.at)] && g.moves == steps);
        while (tq_undo(&g) >= 0) {}
        CHECK(!memcmp(g.at, first.at, sizeof g.at) && g.moves == 0);
    }
    /* the search: on 3 x 3 trays (one in 61 of all the solvable ones) the bound never above the distance and the
     * exact search equal to it; the weighted searches never shorter */
    int exact = 1, weighted = 1;
    for (int r = 0; r < N9; r += 61) {
        if (dist[r] < 0) continue;
        unrank9(r, at);
        if (tq_bound(at, 3) > dist[r] || tq_search(at, 3, 4, 10000000L, NULL) != dist[r]) exact = 0;
        int w = tq_search(at, 3, 8, 10000000L, NULL);
        if (w < dist[r] || (w - dist[r]) % 2) weighted = 0;
    }
    CHECK(exact && weighted);
    free(dist);
    free(queue);

    /* random trays: the par from a seed (exact on 3 x 3; on the larger ones the best found in time, never below the
     * bound), and how long it takes */
    double slowest[TQ_MAX + 1] = {0};
    for (int v = 0; v < 3; v++)
        for (uint32_t seed = 1; seed <= (v == 2 ? 20u : 40u); seed++) {
            tq_new(&g, v, seed);
            clock_t t0 = clock();
            int par = tq_par(&g);
            double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC;
            if (ms > slowest[g.n]) slowest[g.n] = ms;
            CHECK(par == 0 || (par >= tq_bound(g.at, g.n) && (par - tq_bound(g.at, g.n)) % 2 == 0));
            if (g.n == 3) CHECK(par > 0);
        }
    printf("taquin: the slowest par 3 x 3 %.0f ms, 4 x 4 %.0f ms, 5 x 5 %.0f ms\n", slowest[3], slowest[4], slowest[5]);

    /* the levels: each loads, its par found again by the exact search, a chapter's trays all of one size, never
     * smaller than the chapter before, no two alike */
    int last_n = 0;
    for (int l = 0; l < TQ_LEVELS; l++) {
        CHECK(tq_level(&g, l));
        CHECK(!tq_solved(&g) && g.moves == 0 && g.par == tq_level_par[l] && g.par > 0);
        CHECK(tq_search(g.at, g.n, 4, 20000000L, NULL) == g.par);
        if (l % (TQ_LEVELS / TQ_CHAPTERS)) CHECK(g.n == last_n);
        else CHECK(g.n >= last_n);
        last_n = g.n;
        for (int k = 0; k < l; k++) CHECK(strcmp(tq_levels[k] + 1, tq_levels[l] + 1) != 0);
    }
    CHECK(!tq_level(&g, TQ_LEVELS) && !tq_parse(&g, "n:123|456|78") && !tq_parse(&g, "n:213|456|78."));
    CHECK(tq_parse(&g, "p:123|456|7.8") && g.picture && g.n == 3 && tq_script(&g, "8") && tq_solved(&g));

    if (fails) {
        printf("taquin: %d failed\n", fails);
        return 1;
    }
    printf("taquin: all passed\n");
    return 0;
}
