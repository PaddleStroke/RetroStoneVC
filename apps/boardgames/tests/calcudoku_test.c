/*
 * Calcudoku's rules: the arithmetic, the puzzles made from many seeds for every variant (a Latin square, cages that
 * cover the grid and make their targets, one solution by a plain search, the level they were made for, made fast),
 * the 150 designed levels (each with one solution, the chapters growing), the levels' code, the player's digits,
 * notes, hints and undo, and whole games played from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/calcudoku.h"
#include "games/calcudoku_levels.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static int latin_ok(const cdk_puzzle *p)
{
    int n = p->n;
    for (int i = 0; i < n; i++) {
        unsigned r = 0, c = 0;
        for (int j = 0; j < n; j++) {
            r |= 1u << p->sol[i * n + j];
            c |= 1u << p->sol[j * n + i];
        }
        unsigned all = ((1u << (n + 1)) - 1) & ~1u;
        if (r != all || c != all) return 0;
    }
    return 1;
}

static int cages_ok(const cdk_puzzle *p)
{
    int n = p->n, seen[CDK_CELLS] = {0};
    for (int k = 0; k < p->ncages; k++) {
        const cdk_cage *g = &p->cage[k];
        if (g->n < 1 || g->n > CDK_CAGE) return 0;
        int v[CDK_CAGE];
        for (int i = 0; i < g->n; i++) {
            if (p->cage_of[g->cell[i]] != k || seen[g->cell[i]]++) return 0;
            if (i && g->cell[i] <= g->cell[i - 1]) return 0;
            v[i] = p->sol[g->cell[i]];
        }
        if (cdk_value(g->op, v, g->n) != g->target) return 0;
        if ((g->op == CDK_SUB || g->op == CDK_DIV) && g->n != 2) return 0;
        if ((g->op == CDK_ONE) != (g->n == 1)) return 0;
        for (int i = 1; i < g->n; i++) {         /* joined: each cell beside an earlier one, or reached later */
            int linked = 0;
            for (int j = 0; j < g->n; j++) {
                int a = g->cell[i], b = g->cell[j];
                if (abs(a % n - b % n) + abs(a / n - b / n) == 1) linked = 1;
            }
            if (!linked) return 0;
        }
    }
    for (int c = 0; c < n * n; c++)
        if (seen[c] != 1) return 0;
    return 1;
}

