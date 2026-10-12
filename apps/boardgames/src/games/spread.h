/*
 * Spread for two players on one device: a 7 x 7 board, each player's two pieces in opposite corners. A move is a
 * clone (a new piece on an empty cell next to one of yours, diagonals too) or a jump (one of your pieces to an empty
 * cell two cells away, leaving its cell). Then every enemy piece next to the cell reached becomes yours.
 * The end: the board full, or the player to move has no move (the other player then takes every empty cell); the most
 * pieces win (equal: a draw). Also, after 50 moves in a row that added no piece (jumps), the pieces are counted.
 * Boards: plain, or with a few blocked cells (SPD_BOARDS).
 * No SDL; tested by tests/spread_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 ivory (moves first; a1 and g7), 1 ebony (a7 and g1). Cells: x + 7 * y, x the column a..g from player 0's
 * left, y the row 1..7 from their side. The notation: a move is its two cells, "a1b2" (a clone), "a1c3" (a jump).
 */
#ifndef SPREAD_H
#define SPREAD_H

#include <stdint.h>

#define SPD_N 7
#define SPD_CELLS (SPD_N * SPD_N)
#define SPD_AT(x, y) ((y) * SPD_N + (x))
#define SPD_BOARDS 4                            /* plain, four blocks, a cross, four gates */
#define SPD_IDLE 50                             /* moves in a row without a new piece: the count */
#define SPD_MAX_PLY 2600                        /* 49 clones at most, 50 moves apart */
#define SPD_MAX_MOVES 1200                      /* 49 cells, 24 targets each, at most */

enum { SPD_EMPTY, SPD_P0, SPD_P1, SPD_BLOCK };  /* a cell: empty, a player's piece (player + 1), blocked */
enum { SPD_PLAY, SPD_WIN0, SPD_WIN1, SPD_DRAW };

typedef struct {
    uint8_t c[SPD_CELLS];
    uint8_t side;                               /* the player to move */
    uint8_t idle;                               /* moves in a row that added no piece */
} spdpos;

typedef struct { uint8_t from, to; } spdmove;   /* one cell apart: a clone; two: a jump */

typedef struct {
    spdpos pos[SPD_MAX_PLY + 1];
    spdmove move[SPD_MAX_PLY];
    int ply, board;
} spdgame;

int  spd_dist(int a, int b);                    /* cells apart (diagonals count one) */
int  spd_legal(const spdpos *p, spdmove *out);  /* the moves of the side to move; their count */
int  spd_can_move(const spdpos *p, int player);
/* the enemy pieces a move would turn (bit k: cell k) */
uint64_t spd_turns(const spdpos *p, spdmove m);
void spd_make(spdpos *p, spdmove m);
int  spd_count(const spdpos *p, int player);    /* the player's pieces on the board */
int  spd_cell(const char *s);                   /* "a1" -> its cell; -1: not one */

void spd_new(spdgame *g, int board);
int  spd_play(spdgame *g, spdmove m);           /* 0: not legal, or the game is over */
int  spd_undo(spdgame *g);
int  spd_status(const spdgame *g);              /* SPD_* */
/* once the game is over: the player who takes the empty cells (the other one had no move), else -1 */
int  spd_filler(const spdgame *g);
int  spd_score(const spdgame *g, int player);   /* the pieces, and the empty cells for the filler */
int  spd_script(spdgame *g, const char *s);     /* "a1b2 g1f3": 0 at the first that is not legal */
#define spd_pos(g) (&(g)->pos[(g)->ply])

#endif
