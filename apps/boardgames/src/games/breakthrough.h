/*
 * Breakthrough for two players on one device: the rules (moves, captures, the end). No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 8 x 8 squares, numbered as in chess from player 0's side: 0 = a1, 7 = h1, 56 = a8, 63 = h8
 * (rank * 8 + file). A square holds -1 (empty) or its piece's player. Player 0 (ivory) starts on ranks 1 and 2 and
 * moves up the board, first; player 1 (ebony) starts on ranks 7 and 8 and moves down.
 * A move: one square forward, straight or diagonally, to an empty square; or one square diagonally forward onto an
 * enemy piece, taking it (never straight forward). A player wins by reaching the other's home row (rank 8 for
 * player 0, rank 1 for player 1), or by taking all the other's pieces. There are no draws, and a player with pieces
 * always has a move (their most advanced piece can always go forward or take).
 * Notation (script, the screenshots and the tests): "a2a3" (or "a2-a3", "a2xb3"), separated by spaces:
 * "a2a3 b7b6 a3a4".
 */
#ifndef BREAKTHROUGH_H
#define BREAKTHROUGH_H

#include <stdint.h>

#define BT_SQUARES 64
#define BT_PIECES 16                            /* each player's, at the start */
#define BT_MAX_MOVES 48                         /* 16 pieces, 3 ways each at most */
#define BT_MAX_PLY 192                          /* more than a game can last (every move goes forward: 177 at most) */

typedef struct { int8_t from, to, capture; } btmove;    /* capture: 1 when the move takes a piece */

typedef struct {
    int8_t at[BT_SQUARES];
    uint8_t side;                               /* who moves */
    uint8_t left[2];                            /* each player's pieces on the board */
} btpos;

int  bt_square(const char *s);                  /* "e4": its square; -1 */
int  bt_forward(int player);                    /* +8 (player 0, up) or -8 (player 1, down) */
void bt_start(btpos *p);
int  bt_winner(const btpos *p);                 /* the player who has won; -1 while the game goes on */
int  bt_legal(const btpos *p, btmove *out);     /* the legal moves (none once the game is won); their count */
void bt_make(btpos *p, btmove m);               /* plays a (legal) move */

/* A game: every position since the start, so undo is free. */
typedef struct {
    btpos pos[BT_MAX_PLY + 1];
    btmove move[BT_MAX_PLY];
    int ply;
} btgame;
void bt_new(btgame *g);
int  bt_play(btgame *g, btmove m);              /* 0: not a legal move */
int  bt_undo(btgame *g);                        /* 0: nothing to undo */
int  bt_script(btgame *g, const char *moves);   /* plays moves in the notation above; 0: one is bad */
#define bt_pos(g) (&(g)->pos[(g)->ply])

#endif
