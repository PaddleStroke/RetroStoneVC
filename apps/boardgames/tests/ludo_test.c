/*
 * Ludo: the rules' test (the yard and the 6, the extra throw, three 6s, captures and safe squares, the home column and
 * the exact finish, a throw with no move, the win, undo). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/ludo.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static lgame G;
static const lpos *P = &G.pos;

/* a position: each player's four tokens (-1 the yard), the player to throw */
static void setup(const int a[4], const int b[4], int turn)
{
    ludo_new(&G);
    for (int t = 0; t < LUDO_TOKENS; t++) {
        G.pos.at[0][t] = (int8_t)a[t];
        G.pos.at[1][t] = (int8_t)b[t];
    }
    G.pos.turn = (uint8_t)turn;
}

static const int YARD[4] = {-1, -1, -1, -1};

static void test_board(void)
{
    int safe = 0;
    for (int s = 0; s < LUDO_TRACK; s++) safe += ludo_safe(s);
    CHECK(safe == 8 && ludo_safe(0) && ludo_safe(8) && ludo_safe(13) && ludo_safe(26) && !ludo_safe(1));
    CHECK(ludo_square(0, 0) == 0 && ludo_square(1, 0) == 26 && ludo_square(1, 30) == 4 && ludo_square(0, 50) == 50);
    CHECK(ludo_square(0, 51) == -1 && ludo_square(1, -1) == -1);
}

/* out of the yard on a 6 only; a 6 throws again; a non-6 passes the turn */
static void test_yard(void)
{
    ludo_new(&G);
    CHECK(P->turn == 0 && P->phase == LUDO_THROW);
    CHECK(ludo_throw(&G, 5) == LT_PASS && P->turn == 1 && P->phase == LUDO_THROW);
    CHECK(!ludo_move(&G, 0));                   /* no move now: a throw first */
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_movable(P) == 15);
    CHECK(ludo_move(&G, 2) == 1 && P->at[1][2] == 0 && P->turn == 1 && P->phase == LUDO_THROW);
    CHECK(ludo_throw(&G, 3) == LT_MOVE && ludo_movable(P) == 4);    /* only the token out moves */
    CHECK(!ludo_move(&G, 0) && ludo_move(&G, 2) == 1 && P->at[1][2] == 3 && P->turn == 0);
}

/* three 6s in a row: the third is lost, the turn ends (the first two moves stay) */
static void test_sixes(void)
{
    ludo_new(&G);
    CHECK(ludo_script(&G, "6 a 6 a"));
    CHECK(P->turn == 0 && P->at[0][0] == 6 && P->sixes == 2);
    CHECK(ludo_throw(&G, 6) == LT_SIXES && P->turn == 1 && P->sixes == 0 && P->at[0][0] == 6);
    /* a 6 with nothing to move: throw again */
    int a[4] = {LUDO_HOME, LUDO_HOME, LUDO_HOME, 53};
    setup(a, YARD, 0);
    CHECK(ludo_throw(&G, 6) == LT_AGAIN && P->turn == 0 && P->phase == LUDO_THROW);
    CHECK(ludo_throw(&G, 4) == LT_PASS && P->turn == 1);
}

/* captures: a landing sends the other's tokens home (both of a pair); not on a safe square */
static void test_capture(void)
{
    int a[4] = {10, -1, -1, -1}, b[4] = {-1, -1, -1, -1};
    b[0] = 14 + 26;                             /* player 1's token on square 14: 4 ahead of player 0's */
    b[1] = 14 + 26;
    setup(a, b, 0);
    CHECK(ludo_throw(&G, 4) == LT_MOVE && ludo_move(&G, 0) == 3);
    CHECK(P->at[0][0] == 14 && P->at[1][0] == -1 && P->at[1][1] == -1);
    b[0] = 21 + 26;                             /* square 21: a star */
    b[1] = -1;
    a[0] = 15;
    setup(a, b, 0);
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_move(&G, 0) == 1 && P->at[0][0] == 21 && P->at[1][0] == 47);
    /* on player 1's start square (26): safe, both stand there */
    a[0] = 22;
    b[0] = 0;
    setup(a, b, 0);
    CHECK(ludo_throw(&G, 4) == LT_MOVE && ludo_move(&G, 0) == 1 && P->at[1][0] == 0);
    /* a token coming out onto its start square does not capture there either */
    a[0] = 26;
    setup(a, YARD, 1);
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_move(&G, 3) == 1 && P->at[0][0] == 26 && P->at[1][3] == 0);
    /* two tokens of one player do not block: the other passes them */
    int c[4] = {20, 20, -1, -1}, d[4] = {44, -1, -1, -1};   /* player 1 at 44: square 18, two behind 20 */
    setup(c, d, 1);
    CHECK(ludo_throw(&G, 5) == LT_MOVE && ludo_move(&G, 0) == 1 && P->at[1][0] == 49 && P->at[0][0] == 20);
}

