/*
 * Killer Sudoku's rules: the solver and the grader on a known Sudoku (the variants' shared solver grades Sudoku's own
 * way), the cages' sums and the rule of 45, the puzzles made from many seeds at every level (one solution, the
 * level they were made for, quick), the levels' code, the player's digits, notes, clashes, hints and undo, and whole
 * games played from seeds; every designed level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/killersudoku.h"
#include "games/killersudoku_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* every row, column and box holds 1..9; every cage its sum, no digit twice */
static int valid_full(const sv_shape *s, const uint8_t *g)
{
    for (int i = 0; i < 9; i++) {
        unsigned r = 0, c = 0, b = 0;
        for (int j = 0; j < 9; j++) {
            r |= 1u << g[i * 9 + j];
            c |= 1u << g[j * 9 + i];
            b |= 1u << g[(i / 3 * 3 + j / 3) * 9 + i % 3 * 3 + j % 3];
        }
        if (r != SV_ALL || c != SV_ALL || b != SV_ALL) return 0;
    }
    for (int q = 0; q < s->ncages; q++) {
        unsigned m = 0;
        int sum = 0;
        for (int i = 0; i < s->cage[q].n; i++) m |= 1u << g[s->cage[q].cell[i]], sum += g[s->cage[q].cell[i]];
        if (sum != s->cage[q].sum || __builtin_popcount(m) != s->cage[q].n) return 0;
    }
    return 1;
}

static sv_shape S;
static sv_game G, L;

