/*
 * The 15 Puzzle (le taquin) for one player: the rules. Numbered tiles in a square tray with one place empty (the gap);
 * a tile beside the gap slides into it. Slide them back into order: 1, 2, 3... row by row from the top left, the gap
 * last.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 0 3 x 3, 1 4 x 4, 2 5 x 5, and the same three cut from a picture (3, 4, 5: the house's chess
 * pieces on a board, a tile each part of it, its number small in a corner).
 * The shuffle: the tiles in an order drawn by chance, made solvable by its parity. Half the orders of the tiles cannot
 * be slid back: a slide changes the order's parity (an odd number of tiles out of order, counted in reading order
 * without the gap) only when it moves a tile up or down on a board of even width, and then it also moves the gap a
 * row. So an order can be solved when (width odd) its inversions are even; (width even) its inversions plus the gap's
 * row counted from the bottom (1..) are odd. An order that cannot be solved has its first two tiles swapped, which
 * makes it solvable; an order that is already solved is drawn again.
 * Places: x + y * n from the top left; a tile is its number 1..n*n-1, 0 the gap. A move: a tap on a tile in the gap's
 * row or column slides it and every tile between it and the gap (one action, undone at once; the counter counts every
 * tile moved). The notation (script, the screenshots, the tests): the numbers of the tiles tapped, "12 8 7"; "@" lays
 * the tiles in order but the last one, beside the gap in the bottom right corner (a tap on it then solves the tray).
 */
#ifndef TAQUIN_H
#define TAQUIN_H

#include <stdint.h>

#define TQ_VARIANTS 6
#define TQ_MAX 5
#define TQ_PLACES (TQ_MAX * TQ_MAX)
#define TQ_HIST 2048

typedef struct {
    int n, picture;
    int8_t at[TQ_PLACES];                       /* the tile in each place (0: the gap) */
    int gap;
    int8_t hist[TQ_HIST];                       /* the gap's place before each action */
    int nh;
    int moves;                                  /* the tiles moved */
} tq_game;

int  tq_inversions(const int8_t *at, int n);
int  tq_solvable(const int8_t *at, int n);
void tq_shuffle(int n, uint32_t seed, int8_t *at);
void tq_new(tq_game *g, int variant, uint32_t seed);
void tq_load(tq_game *g, int n, const int8_t *at);
int  tq_line(const tq_game *g, int place);      /* the tiles a tap on place would slide (0: not in line) */
int  tq_tap(tq_game *g, int place);             /* slides them: how many; 0: not allowed */
int  tq_undo(tq_game *g);                       /* the last action taken back: the place the gap went back to;
                                                 * -1: none */
int  tq_solved(const tq_game *g);
int  tq_place_of(const tq_game *g, int tile);
int  tq_script(tq_game *g, const char *moves);

#endif
