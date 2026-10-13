/*
 * Hex's rules for RetroStone Board (no SDL): a rhombus of hexagonal cells (7 x 7, 9 x 9, 11 x 11), one stone a turn on
 * an empty cell, the first to join their two sides wins (there is never a draw), the swap on the second move. Tested by
 * tests/hex_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 ivory (plays first, joins the top and the bottom rows), 1 ebony (joins the left and the right columns).
 * Cells: c + r * n, c the column from the left, r the row from the top. A cell's six neighbours: left and right in its
 * row, the two above it (c, c + 1) and the two below it (c - 1, c). Notation: a letter for the column (a, b, ...) and a
 * number for the row from the top: "a1" is the top left corner. "swap": the second move only, ebony takes ivory's first
 * stone instead of placing one; it becomes an ebony stone on its mirror cell (c, r) -> (r, c), across the long
 * diagonal, so that it stands for ebony as it stood for ivory. Then ivory plays: the turn is always the move count's.
 */
#ifndef HEX_H
#define HEX_H

#include <stdint.h>

#define HEX_MAX 11
#define HEX_CELLS (HEX_MAX * HEX_MAX)
#define HEX_SWAP (-1)
#define HEX_NONE (-2)

enum { HEX_EMPTY, HEX_IVORY, HEX_EBONY };      /* a cell: empty, or player p's stone (p + 1) */

typedef struct {
    int n, nmoves;
    uint8_t cell[HEX_CELLS];
    int move[HEX_CELLS + 1];                    /* a cell or HEX_SWAP (at most a move a cell, and the swap) */
    int winner;                                 /* 0, 1, or -1: playing */
} hexgame;

void hex_new(hexgame *g, int n);                /* n: 7, 9 or 11 */
int hex_turn(const hexgame *g);                 /* the player to move */
int hex_last(const hexgame *g);                 /* the cell of the last stone set down (the swap's: its mirror); -1 */
int hex_mirror(int n, int cell);                /* (c, r) -> (r, c) */
int hex_around(int n, int cell, int nb[6]);     /* the neighbours on the board (2 to 6) */
int hex_legal(const hexgame *g, int cell);      /* 1: a stone may go there */
int hex_can_swap(const hexgame *g);             /* 1: the second move, ebony may swap */
int hex_play(hexgame *g, int cell);             /* 0: not allowed */
int hex_swap(hexgame *g);                       /* 0: not allowed */
int hex_undo(hexgame *g);                       /* 0: nothing to undo */
int hex_chain(const hexgame *g, int *cells);    /* the winner's shortest chain from side to side; its length, 0: none */
int hex_cell(int n, const char *s, int *len);   /* "c4": the cell, "swap": HEX_SWAP, else HEX_NONE; len: its length */
int hex_script(hexgame *g, const char *s);      /* plays "a1 b3 swap ..."; 0: a bad or illegal move */

/* the bot (hex_bot.c, bots.h), and what the screen (hex.c) gives it; a headless test defines those two */
const hexgame *hex_live(void);                  /* the game on the screen, read-only: the bot copies it at entry */
int hex_show(const char *move);                 /* main thread: "c4" or "swap" as a tap does (animated); 0: not legal */
struct ai_stats;
int hxb_choose(const hexgame *g, int level, uint64_t seed, struct ai_stats *stats);   /* a cell, HEX_SWAP, HEX_NONE */

#endif