int main(void)
{
    uint8_t sol[SV_CELLS], found[SV_CELLS];

    /* the shared solver on a plain Sudoku (boxes, no cages): singles solve an easy one; a hard one needs a trial */
    {
        memset(&S, 0, sizeof S);
        sv_boxes(&S);
        sv_shape_init(&S);
        uint8_t p[SV_CELLS];
        CHECK(sv_clues_decode(p, "..3.2.6..9..3.5..1..18.64....81.29..7.......8..67.82....26.95..8..2.3..9..5.1.3.."));
        CHECK(sv_grade(&S, p, SV_EXPERT, sol, NULL, 0) == SV_EASY && valid_full(&S, sol));
        CHECK(sv_count(&S, p, 2, NULL) == 1);
        CHECK(sv_clues_decode(p, "8..........36......7..9.2...5...7.......457.....1...3...1....68..85...1..9....4.."));
        CHECK(sv_grade(&S, p, SV_HARD, NULL, NULL, 0) == SV_STUCK);
        p[0] = 0;
        CHECK(sv_count(&S, p, 2, NULL) == 2);
    }

    /* a cage's ways: 3 in two cells is 1 and 2; 17 is 8 and 9; with 1 taken from one cell, 4 in two is 3 there */
    {
        sv_sum k;
        memset(&k, 0, sizeof k);
        k.n = 2;
        k.distinct = 1;
        k.cell[0] = 0, k.cell[1] = 1;
        uint16_t d[SV_CELLS];
        for (int c = 0; c < SV_CELLS; c++) d[c] = SV_ALL;
        sv_ways w;
        k.sum = 3;
        CHECK(sv_sum_ways(&k, d, &w) && w.uni[0] == (1u << 1 | 1u << 2) && w.must == (1u << 1 | 1u << 2));
        k.sum = 17;
        CHECK(sv_sum_ways(&k, d, &w) && w.uni[1] == (1u << 8 | 1u << 9));
        k.sum = 4;
        d[0] = SV_ALL & ~(1u << 1);
        CHECK(sv_sum_ways(&k, d, &w) && w.uni[0] == 1u << 3 && w.uni[1] == 1u << 1);
        k.sum = 2;                              /* no two different digits make 2 */
        CHECK(!sv_sum_ways(&k, d, &w));
    }

    /* the puzzles from many seeds: one solution, their level, made quickly */
    double slowest[KSU_VARIANTS] = {0};
    int hit[KSU_VARIANTS] = {0}, n = 8;
    clock_t t0 = clock();
    for (int level = 0; level < KSU_VARIANTS; level++)
        for (uint32_t seed = 1; seed <= (uint32_t)n; seed++) {
            clock_t t1 = clock();
            ksu_new(&G, level, seed * 7919u + (uint32_t)level);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[level]) slowest[level] = took;
            CHECK(valid_full(&G.s, G.sol));
            for (int c = 0; c < SV_CELLS; c++) CHECK(!G.given[c] && !G.val[c]);
            int g = sv_grade(&G.s, NULL, SV_EXPERT, found, NULL, 0);
            CHECK(g >= SV_EASY && g <= level && !memcmp(found, G.sol, SV_CELLS));
            CHECK(sv_count(&G.s, NULL, 2, NULL) == 1);
            hit[level] += g == level;
            for (int q = 0; q < G.s.ncages; q++) CHECK(G.s.cage[q].n <= ksu_most(level));
        }
    printf("killer sudoku: %d puzzles a level in %.1f s, at their level %d %d %d %d; the slowest %.2f %.2f %.2f %.2f s\n",
           n, (double)(clock() - t0) / CLOCKS_PER_SEC, hit[0], hit[1], hit[2], hit[3], slowest[0], slowest[1],
           slowest[2], slowest[3]);
    CHECK(hit[0] == n && hit[1] == n && hit[2] >= n - 1 && hit[3] >= n / 2);
    for (int level = 0; level < KSU_VARIANTS; level++) CHECK(slowest[level] < 1.0);

    /* every designed level: one solution, its chapter's level, no two the same, the code read back the same; hints
     * alone solve it */
    {
        static const int lo[KSU_CHAPTERS] = {0, 0, 1, 2, 2, 3}, hi[KSU_CHAPTERS] = {0, 1, 1, 2, 2, 3};
        int per = KSU_LEVELS / KSU_CHAPTERS, bad = 0;
        char code[KSU_CODE];
        for (int i = 0; i < KSU_LEVELS; i++) {
            int ch = i / per, ok = ksu_decode(&L, ksu_levels[i]);
            ok = ok && L.level >= lo[ch] && L.level <= hi[ch] && valid_full(&L.s, L.sol);
            ok = ok && sv_count(&L.s, NULL, 2, NULL) == 1;
            ok = ok && sv_cages_encode(&L.s, code, sizeof code) && !strcmp(code, ksu_levels[i]);
            for (int j = 0; ok && j < i; j++) ok = strcmp(ksu_levels[i], ksu_levels[j]) != 0;
            for (int guard = 0; ok && !sv_solved(&L) && guard < 100; guard++) ok = sv_hint(&L) >= 0;
            ok = ok && sv_solved(&L) && L.hist.hints == SV_CELLS;
            if (!ok) printf("killer sudoku level %d: bad (level %d)\n", i + 1, L.level);
            bad += !ok;
        }
        CHECK(bad == 0);
        CHECK(!ksu_decode(&L, "123") && !ksu_decode(&L, ksu_levels[0] + 1));
    }

    /* the same seed: the same puzzle */
    {
        static sv_game a, b;
        ksu_new(&a, SV_MEDIUM, 42);
        ksu_new(&b, SV_MEDIUM, 42);
        CHECK(!memcmp(a.sol, b.sol, SV_CELLS) && !memcmp(a.s.cage_of, b.s.cage_of, SV_CELLS));
    }

    /* the stars' counts: a wrong digit undone is a mistake, written over it is not; the hints counted */
    CHECK(ksu_decode(&G, ksu_levels[0]));
    CHECK(sv_put(&G, 0, G.sol[0] % 9 + 1) && sv_put(&G, 0, G.sol[0]) && G.hist.mistakes == 0);
    CHECK(sv_undo(&G) == 0 && sv_undo(&G) == 0 && G.hist.mistakes == 1);
    CHECK(sv_hint(&G) >= 0 && sv_hint(&G) >= 0 && G.hist.hints == 2);
    CHECK(sv_script(&G, "+81") && sv_solved(&G) && G.hist.mistakes == 1 && G.hist.hints == 2);

    /* the player's grid: notes cleared from the cage and the units, clashes in a cage (twice, or a wrong sum), undo */
    CHECK(ksu_decode(&G, ksu_levels[40]));
    {
        int c = 0, q = G.s.cage_of[0], mate = G.s.cage[q].n > 1 ? G.s.cage[q].cell[1] : -1;
        CHECK(sv_note(&G, 80, 4) && G.notes[80] == 1u << 4);
        if (mate >= 0) {
            CHECK(sv_note(&G, mate, 5) && sv_put(&G, c, 5) && !(G.notes[mate] >> 5 & 1));
            CHECK(sv_put(&G, mate, 5) && sv_clash(&G, mate) && sv_clash(&G, c));   /* twice in the cage */
            CHECK(sv_undo(&G) == mate && sv_undo(&G) >= 0 && G.notes[mate] == 1u << 5 && !G.val[c]);
        }
        for (int i = 0; i < G.s.cage[q].n; i++) {   /* the cage full, one digit wrong: its sum missed */
            int x = G.s.cage[q].cell[i];
            CHECK(sv_put(&G, x, G.sol[x]));
        }
        CHECK(!sv_clash(&G, c));
        int x = G.s.cage[q].cell[0], other = 0;
        for (int d = 1; d <= 9 && !other; d++) {
            int free = d != G.sol[x];
            for (int i = 1; i < G.s.cage[q].n; i++) free &= G.val[G.s.cage[q].cell[i]] != d;
            if (free) other = d;
        }
        CHECK(other && sv_put(&G, x, other) && sv_clash(&G, x));
        while (sv_undo(&G) >= 0) {}
        CHECK(G.hist.moves == 0 && !G.val[c] && !G.notes[80]);
        CHECK(!sv_script(&G, "a1=x") && sv_cell("e5") == 40 && sv_cell("j1") < 0);
    }

    /* whole games from seeds: hints alone solve them; a wrong digit put right first */
    for (uint32_t seed = 1; seed <= 4; seed++)
        for (int level = 0; level < KSU_VARIANTS - 1; level++) {
            ksu_new(&G, level, seed);
            CHECK(sv_put(&G, 40, G.sol[40] % 9 + 1));
            CHECK(sv_hint(&G) == 40 && G.val[40] == G.sol[40]);
            int guard = 0;
            while (!sv_solved(&G) && guard++ < 100) CHECK(sv_hint(&G) >= 0);
            CHECK(sv_solved(&G) && sv_hint(&G) < 0);
        }

    if (fails) {
        printf("killer sudoku: %d failed\n", fails);
        return 1;
    }
    printf("killer sudoku: all passed\n");
    return 0;
}
