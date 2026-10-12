/*
 * Klotski (Huarong Dao) for one player: the rules. Ten wooden blocks in a tray of 4 x 5 cells, two cells empty: the
 * great 2 x 2 block, five blocks of 1 x 2 (standing or lying), four small ones. Blocks only slide, one cell at a time,
 * into empty cells. Bring the great block to the middle of the bottom row: it leaves through the opening there.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The levels are traditional starts of the Chinese game (public domain), each with its fewest moves (the par), found
 * by a breadth-first search over every position (tests/klotski_test.c checks them). A move, as the tradition counts
 * it: one block slid as far as it goes, round corners too, before another block moves (a block slid again at once
 * stays one move). Variant 0 is the classic start, "Heng Dao Li Ma"; then the others from the fewest moves up.
 * Cells: x 0..3 from the left, y 0..4 from the top; a block's place is its top left cell. The notation (script, the
 * screenshots, the tests): a block is named by a cell it covers, its column a..d and its row 1..5 from the top, then
 * its slides, one letter a cell (u d l r): "b4r" slides the block on b4 one cell right, "a5uu" two cells up (one
 * move); "h" a hint (the first move of a shortest way, played), "=" the whole shortest way.
 */
#ifndef KLOTSKI_H
#define KLOTSKI_H

#include <stdint.h>

#define KLT_W 4
#define KLT_H 5
#define KLT_PIECES 10
#define KLT_LEVELS 14
#define KLT_HIST 1024
#define KLT_PATH 24

enum { KLT_BIG, KLT_TALL, KLT_WIDE, KLT_SMALL };

typedef struct { int8_t type, x, y; } klt_piece;

typedef struct {
    int level;
    klt_piece p[KLT_PIECES];
    int8_t hist[KLT_HIST][KLT_PIECES][2];       /* the blocks' places before each move */
    int nh;
    int last;                                   /* the block moved last (its next slide is the same move); -1 */
    int moves;
} klt_game;

const char *klt_name(int level);                /* the start's traditional name (pinyin) */
int  klt_par(int level);                        /* its fewest moves */
void klt_new(klt_game *g, int level);
int  klt_at(const klt_game *g, int x, int y);   /* the block on a cell; -1: empty */
int  klt_can(const klt_game *g, int piece, int dx, int dy);   /* one cell that way */
int  klt_step(klt_game *g, int piece, int dx, int dy);        /* slid one cell (a move, or the same move on) */
/* where a block can go in one move: reach[x + y * 4] the slides it takes (0: its own place; -1: cannot) */
void klt_reach(const klt_game *g, int piece, int8_t *reach);
/* the way there, one cell at a time: the places after each slide (path[2 * i], path[2 * i + 1]); how many; 0: none */
int  klt_path(const klt_game *g, int piece, int x, int y, int8_t *path);
int  klt_move(klt_game *g, int piece, int x, int y);          /* there in one move, by its way; 0: cannot */
int  klt_undo(klt_game *g);                     /* the last move taken back: the block; -1: none */
int  klt_solved(const klt_game *g);
/* the fewest moves left; the first one's block and place (may be NULL); -1: none (never from a level's start) */
int  klt_solve(const klt_game *g, int *piece, int *x, int *y);
int  klt_script(klt_game *g, const char *moves);

#endif
