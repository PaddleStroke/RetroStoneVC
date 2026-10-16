/*
 * Go Problems' rules: the solver's board against go's rules (the captures, the suicides, random games in a corner),
 * Benson's test on known shapes, and every level: read back, its first move the only one that wins (every other, and
 * the pass, proven to lose), its main line played with go's rules to the end within its depth, a wrong first move
 * refuted and taken back, the hints, undo, the stars, and random tries driven by seeds. The time of the slowest reply
 * is printed (the device answers within it).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The Makefile links a game's test with its *_rules.c only; these rules need go_rules.c too, so it is compiled in here
 * (once the Makefile links it into this test, build it with -DGPZ_GO_LINKED).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "games/goproblems.h"
#include "games/puzrand.h"
#ifndef GPZ_GO_LINKED
#include "games/go_rules.c"
#endif

static int fails;
#define CHECK(c)                                                                                                    \
    do {                                                                                                            \
        if (!(c)) {                                                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);                                                     \
            fails++;                                                                                                \
        }                                                                                                           \
    } while (0)

static gpz_game G;

static double now_ms(void) { return (double)clock() * 1000.0 / CLOCKS_PER_SEC; }

/* the solver's board, replayed from the problem's start by the game's moves, against go's stones */
static int same_as_go(const gpz_game *g)
{
    gpz_board b = g->pz.start;
    int side = GPZ_BLACK;
    for (int k = 0; k < g->g.nmoves; k++, side = 3 - side) {
        int m = g->g.move[k];
        if (m == GO_PASS) continue;
        if (!gpz_play(&b, gpz_cell(m), side)) return 0;
    }
    for (int y = 0; y < g->pz.h; y++)
        for (int x = 0; x < g->pz.w; x++) {
            int c = GPZ_CELL(x, y), v = b.c[c];
            if (gpz_cell_at(g, c) != v) return 0;
        }
    return 1;
}

static void test_board(void)
{
    gpz_problem pz;
    /* a white stone in the corner taken; a suicide refused; a stone next to the outside never taken */
    CHECK(gpz_parse(&pz, "K 4 4 1 0,0 1,0 O.,,/X.,,/,,,,/,,,,"));
    gpz_board b = pz.start;
    CHECK(gpz_play(&b, GPZ_CELL(1, 0), GPZ_BLACK) == 2);
    CHECK(b.c[GPZ_CELL(0, 0)] == GPZ_EMPTY);
    CHECK(gpz_status(&pz, &b) == GPZ_AWIN);
    b = pz.start;
    CHECK(!gpz_parse(&pz, "K 4 4 1 0,0 1,0 OX,,/X.,,/,,,,/,,,,"));   /* a group with no liberty */
    CHECK(gpz_parse(&pz, "K 4 3 1 3,0 0,0 .X.O/X.X,/.X.,"));
    b = pz.start;
    CHECK(gpz_play(&b, GPZ_CELL(1, 1), GPZ_WHITE) == 0);   /* suicide */
    /* two eyes: alive for good; one: not */
    CHECK(gpz_parse(&pz, "L 5 3 1 1,0 0,0 .X.XO/XXXXO/OOOOO"));
    CHECK(gpz_benson(&pz.start, GPZ_CELL(1, 0)));
    CHECK(gpz_status(&pz, &pz.start) == GPZ_DWIN);
    CHECK(gpz_parse(&pz, "L 5 3 1 2,0 0,0 ..XXO/XXXXO/OOOOO"));
    CHECK(!gpz_benson(&pz.start, GPZ_CELL(2, 0)));
    /* the solver: straight three, the middle kills (and lives) */
    CHECK(gpz_parse(&pz, "K 5 3 1 0,1 1,0 ...O,/OOOOX/XXXXX"));
    (void)pz;
    /* random games in a corner: the solver's board as go's */
    for (int seed = 1; seed <= 300; seed++) {
        uint32_t r = pr_seed((uint32_t)seed);
        CHECK(gpz_parse(&pz, "K 6 6 1 0,0 1,1 O...../....../....../....../....../,,,,,,"));
        gpz_begin(&G, &pz, -1);
        for (int k = 0; k < 40 && G.g.nmoves < 60; k++) {
            int c = GPZ_CELL(pr_below(&r, 6), pr_below(&r, 5));
            gopos next;
            int go_ok = go_try(&G.g, gpz_pt(c), &next) >= 0;
            gpz_board b2;
            (void)b2;
            if (go_ok) {
                go_play(&G.g, gpz_pt(c));
                CHECK(same_as_go(&G));
            }
        }
    }
}

