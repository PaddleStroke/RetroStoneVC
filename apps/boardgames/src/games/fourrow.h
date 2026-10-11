/*
 * Four in a row for two players on one device: the rules (an upright board of 7 columns and 6 rows, the discs fall
 * to the lowest free hole; four of one player in a line, across, up or on a slant, wins; a full board is a draw).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Holes: column 0..6 from the left, row 0..5 from the bottom; a hole's bit in a mask: row * FR_COLS + column.
 * Player 0 (ivory) drops first. The moves' notation (script, the screenshots, the tests): the columns played, 1 to 7
 * from the left, "4 4 3 5".
 */
#ifndef FOURROW_H
#define FOURROW_H

#include <stdint.h>

#define FR_COLS 7
#define FR_ROWS 6
#define FR_HOLES (FR_COLS * FR_ROWS)
#define FR_BIT(col, row) (1ull << ((row) * FR_COLS + (col)))

enum { FR_PLAY, FR_WIN0, FR_WIN1, FR_DRAW };

typedef struct {
    int8_t hole[FR_ROWS][FR_COLS];      /* -1 empty, else the player whose disc it is */
    uint8_t height[FR_COLS];            /* the discs in each column */
    uint8_t move[FR_HOLES];             /* the columns played, in order */
    int n;                              /* the discs dropped */
    int winner;                         /* -1: nobody yet */
    uint64_t line;                      /* the winning discs (every line of four or more through the last one) */
} fr_game;

void fr_new(fr_game *g);
int  fr_play(fr_game *g, int col);      /* drops the player to move's disc: the row it lands in; -1: full, or over */
int  fr_undo(fr_game *g);               /* takes the last disc back; 0: nothing to undo */
int  fr_turn(const fr_game *g);         /* the player to move */
int  fr_status(const fr_game *g);       /* FR_* */
int  fr_left(const fr_game *g, int player);   /* the discs a player still has (21 at the start) */

#endif
