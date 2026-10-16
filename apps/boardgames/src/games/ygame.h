/*
 * Y's rules for RetroStone Board (no SDL): a triangle of hexagonal cells (9, 13 or 19 a side), one stone a turn on an
 * empty cell, the first to join all three sides with one group of stones wins (a corner cell is on both its sides;
 * there is never a draw), the swap on the second move. Tested by tests/ygame_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 ivory (plays first), 1 ebony. Cells: row r from the top (the apex, one cell) down to the bottom side (n
 * cells), c from the left in its row (0..r): index r (r + 1) / 2 + c. A cell's six neighbours: left and right in its
 * row, (c - 1, r - 1) and (c, r - 1) above it, (c, r + 1) and (c + 1, r + 1) below it. The sides: the left one (c = 0),
 * the right one (c = r), the bottom one (r = n - 1). Notation: a letter for c (a, b, ...) and a number for the row
 * from the top: "a1" is the apex, "a13" and "m13" the bottom corners of 13. "swap": the second move only, ebony takes
 * ivory's first stone instead of placing one: it turns ebony where it stands (both players join the same sides).
 * Then ivory plays: the turn is always the move count's.
 */
#ifndef YGAME_H
#define YGAME_H

#include <stdint.h>

#define YG_MAX 19
#define YG_CELLS (YG_MAX * (YG_MAX + 1) / 2)
#define YG_SWAP (-1)
#define YG_NONE (-2)

enum { YG_EMPTY, YG_IVORY, YG_EBONY };          /* a cell: empty, or player p's stone (p + 1) */
enum { YG_LEFT = 1, YG_RIGHT = 2, YG_BOTTOM = 4, YG_ALL = 7 };   /* the sides a cell or a group touches */

typedef struct {
    int n, nmoves;
    uint8_t cell[YG_CELLS];
    int move[YG_CELLS + 1];                     /* a cell or YG_SWAP (at most a move a cell, and the swap) */
    int winner;                                 /* 0, 1, or -1: playing */
} ygame;

void yg_new(ygame *g, int n);                   /* n: 9, 13 or 19 (else 13) */
int yg_count(int n);                            /* the cells of a board of side n */
int yg_index(int c, int r);                     /* the cell (c, r) */
int yg_row(int cell);
int yg_col(int cell);
int yg_turn(const ygame *g);                    /* the player to move */
int yg_last(const ygame *g);                    /* the cell of the last stone set down (or swapped); -1 */
int yg_around(int n, int cell, int nb[6]);      /* the neighbours on the board (2 to 6) */
int yg_sides(int n, int cell);                  /* the sides the cell is on (YG_LEFT | ...; a corner: two) */
int yg_group(const ygame *g, int cell, int *cells, int *count);   /* the group through a stone: its sides; cells */
int yg_legal(const ygame *g, int cell);         /* 1: a stone may go there */
int yg_can_swap(const ygame *g);                /* 1: the second move, ebony may swap */
int yg_play(ygame *g, int cell);                /* 0: not allowed */
int yg_swap(ygame *g);                          /* 0: not allowed */
int yg_undo(ygame *g);                          /* 0: nothing to undo */
int yg_chain(const ygame *g, int *cells);       /* the winner's Y: the fewest of its stones joining the three sides */
int yg_cell(int n, const char *s, int *len);    /* "c4": the cell, "swap": YG_SWAP, else YG_NONE; len: its length */
int yg_name(int cell, char *s, int cap);        /* a cell's notation ("c4"); its length */
int yg_script(ygame *g, const char *s);         /* plays "a1 b3 swap ..."; 0: a bad or illegal move */

/* the bot (ygame_bot.c, bots.h), and what the screen (ygame.c) gives it; a headless test defines those two */
const ygame *ygame_live(void);                  /* the game on the screen, read-only: the bot copies it at entry */
int ygame_show(const char *move);               /* main thread: "c4" or "swap" as a tap does (animated); 0: not legal */
struct ai_stats;
int ygb_choose(const ygame *g, int level, uint64_t seed, struct ai_stats *stats);   /* a cell, YG_SWAP, YG_NONE */

#endif
