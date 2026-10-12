/*
 * Klotski (Huarong Dao) for one player: the rules. Wooden blocks in a tray of 4 x 5 cells, two cells empty: the great
 * 2 x 2 block, blocks of 1 x 2 (standing or lying) and small ones. Blocks only slide, one cell at a time, into empty
 * cells. Bring the great block to the middle of the bottom row: it leaves through the opening there.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A move, as the tradition counts it: one block slid as far as it goes, round corners too, before another block moves
 * (a block slid again at once stays one move). The par: the fewest moves, found by a breadth-first search over the
 * positions (blocks of one kind alike).
 * The trays: the great block, then 9 or 10 others (six blocks of 1 x 2 and two small ones; five and four, the
 * classic; four and six). The puzzles are made the same way for the levels (tools/klotski_gen.c, offline) and at
 * random from a family: every position some blocks can reach is found (the whole family, by a search), then the
 * fewest moves from each of them, searched back from the ones that are solved; the start is one of them at the
 * distance wanted. The levels' tool lays blocks by chance until two cells are left (most such trays are jammed:
 * their great block never gets out); a random puzzle takes the family of a level drawn by chance (one of the smaller
 * ones), so it is found in a moment. The random variants: 0 easy (15 to 30 moves), 1 medium (30 to 55), 2 hard (the
 * farthest of its family, at least 55).
 * The levels: 150 of our own in six chapters, from a few moves to well over a hundred (klotski_levels.h: the blocks
 * and each one's par, checked by the tests).
 * Cells: x 0..3 from the left, y 0..4 from the top; a block's place is its top left cell. The notation (script, the
 * screenshots, the tests): a block is named by a cell it covers, its column a..d and its row 1..5 from the top, then
 * its slides, one letter a cell (u d l r): "b4r" slides the block on b4 one cell right, "a5uu" two cells up (one
 * move); "h" a hint (the first move of a shortest way, played), "=" the whole shortest way. A tray as text (the
 * levels): each block's kind (B the great block, T standing, W lying, S small) and its top left cell, row then column,
 * "B01 T00 T03 ...".
 */
#ifndef KLOTSKI_H
#define KLOTSKI_H

#include <stdint.h>

#define KLT_W 4
#define KLT_H 5
#define KLT_PIECES 12                           /* at most */
#define KLT_HIST 1024
#define KLT_PATH 24
#define KLT_VARIANTS 3
#define KLT_LEVELS 150
#define KLT_CHAPTERS 6
#define KLT_FAMILY 120000                       /* the most positions a family may have */

enum { KLT_BIG, KLT_TALL, KLT_WIDE, KLT_SMALL };

typedef struct { int8_t type, x, y; } klt_piece;

typedef struct {
    int np;                                     /* the blocks */
    klt_piece p[KLT_PIECES];
    int8_t hist[KLT_HIST][KLT_PIECES][2];       /* the blocks' places before each move */
    int nh;
    int last;                                   /* the block moved last (its next slide is the same move); -1 */
    int moves;
    int par;                                    /* the fewest moves from the start */
} klt_game;

int  klt_parse(klt_game *g, const char *blocks);   /* a tray as text (its par not known: 0); 0: not a tray */
void klt_text(const klt_game *g, char *out);   /* the tray as text (KLT_PIECES * 4 bytes at most) */
extern const char *const klt_levels[KLT_LEVELS];
extern const uint8_t klt_level_par[KLT_LEVELS];
extern const uint32_t klt_level_family[KLT_LEVELS];   /* the size of each level's family */
int  klt_level(klt_game *g, int level);         /* 0: none */
/* a new puzzle at random of the variant (its par found); 0: none found (never seen) */
int  klt_random(klt_game *g, int variant, uint32_t seed);
/* every position of g's family and its fewest moves: a callback for each (its tray, its distance; -1: no way out);
 * the family's size; 0: more than KLT_FAMILY positions (or no memory) */
int  klt_family(const klt_game *g, void (*each)(const klt_game *pos, int dist, void *ctx), void *ctx);
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
/* the fewest moves left; the first one's block and place (may be NULL); -1: none */
int  klt_solve(const klt_game *g, int *piece, int *x, int *y);
int  klt_script(klt_game *g, const char *moves);

#endif
