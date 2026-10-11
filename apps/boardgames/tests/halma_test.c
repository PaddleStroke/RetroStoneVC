/*
 * Chinese checkers' rules test (src/games/halma_rules.c, no SDL): the 121 holes and their neighbours, the notation,
 * steps, single and chained jumps over both colours, no hole visited twice, the win, the anti-blocking rule, undo,
 * whole games played greedily (always won, all undone).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/halma.h"

static hl_game G;
static int fails, checks;

#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static int at(const char *name)
{
    int len;
    return hl_hole(name, &len);
}

static int has(const int *list, int k, int h)
{
    for (int i = 0; i < k; i++)
        if (list[i] == h) return 1;
    return 0;
}

/* an empty board, the turn given */
static void bare(int turn)
{
    hl_new(&G);
    memset(G.cell, 0, sizeof G.cell);
    G.ply = turn;
}

static void put(int x2, int row, int who) { G.cell[hl_at(x2, row)] = (uint8_t)who; }

static void test_board(void)
{
    int n = 0, links = 0, homes[2] = {0, 0}, nb[6], nb2[6];
    for (int r = 0; r < HL_ROWS; r++)
        for (int x = -12; x <= 12; x++) {
            int h = hl_at(x, r);
            if (h < 0) continue;
            n++;
            CHECK(hl_row(h) == r && hl_x2(h) == x);
        }
    CHECK(n == HL_HOLES);
    for (int h = 0; h < HL_HOLES; h++) {
        int k = hl_around(h, nb);
        CHECK(k >= 2 && k <= 6);
        links += k;
        for (int i = 0; i < k; i++) CHECK(has(nb2, hl_around(nb[i], nb2), h));   /* both ways */
        if (hl_home(h) >= 0) homes[hl_home(h)]++;
        char name[4];
        hl_name(h, name);
        CHECK(at(name) == h);
    }
    CHECK(links == 2 * 312);                    /* the star's 312 lines between neighbours */
    CHECK(homes[0] == HL_MARBLES && homes[1] == HL_MARBLES);
    CHECK(hl_around(at("a1"), nb) == 2 && hl_around(at("m5"), nb) == 2 && hl_around(at("e9"), nb) == 6);
    CHECK(at("a1") == 0 && at("a17") == HL_HOLES - 1 && at("n5") < 0 && at("e4") < 0 && at("a18") < 0);
    CHECK(hl_goal(0, at("a17")) && hl_goal(1, at("a1")) && !hl_goal(0, at("a1")) && !hl_goal(0, at("a5")));
}

static void test_start(void)
{
    int to[HL_HOLES];
    hl_new(&G);
    CHECK(hl_turn(&G) == 0 && hl_status(&G) == HL_PLAY);
    CHECK(G.cell[at("a1")] == HL_IVORY && G.cell[at("d4")] == HL_IVORY && G.cell[at("a17")] == HL_EBONY);
    int k = hl_targets(&G, at("d4"), to);       /* two steps */
    CHECK(k == 2 && has(to, k, at("h5")) && has(to, k, at("i5")));
    k = hl_targets(&G, at("c3"), to);           /* two jumps, over d4 and over c4 */
    CHECK(k == 2 && has(to, k, at("i5")) && has(to, k, at("g5")));
    CHECK(hl_targets(&G, at("a1"), to) == 0);   /* walled in */
    CHECK(hl_targets(&G, at("a17"), to) == 0);  /* not ebony's turn */
    CHECK(!hl_script(&G, "d4-d6"));             /* not a step, not a jump */
    CHECK(hl_script(&G, "c3-i5 a15-g13"));      /* a jump each */
    CHECK(G.cell[at("i5")] == HL_IVORY && G.cell[at("c3")] == HL_EMPTY && hl_turn(&G) == 0);
    CHECK(G.cell[at("g13")] == HL_EBONY);
}

/* a chain of three jumps from e9 (0, 8): over ebony to (4, 8), over ivory to (6, 10), over ebony to (2, 10); from
 * (4, 8) it could jump back over the first marble onto e9, where it started: not allowed */
static void test_chain(void)
{
    int to[HL_HOLES];
    hl_path p;
    bare(0);
    put(0, 8, HL_IVORY);
    put(2, 8, HL_EBONY);
    put(5, 9, HL_IVORY);
    put(4, 10, HL_EBONY);
    int from = hl_at(0, 8), end = hl_at(2, 10), k = hl_targets(&G, from, to);
    CHECK(has(to, k, hl_at(4, 8)) && has(to, k, hl_at(6, 10)) && has(to, k, end));
    CHECK(!has(to, k, from));
    CHECK(has(to, k, hl_at(-1, 9)) && has(to, k, hl_at(-2, 8)) && !has(to, k, hl_at(2, 8)));   /* steps */
    CHECK(hl_route(&G, from, end, &p) && p.n == 4 && p.hole[1] == hl_at(4, 8) && p.hole[2] == hl_at(6, 10));
    CHECK(hl_valid(&G, &p));
    hl_path back = {3, {(uint8_t)from, (uint8_t)hl_at(4, 8), (uint8_t)from}};   /* there and back again */
    CHECK(!hl_valid(&G, &back));
    hl_path two = {3, {(uint8_t)from, (uint8_t)hl_at(-1, 9), (uint8_t)hl_at(-2, 10)}};   /* a step then more */
    CHECK(!hl_valid(&G, &two));
    hl_path over_none = {2, {(uint8_t)from, (uint8_t)hl_at(-4, 8)}};   /* a jump over an empty hole */
    CHECK(!hl_valid(&G, &over_none));
    CHECK(hl_play(&G, &p) && G.cell[end] == HL_IVORY && !G.cell[from] && hl_turn(&G) == 1);
    CHECK(hl_last(&G)->n == 4);
    CHECK(hl_undo(&G) && G.cell[from] == HL_IVORY && !G.cell[end] && hl_turn(&G) == 0 && !hl_last(&G));
    CHECK(!hl_undo(&G));
}

