/*
 * Sudoku's rules: the solver and the grader on known grids, the puzzles made from many seeds (one solution, the
 * level they were made for, their clues a subset of the solution), the player's digits, notes, hints and undo, and
 * whole games played from a seed with hints and wrong digits.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/sudoku.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static void grid_of(const char *s, uint8_t *g)
{
    for (int c = 0; c < SU_CELLS; c++) g[c] = s[c] == '.' ? 0 : (uint8_t)(s[c] - '0');
}

static int valid_full(const uint8_t *g)
{
    for (int i = 0; i < 9; i++) {
        unsigned r = 0, c = 0, b = 0;
        for (int j = 0; j < 9; j++) {
            r |= 1u << g[i * 9 + j];
            c |= 1u << g[j * 9 + i];
            b |= 1u << g[(i / 3 * 3 + j / 3) * 9 + i % 3 * 3 + j % 3];
        }
        if (r != 0x3fe || c != 0x3fe || b != 0x3fe) return 0;
    }
    return 1;
}

int main(void)
{
    uint8_t g[SU_CELLS], sol[SU_CELLS];

    /* a puzzle singles solve (a classic easy one), and one that needs more */
    grid_of("..3.2.6..9..3.5..1..18.64....81.29..7.......8..67.82....26.95..8..2.3..9..5.1.3..", g);
    CHECK(su_count(g, 2, sol) == 1);
    CHECK(valid_full(sol));
    CHECK(su_rate(g) == SU_EASY);
    grid_of("8..........36......7..9.2...5...7.......457.....1...3...1....68..85...1..9....4..", g);
    CHECK(su_count(g, 2, NULL) == 1);
    CHECK(su_rate(g) == 4);                     /* beyond every way the grader knows */
    grid_of("..3.2.6..9..3.5..1..18.64....81.29..7.......8..67.82....26.95..8..2.3..9..5.1.3..", g);
    g[0] = 0, g[2] = 0;                         /* fewer clues: more solutions */
    for (int c = 0; c < 20; c++) g[c] = 0;
    CHECK(su_count(g, 2, NULL) == 2);
    uint8_t bad[SU_CELLS] = {0};
    bad[0] = bad[1] = 5;                        /* a clash: none */
    CHECK(su_count(bad, 2, NULL) == 0);

    /* the puzzles from many seeds: one solution, of their level, the clues the solution's */
    int counts[SU_VARIANTS] = {0}, clues[SU_VARIANTS][2] = {{81, 0}, {81, 0}, {81, 0}, {81, 0}};
    int rated[SU_VARIANTS][5] = {{0}};
    double slowest[SU_VARIANTS] = {0};
    clock_t t0 = clock();
    for (int level = 0; level < SU_VARIANTS; level++)
        for (uint32_t seed = 1; seed <= 60; seed++) {
            uint8_t p[SU_CELLS], s[SU_CELLS];
            clock_t t1 = clock();
            su_make(level, seed * 7919u + (uint32_t)level, p, s);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[level]) slowest[level] = took;
            CHECK(valid_full(s));
            uint8_t found[SU_CELLS];
            CHECK(su_count(p, 2, found) == 1);
            CHECK(!memcmp(found, s, SU_CELLS));
            int n = 0;
            for (int c = 0; c < SU_CELLS; c++) {
                if (p[c]) { n++; CHECK(p[c] == s[c]); }
                CHECK(!p[c] == !p[SU_CELLS - 1 - c]);   /* the clues opposite each other round the centre */
            }
            int r = su_rate(p);
            rated[level][r]++;
            int ok = level == SU_HARD ? r == 2 || r == 3 : level == SU_EXPERT ? r == 4 : r == level;
            if (!ok) printf("level %d seed %u: graded %d\n", level, seed, r);
            CHECK(ok);
            if (level == SU_EASY) CHECK(n >= 36);
            if (level == SU_MEDIUM) CHECK(n >= 28);
            if (n < clues[level][0]) clues[level][0] = n;
            if (n > clues[level][1]) clues[level][1] = n;
            counts[level]++;
        }
    printf("sudoku: %d puzzles a level in %.1f s; clues easy %d-%d, medium %d-%d, hard %d-%d, expert %d-%d; "
           "hard graded 2: %d, 3: %d; the slowest %.2f %.2f %.2f %.2f s\n",
           counts[0], (double)(clock() - t0) / CLOCKS_PER_SEC, clues[0][0], clues[0][1], clues[1][0], clues[1][1],
           clues[2][0], clues[2][1], clues[3][0], clues[3][1], rated[2][2], rated[2][3], slowest[0], slowest[1],
           slowest[2], slowest[3]);

    /* the same seed: the same puzzle */
    su_game a, b;
    su_new(&a, SU_MEDIUM, 42);
    su_new(&b, SU_MEDIUM, 42);
    CHECK(!memcmp(a.val, b.val, SU_CELLS) && !memcmp(a.sol, b.sol, SU_CELLS));

    /* the stars' counts (bench.h): a wrong digit undone is a mistake, written over it is not; the hints counted */
    {
        su_game S;
        su_new(&S, SU_EASY, 11);
        int w = -1;
        for (int c = 0; c < SU_CELLS && w < 0; c++)
            if (!S.given[c]) w = c;
        int bad = S.sol[w] % 9 + 1;
        CHECK(su_put(&S, w, bad) && su_put(&S, w, S.sol[w]) && S.hist.mistakes == 0);   /* written over */
        CHECK(su_undo(&S) == w && su_undo(&S) == w && S.hist.mistakes == 1);   /* undone */
        su_new(&S, SU_EASY, 11);
        CHECK(su_hint(&S) >= 0 && S.hist.hints == 1);
        CHECK(su_hint(&S) >= 0 && su_hint(&S) >= 0 && S.hist.hints == 3);
        CHECK(su_script(&S, "+81") && su_solved(&S) && S.hist.mistakes == 0 && S.hist.hints == 3);
    }

    /* the player's grid: digits, the notes they clear, clashes, undo */
    su_game G;
    su_new(&G, SU_EASY, 7);
    int e = -1, given = -1;
    for (int c = 0; c < SU_CELLS; c++) {
        if (!G.given[c] && e < 0) e = c;
        if (G.given[c] && given < 0) given = c;
    }
    CHECK(!su_put(&G, given, 1) && !su_note(&G, given, 1) && !su_erase(&G, given));
    int peer = e % 9 == 8 ? e - 1 : e + 1;      /* a cell beside it, in its row */
    while (G.given[peer]) peer = peer / 9 == e / 9 && peer % 9 < 8 ? peer + 1 : (peer + 9) % SU_CELLS;
    CHECK(su_note(&G, peer, 4) && su_note(&G, peer, 6) && G.notes[peer] == (1u << 4 | 1u << 6));
    CHECK(su_note(&G, e, 2) && G.notes[e] == 1u << 2);
    CHECK(G.hist.moves == 3);
    int same_unit = peer / 9 == e / 9 || peer % 9 == e % 9;
    CHECK(su_put(&G, e, 4) && G.val[e] == 4 && G.notes[e] == 0);
    if (same_unit) CHECK(G.notes[peer] == 1u << 6);   /* the note 4 gone from its row */
    CHECK(!su_put(&G, e, 4));                   /* the same digit again: nothing */
    CHECK(G.hist.moves == 4);
    CHECK(su_undo(&G) == e && G.val[e] == 0 && G.notes[e] == 1u << 2 && G.notes[peer] == (1u << 4 | 1u << 6));
    CHECK(G.hist.moves == 3);
    CHECK(su_put(&G, e, G.sol[e]) && !su_clash(&G, e));
    int d = G.sol[e];
    for (int c = 0; c < 9; c++) {               /* the same digit elsewhere in its row: a clash */
        int x = e / 9 * 9 + c;
        if (x != e && !G.given[x] && !G.val[x]) {
            CHECK(su_put(&G, x, d) && su_clash(&G, x) && su_clash(&G, e));
            CHECK(su_erase(&G, x) && !G.val[x] && !su_clash(&G, e));
            break;
        }
    }
    CHECK(su_erase(&G, e) && G.val[e] == 0);
    CHECK(!su_erase(&G, e) || G.notes[e]);

    /* a hint puts a wrong digit right first, else fills a cell */
    su_new(&G, SU_HARD, 99);
    int w = -1;
    for (int c = 0; c < SU_CELLS && w < 0; c++)
        if (!G.given[c]) w = c;
    CHECK(su_put(&G, w, G.sol[w] % 9 + 1));
    CHECK(su_hint(&G) == w && G.val[w] == G.sol[w]);
    int before = 0, after = 0;
    for (int c = 0; c < SU_CELLS; c++) before += G.val[c] != 0;
    int h = su_hint(&G);
    for (int c = 0; c < SU_CELLS; c++) after += G.val[c] != 0;
    CHECK(h >= 0 && after == before + 1 && G.val[h] == G.sol[h]);

    /* whole games from seeds: hints alone solve every level; a game with mistakes, put right, then undone back to
     * the start */
    for (uint32_t seed = 1; seed <= 12; seed++)
        for (int level = 0; level < SU_VARIANTS; level++) {
            su_new(&G, level, seed);
            int guard = 0;
            while (!su_solved(&G) && guard++ < 100) CHECK(su_hint(&G) >= 0);
            CHECK(su_solved(&G));
            CHECK(su_hint(&G) < 0);
        }
    su_new(&G, SU_MEDIUM, 5);
    su_game start = G;
    char moves[64];
    int c0 = -1;
    for (int c = 0; c < SU_CELLS && c0 < 0; c++)
        if (!G.given[c]) c0 = c;
    snprintf(moves, sizeof moves, "%c%d=%d %c%d'3 h +200", 'a' + c0 % 9, c0 / 9 + 1, G.sol[c0] % 9 + 1,
             'a' + (c0 + 1) % 9, (c0 + 1) / 9 + 1);
    CHECK(su_script(&G, moves) || G.given[c0 + 1]);
    CHECK(su_solved(&G));
    while (su_undo(&G) >= 0) {}
    CHECK(!memcmp(G.val, start.val, SU_CELLS) && !memcmp(G.notes, start.notes, sizeof G.notes) && G.hist.moves == 0);
    CHECK(!su_script(&G, "a1=x") && su_cell("e5") == 40 && su_cell("j1") < 0);

    if (fails) {
        printf("sudoku: %d failed\n", fails);
        return 1;
    }
    printf("sudoku: all passed\n");
    return 0;
}
