/*
 * Lines of Action for two players on one device: the rules (the moves, the captures, the connected groups, the end).
 * No engine, no clock.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The board: 8 x 8 squares, numbered as in chess from player 0's side: 0 = a1, 7 = h1, 56 = a8, 63 = h8
 * (rank * 8 + file). A square holds 0 (empty) or its piece's player + 1. Player 0 (ebony) moves first and starts on
 * b1..g1 and b8..g8; player 1 (ivory) on a2..a7 and h2..h7.
 * A move: a piece goes in a straight line (along a row, a column or a diagonal) exactly as many squares as there are
 * pieces, of both players and itself included, on that whole line. It may pass over its own pieces, never over the
 * other player's; it may land on an enemy piece (taking it), never on its own.
 * The end: a player whose pieces all touch as one group (the 8 squares around a piece; a single piece is a group) wins.
 * A move that joins both players' groups at once wins for the player who moved. A player with no legal move passes
 * (the turn goes back to the other player, by itself); if neither can move, or after LOA_MAX_PLY moves, a draw.
 * Notation (script, the screenshots and the tests): "b1-b3", "a2xc4" (a capture; "b1b3" is read too), separated by
 * spaces: "b1-d3 a6-c6 g8xd5".
 */
#ifndef LOA_H
#define LOA_H

#include <stdint.h>

#define LOA_SQUARES 64
#define LOA_PIECES 12                           /* each player's, at the start */
#define LOA_MAX_MOVES 96                        /* 12 pieces, 8 directions each at most */
#define LOA_MAX_PLY 1000                        /* a game this long ends in a draw */

typedef struct { int8_t from, to, capture; } lmove;   /* capture: 1 when the move takes a piece */

enum { LOA_PLAY, LOA_WIN0, LOA_WIN1, LOA_DRAW };

typedef struct {
    uint8_t at[LOA_SQUARES];                    /* 0 empty, else player + 1 */
    uint8_t side;                               /* who moves */
    uint8_t passed;                             /* 1: the player before had no move (the turn came back) */
    uint8_t result;                             /* LOA_* */
} lpos;

int  loa_square(const char *s);                 /* "e4": its square; -1 */
void loa_start(lpos *p);
int  loa_line(const lpos *p, int s, int dx, int dy);   /* the pieces on the whole line through s, direction (dx, dy) */
int  loa_legal(const lpos *p, int side, lmove *out);   /* side's legal moves (whatever the result); their count */
uint64_t loa_group(const lpos *p, int s);       /* the squares of the group of the piece on s (8-neighbours) */
int  loa_joined(const lpos *p, int player);     /* 1: all the player's pieces are one group */
void loa_make(lpos *p, lmove m);                /* plays a (legal) move: the result, the next player, a pass */

/* A game: every position since the start, so undo is free. */
typedef struct {
    lpos pos[LOA_MAX_PLY + 1];
    lmove move[LOA_MAX_PLY];
    int ply;
} lgame;
void loa_new(lgame *g);
int  loa_play(lgame *g, lmove m);               /* 0: not a legal move (or the game is over) */
int  loa_undo(lgame *g);                        /* 0: nothing to undo */
/* the move written at s ("b1-b3", "a2xc4", "b1b3"), among legal: its index, -1 if none; *used: the characters read */
int  loa_find(const lmove *legal, int n, const char *s, int *used);
#define loa_pos(g) (&(g)->pos[(g)->ply])

#endif
