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
 * The par: the fewest tiles moved, found by an iterative deepening search (IDA*) whose bound is each tile's distance
 * from its place (Manhattan) plus the linear conflicts (two tiles of their own row or column in the wrong order: one
 * of them must leave it; counted exactly, by the longest run already in order). Always exact on 3 x 3; on a random
 * 4 x 4 or 5 x 5 tray the exact search may take too long, so a weighted one (the bound counted 1.5 to 4 times: faster,
 * maybe longer) gives the best way it finds in time, and that is the par (0 when even that fails).
 * The levels: 150 of our own in six chapters (3 x 3, its picture, 4 x 4, its picture, 4 x 4 longer, 5 x 5), made by
 * tools/taquin_gen.c (taquin_levels.h): trays slid out of order by chance, each one's exact par checked by the tests.
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
#define TQ_LEVELS 150
#define TQ_CHAPTERS 6

typedef struct {
    int n, picture;
    int8_t at[TQ_PLACES];                       /* the tile in each place (0: the gap) */
    int gap;
    int8_t hist[TQ_HIST];                       /* the gap's place before each action */
    int nh;
    int moves;                                  /* the tiles moved */
    int par;                                    /* the fewest tiles moved from the start (0: not known) */
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
/* the least the tiles must move: Manhattan distances plus linear conflicts */
int  tq_bound(const int8_t *at, int n);
/* a way to order the tiles, by IDA*, the bound counted wq quarters (4: the shortest way; more: faster, maybe longer),
 * looking at no more than `budget` positions: its length; -1: none within the budget (*looked: the positions seen) */
int  tq_search(const int8_t *at, int n, int wq, long budget, long *looked);
int  tq_par(const tq_game *g);                 /* the par of the tray as it is: exact, else the best found in time */
/* the levels: "n:123|4.5|786" ('p:' cut from the picture), the tiles 1..9 a..o, '.' the gap; their pars */
int  tq_parse(tq_game *g, const char *level);
extern const char *const tq_levels[TQ_LEVELS];
extern const uint8_t tq_level_par[TQ_LEVELS];
int  tq_level(tq_game *g, int level);           /* 0: none */

#endif