/* the home column: entered after a full lap, the exact number to the centre */
static void test_home(void)
{
    int a[4] = {48, -1, -1, -1};
    setup(a, YARD, 0);
    CHECK(ludo_throw(&G, 5) == LT_MOVE && ludo_move(&G, 0) == 1 && P->at[0][0] == 53);
    CHECK(ludo_square(0, 53) == -1);            /* in the home column: off the track, safe from all */
    CHECK(ludo_throw(&G, 4) == LT_PASS);        /* player 1 cannot move */
    CHECK(ludo_throw(&G, 4) == LT_PASS && P->at[0][0] == 53);   /* 53 + 4 > 56: no move, the turn passes */
    CHECK(ludo_throw(&G, 2) == LT_PASS);
    CHECK(ludo_throw(&G, 3) == LT_MOVE && ludo_move(&G, 0) == 1 && P->at[0][0] == LUDO_HOME && ludo_home(P, 0) == 1);
    /* a token passing the other player's start goes on round (its own lap ends at 50) */
    int b[4] = {-1, -1, -1, 24};
    setup(YARD, b, 1);
    CHECK(ludo_throw(&G, 5) == LT_MOVE && ludo_move(&G, 3) == 1 && P->at[1][3] == 29 && ludo_square(1, 29) == 3);
}

/* the win: the last token home ends the game, even on a 6 */
static void test_win(void)
{
    int a[4] = {LUDO_HOME, LUDO_HOME, LUDO_HOME, 50};
    setup(a, YARD, 0);
    CHECK(ludo_throw(&G, 6) == LT_MOVE && ludo_move(&G, 3) == 1);
    CHECK(P->winner == 0 && P->phase == LUDO_OVER && !ludo_throw(&G, 3));
}

/* undo: back to the last move's choice, the throws after it too; the throw itself stays */
static void test_undo(void)
{
    ludo_new(&G);
    CHECK(!ludo_undo(&G));
    CHECK(ludo_script(&G, "6 a 4 a 3 6 b"));    /* player 0: out, 4; player 1: 3 (no move); player 0: 6, out */
    CHECK(P->turn == 0 && P->at[0][0] == 4 && P->at[0][1] == 0 && P->throws == 4);
    CHECK(ludo_undo(&G) && P->phase == LUDO_MOVE && P->die == 6 && P->turn == 0 && P->at[0][1] == -1);
    CHECK(ludo_undo(&G) && P->phase == LUDO_MOVE && P->die == 4 && P->at[0][0] == 0 && P->throws == 2);
    CHECK(ludo_move(&G, 0) == 1 && P->at[0][0] == 4);   /* another choice: the same throw */
    CHECK(ludo_undo(&G) && ludo_undo(&G) && P->die == 6 && P->at[0][0] == -1 && P->phase == LUDO_MOVE);
    CHECK(ludo_undo(&G) && G.n == 0 && P->phase == LUDO_THROW && P->throws == 0);
    CHECK(!ludo_undo(&G));
    CHECK(!ludo_script(&G, "6 e") && !ludo_script(&G, "7"));
}

/* a whole game: player 0 always moves its first token that can; player 1 throws 1s and never leaves its yard */
static void test_game(void)
{
    static const int seq[9] = {6, 6, 1, 6, 2, 3, 6, 4, 5};
    ludo_new(&G);
    int k = 0, guard = 0;
    while (P->phase != LUDO_OVER && guard++ < 4000) {
        if (P->phase == LUDO_THROW) {
            CHECK(ludo_throw(&G, P->turn ? 1 : seq[k++ % 9]));
        } else {
            int m = ludo_movable(P), t = 0;
            while (!(m >> t & 1)) t++;
            CHECK(ludo_move(&G, t));
        }
    }
    CHECK(P->winner == 0 && P->phase == LUDO_OVER && ludo_home(P, 0) == 4 && ludo_home(P, 1) == 0);
    while (ludo_undo(&G)) {}                    /* undo all the way */
    CHECK(G.n == 0 && P->throws == 0 && P->at[0][0] == -1 && P->winner == -1);
}

int main(void)
{
    test_board();
    test_yard();
    test_sixes();
    test_capture();
    test_home();
    test_win();
    test_undo();
    test_game();
    printf("ludo: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
