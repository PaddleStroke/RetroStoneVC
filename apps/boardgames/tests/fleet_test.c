/*
 * Hidden Fleet's rules: the fleets, the parts of ships, the code of the levels, the solver and the grader on seas
 * drawn by hand, the puzzles made from seeds (one solution, no harder than their level, quick), the player's ship
 * cells, water, drags, the faults shown and the ships found, hints and undo, whole games played from a seed, and
 * every level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/fleet.h"
#include "games/fleet_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the player's sea with the ship cells of sh[] (1: a ship cell): solved? */
static int keeps_rules(const hfl_puzzle *p, const uint8_t *sh)
{
    static hfl_game g;
    memset(&g, 0, sizeof g);
    g.p = *p;
    for (int c = 0; c < p->n * p->n; c++) g.st[c] = sh[c] ? HFL_SHIP : HFL_WATER;
    return hfl_solved(&g);
}

int main(void)
{
    hfl_puzzle p;
    uint8_t sol[HFL_CELLS];

    /* the fleets: the ship cells grow with the sea */
    {
        int f[HFL_LONG + 1], cells[11] = {0};
        for (int n = 6; n <= 10; n++) {
            CHECK(hfl_fleet(n, f) >= 6);
            for (int l = 1; l <= HFL_LONG; l++) cells[n] += l * f[l];
            CHECK(cells[n] * 3 < n * n);
        }
        CHECK(cells[6] == 10 && cells[10] == 20 && cells[8] > cells[7] && cells[9] > cells[8]);
    }
    /* the parts: a ship of three across, one of one */
    {
        uint8_t sh[16] = {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0};
        CHECK(hfl_part(sh, 4, 0) == HFL_LEFT && hfl_part(sh, 4, 1) == HFL_MIDDLE && hfl_part(sh, 4, 2) == HFL_RIGHT);
        CHECK(hfl_part(sh, 4, 11) == HFL_ONE);
        uint8_t down[16] = {0, 1, 0, 0, 0, 1, 0, 0};
        CHECK(hfl_part(down, 4, 1) == HFL_TOP && hfl_part(down, 4, 5) == HFL_BOTTOM);
    }
    /* the code: back and forth; bad ones refused */
    {
        char back[256];
        CHECK(hfl_decode("6:223030031213:dcvceo", &p) && p.n == 6 && p.row[0] == 2 && p.col[5] == 3);
        CHECK(p.given[2 * 6 + 3] == HFL_BOTTOM && p.given[4 * 6 + 2] == HFL_ONE);
        CHECK(hfl_encode(&p, back, sizeof back) && !strcmp(back, "6:223030031213:dcvceo"));
        CHECK(!hfl_decode("6:22303003121:dcv", &p) && !hfl_decode("6:223030031213:dcx", &p));
        CHECK(!hfl_decode("6:223030031213:zcv", &p) && !hfl_decode("6:223030031213dcv", &p));
    }
    /* drawn by hand: the counts of a known fleet with every ship cell shown solve at once; with impossible counts
     * nothing */
    {
        static const uint8_t SH[36] = {1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
                                       1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0};
        memset(&p, 0, sizeof p);
        p.n = 6;
        for (int c = 0; c < 36; c++) {
            p.row[c / 6] = (int8_t)(p.row[c / 6] + SH[c]);
            p.col[c % 6] = (int8_t)(p.col[c % 6] + SH[c]);
            p.given[c] = SH[c] ? (uint8_t)hfl_part(SH, 6, c) : HFL_NONE;
        }
        CHECK(hfl_grade(&p, sol) == BENCH_EASY && !memcmp(sol, SH, 36) && hfl_count(&p, 2) == 1);
        CHECK(keeps_rules(&p, SH));
        p.row[0] = 6;
        CHECK(hfl_count(&p, 2) == 0 && hfl_grade(&p, NULL) == BENCH_NONE);
    }

    /* the puzzles from seeds: one solution, no harder than their level, quick */
    clock_t t0 = clock();
    double slowest[HFL_VARIANTS] = {0};
    int at_level[HFL_VARIANTS] = {0};
    for (int v = 0; v < HFL_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 6; seed++) {
            clock_t t1 = clock();
            hfl_make(hfl_size(v), hfl_level(v), seed * 7919u + (uint32_t)v, &p, sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            CHECK(p.n == hfl_size(v) && keeps_rules(&p, sol));
            uint8_t found[HFL_CELLS];
            int lv = hfl_grade(&p, found);
            CHECK(lv >= BENCH_EASY && lv <= hfl_level(v));
            at_level[v] += lv == hfl_level(v);
            CHECK(!memcmp(found, sol, (size_t)p.n * p.n));
            CHECK(hfl_count(&p, 2) == 1);
        }
    printf("fleet: 6 puzzles a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < HFL_VARIANTS; v++) printf(" %.3f", slowest[v]);
    printf(" s; at their level");
    for (int v = 0; v < HFL_VARIANTS; v++) printf(" %d", at_level[v]);
    printf("\n");
    for (int v = 0; v < HFL_VARIANTS; v++) CHECK(slowest[v] < 0.6 && at_level[v] >= 4);

    /* the same seed: the same puzzle */
    static hfl_game a, b;
    hfl_new(&a, 7, 42);
    hfl_new(&b, 7, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's cells: a tap a ship cell or not, a hold water; shown cells stay; undo */
    static hfl_game G;
    CHECK(hfl_decode(hfl_levels[0], &p) && hfl_load(&G, &p));
    int n = G.p.n, f = -1, shown = -1;
    for (int c = 0; c < n * n; c++) {
        if (!G.p.given[c] && f < 0) f = c;
        if (G.p.given[c] && shown < 0) shown = c;
    }
    {                                           /* a shown one-cell ship is found at once */
        int count[HFL_LONG + 1];
        hfl_found(&G, count);
        CHECK(count[1] >= 1);
    }
    CHECK(f >= 0 && shown >= 0 && !hfl_free(&G, shown) && G.st[shown] != HFL_EMPTY);
    CHECK(!hfl_paint(&G, &shown, 1, HFL_EMPTY));
    CHECK(hfl_tap_state(&G, f) == HFL_SHIP && hfl_hold_state(&G, f) == HFL_WATER);
    CHECK(hfl_paint(&G, &f, 1, HFL_SHIP) && hfl_tap_state(&G, f) == HFL_EMPTY && hfl_hold_state(&G, f) == HFL_WATER);
    CHECK(hfl_paint(&G, &f, 1, HFL_WATER) && hfl_tap_state(&G, f) == HFL_SHIP);
    CHECK(G.hist.moves == 2 && hfl_undo(&G) == f && G.st[f] == HFL_SHIP && hfl_undo(&G) == f && !G.st[f]);
    CHECK(hfl_undo(&G) < 0);
    /* the faults and the ships found: two cells touching at a corner, a ship too long, a ship closed in water */
    {
        static hfl_game F;
        memset(&F, 0, sizeof F);
        F.p.n = 6;
        F.st[0] = F.st[7] = HFL_SHIP;
        CHECK(hfl_clash(&F, 0) && hfl_clash(&F, 7) && hfl_line(&F, 0, 0) == 1 && hfl_line(&F, 1, 1) == 1);
        F.st[7] = HFL_EMPTY;
        for (int x = 0; x < 5; x++) F.st[2 * 6 + x] = HFL_SHIP;
        CHECK(hfl_clash(&F, 12) && !hfl_clash(&F, 0));
        memset(F.st, 0, sizeof F.st);
        F.st[6] = F.st[7] = HFL_SHIP;           /* a ship of two at a2-b2 */
        int count[HFL_LONG + 1];
        hfl_found(&F, count);
        CHECK(count[2] == 0);
        F.st[0] = F.st[1] = F.st[8] = F.st[12] = F.st[13] = HFL_WATER;
        hfl_found(&F, count);
        CHECK(count[2] == 1 && count[1] == 0);
    }
    /* the notation: a drag puts water down a column (shown cells left as they are) */
    n = G.p.n;
    {
        int col = -1;
        for (int x = 0; x < n && col < 0; x++)
            if (!G.p.given[x] && !G.p.given[n + x] && !G.p.given[2 * n + x]) col = x;
        char m[16];
        snprintf(m, sizeof m, "%c1-%c3", 'a' + col, 'a' + col);
        CHECK(col >= 0 && hfl_script(&G, m) && G.st[col] == HFL_WATER && G.st[n + col] == HFL_WATER);
        CHECK(G.hist.moves == 1 && hfl_undo(&G) >= 0 && !G.st[col] && !G.st[2 * n + col]);
    }
    CHECK(!hfl_script(&G, "z9") && !hfl_script(&G, "a1-c3"));

    /* a hint: a wrong cell put right first, else the next cell found; hints alone solve a puzzle */
    for (int l = 0; l < HFL_LEVELS; l += 37) {
        CHECK(hfl_decode(hfl_levels[l], &p) && hfl_load(&G, &p));
        n = G.p.n;
        int wrong = -1;
        for (int c = 0; c < n * n && wrong < 0; c++)
            if (!G.sol[c] && hfl_free(&G, c)) wrong = c;
        CHECK(hfl_paint(&G, &wrong, 1, HFL_SHIP) && hfl_hint(&G) == wrong && G.st[wrong] == HFL_WATER);
        int guard = 0;
        while (!hfl_solved(&G) && guard++ < n * n) {
            int c = hfl_hint(&G);
            CHECK(c >= 0 && (G.st[c] == HFL_SHIP) == G.sol[c]);
            if (c < 0) break;
        }
        CHECK(hfl_solved(&G) && G.hist.hints > 0);
    }

    /* whole games from a seed: random taps, holds and drags, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 20; seed++) {
        uint32_t s = pr_seed(seed);
        hfl_new(&G, (int)(seed % HFL_VARIANTS), seed);
        n = G.p.n;
        static hfl_game first;
        first = G;
        for (int i = 0; i < 40; i++) {
            int c = pr_below(&s, n * n), kind = pr_below(&s, 3);
            if (kind < 2) {
                hfl_paint(&G, &c, 1, kind ? hfl_hold_state(&G, c) : hfl_tap_state(&G, c));
            } else {
                int cells[HFL_MAX], k = 0, was = G.st[c];
                for (int x = c; x < (c / n + 1) * n; x++)
                    if (G.st[x] == was && hfl_free(&G, x)) cells[k++] = x;
                hfl_paint(&G, cells, k, hfl_hold_state(&G, c));
            }
        }
        int guard = 0;
        while (!hfl_solved(&G) && guard++ < 2 * n * n) CHECK(hfl_hint(&G) >= 0);
        CHECK(hfl_solved(&G));
        while (hfl_undo(&G) >= 0) {}
        CHECK(!memcmp(G.st, first.st, sizeof G.st) && G.hist.moves == 0);
    }
    /* the stars' mistakes: a wrong cell taken back with undo */
    hfl_decode(hfl_levels[0], &p);
    hfl_load(&G, &p);
    {
        int c = -1;
        for (int i = 0; i < G.p.n * G.p.n && c < 0; i++)
            if (!G.sol[i] && hfl_free(&G, i)) c = i;
        hfl_paint(&G, &c, 1, HFL_SHIP);
        hfl_undo(&G);
        CHECK(G.hist.mistakes == 1 && G.hist.hints == 0);
    }

    /* the levels: each decodes and encodes back, has one solution found by reasoning alone (easy in chapter 1, hard
     * at the end), and the sizes never shrink */
    {
        int last_n = 0, grades[3] = {0, 0, 0};
        clock_t t1 = clock();
        for (int l = 0; l < HFL_LEVELS; l++) {
            hfl_puzzle q;
            static hfl_game L;
            char back[512];
            int ok = hfl_decode(hfl_levels[l], &q) && hfl_load(&L, &q) && hfl_count(&q, 2) == 1;
            ok = ok && keeps_rules(&q, L.sol);
            if (!ok) printf("fleet level %d: not one solution\n", l + 1);
            CHECK(ok);
            CHECK(hfl_encode(&q, back, sizeof back) && !strcmp(back, hfl_levels[l]));
            CHECK(q.n >= last_n);
            last_n = q.n;
            if (ok) grades[L.level]++;
            if (l < HFL_LEVELS / HFL_CHAPTERS) CHECK(L.level == BENCH_EASY);
        }
        hfl_puzzle q;
        CHECK(hfl_decode(hfl_levels[HFL_LEVELS - 1], &q) && hfl_grade(&q, NULL) == BENCH_HARD);
        printf("fleet: %d levels, one solution each (easy %d, medium %d, hard %d) in %.1f s\n", HFL_LEVELS, grades[0],
               grades[1], grades[2], (double)(clock() - t1) / CLOCKS_PER_SEC);
    }

    if (fails) printf("fleet: %d checks failed\n", fails);
    else printf("fleet: all checks passed\n");
    return fails != 0;
}