/* ivory has 9 marbles in the top point; the 10th steps in: a win. The same with 3 of ebony's marbles never moved from
 * there: ivory wins when the 7 other holes are full of its own (the anti-blocking rule) */
static void test_win(void)
{
    int top[HL_MARBLES], n = 0;
    for (int h = 0; h < HL_HOLES; h++)
        if (hl_home(h) == 1) top[n++] = h;
    bare(0);
    for (int i = 1; i < HL_MARBLES; i++) G.cell[top[i]] = HL_IVORY;
    int gap = top[0];                           /* a hole of the bottom row of the point (row 13) */
    put(hl_x2(gap) - 1, 12, HL_IVORY);
    put(0, 0, HL_EBONY);                        /* ebony somewhere */
    CHECK(hl_status(&G) == HL_PLAY && hl_arrived(&G, 0) == 9);
    hl_path p;
    CHECK(hl_route(&G, hl_at(hl_x2(gap) - 1, 12), gap, &p) && hl_play(&G, &p));
    CHECK(hl_status(&G) == HL_WIN0 && hl_arrived(&G, 0) == 10);
    CHECK(!hl_play(&G, &p));                    /* over */
    CHECK(hl_undo(&G) && hl_status(&G) == HL_PLAY);

    bare(0);
    for (int i = 0; i < 3; i++) G.cell[top[HL_MARBLES - 1 - i]] = HL_EBONY;   /* the tip: ebony never left */
    for (int i = 1; i < HL_MARBLES - 3; i++) G.cell[top[i]] = HL_IVORY;
    put(hl_x2(gap) - 1, 12, HL_IVORY);
    CHECK(hl_status(&G) == HL_PLAY && hl_arrived(&G, 0) == 6);
    CHECK(hl_route(&G, hl_at(hl_x2(gap) - 1, 12), gap, &p) && hl_play(&G, &p));
    CHECK(hl_status(&G) == HL_WIN0);

    bare(0);                                    /* ebony's point full of ebony alone is no win for ivory */
    for (int h = 0; h < HL_HOLES; h++)
        if (hl_home(h) == 1) G.cell[h] = HL_EBONY;
    put(0, 8, HL_IVORY);
    CHECK(hl_status(&G) == HL_PLAY);
}

/* a game, then undo all the way back to the start */
static void test_undo(void)
{
    static hl_game start;
    hl_new(&start);
    hl_new(&G);
    CHECK(hl_script(&G, "d4-i5 a14-e13 c3-d4 a15-a14 b3-h5 b15-a15 i5-i6"));
    CHECK(G.ply == 7 && hl_turn(&G) == 1);
    while (hl_undo(&G)) {}
    CHECK(G.ply == 0 && !memcmp(G.cell, start.cell, sizeof G.cell));
}

/* hex distance between two holes */
static int dist(int a, int b)
{
    int dr = abs(hl_row(a) - hl_row(b)), dx = abs(hl_x2(a) - hl_x2(b));
    return dr + (dx > dr ? (dx - dr) / 2 : 0);
}

/* whole games, each player taking the move that goes furthest towards the far tip (a little chance between equals):
 * every move found is valid, every game ends with a winner, and undo takes it all back */
static void test_games(void)
{
    static hl_game start;
    unsigned rng = 12345;
    hl_new(&start);
    for (int game = 0; game < 20; game++) {
        hl_new(&G);
        int plies = 0, bad = 0;
        while (hl_status(&G) == HL_PLAY && plies < 600) {
            int p = hl_turn(&G), tip = p ? 0 : HL_HOLES - 1, bf = -1, bt = -1, best = -(1 << 30);
            for (int h = 0; h < HL_HOLES; h++) {
                if (G.cell[h] != p + 1) continue;
                int to[HL_HOLES], k = hl_targets(&G, h, to);
                for (int i = 0; i < k; i++) {
                    rng = rng * 1103515245u + 12345u;
                    int gain = 8 * (dist(h, tip) - dist(to[i], tip) - 10 * (hl_goal(p, h) && !hl_goal(p, to[i])));
                    gain += (int)(rng >> 16) % 3;
                    if (gain > best) best = gain, bf = h, bt = to[i];
                }
            }
            hl_path path;
            if (bf < 0 || !hl_route(&G, bf, bt, &path) || !hl_valid(&G, &path) || !hl_play(&G, &path)) {
                bad = 1;
                break;
            }
            plies++;
        }
        int st = hl_status(&G);
        CHECK(!bad && (st == HL_WIN0 || st == HL_WIN1));
        CHECK(hl_arrived(&G, st) > 0);
        while (hl_undo(&G)) {}
        CHECK(G.ply == 0 && !memcmp(G.cell, start.cell, sizeof G.cell));
    }
}

int main(void)
{
    test_board();
    test_start();
    test_chain();
    test_win();
    test_undo();
    test_games();
    printf("halma: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
