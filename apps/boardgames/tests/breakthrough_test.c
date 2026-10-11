/*
 * Breakthrough: the rules' test (the start, the moves, the captures, the ends, undo, the move counts). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/breakthrough.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static btgame G;
static btmove L[BT_MAX_MOVES];

/* a position: the ivory pieces ("a2 c4"), the ebony ones, who moves */
static void setup(const char *ivory, const char *ebony, int side)
{
    bt_new(&G);
    btpos *p = &G.pos[0];
    memset(p->at, -1, sizeof p->at);
    p->left[0] = p->left[1] = 0;
    const char *list[2] = {ivory, ebony};
    for (int pl = 0; pl < 2; pl++)
        for (const char *t = list[pl]; *t; t++) {
            int s = bt_square(t);
            if (s < 0) continue;
            p->at[s] = (int8_t)pl;
            p->left[pl]++;
            t++;
        }
    p->side = (uint8_t)side;
}

static const btpos *pos(void) { return bt_pos(&G); }
static int nlegal(void) { return bt_legal(pos(), L); }
static int at(const char *s) { return pos()->at[bt_square(s)]; }

static int has(const char *from, const char *to)
{
    int n = nlegal(), f = bt_square(from), t = bt_square(to);
    for (int i = 0; i < n; i++)
        if (L[i].from == f && L[i].to == t) return 1;
    return 0;
}

/* the move counts: every line of play to a depth */
static long perft(btpos *p, int depth)
{
    btmove m[BT_MAX_MOVES];
    int n = bt_legal(p, m);
    if (depth == 1) return n;
    long sum = 0;
    for (int i = 0; i < n; i++) {
        btpos q = *p;
        bt_make(&q, m[i]);
        sum += perft(&q, depth - 1);
    }
    return sum;
}

/* the start: 16 pieces each on their first two rows; 22 moves (the corner pieces 2, the others 3) */
static void test_start(void)
{
    bt_new(&G);
    CHECK(pos()->side == 0 && pos()->left[0] == 16 && pos()->left[1] == 16 && bt_winner(pos()) < 0);
    CHECK(at("a1") == 0 && at("h2") == 0 && at("a7") == 1 && at("h8") == 1 && at("d3") < 0 && at("e6") < 0);
    CHECK(nlegal() == 22 && has("a2", "a3") && has("a2", "b3") && !has("a1", "a2") && !has("a2", "a4"));
    btpos p = *pos();
    CHECK(perft(&p, 1) == 22 && perft(&p, 2) == 484 && perft(&p, 3) == 11132);
    CHECK(bt_script(&G, "a2a3") && pos()->side == 1 && nlegal() == 22 && has("h7", "g6") && !has("a3", "a4"));
}

/* moves: forward only; straight on only to an empty square; diagonally onto an enemy piece takes it */
static void test_moves(void)
{
    setup("d4", "d5", 0);                       /* blocked straight on: no straight capture */
    CHECK(nlegal() == 2 && !has("d4", "d5") && has("d4", "c5") && has("d4", "e5"));
    CHECK(!bt_script(&G, "d4d5") && !bt_script(&G, "d4xd5"));
    setup("d4 e5", "c5 d5", 0);                 /* a diagonal capture; never onto an own piece */
    CHECK(nlegal() == 1 + 3 && !has("d4", "e5") && has("d4", "c5") && has("e5", "e6"));
    CHECK(bt_script(&G, "d4xc5") && at("c5") == 0 && at("d4") < 0 && pos()->left[1] == 1 && G.move[0].capture);
    CHECK(pos()->side == 1 && nlegal() == 3 && has("d5", "d4") && has("d5", "e4") && !has("d5", "d6"));   /* down */
    setup("b4", "a5", 1);                       /* the edge: two ways; ebony captures too */
    CHECK(nlegal() == 2 && has("a5", "b4") && has("a5", "a4") && !has("a5", "b6"));
    CHECK(bt_script(&G, "a5xb4") && pos()->left[0] == 0);
    setup("h2", "a8", 0);                       /* the other edge */
    CHECK(nlegal() == 2 && has("h2", "h3") && has("h2", "g3"));
}

/* the ends: the far row, the last piece taken; no moves once won */
static void test_ends(void)
{
    setup("c7 a2", "a7 h8", 0);
    CHECK(bt_winner(pos()) < 0 && bt_script(&G, "c7c8") && bt_winner(pos()) == 0 && nlegal() == 0);
    setup("c7 a2", "a7 h8 b8", 0);              /* onto the home row by a capture */
    CHECK(bt_script(&G, "c7b8") && bt_winner(pos()) == 0 && pos()->left[1] == 2);
    setup("a3 h5", "b2 g7", 1);                 /* ebony reaches rank 1 */
    CHECK(bt_script(&G, "b2b1") && bt_winner(pos()) == 1);
    setup("d4 a2", "e5", 0);                    /* the last ebony piece taken */
    CHECK(bt_script(&G, "d4e5") && bt_winner(pos()) == 0 && nlegal() == 0);
    setup("d4", "e5 h8", 1);
    CHECK(bt_script(&G, "e5d4") && bt_winner(pos()) == 1);
    CHECK(!bt_script(&G, "h8h7"));              /* the game is over */
}

/* undo: a move at a time, a capture given back, to the start; bad notation */
static void test_undo(void)
{
    bt_new(&G);
    btpos start = *pos();
    CHECK(bt_script(&G, "d2d3 e7e6 d3d4 e6e5 d4e5"));
    CHECK(G.ply == 5 && pos()->left[1] == 15 && at("e5") == 0);
    CHECK(bt_undo(&G) && at("e5") == 1 && at("d4") == 0 && pos()->left[1] == 16 && pos()->side == 0);
    while (bt_undo(&G)) {}
    CHECK(G.ply == 0 && !memcmp(pos(), &start, sizeof start) && !bt_undo(&G));
    CHECK(!bt_script(&G, "d2d4") && !bt_script(&G, "d7d6") && !bt_script(&G, "d2") && !bt_script(&G, "zz"));
    CHECK(!bt_script(&G, "i2i3") && !bt_script(&G, "d2xd3x") && G.ply == 0);
    CHECK(bt_script(&G, "  a2-a3   h7h6 ") && G.ply == 2);
}

/* a whole game of first moves ends with a winner, every move forward */
static void test_game(void)
{
    bt_new(&G);
    int ok = 1;
    while (bt_winner(pos()) < 0 && G.ply < BT_MAX_PLY) {
        int n = nlegal(), fwd = bt_forward(pos()->side);
        ok &= n > 0;
        for (int i = 0; i < n; i++) ok &= (L[i].to >> 3) == (L[i].from + fwd) >> 3;
        if (!n || !bt_play(&G, L[(G.ply * 5) % n])) break;
    }
    CHECK(ok && bt_winner(pos()) >= 0 && G.ply <= 177);
}

int main(void)
{
    test_start();
    test_moves();
    test_ends();
    test_undo();
    test_game();
    printf("breakthrough: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
