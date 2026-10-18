/*
 * Norinori's rules: the solver and the grader on grids drawn by hand (a 2 x 2 region with four answers, three columns
 * with none, regions in two pieces refused), the puzzles made from seeds (one solution, and how long they take), the
 * player's shading, dots, drags, the faults shown, hints and undo, whole games played from a seed, and every level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/norinori.h"
#include "games/norinori_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* the player's grid with the shading of sh[] (1: shaded): solved? */
static int keeps_rules(const nrn_puzzle *p, const uint8_t *sh)
{
    static nrn_game g;
    memset(&g, 0, sizeof g);
    g.p = *p;
    for (int c = 0; c < p->n * p->n; c++) g.st[c] = sh[c] ? NRN_SHADE : NRN_EMPTY;
    return nrn_solved(&g);
}

int main(void)
{
    nrn_puzzle p;
    uint8_t sol[NRN_CELLS];

    /* drawn by hand: one region of 2 x 2 has four dominoes; a 3 x 3 of three columns has none that keep apart */
    CHECK(nrn_decode("2:aaaa", &p) && p.regions == 1 && nrn_count(&p, 9) == 4);
    CHECK(nrn_grade(&p, NULL) == BENCH_GUESS);
    CHECK(nrn_decode("3:abcabcabc", &p) && p.regions == 3 && nrn_count(&p, 9) == 0);
    CHECK(nrn_grade(&p, NULL) == BENCH_NONE);
    /* regions read in the order of their first cells; one wrapped round another */
    CHECK(nrn_decode("3:abbaccddc", &p) && p.regions == 4 && p.region[8] == 2);
    CHECK(nrn_decode("3:abaaaaaaa", &p) && p.regions == 2);
    CHECK(!nrn_decode("3:ababbbbbb", &p));     /* a region in two pieces */
    CHECK(!nrn_decode("3:aaaa", &p));          /* too short */
    {
        uint8_t sh[4] = {1, 1, 0, 0};           /* the 2 x 2: the top row shaded keeps the rules, three do not */
        nrn_decode("2:aaaa", &p);
        CHECK(keeps_rules(&p, sh));
        sh[2] = 1;
        CHECK(!keeps_rules(&p, sh));
    }

    /* the puzzles from seeds (made, or a level turned when the maker finds none in time): one solution, quick */
    clock_t t0 = clock();
    double slowest[NRN_VARIANTS] = {0};
    for (int v = 0; v < NRN_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 3; seed++) {
            clock_t t1 = clock();
            static nrn_game R;
            nrn_new(&R, v, seed * 7919u + (uint32_t)v);
            p = R.p;
            memcpy(sol, R.sol, sizeof sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            CHECK(p.n == nrn_size(v) && keeps_rules(&p, sol));
            uint8_t found[NRN_CELLS];
            int lv = nrn_grade(&p, found);
            CHECK(lv >= BENCH_EASY && lv <= BENCH_HARD);
            CHECK(!memcmp(found, sol, (size_t)p.n * p.n));
            CHECK(nrn_count(&p, 2) == 1);
        }
    printf("norinori: 3 puzzles a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < NRN_VARIANTS; v++) printf(" %.2f", slowest[v]);
    printf(" s\n");
    for (int v = 0; v < NRN_VARIANTS; v++) CHECK(slowest[v] < 1.5);

    /* the same seed: the same puzzle */
    static nrn_game a, b;
    nrn_new(&a, 4, 42);
    nrn_new(&b, 4, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's cells: a tap shades or not, a hold a dot; undo */
    static nrn_game G;
    nrn_new(&G, 0, 7);
    int n = G.p.n, f = 0;
    CHECK(nrn_tap_state(&G, f) == NRN_SHADE && nrn_hold_state(&G, f) == NRN_DOT);
    CHECK(nrn_paint(&G, &f, 1, NRN_SHADE) && nrn_tap_state(&G, f) == NRN_EMPTY && nrn_hold_state(&G, f) == NRN_DOT);
    CHECK(nrn_paint(&G, &f, 1, NRN_DOT) && nrn_tap_state(&G, f) == NRN_SHADE && nrn_hold_state(&G, f) == NRN_EMPTY);
    CHECK(G.hist.moves == 2 && nrn_undo(&G) == f && G.st[f] == NRN_SHADE && nrn_undo(&G) == f && !G.st[f]);
    CHECK(nrn_undo(&G) < 0);
    /* the faults: three in a row, a lone cell walled in by dots, a region with three, a domino */
    {
        static nrn_game F;
        memset(&F, 0, sizeof F);
        nrn_decode("3:aaabbbccc", &F.p);        /* three rows */
        F.st[0] = F.st[1] = F.st[2] = NRN_SHADE;
        CHECK(nrn_crowded(&F, 0) && nrn_crowded(&F, 1) && nrn_region(&F, 0) == 1 && nrn_region(&F, 1) == 0);
        CHECK(nrn_partner(&F, 0) < 0 && nrn_partner(&F, 1) < 0);
        F.st[2] = NRN_EMPTY;
        CHECK(!nrn_crowded(&F, 0) && nrn_partner(&F, 0) == 1 && nrn_partner(&F, 1) == 0 && nrn_region(&F, 0) == 2);
        F.st[0] = F.st[1] = NRN_EMPTY;
        F.st[4] = NRN_SHADE;
        F.st[1] = F.st[3] = F.st[5] = F.st[7] = NRN_DOT;
        CHECK(nrn_crowded(&F, 4));
        F.st[7] = NRN_EMPTY;
        CHECK(!nrn_crowded(&F, 4));
    }
    /* the notation: a tap dragged down a column shades every empty cell of it */
    nrn_new(&G, 3, 3);
    n = G.p.n;
    CHECK(nrn_script(&G, "a1-a3") && G.st[0] == NRN_SHADE && G.st[n] == NRN_SHADE && G.st[2 * n] == NRN_SHADE);
    CHECK(G.hist.moves == 1 && nrn_undo(&G) >= 0 && !G.st[0] && !G.st[n] && !G.st[2 * n]);
    CHECK(!nrn_script(&G, "z9") && !nrn_script(&G, "a1-c3"));

    /* a hint: a wrong cell put right first, else the next cell found; hints alone solve a puzzle */
    for (int l = 0; l < NRN_LEVELS; l += 37) {
        CHECK(nrn_decode(nrn_levels[l], &p) && nrn_load(&G, &p));
        n = G.p.n;
        int wrong = -1;
        for (int c = 0; c < n * n && wrong < 0; c++)
            if (!G.sol[c]) wrong = c;
        CHECK(nrn_paint(&G, &wrong, 1, NRN_SHADE) && nrn_hint(&G) == wrong && G.st[wrong] == NRN_DOT);
        int guard = 0;
        while (!nrn_solved(&G) && guard++ < n * n) {
            int c = nrn_hint(&G);
            CHECK(c >= 0 && (G.st[c] == NRN_SHADE) == G.sol[c]);
            if (c < 0) break;
        }
        CHECK(nrn_solved(&G) && G.hist.hints > 0);
    }

    /* whole games from a seed: random taps, holds and drags, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 12; seed++) {
        uint32_t s = pr_seed(seed);
        CHECK(nrn_decode(nrn_levels[seed * 11 % NRN_LEVELS], &p) && nrn_load(&G, &p));
        n = G.p.n;
        static nrn_game first;
        first = G;
        for (int i = 0; i < 40; i++) {
            int c = pr_below(&s, n * n), kind = pr_below(&s, 3);
            if (kind < 2) {
                nrn_paint(&G, &c, 1, kind ? nrn_hold_state(&G, c) : nrn_tap_state(&G, c));
            } else {
                int cells[NRN_MAX], k = 0, was = G.st[c];
                for (int x = c; x < (c / n + 1) * n; x++)
                    if (G.st[x] == was) cells[k++] = x;
                nrn_paint(&G, cells, k, nrn_tap_state(&G, c));
            }
        }
        int guard = 0;
        while (!nrn_solved(&G) && guard++ < 2 * n * n) CHECK(nrn_hint(&G) >= 0);
        CHECK(nrn_solved(&G));
        while (nrn_undo(&G) >= 0) {}
        CHECK(!memcmp(G.st, first.st, sizeof G.st) && G.hist.moves == 0);
    }
    /* the stars' mistakes: a wrong cell taken back with undo */
    nrn_decode(nrn_levels[0], &p);
    nrn_load(&G, &p);
    {
        int c = -1;
        for (int i = 0; i < G.p.n * G.p.n && c < 0; i++)
            if (!G.sol[i]) c = i;
        nrn_paint(&G, &c, 1, NRN_SHADE);
        nrn_undo(&G);
        CHECK(G.hist.mistakes == 1 && G.hist.hints == 0);
    }

    /* the levels: each decodes and encodes back, has one solution found by reasoning alone (easy in chapter 1, hard
     * at the end), and the sizes never shrink */
    {
        int last_n = 0, grades[3] = {0, 0, 0};
        clock_t t1 = clock();
        for (int l = 0; l < NRN_LEVELS; l++) {
            nrn_puzzle q;
            static nrn_game L;
            char back[NRN_CELLS + 8];
            int ok = nrn_decode(nrn_levels[l], &q) && nrn_load(&L, &q) && nrn_count(&q, 2) == 1;
            ok = ok && keeps_rules(&q, L.sol);
            if (!ok) printf("norinori level %d: not one solution\n", l + 1);
            CHECK(ok);
            CHECK(nrn_encode(&q, back, sizeof back) && !strcmp(back, nrn_levels[l]));
            CHECK(q.n >= last_n);
            last_n = q.n;
            if (ok) grades[L.level]++;
            if (l < NRN_LEVELS / NRN_CHAPTERS) CHECK(L.level == BENCH_EASY);
        }
        nrn_puzzle q;
        CHECK(nrn_decode(nrn_levels[NRN_LEVELS - 1], &q) && nrn_grade(&q, NULL) == BENCH_HARD);
        printf("norinori: %d levels, one solution each (easy %d, medium %d, hard %d) in %.1f s\n", NRN_LEVELS,
               grades[0], grades[1], grades[2], (double)(clock() - t1) / CLOCKS_PER_SEC);
    }

    if (fails) printf("norinori: %d checks failed\n", fails);
    else printf("norinori: all checks passed\n");
    return fails != 0;
}
