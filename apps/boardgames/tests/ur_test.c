/*
 * The Royal Game of Ur: the rules' test (the paths, entering, the rosettes' extra throw, the safe central rosette,
 * captures in the shared row only, no landing on one's own piece, the exact bear-off, a 0 and a throw with no move,
 * the win, undo). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/ur.h"

static int checks, failed;
#define CHECK(c) do { checks++; if (!(c)) { failed++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static ugame G;
static const upos *P = &G.pos;

/* a position: the places of each player's pieces on the board (0-terminated), the rest waiting; the player to throw */
static void setup(const int *a, const int *b, int turn)
{
    ur_new(&G);
    const int *l[2] = {a, b};
    for (int pl = 0; pl < 2; pl++)
        for (int i = 0; l[pl] && l[pl][i]; i++) {
            G.pos.n[pl][0]--;
            G.pos.n[pl][l[pl][i]]++;
        }
    G.pos.turn = (uint8_t)turn;
}

/* the paths: 14 squares each, each step to a neighbouring square; the middle row shared, the others each player's */
static void test_paths(void)
{
    int seen[3][8] = {{0}}, c, r, pc = 0, pr = 0;
    for (int pl = 0; pl < 2; pl++)
        for (int s = 1; s <= 14; s++) {
            CHECK(ur_cell(pl, s, &c, &r));
            seen[r][c]++;
            if (s > 1) CHECK(abs(c - pc) + abs(r - pr) == 1);
            pc = c, pr = r;
            CHECK((r == 1) == ur_shared(s));
            CHECK(r == 1 || r == (pl ? 0 : 2));
        }
    int squares = 0;
    for (r = 0; r < 3; r++)
        for (c = 0; c < 8; c++) {
            squares += seen[r][c] > 0;
            CHECK(seen[r][c] == (r == 1 ? 2 : (c == 4 || c == 5) ? 0 : 1));   /* the gap: cols 4, 5 */
        }
    CHECK(squares == 20);
    CHECK(!ur_cell(0, 0, &c, &r) && !ur_cell(1, 15, &c, &r));
    /* the five rosettes: the corners of the 4-square ends, of the 2-square ends, the middle of the shared row */
    int ros[3][8] = {{0}};
    for (int pl = 0; pl < 2; pl++)
        for (int s = 1; s <= 14; s++)
            if (ur_rosette(s) && ur_cell(pl, s, &c, &r)) ros[r][c] = 1;
    CHECK(ros[0][0] && ros[2][0] && ros[0][6] && ros[2][6] && ros[1][3]);
    CHECK(ros[0][0] + ros[0][6] + ros[2][0] + ros[2][6] + ros[1][3] == 5 && !ros[1][0] && !ros[2][7]);
}

/* entering: a waiting piece goes to the place thrown; the turn passes (no rosette) */
static void test_enter(void)
{
    ur_new(&G);
    CHECK(P->turn == 0 && P->phase == UR_THROW && P->n[0][0] == 7 && P->n[1][0] == 7);
    CHECK(!ur_move(&G, 0));                     /* a throw first */
    CHECK(ur_throw(&G, 3) == UT_MOVE && ur_movable(P) == 1 && ur_dest(P, 0) == 3);
    CHECK(ur_move(&G, 0) == 1 && P->n[0][3] == 1 && P->n[0][0] == 6 && P->turn == 1 && P->phase == UR_THROW);
    /* the other player's private squares are their own: the same place number, another square, no capture */
    CHECK(ur_throw(&G, 3) == UT_MOVE && ur_move(&G, 0) == 1 && P->n[1][3] == 1 && P->n[0][3] == 1);
    CHECK(!ur_script(&G, "5") && !ur_script(&G, "2 p"));
}

