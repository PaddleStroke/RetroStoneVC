/*
 * Hitori's rules: the solver and the grader on grids drawn by hand (twins side by side, a sandwich, shading that would
 * cut the grid, a lone number), the puzzles made from many seeds (one solution, the level they were made for, well
 * under a second each), the player's shading, rings, drags, the faults shown, hints and undo, whole games played from
 * a seed, and every level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/hitori.h"
#include "games/hitori_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the player's grid with the shading of shade[] (1: shaded): solved? */
static int keeps_rules(const hto_puzzle *p, const uint8_t *shade)
{
    static hto_game g;
    memset(&g, 0, sizeof g);
    g.p = *p;
    for (int c = 0; c < p->n * p->n; c++) g.st[c] = shade[c] ? HTO_SHADE : HTO_EMPTY;
    return hto_solved(&g);
}

int main(void)
{
    hto_puzzle p;
    uint8_t sol[HTO_CELLS];

    /* drawn by hand: a Latin square has nothing to shade */
    CHECK(hto_decode("3:123231312", &p) && hto_count(&p, 3) == 1 && hto_grade(&p, sol) == BENCH_EASY);
    for (int c = 0; c < 9; c++) CHECK(!sol[c]);
    CHECK(!hto_decode("3:123231314", &p) && !hto_decode("3:12323131", &p) && !hto_decode("3:023231312", &p));
    /* one twin to shade: a1 and c1 hold 1; b1 between them stays (A B A), a1 or c1 goes; the column of c1 decides */
    CHECK(hto_decode("3:121312231", &p));
    int k = hto_count(&p, 5);
    CHECK(k >= 1);
    if (k == 1) {
        CHECK(hto_grade(&p, sol) >= BENCH_EASY && keeps_rules(&p, sol));
        CHECK(!sol[1]);
    }
    /* shaded cells side by side: never a solution */
    {
        uint8_t sh[9] = {1, 1, 0, 0, 0, 0, 0, 0, 0};
        CHECK(!keeps_rules(&p, sh));
    }

    /* the puzzles from many seeds: one solution, of their level, quick */
    clock_t t0 = clock();
    double slowest[HTO_VARIANTS] = {0};
    int at_level[HTO_VARIANTS] = {0};
    for (int v = 0; v < HTO_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 40; seed++) {
            clock_t t1 = clock();
            hto_make(hto_size(v), hto_level(v), seed * 7919u + (uint32_t)v, &p, sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            CHECK(p.n >= 4 && p.n <= hto_size(v) && keeps_rules(&p, sol));
            uint8_t found[HTO_CELLS];
            int lv = hto_grade(&p, found), shaded = 0;
            CHECK(lv >= BENCH_EASY && lv <= hto_level(v));
            at_level[v] += lv == hto_level(v) && p.n == hto_size(v);
            CHECK(!memcmp(found, sol, (size_t)p.n * p.n));
            CHECK(hto_count(&p, 2) == 1);
            for (int c = 0; c < p.n * p.n; c++) shaded += sol[c];
            CHECK(shaded * 100 >= 18 * p.n * p.n);
        }
    printf("hitori: 40 puzzles a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < HTO_VARIANTS; v++) printf(" %.3f", slowest[v]);
    printf(" s; at their level and size");
    for (int v = 0; v < HTO_VARIANTS; v++) printf(" %d", at_level[v]);
    printf("\n");
    for (int v = 0; v < HTO_VARIANTS; v++) CHECK(slowest[v] < 0.5 && at_level[v] >= 36);

    /* the same seed: the same puzzle */
    static hto_game a, b;
    hto_new(&a, 7, 42);
    hto_new(&b, 7, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's cells: a tap shades or not, a hold rings; undo */
    static hto_game G;
    hto_new(&G, 0, 7);
    int n = G.p.n, f = 0;
    CHECK(hto_tap_state(&G, f) == HTO_SHADE && hto_hold_state(&G, f) == HTO_RING);
    CHECK(hto_paint(&G, &f, 1, HTO_SHADE) && hto_tap_state(&G, f) == HTO_EMPTY && hto_hold_state(&G, f) == HTO_RING);
    CHECK(hto_paint(&G, &f, 1, HTO_RING) && hto_tap_state(&G, f) == HTO_SHADE && hto_hold_state(&G, f) == HTO_EMPTY);
    CHECK(!hto_paint(&G, &f, 1, HTO_RING));     /* nothing changed: no move */
    CHECK(G.hist.moves == 2 && hto_undo(&G) == f && G.st[f] == HTO_SHADE && hto_undo(&G) == f && !G.st[f]);
    CHECK(hto_undo(&G) < 0);
    /* the faults: shaded cells touching, twins both ringed, a cell cut off */
    {
        static hto_game F;
        memset(&F, 0, sizeof F);
        hto_decode("3:121312231", &F.p);
        F.st[0] = F.st[1] = HTO_SHADE;
        CHECK(hto_touching(&F, 0) && hto_touching(&F, 1) && !hto_touching(&F, 2));
        F.st[1] = HTO_EMPTY;
        F.st[3] = HTO_SHADE;                    /* a1 a2 shaded: b1 still joined; now b1's way: c1, b2 */
        CHECK(hto_touching(&F, 0) && hto_touching(&F, 3));
        memset(F.st, 0, sizeof F.st);
        F.st[1] = F.st[3] = HTO_SHADE;          /* b1 and a2 shaded: a1 cut off */
        CHECK(hto_apart(&F, 0) && !hto_apart(&F, 4));
        memset(F.st, 0, sizeof F.st);
        F.st[0] = F.st[2] = HTO_RING;           /* the two 1s of the top row ringed */
        CHECK(hto_twin(&F, 0) && hto_twin(&F, 2) && !hto_twin(&F, 1));
    }
    /* the notation: a tap dragged along a row shades its empty cells */
    hto_new(&G, 4, 3);
    n = G.p.n;
    CHECK(hto_script(&G, "a1-c1") && G.st[0] == HTO_SHADE && G.st[1] == HTO_SHADE && G.st[2] == HTO_SHADE);
    CHECK(hto_script(&G, ".a2-a3") && G.st[n] == HTO_RING && G.st[2 * n] == HTO_RING);
    while (hto_undo(&G) >= 0) {}
    CHECK(!G.st[0] && !G.st[n] && !hto_script(&G, "z9") && !hto_script(&G, "a1-c3"));

    /* a hint: a wrong cell put right first, else the next cell found; hints alone solve a puzzle */
    for (int v = 0; v < HTO_VARIANTS; v++) {
        hto_new(&G, v, 100u + (uint32_t)v);
        n = G.p.n;
        int wrong = -1;
        for (int c = 0; c < n * n && wrong < 0; c++)
            if (!G.sol[c]) wrong = c;
        CHECK(hto_paint(&G, &wrong, 1, HTO_SHADE) && hto_hint(&G) == wrong && G.st[wrong] == HTO_RING);
        int guard = 0;
        while (!hto_solved(&G) && guard++ < n * n) {
            int c = hto_hint(&G);
            CHECK(c >= 0 && (G.st[c] == HTO_SHADE) == G.sol[c]);
            if (c < 0) break;
        }
        CHECK(hto_solved(&G) && G.hist.hints > 0);
    }

    /* whole games from a seed: random taps, holds and drags, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 20; seed++) {
        uint32_t s = pr_seed(seed);
        hto_new(&G, (int)(seed % HTO_VARIANTS), seed);
        n = G.p.n;
        static hto_game first;
        first = G;
        for (int i = 0; i < 40; i++) {
            int c = pr_below(&s, n * n), kind = pr_below(&s, 3);
            if (kind < 2) {
                hto_paint(&G, &c, 1, kind ? hto_hold_state(&G, c) : hto_tap_state(&G, c));
            } else {
                int cells[HTO_MAX], m = 0, was = G.st[c];
                for (int x = c; x < (c / n + 1) * n; x++)
                    if (G.st[x] == was) cells[m++] = x;
                hto_paint(&G, cells, m, hto_tap_state(&G, c));
            }
        }
        int guard = 0;
        while (!hto_solved(&G) && guard++ < 2 * n * n) CHECK(hto_hint(&G) >= 0);
        CHECK(hto_solved(&G));
        while (hto_undo(&G) >= 0) {}
        CHECK(!memcmp(G.st, first.st, sizeof G.st) && G.hist.moves == 0);
    }
    /* the stars' mistakes: a wrong cell taken back with undo */
    hto_new(&G, 0, 11);
    {
        int c = -1;
        for (int i = 0; i < G.p.n * G.p.n && c < 0; i++)
            if (!G.sol[i]) c = i;
        hto_paint(&G, &c, 1, HTO_SHADE);
        hto_undo(&G);
        CHECK(G.hist.mistakes == 1 && G.hist.hints == 0);
    }

    /* the levels: each decodes and encodes back, has one solution found by reasoning alone (easy in chapter 1, hard
     * at the end), and the sizes never shrink */
    {
        int last_n = 0, grades[3] = {0, 0, 0};
        clock_t t1 = clock();
        for (int l = 0; l < HTO_LEVELS; l++) {
            hto_puzzle q;
            static hto_game L;
            char back[HTO_CELLS + 64];
            int ok = hto_decode(hto_levels[l], &q) && hto_load(&L, &q) && hto_count(&q, 2) == 1;
            ok = ok && keeps_rules(&q, L.sol);
            if (!ok) printf("hitori level %d: not one solution\n", l + 1);
            CHECK(ok);
            CHECK(hto_encode(&q, back, sizeof back) && !strcmp(back, hto_levels[l]));
            CHECK(q.n >= last_n);
            last_n = q.n;
            if (ok) grades[L.level]++;
            if (l < HTO_LEVELS / HTO_CHAPTERS) CHECK(L.level == BENCH_EASY);
        }
        hto_puzzle q;
        CHECK(hto_decode(hto_levels[HTO_LEVELS - 1], &q) && hto_grade(&q, NULL) == BENCH_HARD);
        printf("hitori: %d levels, one solution each (easy %d, medium %d, hard %d) in %.1f s\n", HTO_LEVELS,
               grades[0], grades[1], grades[2], (double)(clock() - t1) / CLOCKS_PER_SEC);
    }

    if (fails) printf("hitori: %d checks failed\n", fails);
    else printf("hitori: all checks passed\n");
    return fails != 0;
}
