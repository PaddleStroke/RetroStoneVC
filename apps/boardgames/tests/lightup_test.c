/*
 * Light Up's rules: the solver and the grader on puzzles drawn by hand, the puzzles made from many seeds (one
 * solution, the level they were made for, well under a second each), the player's lamps, dots, drags, hints and
 * undo, and whole games played from a seed with wrong cells and hints.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/lightup.h"
#include "games/puzrand.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* a puzzle from rows of text: '.' white, '#' a wall, '0'..'4' a numbered wall */
static void puzzle_of(lu_puzzle *p, int n, const char *const *rows)
{
    memset(p, 0, sizeof *p);
    p->n = n;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            char ch = rows[y][x];
            p->wall[y * n + x] = (int8_t)(ch == '.' ? LU_WHITE : ch == '#' ? LU_WALL : ch - '0');
        }
}

/* the rules checked from scratch: every white cell lit, no lamp seeing another, every number right */
static int keeps_rules(const lu_puzzle *p, const uint8_t *lamps)
{
    lu_game g;
    memset(&g, 0, sizeof g);
    g.p = *p;
    for (int c = 0; c < p->n * p->n; c++) g.st[c] = lamps[c] ? LU_LAMP : LU_EMPTY;
    return lu_solved(&g);
}

int main(void)
{
    lu_puzzle p;
    uint8_t sol[LU_CELLS];

    /* drawn by hand: a 4 needs its four lamps, then one corner is left that only one cell lights */
    static const char *const four[] = {"....", ".4..", "....", "...."};
    puzzle_of(&p, 4, four);
    CHECK(lu_grade(&p, sol) == BENCH_EASY);
    CHECK(sol[1] && sol[4] && sol[6] && sol[9] && sol[15]);
    int lamps = 0;
    for (int c = 0; c < 16; c++) lamps += sol[c];
    CHECK(lamps == 5 && keeps_rules(&p, sol));
    CHECK(lu_count(&p, 3) == 1);
    /* an open field: many ways; a 4 in a corner: none */
    static const char *const open[] = {"....", "....", "....", "...."};
    puzzle_of(&p, 4, open);
    CHECK(lu_count(&p, 2) == 2);
    CHECK(lu_grade(&p, NULL) == BENCH_GUESS);
    static const char *const none[] = {"4...", "....", "....", "...."};
    puzzle_of(&p, 4, none);
    CHECK(lu_count(&p, 2) == 0);
    CHECK(lu_grade(&p, NULL) == BENCH_NONE);
    /* a 1 between two cells that see each other's lines: the cell seeing both of them can hold no lamp (medium) */
    static const char *const mid[] = {".#...", "#1.#.", "..#..", "..#..", "#...#"};
    puzzle_of(&p, 5, mid);
    int g = lu_grade(&p, sol);
    CHECK(g >= BENCH_EASY && g <= BENCH_HARD ? keeps_rules(&p, sol) && lu_count(&p, 2) == 1 : lu_count(&p, 2) != 1);

    /* the puzzles from many seeds: one solution, of their level, quick */
    clock_t t0 = clock();
    double slowest[LU_VARIANTS] = {0};
    int walls[LU_VARIANTS] = {0}, numbers[LU_VARIANTS] = {0};
    for (int v = 0; v < LU_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 40; seed++) {
            clock_t t1 = clock();
            lu_make(lu_size(v), lu_level(v), seed * 7919u + (uint32_t)v, &p, sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            CHECK(p.n == lu_size(v));
            CHECK(keeps_rules(&p, sol));
            uint8_t found[LU_CELLS];
            int lv = lu_grade(&p, found);
            if (lv != lu_level(v)) printf("variant %d seed %u: graded %d\n", v, seed, lv);
            CHECK(lv == lu_level(v));
            CHECK(!memcmp(found, sol, (size_t)p.n * p.n));
            CHECK(lu_count(&p, 2) == 1);
            for (int c = 0; c < p.n * p.n; c++) {
                walls[v] += p.wall[c] != LU_WHITE;
                numbers[v] += p.wall[c] >= 0;
                CHECK(p.wall[c] != LU_WHITE || p.wall[p.n * p.n - 1 - c] == LU_WHITE);   /* the walls' twins */
            }
        }
    printf("lightup: 40 puzzles a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < LU_VARIANTS; v++) printf(" %.3f", slowest[v]);
    printf(" s; numbered walls");
    for (int v = 0; v < LU_VARIANTS; v++) printf(" %d%%", numbers[v] * 100 / (walls[v] ? walls[v] : 1));
    printf("\n");
    for (int v = 0; v < LU_VARIANTS; v++) CHECK(slowest[v] < 0.5);

    /* the same seed: the same puzzle */
    lu_game a, b;
    lu_new(&a, 4, 42);
    lu_new(&b, 4, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's cells: a tap puts a lamp or takes it out, a hold a dot; a wall takes nothing; undo */
    lu_game G;
    lu_new(&G, 0, 7);
    int w = -1, wall = -1, n = G.p.n;
    for (int c = 0; c < n * n; c++) {
        if (lu_white(&G, c) && w < 0) w = c;
        if (!lu_white(&G, c) && wall < 0) wall = c;
    }
    CHECK(lu_tap_state(&G, w) == LU_LAMP && lu_hold_state(&G, w) == LU_DOT);
    CHECK(lu_paint(&G, &w, 1, LU_LAMP) && G.st[w] == LU_LAMP && lu_lit(&G, w));
    CHECK(lu_tap_state(&G, w) == LU_EMPTY && lu_hold_state(&G, w) == LU_DOT);
    CHECK(!lu_paint(&G, &w, 1, LU_LAMP));       /* nothing changed: no move */
    CHECK(!lu_paint(&G, &wall, 1, LU_LAMP) && G.st[wall] == LU_EMPTY);
    CHECK(lu_paint(&G, &w, 1, LU_DOT) && G.st[w] == LU_DOT && !lu_lit(&G, w));
    CHECK(lu_tap_state(&G, w) == LU_LAMP && lu_hold_state(&G, w) == LU_EMPTY);
    CHECK(G.hist.moves == 2);
    CHECK(lu_undo(&G) == w && G.st[w] == LU_LAMP && G.hist.moves == 1);
    CHECK(lu_undo(&G) == w && G.st[w] == LU_EMPTY && G.hist.moves == 0);
    CHECK(lu_undo(&G) < 0);
    {                                           /* the stars' counts (bench.h): a wrong lamp undone */
        lu_game S;
        lu_new(&S, 0, 7);
        int f = -1;
        for (int c = 0; c < n * n && f < 0; c++)
            if (lu_white(&S, c) && !S.sol[c]) f = c;
        CHECK(lu_paint(&S, &f, 1, LU_LAMP) && S.hist.mistakes == 0);
        CHECK(lu_undo(&S) == f && S.hist.mistakes == 1);
        CHECK(lu_hint(&S) >= 0 && S.hist.hints == 1);
    }
    /* two lamps in one stretch: both clash */
    int w2 = -1;
    for (int c = w + 1; c < n * n && c / n == w / n && lu_white(&G, c); c++) w2 = c;
    if (w2 >= 0) {
        int two[2] = {w, w2};
        CHECK(lu_paint(&G, two, 2, LU_LAMP) && lu_clash(&G, w) && lu_clash(&G, w2) && !lu_solved(&G));
        CHECK(lu_undo(&G) >= 0 && G.st[w] == LU_EMPTY && G.st[w2] == LU_EMPTY);   /* one action: one undo */
    }
    /* the notation: a tap, a hold, a drag, a hint */
    lu_new(&G, 3, 5);
    char cell[16];
    for (w = 0; w < G.p.n * G.p.n && !lu_white(&G, w); w++) {}
    snprintf(cell, sizeof cell, "%c%d", (char)('a' + w % G.p.n), (w / G.p.n + 1) % 100);
    char moves[64];
    snprintf(moves, sizeof moves, "%s .%s %s", cell, cell, cell);
    CHECK(lu_script(&G, moves) && G.st[w] == LU_LAMP && G.hist.moves == 3);
    CHECK(!lu_script(&G, "z9") && !lu_script(&G, "a1-c3"));
    lu_new(&G, 3, 5);
    CHECK(lu_script(&G, ".a1-j1") || !lu_white(&G, 0));
    for (int c = 0; c < G.p.n && lu_white(&G, 0); c++) {   /* the drag: every white cell of row 1 as a1 was */
        if (!lu_white(&G, c)) continue;
        CHECK(G.st[c] == LU_DOT);
    }

    /* a hint: a wrong cell put right first, else the next cell found; hints alone solve a puzzle */
    for (int v = 0; v < LU_VARIANTS; v++) {
        lu_new(&G, v, 100u + (uint32_t)v);
        n = G.p.n;
        int wrong = -1;
        for (int c = 0; c < n * n && wrong < 0; c++)
            if (lu_white(&G, c) && !G.sol[c]) wrong = c;
        CHECK(lu_paint(&G, &wrong, 1, LU_LAMP));
        CHECK(lu_hint(&G) == wrong && G.st[wrong] == LU_DOT);
        int hints = 0;
        while (!lu_solved(&G) && hints < n * n) {
            int c = lu_hint(&G);
            CHECK(c >= 0);
            if (c < 0) break;
            CHECK((G.st[c] == LU_LAMP) == G.sol[c]);
            hints++;
        }
        CHECK(lu_solved(&G));
    }

    /* whole games from a seed: random taps, holds and drags, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 30; seed++) {
        uint32_t s = pr_seed(seed);
        lu_new(&G, (int)(seed % LU_VARIANTS), seed);
        n = G.p.n;
        lu_game start = G;
        for (int i = 0; i < 40; i++) {
            int c = pr_below(&s, n * n), kind = pr_below(&s, 3);
            if (!lu_white(&G, c)) continue;
            if (kind < 2) {
                lu_paint(&G, &c, 1, kind ? lu_hold_state(&G, c) : lu_tap_state(&G, c));
            } else {
                int cells[LU_MAX], k = 0, was = G.st[c];
                for (int x = c; x < (c / n + 1) * n; x++)
                    if (lu_white(&G, x) && G.st[x] == was) cells[k++] = x;
                lu_paint(&G, cells, k, lu_tap_state(&G, c));
            }
        }
        int guard = 0;
        while (!lu_solved(&G) && guard++ < 2 * n * n) CHECK(lu_hint(&G) >= 0);
        CHECK(lu_solved(&G));
        while (lu_undo(&G) >= 0) {}
        CHECK(!memcmp(G.st, start.st, sizeof G.st) && G.hist.moves == 0);
    }

    if (fails) printf("lightup: %d checks failed\n", fails);
    else printf("lightup: all checks passed\n");
    return fails != 0;
}