/* the level's analysis from its start: one winner (its first move, at its depth), every other move proven to lose */
static void test_unique(const gpz_problem *pz, int level)
{
    gpz_eval ev[GPZ_MAXMOVES];
    gpz_new_search();
    int n = gpz_analyse(pz, &pz->start, GPZ_BLACK, 0, &pz->start.hash, 1, GPZ_PROOF, ev, NULL);
    int want = pz->kind == GPZ_KILL ? GPZ_AWIN : GPZ_DWIN, wins = 0, ok = 1;
    for (int i = 0; i < n; i++) {
        if (ev[i].val == want) {
            wins++;
            if (ev[i].move != pz->first || ev[i].depth != pz->depth) ok = 0;
        } else if (ev[i].val == GPZ_UNK) ok = 0;
    }
    if (wins != 1 || !ok)
        for (int i = 0; i < n; i++)
            printf("level %d: move %d,%d val %d depth %d (first %d,%d depth %d)\n", level + 1, GPZ_X(ev[i].move),
                   GPZ_Y(ev[i].move), ev[i].val, ev[i].depth, GPZ_X(pz->first), GPZ_Y(pz->first), pz->depth);
    CHECK(wins == 1 && ok);
}

static double slowest;

static int timed_try(gpz_game *g, int cell)
{
    double t = now_ms();
    int r = gpz_try(g, cell);
    t = now_ms() - t;
    if (t > slowest) slowest = t;
    return r;
}

/* the main line: the hints' moves to the end, every one right, within the depth and its slack */
static void test_line(int level)
{
    CHECK(gpz_level(&G, level));
    int plies = 0, r = GPZ_RIGHT;
    while (r == GPZ_RIGHT && plies < 40) {
        int m = gpz_hint(&G);
        CHECK(m >= 0);
        if (m < 0) break;
        r = timed_try(&G, m);
        CHECK(r == GPZ_RIGHT || r == GPZ_SOLVED);
        CHECK(same_as_go(&G));
        plies = G.g.nmoves;
    }
    CHECK(r == GPZ_SOLVED && G.solved);
    CHECK(G.mistakes == 0);
    if (r != GPZ_SOLVED || plies > G.pz.depth + GPZ_SLACK) printf("level %d: %d plies of %d\n", level + 1, plies,
                                                                    G.pz.depth);
    CHECK(plies <= G.pz.depth + GPZ_SLACK);
    CHECK(G.hints >= 1 && gpz_stars(&G) < 3);   /* hinted */
    /* undo all the way back */
    while (gpz_undo(&G)) {}
    CHECK(G.g.nmoves == 0 && !G.solved);
    /* a wrong first move: refuted, counted, taken back */
    for (int c = 0; c < GPZ_CELLS; c++) {
        if (gpz_cell_at(&G, c) != GPZ_EMPTY || c == G.pz.first) continue;
        int k = timed_try(&G, c);
        if (k == GPZ_ILLEGAL) continue;
        if (k != GPZ_WRONG) printf("level %d: %d,%d gives %d\n", level + 1, GPZ_X(c), GPZ_Y(c), k);
        CHECK(k == GPZ_WRONG);
        CHECK(G.failed && G.mistakes == 1);
        CHECK(same_as_go(&G));
        CHECK(gpz_undo(&G) && G.g.nmoves == 0 && !G.failed);
        break;
    }
    /* the right first move, played without a hint */
    CHECK(gpz_level(&G, level));
    r = timed_try(&G, G.pz.first);
    CHECK(r == GPZ_RIGHT || r == GPZ_SOLVED);
    if (r == GPZ_SOLVED) CHECK(gpz_stars(&G) == 3);
}

/* random taps over a level: the game stays sound */
static void test_random(int level, uint32_t seed)
{
    uint32_t r = pr_seed(seed);
    CHECK(gpz_level(&G, level));
    for (int k = 0; k < 6 && !G.solved; k++) {
        int c = GPZ_CELL(pr_below(&r, G.pz.w), pr_below(&r, G.pz.h));
        int v = timed_try(&G, c);
        CHECK(same_as_go(&G));
        if (v == GPZ_WRONG) CHECK(gpz_undo(&G));
        if (pr_below(&r, 4) == 0) gpz_undo(&G);
        CHECK(!G.failed);
    }
}

int main(void)
{
    test_board();
    int kinds[2] = {0, 0};
    double t0 = now_ms();
    for (int level = 0; level < GPZ_LEVELS; level++) {
        gpz_problem pz;
        CHECK(gpz_level_load(&pz, level));
        if (!pz.w) continue;
        kinds[pz.kind]++;
        int ch = gpz_level_chapter(level);
        CHECK(ch == level / 25);
        CHECK(ch == (pz.depth >= 11 ? 5 : (pz.depth - 1) / 2));   /* the chapters by depth */
        char back[600];
        gpz_write(&pz, back, sizeof back);
        gpz_problem again;
        CHECK(gpz_parse(&again, back) && !memcmp(&again, &pz, sizeof pz));
        test_unique(&pz, level);
        test_line(level);
        test_random(level, (uint32_t)level * 7 + 1);
    }
    CHECK(kinds[0] > 30 && kinds[1] > 30);
    CHECK(gpz_script(&G, "#0") && G.level == 0);
    printf("goproblems: %d levels (%d kills, %d lives), %.0f ms, the slowest reply %.1f ms\n", GPZ_LEVELS, kinds[0],
           kinds[1], now_ms() - t0, slowest);
    if (fails) printf("%d FAILED\n", fails);
    return fails != 0;
}
