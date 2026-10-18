/*
 * Ring and Fork for RetroStone Board: the rules (no SDL). A hexagonal board of hexagonal cells, 6 or 8 cells a side;
 * one stone a turn on an empty cell; the first to make one of three shapes with a group of stones wins:
 *   a ring:   a loop of stones round at least one cell (empty or not, either player's: a solid patch of seven of one's
 *             stones is a ring round its middle one); a cell on the board's rim is never inside a ring;
 *   a bridge: a group touching two of the six corner cells;
 *   a fork:   a group touching three of the six sides (a corner cell belongs to no side).
 * A full board without a shape: a draw. The swap (a variant): on the second move only, ebony may take ivory's first
 * stone instead of placing one (it turns ebony where it stands); then ivory plays. Tested by tests/havannah_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Players: 0 ivory (plays first), 1 ebony; the turn is the move count's. Cells: axial coordinates kept in a square of
 * HV_W x HV_W, x the column (0..2n-2), y the row from the top (0..2n-2): cell y * HV_W + x, on the board when
 * n - 1 <= x + y <= 3 (n - 1). With q = x - (n - 1), r = y - (n - 1), s = -q - r: the corners are where two of q, r,
 * s are +-(n - 1); the sides are where one is. The six neighbours of (x, y), in turn round it: (x+1, y), (x+1, y-1),
 * (x, y-1), (x-1, y), (x-1, y+1), (x, y+1). On the screen the cell's centre is at (q + r / 2, r * 0.866) cells.
 * Notation: a letter for the column (a, b...) and a number for the row from the top: "f1" is the top left corner of
 * 6 ("a6" the left one); "swap" (the second move only, with the swap variant). Moves are separated by spaces.
 */
#ifndef HAVANNAH_H
#define HAVANNAH_H

#include <stdint.h>

#define HV_MAXN 8
#define HV_W (2 * HV_MAXN - 1)
#define HV_SQ (HV_W * HV_W)
#define HV_SWAP (-1)
#define HV_NONE (-2)
#define HV_DRAW 2                               /* hvgame.winner: a full board, nobody */

enum { HV_EMPTY, HV_IVORY, HV_EBONY };          /* a cell: empty, or player p's stone (p + 1) */
enum { HV_RING = 1, HV_BRIDGE = 2, HV_FORK = 4 };   /* the shapes */

typedef struct {
    int n, swap;                                /* cells a side (6 or 8); the swap allowed */
    int nmoves;
    uint8_t cell[HV_SQ];
    int16_t move[HV_SQ + 1];                    /* a cell or HV_SWAP */
    int winner;                                 /* -1 playing, 0, 1, HV_DRAW */
    int how;                                    /* the winner's shapes (HV_RING | ...) */
} hvgame;

extern const int HV_DX[6], HV_DY[6];            /* the neighbours in turn round a cell */

void hv_new(hvgame *g, int n, int swap);        /* n: 6 or 8 (else 8) */
int  hv_count(int n);                           /* the cells of the board: 3 n (n - 1) + 1 */
int  hv_on(int n, int x, int y);                /* 1: (x, y) is on the board */
int  hv_valid(int n, int cell);                 /* 1: the cell is on the board */
int  hv_corner(int n, int cell);                /* its corner 0..5, else -1 */
int  hv_side(int n, int cell);                  /* its side 0..5 (not a corner), else -1 */
int  hv_around(int n, int cell, int nb[6]);     /* the neighbours on the board, in turn round it (-1 off it); count */
int  hv_turn(const hvgame *g);
int  hv_last(const hvgame *g);                  /* the last stone set down (or swapped), -1 */
int  hv_legal(const hvgame *g, int cell);
int  hv_can_swap(const hvgame *g);
int  hv_play(hvgame *g, int cell);              /* 0: not allowed */
int  hv_swap(hvgame *g);
int  hv_undo(hvgame *g);
/* the shapes player p's stones make on this board (p's stone at `at` being the newest: its group for the bridge and
 * the fork; -1: every group) */
int  hv_shapes(int n, const uint8_t *cell, int p, int at);
/* 1: player p's stones close a ring; inside (may be NULL): 1 for the cells inside one */
int  hv_ring(int n, const uint8_t *cell, int p, uint8_t *inside);
/* the winner's shapes to light: their stones (a ring's loop, the fewest stones of a bridge or a fork); count */
int  hv_shape_cells(const hvgame *g, int *cells);
int  hv_cell(int n, const char *s, int *len);   /* "c4": the cell; "swap": HV_SWAP; else HV_NONE */
int  hv_name(int cell, char *s, int cap);
int  hv_script(hvgame *g, const char *s);       /* plays "f6 e7 ..."; 0: a bad or illegal move */

/* the bot (havannah_bot.c, bots.h), and what the screen (havannah.c) gives it; a headless test defines those two */
const hvgame *havannah_live(void);
int havannah_show(const char *move);
struct ai_stats;
int hvb_choose(const hvgame *g, int level, uint64_t seed, struct ai_stats *stats);   /* a cell, HV_SWAP, HV_NONE */
int hvb_wins(const hvgame *g, int cell);           /* the bot's own quick test of a winning stone (the tests) */

#endif