int main(void)
{
    int v4[] = {6, 2}, v3[] = {2, 3, 4};
    CHECK(cdk_value(CDK_SUB, v4, 2) == 4);
    CHECK(cdk_value(CDK_DIV, v4, 2) == 3);
    CHECK(cdk_value(CDK_ADD, v3, 3) == 9);
    CHECK(cdk_value(CDK_MUL, v3, 3) == 24);
    int v5[] = {5, 2};
    CHECK(cdk_value(CDK_DIV, v5, 2) == -1);

    /* the puzzles of every variant from many seeds */
    int seeds_per = 60;
    for (int var = 0; var < CDK_VARIANTS; var++) {
        int n = cdk_size_of(var), level = cdk_level_of(var), exact = 0, cages = 0;
        double slowest = 0, total = 0;
        int tries = n == 9 ? seeds_per / 2 : seeds_per;
        for (int s = 1; s <= tries; s++) {
            cdk_puzzle p;
            clock_t t0 = clock();
            cdk_make(n, level, (uint32_t)s * 7919u + (uint32_t)var, &p);
            double took = (double)(clock() - t0) / CLOCKS_PER_SEC;
            total += took;
            if (took > slowest) slowest = took;
            CHECK(p.n == n);
            CHECK(latin_ok(&p));
            CHECK(cages_ok(&p));
            int r = cdk_rate(&p);
            CHECK(r >= 0 && r <= level);
            exact += r == level;
            cages += p.ncages;
            if (s <= 12) CHECK(cdk_count(&p, 2) == 1);
        }
        printf("calcudoku %dx%d level %d: %d/%d of the level, %.1f cages, %.1f ms on average, slowest %.1f ms\n", n, n,
               level, exact, tries, (double)cages / tries, 1000 * total / tries, 1000 * slowest);
        CHECK(exact >= tries * 9 / 10);
        CHECK(slowest < 0.35);
    }

    /* the designed levels: each decodes (its code again the same), its cages make their targets, one solution; the
     * chapters grow */
    int last_n = 0, weight[CDK_CHAPTERS] = {0};
    for (int i = 0; i < CDK_STAGES; i++) {
        cdk_puzzle p;
        char again[3 * CDK_CELLS + 8];
        int ok = cdk_decode(cdk_levels[i], &p);
        CHECK(ok);
        if (!ok) continue;
        CHECK(latin_ok(&p) && cages_ok(&p));
        CHECK(cdk_count(&p, 2) == 1);
        int r = cdk_rate(&p);
        CHECK(r >= CDK_EASY && r <= CDK_HARD);
        weight[i / (CDK_STAGES / CDK_CHAPTERS)] += p.n + 2 * r;   /* a chapter's weight: sizes and grades */
        CHECK(cdk_encode(&p, again, sizeof again) && !strcmp(again, cdk_levels[i]));
        if (i % (CDK_STAGES / CDK_CHAPTERS) == 0) {   /* a chapter starts no smaller than the last one started */
            CHECK(p.n >= last_n);
            last_n = p.n;
        }
    }
    for (int c = 1; c < CDK_CHAPTERS; c++) CHECK(weight[c] > weight[c - 1]);
    {
        cdk_puzzle a, b;                        /* a puzzle made at random through the code and back */
        char code[3 * CDK_CELLS + 8];
        cdk_make(9, CDK_HARD, 77, &a);
        CHECK(cdk_encode(&a, code, sizeof code) && cdk_decode(code, &b));
        CHECK(b.ncages == a.ncages && !memcmp(a.sol, b.sol, sizeof a.sol) && cages_ok(&b));
        CHECK(cdk_rate(&b) == cdk_rate(&a));
        CHECK(!cdk_decode("4:1234", &b) && !cdk_decode("3:123231312:000000000:=====", &b));
    }

    /* the player's grid */
    cdk_game g;
    cdk_new(&g, 1 * CDK_LEVELS + 1, 12345);     /* 5 x 5 medium */
    int n = g.p.n;
    CHECK(n == 5);
    CHECK(!cdk_solved(&g));
    CHECK(cdk_note(&g, 0, 3) && g.notes[0] == 1u << 3);
    CHECK(cdk_note(&g, 1, 3) && cdk_note(&g, n, 3));
    int d = g.p.sol[0] == 3 ? 4 : 3;             /* 3 put elsewhere in row 0: the notes 3 of row 0 go */
    int at = -1;
    for (int c = 2; c < n; c++)
        if (g.p.sol[c] != 3) { at = c; break; }
    (void)d;
    CHECK(cdk_put(&g, at, 3));
    CHECK(!(g.notes[0] >> 3 & 1) && !(g.notes[1] >> 3 & 1) && (g.notes[n] >> 3 & 1));
    CHECK(!cdk_note(&g, at, 2));                 /* no notes in a filled cell */
    CHECK(g.p.sol[at] != 3);
    CHECK(cdk_hint(&g) == at && g.val[at] == g.p.sol[at]);   /* the wrong digit put right first */
    int moves = g.hist.moves;
    CHECK(cdk_undo(&g) == at && g.val[at] == 3 && g.hist.moves == moves - 1);
    CHECK(cdk_undo(&g) == at && g.val[at] == 0);
    CHECK(g.notes[0] >> 3 & 1);                  /* the notes came back */
    CHECK(cdk_erase(&g, 0) && !g.notes[0]);
    CHECK(!cdk_erase(&g, 0));
    while (cdk_undo(&g) >= 0) {}
    CHECK(g.hist.moves == 0 && !g.notes[0] && !g.notes[1]);
    CHECK(cdk_script(&g, "a1=1 a1=2 b1'3 h"));
    CHECK(!cdk_script(&g, "z9=1"));
    CHECK(!cdk_script(&g, "a1=9"));
    /* a clash: the same digit twice in a row */
    cdk_new(&g, 0, 99);
    CHECK(cdk_put(&g, 0, 1) && cdk_put(&g, 1, 1) && cdk_clash(&g, 0) && cdk_clash(&g, 1));

    /* whole games from seeds: wrong digits, notes and hints until solved; then undone to the start */
    for (uint32_t seed = 1; seed <= 40; seed++) {
        uint32_t rng = pr_seed(seed);
        cdk_new(&g, (int)(seed % CDK_VARIANTS), seed);
        n = g.p.n;
        int steps = 0;
        while (!cdk_solved(&g) && steps < 2000) {
            int c = pr_below(&rng, n * n), roll = steps < 2 * n * n ? pr_below(&rng, 10) : 9;
            if (roll < 4) cdk_put(&g, c, 1 + pr_below(&rng, n));
            else if (roll < 6) cdk_note(&g, c, 1 + pr_below(&rng, n));
            else if (roll < 7) cdk_erase(&g, c);
            else CHECK(cdk_hint(&g) >= 0);
            steps++;
        }
        CHECK(cdk_solved(&g));
        CHECK(cdk_hint(&g) < 0);
        int undos = 0;
        while (cdk_undo(&g) >= 0) undos++;
        CHECK(g.hist.moves == 0);
        for (int c = 0; c < n * n; c++) CHECK(!g.val[c] && !g.notes[c]);
    }

    printf(fails ? "calcudoku: %d FAILED\n" : "calcudoku: all passed\n", fails);
    return fails != 0;
}