/* a 0 is lost; a throw no piece can use is lost */
static void test_pass(void)
{
    ur_new(&G);
    CHECK(ur_throw(&G, 0) == UT_PASS && P->turn == 1 && P->phase == UR_THROW && P->throws == 1);
    int a[] = {1, 2, 3, 4, 0};                  /* four on their own squares, three waiting: a 1 is blocked */
    setup(a, NULL, 0);
    G.pos.n[0][0] = 0;                          /* none waiting: the other three borne off */
    G.pos.n[0][UR_OFF] = 3;
    CHECK(ur_throw(&G, 1) == UT_MOVE && ur_movable(P) == 1 << 4);   /* only the front one moves */
    setup(a, NULL, 0);
    G.pos.n[0][0] = 0;
    G.pos.n[0][UR_OFF] = 3;
    G.pos.n[0][5] = 1;                          /* now 5 is taken too: */
    G.pos.n[0][UR_OFF] = 2;
    CHECK(ur_throw(&G, 1) == UT_MOVE && ur_movable(P) == 1 << 5);
    int b[] = {1, 2, 3, 4, 5, 6, 0};
    setup(b, NULL, 0);
    G.pos.n[0][0] = 0;
    G.pos.n[0][UR_OFF] = 1;
    G.pos.n[0][6] = 0;                          /* 1..5 and 7 */
    G.pos.n[0][7] = 1;
    CHECK(ur_throw(&G, 1) == UT_MOVE && ur_movable(P) == (1 << 5 | 1 << 7));
}

/* rosettes: another throw (each player's own and the central one); elsewhere the turn passes */
static void test_rosettes(void)
{
    ur_new(&G);
    CHECK(ur_script(&G, "4 a") && P->turn == 0 && P->phase == UR_THROW && P->n[0][4] == 1);
    CHECK(ur_script(&G, "4 e") && P->turn == 0 && P->n[0][8] == 1);   /* 4 -> 8: the central rosette */
    CHECK(ur_script(&G, "2 a") && P->turn == 1);
    int a[] = {11, 0};
    setup(a, NULL, 0);
    CHECK(ur_script(&G, "3 l") && P->n[0][14] == 1 && P->turn == 0);  /* 11 -> 14 */
    /* bearing off from the last rosette: no extra throw */
    CHECK(ur_script(&G, "1 o") && P->n[0][UR_OFF] == 1 && P->turn == 1);
}

/* captures: in the shared row, not on the central rosette */
static void test_capture(void)
{
    int a[] = {4, 0}, b[] = {6, 0};
    setup(a, b, 0);
    CHECK(ur_throw(&G, 2) == UT_MOVE && ur_takes(P, 4) && ur_move(&G, 4) == 2);
    CHECK(P->n[0][6] == 1 && P->n[1][6] == 0 && P->n[1][0] == 7 && P->turn == 1);
    /* the central rosette: the piece there is safe, the move is not allowed */
    int c[] = {5, 0}, d[] = {8, 0};
    setup(c, d, 0);
    G.pos.n[0][0] = 0;                          /* nothing else to move */
    G.pos.n[0][UR_OFF] = 6;
    CHECK(ur_throw(&G, 3) == UT_PASS && P->n[1][8] == 1 && P->turn == 1);
    setup(c, d, 0);
    CHECK(ur_throw(&G, 3) == UT_MOVE && ur_dest(P, 5) == -1 && !ur_move(&G, 5) && ur_movable(P) == 1);
    /* passing over pieces is allowed; landing beyond the central rosette's piece */
    setup(c, d, 0);
    CHECK(ur_throw(&G, 4) == UT_MOVE && ur_move(&G, 5) == 1 && P->n[0][9] == 1 && P->n[1][8] == 1);
    /* the piece on the central rosette, once it leaves, can be hit again */
    int e[] = {9, 0}, f[] = {8, 0};
    setup(e, f, 1);
    CHECK(ur_throw(&G, 1) == UT_MOVE && ur_takes(P, 8) && ur_move(&G, 8) == 2 && P->n[0][9] == 0 && P->n[0][0] == 7);
    /* the last two squares are private again: no capture there */
    int g[] = {12, 0}, h[] = {13, 0};
    setup(g, h, 0);
    CHECK(ur_throw(&G, 1) == UT_MOVE && !ur_takes(P, 12) && ur_move(&G, 12) == 1 && P->n[1][13] == 1);
    /* landing on one's own piece: not allowed */
    int k[] = {6, 7, 0};
    setup(k, NULL, 0);
    CHECK(ur_throw(&G, 1) == UT_MOVE && ur_dest(P, 6) == -1 && ur_dest(P, 7) == 8);
}

