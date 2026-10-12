/*
 * Skyscrapers' rules: the buildings seen from an edge, the solver and the grader on towns drawn by hand, the puzzles
 * made from many seeds (one solution, the level they were made for, well under a second each), the player's heights,
 * notes, clashes, edge numbers, hints and undo, and whole games played from a seed.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/puzrand.h"
#include "games/skyscrapers.h"

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

/* every edge number of a full town */
static void clues_of(sky_puzzle *p, const uint8_t *town)
{
    int n = p->n;
    for (int e = 0; e < 4; e++)
        for (int i = 0; i < n; i++) {
            uint8_t hs[SKY_MAX];
            for (int j = 0; j < n; j++) hs[j] = town[sky_line(n, e, i, j)];
            p->clue[e][i] = (uint8_t)sky_seen(hs, n);
        }
}

static int keeps_rules(const sky_puzzle *p, const uint8_t *town)
{
    int n = p->n;
    for (int i = 0; i < n; i++) {
        unsigned r = 0, c = 0;
        for (int j = 0; j < n; j++) r |= 1u << town[i * n + j], c |= 1u << town[j * n + i];
        if (r != c || r != ((1u << (n + 1)) - 2)) return 0;
    }
    sky_puzzle all = *p;
    clues_of(&all, town);
    for (int e = 0; e < 4; e++)
        for (int i = 0; i < n; i++)
            if (p->clue[e][i] && p->clue[e][i] != all.clue[e][i]) return 0;
    return 1;
}

