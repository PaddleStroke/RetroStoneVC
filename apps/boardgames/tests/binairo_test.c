/*
 * Binairo's rules: the solver and the grader on grids drawn by hand, the puzzles made from many seeds (one solution,
 * the level they were made for, well under a second each), the player's discs, drags, the faults shown, hints and
 * undo, and whole games played from a seed with wrong discs and hints.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/binairo.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a grid from rows of text: '.' empty, 'o' ivory, 'x' ebony */
static void puzzle_of(bn_puzzle *p, int n, const char *const *rows)
{
    memset(p, 0, sizeof *p);
    p->n = n;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            p->given[y * n + x] = rows[y][x] == 'o' ? BN_IVORY : rows[y][x] == 'x' ? BN_EBONY : BN_EMPTY;
}

static int keeps_rules(int n, const uint8_t *grid)
{
    bn_game g;
    memset(&g, 0, sizeof g);
    g.p.n = n;
    for (int c = 0; c < n * n; c++) g.st[c] = grid[c];
    return bn_solved(&g);
}

int main(void)
{
    bn_puzzle p;
    uint8_t sol[BN_CELLS];

    /* drawn by hand: a full grid but for a few cells; an empty grid (many ways); three ivory in a row (none) */
    static const char *const full[] = {"oxoxox", "xoxoxo", "oxxoox", "xooxxo", "oxoxxo", "xoxoox"};
    puzzle_of(&p, 6, full);
    CHECK(keeps_rules(6, p.given));
    p.given[0] = p.given[7] = p.given[14] = p.given[35] = BN_EMPTY;
    CHECK(bn_grade(&p, sol) == GL_EASY && keeps_rules(6, sol));
    CHECK(bn_count(&p, 3) == 1);
    static const char *const empty[] = {"......", "......", "......", "......", "......", "......"};
    puzzle_of(&p, 6, empty);
    CHECK(bn_count(&p, 2) == 2);
    CHECK(bn_grade(&p, NULL) == GL_GUESS);
    static const char *const bad[] = {"ooo...", "......", "......", "......", "......", "......"};
    puzzle_of(&p, 6, bad);
    CHECK(bn_count(&p, 2) == 0 && bn_grade(&p, NULL) == GL_NONE);
    /* a row with two cells left: its column's count says which */
    static const char *const two[] = {"oxoxox", "xoxoxo", "oxxoox", "xooxxo", "ox..xo", "xoxoox"};
    puzzle_of(&p, 6, two);
    CHECK(bn_count(&p, 3) == 1 && bn_grade(&p, sol) == GL_EASY && sol[26] == BN_IVORY && sol[27] == BN_EBONY);

    /* the puzzles from many seeds: one solution, of their level, quick */
    clock_t t0 = clock();
    double slowest[BN_VARIANTS] = {0};
    int givens[BN_VARIANTS] = {0};
    for (int v = 0; v < BN_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 40; seed++) {
            clock_t t1 = clock();
            bn_make(bn_size(v), bn_level(v), seed * 7919u + (uint32_t)v, &p, sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            int n = p.n;
            CHECK(n == bn_size(v) && keeps_rules(n, sol));
            uint8_t found[BN_CELLS];
            int lv = bn_grade(&p, found);
            if (lv != bn_level(v)) printf("variant %d seed %u: graded %d\n", v, seed, lv);
            CHECK(lv == bn_level(v));
            CHECK(!memcmp(found, sol, (size_t)n * n));
            CHECK(bn_count(&p, 2) == 1);
            for (int c = 0; c < n * n; c++) {
                CHECK(!p.given[c] || p.given[c] == sol[c]);
                givens[v] += p.given[c] != 0;
            }
        }
    printf("binairo: 40 puzzles a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < BN_VARIANTS; v++) printf(" %.3f", slowest[v]);
    printf(" s; given");
    for (int v = 0; v < BN_VARIANTS; v++) printf(" %d%%", givens[v] * 100 / (40 * bn_size(v) * bn_size(v)));
    printf("\n");
    for (int v = 0; v < BN_VARIANTS; v++) CHECK(slowest[v] < 0.5);

    /* the same seed: the same puzzle */
    bn_game a, b;
    bn_new(&a, 7, 42);
    bn_new(&b, 7, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's discs: a tap ivory, ebony, empty; a hold the other way; the given ones stay; undo */
    bn_game G;
    bn_new(&G, 0, 7);
    int n = G.p.n, f = -1, given = -1;
    for (int c = 0; c < n * n; c++) {
        if (bn_free(&G, c) && f < 0) f = c;
        if (!bn_free(&G, c) && given < 0) given = c;
    }
    CHECK(bn_tap_state(&G, f) == BN_IVORY && bn_hold_state(&G, f) == BN_EBONY);
    CHECK(bn_paint(&G, &f, 1, BN_IVORY) && bn_tap_state(&G, f) == BN_EBONY && bn_hold_state(&G, f) == BN_EMPTY);
    CHECK(bn_paint(&G, &f, 1, BN_EBONY) && bn_tap_state(&G, f) == BN_EMPTY);
    CHECK(!bn_paint(&G, &given, 1, BN_EMPTY) && G.st[given] == G.p.given[given]);
    CHECK(G.hist.moves == 2 && bn_undo(&G) == f && G.st[f] == BN_IVORY && bn_undo(&G) == f && !G.st[f]);
    CHECK(bn_undo(&G) < 0 && G.hist.moves == 0);
    /* the faults: three alike, a line too full, two full lines alike */
    bn_game F;
    memset(&F, 0, sizeof F);
    F.p.n = 6;
    for (int x = 0; x < 3; x++) F.st[x] = BN_EBONY;
    CHECK(bn_triple(&F, 0) && bn_triple(&F, 2) && !bn_triple(&F, 3) && !bn_line_over(&F, 0, 0));
    F.st[4] = BN_EBONY;
    CHECK(bn_line_over(&F, 0, 0) && bn_count_line(&F, 0, 0, BN_EBONY) == 4 && !bn_line_over(&F, 1, 0));
    puzzle_of(&p, 6, full);
    for (int c = 0; c < 36; c++) F.st[c] = p.given[c];
    CHECK(!bn_line_twin(&F, 0, 0));
    for (int x = 0; x < 6; x++) F.st[6 + x] = F.st[x];
    CHECK(bn_line_twin(&F, 0, 0) && bn_line_twin(&F, 0, 1) && !bn_line_twin(&F, 0, 2) && !bn_solved(&F));
    /* the notation: taps, a hold, a drag along a row (only the cells as the first was) */
    bn_new(&G, 3, 5);
    int c0 = -1;
    for (int c = 0; c < G.p.n && c0 < 0; c++)
        if (bn_free(&G, c)) c0 = c;
    char moves[64];
    snprintf(moves, sizeof moves, "%c1 %c1 .%c1", (char)('a' + c0), (char)('a' + c0), (char)('a' + c0));
    CHECK(bn_script(&G, moves) && G.st[c0] == BN_IVORY && G.hist.moves == 3);
    snprintf(moves, sizeof moves, "%c1-h1", (char)('a' + c0));
    bn_game before = G;
    CHECK(bn_script(&G, moves) && G.hist.moves == 4);
    for (int c = c0; c < 8; c++)
        CHECK(G.st[c] == (bn_free(&G, c) && before.st[c] == BN_IVORY ? BN_EBONY : before.st[c]));
    CHECK(!bn_script(&G, "z9") && !bn_script(&G, "a1-c3"));

    /* a hint: a wrong disc put right first, else the next cell found; hints alone solve a puzzle */
    for (int v = 0; v < BN_VARIANTS; v++) {
        bn_new(&G, v, 100u + (uint32_t)v);
        n = G.p.n;
        for (f = 0; !bn_free(&G, f); f++) {}
        int wrong = G.sol[f] == BN_IVORY ? BN_EBONY : BN_IVORY;
        CHECK(bn_paint(&G, &f, 1, wrong) && bn_hint(&G) == f && G.st[f] == G.sol[f]);
        int guard = 0;
        while (!bn_solved(&G) && guard++ < n * n) {
            int c = bn_hint(&G);
            CHECK(c >= 0 && G.st[c] == G.sol[c]);
            if (c < 0) break;
        }
        CHECK(bn_solved(&G));
    }

    /* whole games from a seed: random taps, holds and drags, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 30; seed++) {
        uint32_t s = pr_seed(seed);
        bn_new(&G, (int)(seed % BN_VARIANTS), seed);
        n = G.p.n;
        bn_game start = G;
        for (int i = 0; i < 50; i++) {
            int c = pr_below(&s, n * n), kind = pr_below(&s, 3);
            if (!bn_free(&G, c)) continue;
            if (kind < 2) {
                bn_paint(&G, &c, 1, kind ? bn_hold_state(&G, c) : bn_tap_state(&G, c));
            } else {
                int cells[BN_MAX], k = 0, was = G.st[c];
                for (int y = c; y < n * n; y += n)
                    if (bn_free(&G, y) && G.st[y] == was) cells[k++] = y;
                bn_paint(&G, cells, k, bn_tap_state(&G, c));
            }
        }
        int guard = 0;
        while (!bn_solved(&G) && guard++ < 2 * n * n) CHECK(bn_hint(&G) >= 0);
        CHECK(bn_solved(&G));
        while (bn_undo(&G) >= 0) {}
        CHECK(!memcmp(G.st, start.st, sizeof G.st) && G.hist.moves == 0);
    }

    if (fails) printf("binairo: %d checks failed\n", fails);
    else printf("binairo: all checks passed\n");
    return fails != 0;
}