/* bearing off: the exact number only */
static void test_bear_off(void)
{
    int a[] = {13, 0};
    setup(a, NULL, 0);
    G.pos.n[0][0] = 0;
    G.pos.n[0][UR_OFF] = 6;
    CHECK(ur_throw(&G, 3) == UT_PASS && P->n[0][13] == 1);   /* 13 + 3 > 15 */
    CHECK(ur_throw(&G, 0) == UT_PASS);
    CHECK(ur_throw(&G, 2) == UT_MOVE && ur_dest(P, 13) == UR_OFF);
    CHECK(ur_move(&G, 13) == 1 && P->winner == 0 && P->phase == UR_OVER && !ur_throw(&G, 1));
    int b[] = {12, 0};                          /* from 12: a 3 */
    setup(NULL, b, 1);
    CHECK(ur_throw(&G, 3) == UT_MOVE && ur_dest(P, 12) == UR_OFF && ur_move(&G, 12) == 1);
    CHECK(P->n[1][UR_OFF] == 1 && P->n[1][0] == 6 && P->winner == -1 && P->turn == 0);
}

/* undo: back to the last move's choice, the throws after it too */
static void test_undo(void)
{
    ur_new(&G);
    CHECK(!ur_undo(&G));
    CHECK(ur_script(&G, "2 a 0 3 c"));          /* 0: 2; 1: a 0; 0: 2 -> 5 */
    CHECK(P->turn == 1 && P->n[0][5] == 1 && P->throws == 3);
    CHECK(ur_undo(&G) && P->phase == UR_MOVE && P->roll == 3 && P->turn == 0 && P->n[0][2] == 1);
    CHECK(ur_undo(&G) && P->phase == UR_MOVE && P->roll == 2 && P->n[0][0] == 7 && P->throws == 1);
    CHECK(ur_undo(&G) && G.n == 0 && P->phase == UR_THROW && P->throws == 0);
    CHECK(!ur_undo(&G));
}

/* whole games: each side moves the furthest piece that can, throws from a fixed sequence; someone wins, the pieces
 * always add up to seven */
static void test_game(void)
{
    static const int seq[16] = {2, 1, 3, 2, 0, 2, 4, 2, 1, 3, 2, 2, 1, 2, 3, 2};
    for (int start = 0; start < 16; start++) {
        ur_new(&G);
        int k = start, guard = 0, ok = 1;
        while (P->phase != UR_OVER && guard++ < 3000) {
            if (P->phase == UR_THROW) {
                ok &= ur_throw(&G, seq[k++ % 16]) != 0;
                k += k % 5 == 0;
            } else {
                int m = ur_movable(P), f = UR_OFF - 1;
                while (!(m >> f & 1)) f--;
                ok &= ur_move(&G, f) != 0;
            }
            for (int pl = 0; pl < 2; pl++) {
                int sum = 0;
                for (int s = 0; s <= UR_OFF; s++) sum += P->n[pl][s];
                ok &= sum == UR_PIECES;
                for (int s = 1; s < UR_OFF; s++) ok &= P->n[pl][s] <= 1;
            }
            for (int s = UR_SHARED_LO; s <= UR_SHARED_HI; s++) ok &= !(P->n[0][s] && P->n[1][s]);
        }
        CHECK(ok && P->phase == UR_OVER && P->n[P->winner][UR_OFF] == UR_PIECES);
        while (ur_undo(&G)) {}
        CHECK(G.n == 0 && P->n[0][0] == 7 && P->n[1][0] == 7 && P->winner == -1);
    }
}

int main(void)
{
    test_paths();
    test_enter();
    test_pass();
    test_rosettes();
    test_capture();
    test_bear_off();
    test_undo();
    test_game();
    printf("ur: %d checks, %d failed\n", checks, failed);
    return failed != 0;
}
