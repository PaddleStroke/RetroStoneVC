/*
 * Row Shift for one player: the rules. An n x n grid of tiles (3 x 3 to 6 x 6); a whole row slides left or right, a
 * whole column up or down, and it wraps round: the tile pushed off one end comes back in at the other. Bring the
 * tiles back to their places: numbered tiles in order (1, 2, 3... row by row from the top left), or colour bands
 * (each row its own colour, shown beside it on the frame; tiles of one colour are alike, so it is easier).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A tile is its home place (x + y * n when solved); in colour bands its colour is its home row (home / n). A move: one
 * line slid by any distance (a gesture); a line slid again at once is the same move (merged; slid back home: no move
 * at all), so the moves counted are the length of the path kept. A move as a byte: line * 8 + amount, the lines 0..n-1
 * the rows (amount: steps to the right), n..2n-1 the columns (steps down), amount 1..n-1 (a step back is n-1).
 * The notation (script, the screenshots, the tests): "R2+1" row 2 one step right, "C1-2" column 1 two steps up (lines
 * from 1, from the top and the left).
 * The fewest moves: a meeting in the middle. Every position within b moves of the solved grid is kept (a hash of it,
 * its distance), found by a walk over the moves in a canonical order (a line never twice in a row; rows commute with
 * rows and columns with columns, so two rows in a row go in increasing order); then a walk from the position, a moves
 * deep, stops at the first position kept: the least depth + distance is the fewest moves, exact up to a + b.
 * Variants (the random tab): 0..3 numbered tiles 3 x 3 .. 6 x 6, 4..7 colour bands 3 x 3 .. 6 x 6: a solved grid
 * scrambled by random moves (each one changing the grid), its par the fewest moves when the search reaches it, else
 * the scramble's length (the best known). The levels: tools/loopshift_gen.c (loopshift_levels.h).
 */
#ifndef LOOPSHIFT_H
#define LOOPSHIFT_H

#include <stdint.h>

#define LSH_MIN 3
#define LSH_MAX 6
#define LSH_CELLS (LSH_MAX * LSH_MAX)
#define LSH_HIST 1024
#define LSH_SOL 48
#define LSH_VARIANTS 8
#define LSH_LEVELS 150
#define LSH_CHAPTERS 6

typedef struct {
    int n, colour;
    int8_t at[LSH_CELLS];                       /* the tile (its home place) in each place */
    int8_t start[LSH_CELLS];
    uint8_t hist[LSH_HIST];                     /* the moves made, merged */
    int nh;
    uint8_t sol[LSH_SOL];                       /* a known way from the start (the inverse of the scramble, or the
                                                 * shortest found) */
    int nsol;
    int par;                                    /* the fewest moves from the start (the best known); 0: none */
    int exact;                                  /* 1: par is proven the fewest */
} lsh_game;

static inline int lsh_line(int move) { return move >> 3; }
static inline int lsh_amount(int move) { return move & 7; }
static inline int lsh_move(int line, int amount) { return line << 3 | amount; }
static inline int lsh_inverse(int n, int move) { return lsh_move(lsh_line(move), n - lsh_amount(move)); }
/* the move as a step count with its sign, the shorter way round (n even, half way: forwards) */
static inline int lsh_signed(int n, int move)
{
    int k = lsh_amount(move);
    return 2 * k > n ? k - n : k;
}

void lsh_apply(int8_t *at, int n, int move);
int  lsh_solved_at(const int8_t *at, int n, int colour);
void lsh_init(lsh_game *g, int n, int colour);  /* solved */
int  lsh_solved(const lsh_game *g);
/* a move: 1 played (merged with the last when it slides the same line), 0 not a move of this grid */
int  lsh_play(lsh_game *g, int move);
int  lsh_undo(lsh_game *g);                     /* the move taken back, -1: none */
int  lsh_moves(const lsh_game *g);
/* a random scramble of `depth` moves, each one changing the grid (colour bands: its colours), in canonical order;
 * sol: its inverse */
void lsh_scramble(lsh_game *g, int n, int colour, int depth, uint32_t seed);
/* the fewest moves from at to the solved grid when they are at most a + b (with b moves kept from the solved side),
 * and a shortest way (path, up to max moves); -1: more than a + b. `upper`: a way of that length is known already
 * (0: none). */
int  lsh_solve(const int8_t *at, int n, int colour, int a, int b, int upper, uint8_t *path, int max);
void lsh_solve_free(void);                      /* the kept positions freed */
void lsh_depths(int n, int *a, int *b);         /* the search's depths for play (quick on a phone) */
/* the next move towards the solved grid: a shortest one when the search finds it, else on a known way (the moves made
 * taken back, then the start's known way); -1: solved */
int  lsh_hint(const lsh_game *g);
void lsh_new(lsh_game *g, int variant, uint32_t seed);   /* a random puzzle of a variant, its par found */
int  lsh_script(lsh_game *g, const char *moves);
int  lsh_parse_move(int n, const char *s, int *used);    /* "R2+1": the move, -1 bad */
/* the levels: "<n><n|c><x|b>:<tiles>:<way>" (n the side; numbers or colours; par exact or best known; the tiles 'a'..
 * by home place; the way: a line letter 'a'.. and the amount digit a move) */
int  lsh_parse(lsh_game *g, const char *level);
extern const char *const lsh_levels[LSH_LEVELS];
int  lsh_level(lsh_game *g, int level);         /* 0: none */
int  lsh_level_size(int level);                 /* its side, 0: none */

#endif
