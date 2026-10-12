/*
 * Same Game for one player: the rules. A grid full of coloured blocks. Remove a group of two or more blocks of one
 * colour joined side by side: the blocks above fall into the gaps, and empty columns close up to the left. A group of
 * n blocks scores (n - 2)^2; clearing the whole board adds SM_BONUS. The game ends when no group is left.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 0 10 x 10 blocks in 3 colours, 1 15 x 10 in 4, 2 20 x 12 in 5. Every board can be cleared: it is made
 * backwards from the empty board, a group at a time put back where its removal would leave the board as it was (a
 * group of two or three, upright or across, never touching its own colour; a new column when it fills one), until the
 * grid is full; the way back is kept (the hint follows it while the board is still on it).
 * Places: x from the left, y from the bottom (0..). The notation (script, the screenshots, the tests): a removal is the
 * place tapped, its column a letter and its row a number from the bottom, "c1 a4"; "=k" plays the next k removals of
 * the way that clears the board (while the board is on it; "=": all of it); "~" takes the first group from the bottom
 * left, again and again until none is left (a careless game).
 */
#ifndef SAMEGAME_H
#define SAMEGAME_H

#include <stdint.h>

#define SM_VARIANTS 3
#define SM_W 20
#define SM_H 12
#define SM_HIST (SM_W * SM_H / 2 + 1)
#define SM_BONUS 1000

typedef struct { int8_t c[SM_W][SM_H]; } sm_board;   /* each column from the bottom; 0: empty */

typedef struct {
    int w, h, colours;
    sm_board b, start;
    int score, removed;                         /* removed: the groups taken */
    struct { sm_board b; int score; } hist[SM_HIST];
    int nh;
    /* the way that clears the board from the start: each removal's place, in turn */
    uint8_t way[SM_HIST][2];
    int nway;
} sm_game;

void sm_new(sm_game *g, int variant, uint32_t seed);
/* the group at (x, y): its places marked in mask (when not NULL, [x][y] 1), its size (0: no block there) */
int  sm_group(const sm_board *b, int w, int h, int x, int y, uint8_t mask[SM_W][SM_H]);
int  sm_points(int n);                          /* (n - 2)^2 */
int  sm_remove(sm_game *g, int x, int y);       /* the blocks removed (0: not a group of two or more) */
int  sm_undo(sm_game *g);
int  sm_over(const sm_game *g);                 /* no group left */
int  sm_cleared(const sm_game *g);
int  sm_blocks(const sm_game *g);
/* the hint: a group to take, (x, y) one of its places; 2: it keeps the board clearable (on the kept way, or a way
 * found from here); 1: no way to clear found, the group scoring most; 0: no group */
int  sm_hint(const sm_game *g, int *x, int *y);
/* a way to clear b from here, found by a search of at most `cap` boards: its first group's place; 1 found, 0 none
 * (or not within cap) */
int  sm_solve(const sm_board *b, int w, int h, int cap, int *x, int *y);
int  sm_script(sm_game *g, const char *moves);

#endif
