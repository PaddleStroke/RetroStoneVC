/*
 * Tic-tac-toe for two players on one device: the rules of the two variants.
 * Classic: a 3 x 3 board; three of one player in a row (across, down or on a diagonal) wins; a full board is a draw.
 * Ultimate: nine small boards in a big one. The square played in a small board sends the other player to the small
 * board in the same place for their next move; if that board is decided, they may play in any open board. Three in a
 * row in a small board claims it (a full one without a winner is nobody's), three claimed boards in a row on the big
 * board win; once no line of the big board can still be won by anyone, the game is a draw.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Squares and boards are numbered 0..8 from the top left, row by row. A cell: classic, its square; Ultimate,
 * board * 9 + square. Player 0 (ivory, the crosses) plays first. The moves' notation (script, the screenshots, the
 * tests): squares 1 to 9 like a telephone's keys, "5 1 9"; Ultimate "board.square", "5.5 5.1 1.9".
 */
#ifndef TICTACTOE_H
#define TICTACTOE_H

#include <stdint.h>

#define TT_CELLS 81

enum { TT_PLAY, TT_WIN0, TT_WIN1, TT_DRAW };
enum { TT_OPEN = -1, TT_FULL = 2 };            /* a small board: open, claimed by player 0 or 1, or full (nobody's) */

typedef struct {
    int ultimate;
    int8_t cell[TT_CELLS];              /* -1 empty, else the player whose mark it is */
    int8_t board[9];                    /* Ultimate: each small board's state (TT_OPEN, 0, 1, TT_FULL) */
    uint8_t move[TT_CELLS];             /* the cells played, in order */
    int n;                              /* the marks made */
    int winner;                         /* -1: nobody yet */
    uint16_t line;                      /* the winning squares (classic) or boards (Ultimate): every winning line */
} tt_game;

extern const int8_t tt_lines[8][3];    /* the eight lines of a 3 x 3 board */

void tt_new(tt_game *g, int ultimate);
int  tt_cells(const tt_game *g);       /* 9 or 81 */
int  tt_legal(const tt_game *g, int cell);
int  tt_play(tt_game *g, int cell);    /* the player to move's mark; 0: not allowed (or the game is over) */
int  tt_undo(tt_game *g);              /* takes the last mark back; 0: nothing to undo */
int  tt_turn(const tt_game *g);        /* the player to move */
int  tt_status(const tt_game *g);      /* TT_* */
int  tt_target(const tt_game *g);      /* Ultimate: the small board the player to move is sent to; -1: any open one */
int  tt_claimed(const tt_game *g, int player);   /* Ultimate: the small boards a player has claimed */
int  tt_script(tt_game *g, const char *s);       /* plays "5 1 9" or "5.5 5.1"; 0: a bad move (those before stay) */

#endif
