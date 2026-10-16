/*
 * Diagonal Sudoku's rules: the puzzles made at every level (one solution, the level they were made for, their
 * clues the solution's and opposite each other round the centre, quick), the levels' code, the player's digits, notes,
 * clashes on a diagonal, hints and undo, whole games played from seeds; every designed level.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/diagonalsudoku.h"
#include "games/diagonalsudoku_levels.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* every row, column, box and both diagonals hold 1..9 */
static int valid_full(const sv_shape *s, const uint8_t *g)
{
    (void)s;
    unsigned r[9] = {0}, c[9] = {0}, b[9] = {0}, d1 = 0, d2 = 0;
    for (int i = 0; i < SV_CELLS; i++) {
        r[i / 9] |= 1u << g[i];
        c[i % 9] |= 1u << g[i];
        b[sv_box(i)] |= 1u << g[i];
        if (i / 9 == i % 9) d1 |= 1u << g[i];
        if (i / 9 + i % 9 == 8) d2 |= 1u << g[i];
    }
    for (int i = 0; i < 9; i++)
        if (r[i] != SV_ALL || c[i] != SV_ALL || b[i] != SV_ALL) return 0;
    return d1 == SV_ALL && d2 == SV_ALL;
}


static sv_game G, L;

int main(void)
{
    uint8_t p[SV_CELLS], sol[SV_CELLS], found[SV_CELLS];

    /* the puzzles from many seeds: one solution, their level, the clues the solution's, quick */
    double slowest[DSU_VARIANTS] = {0};
    int hit[DSU_VARIANTS] = {0}, n = 12;
    clock_t t0 = clock();
    for (int level = 0; level < DSU_VARIANTS; level++)
        for (uint32_t seed = 1; seed <= (uint32_t)n; seed++) {
            clock_t t1 = clock();
            dsu_new(&G, level, seed * 7919u + (uint32_t)level);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[level]) slowest[level] = took;
            CHECK(G.s.diag && G.s.nunits == 29 && valid_full(&G.s, G.sol));
            int clues = 0;
            for (int c = 0; c < SV_CELLS; c++) {
                p[c] = G.given[c] ? G.val[c] : 0;
                clues += G.given[c];
                CHECK(!G.given[c] || G.val[c] == G.sol[c]);
                CHECK(!G.given[c] == !G.given[SV_CELLS - 1 - c]);
            }
            int g = sv_grade(&G.s, p, SV_EXPERT, found, NULL, 0);
            CHECK(g >= SV_EASY && g <= level && !memcmp(found, G.sol, SV_CELLS));
            CHECK(sv_count(&G.s, p, 2, NULL) == 1);
            CHECK(clues >= dsu_floor(level));
            hit[level] += g == level;
        }
    printf("diagonal sudoku: %d puzzles a level in %.1f s, at their level %d %d %d %d; the slowest %.2f %.2f %.2f %.2f s\n",
           n, (double)(clock() - t0) / CLOCKS_PER_SEC, hit[0], hit[1], hit[2], hit[3], slowest[0], slowest[1],
           slowest[2], slowest[3]);
    CHECK(hit[0] == n && hit[1] >= n - 1 && hit[2] >= n * 2 / 3 && hit[3] >= n / 2);
    for (int level = 0; level < DSU_VARIANTS; level++) CHECK(slowest[level] < 1.0);

    /* every designed level: one solution, its chapter's level, no two the same, the code read back the same; hints
     * alone solve it */
    {
        static const int lo[DSU_CHAPTERS] = {0, 0, 1, 2, 2, 3}, hi[DSU_CHAPTERS] = {0, 1, 1, 2, 2, 3};
        int per = DSU_LEVELS / DSU_CHAPTERS, bad = 0;
        char code[SV_CELLS + 1] = {0};
        for (int i = 0; i < DSU_LEVELS; i++) {
            int ch = i / per, ok = dsu_decode(&L, dsu_levels[i]);
            for (int c = 0; ok && c < SV_CELLS; c++) p[c] = L.given[c] ? L.val[c] : 0;
            ok = ok && L.level >= lo[ch] && L.level <= hi[ch] && valid_full(&L.s, L.sol);
            ok = ok && sv_count(&L.s, p, 2, NULL) == 1;
            if (ok) sv_clues_encode(p, code);
            ok = ok && !strcmp(code, dsu_levels[i]);
            for (int j = 0; ok && j < i; j++) ok = strcmp(dsu_levels[i], dsu_levels[j]) != 0;
            for (int guard = 0; ok && !sv_solved(&L) && guard < 100; guard++) ok = sv_hint(&L) >= 0;
            ok = ok && sv_solved(&L);
            if (!ok) printf("diagonal sudoku level %d: bad (level %d)\n", i + 1, L.level);
            bad += !ok;
        }
        CHECK(bad == 0);
        CHECK(!dsu_decode(&L, "123") && !dsu_decode(&L, dsu_levels[0] + 1));
    }

    /* the same seed: the same puzzle */
    {
        static sv_game a, b;
        dsu_new(&a, SV_MEDIUM, 42);
        dsu_new(&b, SV_MEDIUM, 42);
        CHECK(!memcmp(a.sol, b.sol, SV_CELLS) && !memcmp(a.given, b.given, SV_CELLS));
    }

    /* the player's grid: a clash on a diagonal (not in the cells' row, column or box), notes cleared there, undo; the
     * stars' counts */
    CHECK(dsu_decode(&G, dsu_levels[60]));
    {
        int a = -1, b = -1;
        for (int x = 0; x < SV_CELLS && a < 0; x++)
            for (int y = x + 1; y < SV_CELLS && a < 0; y++)
                if (!G.given[x] && !G.given[y] && x / 9 == x % 9 && y / 9 == y % 9 && sv_box(x) != sv_box(y))
                    a = x, b = y;
        CHECK(a >= 0);
        int d = G.sol[a];                       /* right in a, wrong in b */
        CHECK(sv_note(&G, b, d) && sv_put(&G, a, d) && !G.notes[b]);
        CHECK(sv_put(&G, b, d) && sv_clash(&G, a) && sv_clash(&G, b));
        CHECK(sv_undo(&G) == b && !sv_clash(&G, a) && sv_undo(&G) == a && G.notes[b] == 1u << d);
        int wrong = G.sol[a] % 9 + 1, slips = G.hist.mistakes;
        CHECK(sv_put(&G, a, wrong) && sv_undo(&G) == a && G.hist.mistakes == slips + 1);
        CHECK(sv_hint(&G) >= 0 && G.hist.hints == 1);
        while (sv_undo(&G) >= 0) {}
        CHECK(G.hist.moves == 0 && !G.notes[b]);
    }

    /* whole games from seeds: hints alone solve them; a wrong digit put right first */
    for (uint32_t seed = 1; seed <= 4; seed++)
        for (int level = 0; level < DSU_VARIANTS; level++) {
            dsu_new(&G, level, seed);
            int e = 0;
            while (G.given[e]) e++;
            CHECK(sv_put(&G, e, G.sol[e] % 9 + 1));
            CHECK(sv_hint(&G) == e && G.val[e] == G.sol[e]);
            int guard = 0;
            while (!sv_solved(&G) && guard++ < 100) CHECK(sv_hint(&G) >= 0);
            CHECK(sv_solved(&G) && sv_hint(&G) < 0);
        }
    CHECK(dsu_decode(&G, dsu_levels[0]) && sv_script(&G, "+81") && sv_solved(&G));
    (void)sol;

    if (fails) {
        printf("diagonal sudoku: %d failed\n", fails);
        return 1;
    }
    printf("diagonal sudoku: all passed\n");
    return 0;
}