int main(void)
{
    /* the buildings seen */
    static const uint8_t up[4] = {1, 2, 3, 4}, down[4] = {4, 3, 2, 1}, mixed[4] = {2, 1, 4, 3}, hole[4] = {2, 0, 4, 3};
    CHECK(sky_seen(up, 4) == 4 && sky_seen(down, 4) == 1 && sky_seen(mixed, 4) == 2 && sky_seen(hole, 4) == -1);
    CHECK(sky_line(4, SKY_TOP, 1, 0) == 1 && sky_line(4, SKY_BOTTOM, 1, 0) == 13 && sky_line(4, SKY_LEFT, 2, 1) == 9 &&
          sky_line(4, SKY_RIGHT, 2, 0) == 11);

    /* drawn by hand: a town of 4 told by all its edges; its edges halved; a row seeing the tallest first from both
     * ends (no town) */
    static const uint8_t town[16] = {1, 2, 3, 4, 2, 3, 4, 1, 3, 4, 1, 2, 4, 1, 2, 3};
    sky_puzzle p;
    memset(&p, 0, sizeof p);
    p.n = 4;
    clues_of(&p, town);
    uint8_t sol[SKY_CELLS];
    int lv = sky_grade(&p, sol);
    CHECK(lv >= BENCH_EASY && lv <= BENCH_HARD && !memcmp(sol, town, 16) && sky_count(&p, 2) == 1);
    sky_puzzle few = p;
    for (int e = 0; e < 4; e++) few.clue[e][1] = few.clue[e][3] = 0;
    CHECK(sky_count(&few, 2) >= 1);
    sky_puzzle none;
    memset(&none, 0, sizeof none);
    none.n = 4;
    none.clue[SKY_LEFT][0] = none.clue[SKY_RIGHT][0] = 1;
    CHECK(sky_count(&none, 2) == 0 && sky_grade(&none, NULL) == BENCH_NONE);
    sky_puzzle open;
    memset(&open, 0, sizeof open);
    open.n = 4;
    CHECK(sky_count(&open, 2) == 2 && sky_grade(&open, NULL) == BENCH_GUESS);

    /* the puzzles from many seeds: one solution, of their level, quick */
    clock_t t0 = clock();
    double slowest[SKY_VARIANTS] = {0};
    int told[SKY_VARIANTS] = {0}, given[SKY_VARIANTS] = {0};
    for (int v = 0; v < SKY_VARIANTS; v++)
        for (uint32_t seed = 1; seed <= 40; seed++) {
            clock_t t1 = clock();
            sky_make(sky_size(v), sky_level(v), seed * 7919u + (uint32_t)v, &p, sol);
            double took = (double)(clock() - t1) / CLOCKS_PER_SEC;
            if (took > slowest[v]) slowest[v] = took;
            int n = p.n;
            CHECK(n == sky_size(v) && keeps_rules(&p, sol));
            uint8_t found[SKY_CELLS];
            lv = sky_grade(&p, found);
            if (lv != sky_level(v)) printf("variant %d seed %u: graded %d\n", v, seed, lv);
            CHECK(lv == sky_level(v));
            CHECK(!memcmp(found, sol, (size_t)n * n));
            CHECK(sky_count(&p, 2) == 1);
            for (int c = 0; c < n * n; c++) {
                CHECK(!p.given[c] || p.given[c] == sol[c]);
                given[v] += p.given[c] != 0;
            }
            for (int e = 0; e < 4; e++)
                for (int i = 0; i < n; i++) told[v] += p.clue[e][i] != 0;
        }
    printf("skyscrapers: 40 puzzles a variant in %.1f s; the slowest", (double)(clock() - t0) / CLOCKS_PER_SEC);
    for (int v = 0; v < SKY_VARIANTS; v++) printf(" %.3f", slowest[v]);
    printf(" s; edge numbers told / heights given (a puzzle)");
    for (int v = 0; v < SKY_VARIANTS; v++) printf(" %.1f/%.1f", told[v] / 40.0, given[v] / 40.0);
    printf("\n");
    for (int v = 0; v < SKY_VARIANTS; v++) CHECK(slowest[v] < 0.5);

    /* the same seed: the same puzzle */
    sky_game a, b;
    sky_new(&a, 7, 42);
    sky_new(&b, 7, 42);
    CHECK(!memcmp(&a.p, &b.p, sizeof a.p) && !memcmp(a.sol, b.sol, sizeof a.sol));

    /* the player's town: heights, the notes they clear, clashes, undo */
    sky_game G;
    sky_new(&G, 3, 7);
    int n = G.p.n, f = -1, g2 = -1;
    for (int c = 0; c < n * n && (f < 0 || g2 < 0); c++) {
        if (!sky_free(&G, c)) continue;
        if (f < 0) f = c;
        else if (c / n == f / n) g2 = c;
    }
    CHECK(f >= 0 && g2 >= 0);
    CHECK(sky_note(&G, g2, 2) && sky_note(&G, g2, 3) && SKY_NOTE(G.st[g2], 2) && SKY_NOTE(G.st[g2], 3));
    CHECK(sky_note(&G, f, 1) && SKY_NOTE(G.st[f], 1));
    CHECK(sky_put(&G, f, 2) && SKY_HEIGHT(G.st[f]) == 2 && !SKY_NOTE(G.st[f], 1));
    CHECK(!SKY_NOTE(G.st[g2], 2) && SKY_NOTE(G.st[g2], 3));   /* the note 2 gone from its row */
    CHECK(!sky_put(&G, f, 2) && !sky_note(&G, f, 4));
    CHECK(G.hist.moves == 4);
    CHECK(sky_undo(&G) >= 0 && !SKY_HEIGHT(G.st[f]) && SKY_NOTE(G.st[f], 1) && SKY_NOTE(G.st[g2], 2));
    CHECK(sky_put(&G, f, 3) && sky_put(&G, g2, 3) && sky_clash(&G, f) && sky_clash(&G, g2));
    CHECK(sky_clear(&G, g2) && !sky_clash(&G, f) && !G.st[g2]);
    CHECK(sky_clear(&G, f) && !sky_clear(&G, f));
    for (int c = 0; c < n * n; c++)
        if (!sky_free(&G, c)) CHECK(!sky_put(&G, c, 1) && !sky_clear(&G, c) && SKY_HEIGHT(G.st[c]) == G.p.given[c]);
    /* the edge numbers: open while their line is not full, then right or wrong */
    sky_game E;
    memset(&E, 0, sizeof E);
    E.p.n = 4;
    E.p.clue[SKY_LEFT][0] = 4;
    E.p.clue[SKY_RIGHT][0] = 2;
    for (int x = 0; x < 3; x++) E.st[x] = (uint16_t)(x + 1);
    CHECK(sky_edge_state(&E, SKY_LEFT, 0) == 0);
    E.st[3] = 4;
    CHECK(sky_edge_state(&E, SKY_LEFT, 0) == 1 && sky_edge_state(&E, SKY_RIGHT, 0) == -1);
    CHECK(sky_edge_state(&E, SKY_TOP, 0) == 0);
    /* the notation */
    sky_new(&G, 0, 5);
    n = G.p.n;
    for (f = 0; !sky_free(&G, f); f++) {}
    char moves[32];
    char col = (char)('a' + f % n), row = (char)('1' + f / n);
    snprintf(moves, sizeof moves, "%c%c'1 %c%c'2 %c%c=3", col, row, col, row, col, row);
    CHECK(sky_script(&G, moves) && SKY_HEIGHT(G.st[f]) == 3 && G.hist.moves == 3);
    CHECK(!sky_script(&G, "z9=1") && !sky_script(&G, "a1=9"));

    /* a hint: a wrong height put right first, else the next cell found; hints alone solve a town */
    for (int v = 0; v < SKY_VARIANTS; v++) {
        sky_new(&G, v, 100u + (uint32_t)v);
        n = G.p.n;
        for (f = 0; !sky_free(&G, f); f++) {}
        int wrong = G.sol[f] % n + 1;
        CHECK(sky_put(&G, f, wrong) && sky_hint(&G) == f && SKY_HEIGHT(G.st[f]) == G.sol[f]);
        int guard = 0;
        while (!sky_solved(&G) && guard++ < n * n) {
            int c = sky_hint(&G);
            CHECK(c >= 0 && SKY_HEIGHT(G.st[c]) == G.sol[c]);
            if (c < 0) break;
        }
        CHECK(sky_solved(&G));
    }

    /* whole games from a seed: random heights, notes and clearings, then hints to the end; undo all the way back */
    for (uint32_t seed = 1; seed <= 30; seed++) {
        uint32_t s = pr_seed(seed);
        sky_new(&G, (int)(seed % SKY_VARIANTS), seed);
        n = G.p.n;
        sky_game start = G;
        for (int i = 0; i < 50; i++) {
            int c = pr_below(&s, n * n), kind = pr_below(&s, 3), h = pr_below(&s, n) + 1;
            if (kind == 0) sky_put(&G, c, h);
            else if (kind == 1) sky_note(&G, c, h);
            else sky_clear(&G, c);
        }
        int guard = 0;
        while (!sky_solved(&G) && guard++ < 2 * n * n) CHECK(sky_hint(&G) >= 0);
        CHECK(sky_solved(&G));
        while (sky_undo(&G) >= 0) {}
        CHECK(!memcmp(G.st, start.st, sizeof G.st) && G.hist.moves == 0);
    }

    if (fails) printf("skyscrapers: %d checks failed\n", fails);
    else printf("skyscrapers: all checks passed\n");
    return fails != 0;
}
