/*
 * Block Puzzle for one player: the rules. A 10 x 10 grid; three pieces (polyominoes, never turned) are offered at a
 * time. Lay them on the grid in any order, each where it fits; when all three are down, three more come. A row or a
 * column made full is cleared at once (rows and columns together). The game ends when none of the pieces offered fits.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The score: a point a cell laid; lines cleared together score 10 for the first, 20 for the second, 30 for the third...
 * (10 * L * (L + 1) / 2: the combo); a clear right after a clear (the streak) adds 10 a line for each clear before it
 * in the row (a placement that clears nothing breaks the streak).
 * Two ways to play:
 * - Classic (the random tab): endless, the pieces drawn by chance (the game's own seeded numbers, puzrand.h, kept with
 *   each position: undo brings back the same pieces). A deal of three where none fits is drawn again (twice at most).
 *   The best score is kept by the UI.
 * - Levels (blocks_levels.h, made by tools/blocks_gen.c): a designed starting grid, a fixed sequence of pieces (dealt
 *   three at a time in its order) and a goal: clear N lines, reach a score, clear the gold blocks (laid on the grid at
 *   the start, cleared with their row or column), or cover the pattern (marked empty cells, each covered once). Each
 *   level keeps a way that reaches its goal in `par` pieces (the fewest a beam search found): the test replays them
 *   all. The goal reached: solved; the pieces gone or none fitting before it: failed (undo, or play again).
 * The stars of a level: by the pieces it took against par (ui_par_stars: 3 at par, 2 up to half as many again, else
 * 1). Classic: by the score (BK_STAR2, BK_STAR3).
 * Places: x the column from the left, y the row from the top (0..9); a piece is laid by its box's top left. The
 * notation (script, the screenshots, the tests): a placement is the tray's place (a b c), the column (a..j) and the
 * row (1..10 from the top) of the piece's top left: "ae7 bc1"; "=k" plays k placements of the hint ("=": until the
 * end); "#..." (a level's grid, 100 cells as in the levels' data) sets the grid.
 */
#ifndef BLOCKS_H
#define BLOCKS_H

#include <stdint.h>

#define BK_N 10
#define BK_CELLS (BK_N * BK_N)
#define BK_FULL 0x3ffu                          /* a full row */
#define BK_SHAPES 37
#define BK_NONE 0xff                            /* a tray's place used */
#define BK_SEQ 64                               /* a level's pieces, at most */
#define BK_HIST 1024                            /* undo's memory (the oldest half forgotten past it) */
#define BK_GOLD 13                              /* a gold block's colour (col[]) */
#define BK_LEVELS 150
#define BK_CHAPTERS 6
#define BK_STAR2 600                            /* classic: 2 stars from this score, 3 from the next */
#define BK_STAR3 1500

enum { BK_CLASSIC, BK_LINES, BK_SCORE, BK_GOLDS, BK_PATTERN };   /* the goals (classic: none) */

typedef struct {
    uint8_t w, h, n, fam;                       /* its box, its cells, its family (the colour) */
    uint8_t row[5];                             /* bit x: the cell (x, y) of row y */
} bk_shape;
extern const bk_shape bk_shapes[BK_SHAPES];
int bk_colour(int shape);                       /* its lacquer (hues.h, 1..12) */

/* what the rules need of a position (the search copies it a lot): no colours */
typedef struct {
    uint16_t fill[BK_N], gold[BK_N], want[BK_N];   /* the blocks; the gold ones among them; the pattern still open */
    uint8_t hand[3];                            /* the tray: a shape, or BK_NONE */
    uint8_t streak;                             /* the clears in a row just made */
    uint16_t next;                              /* a level: its sequence's next piece */
    uint16_t placed, lines;
    int32_t score;
} bk_core;

typedef struct {
    bk_core k;
    uint8_t col[BK_CELLS];                      /* each cell's colour (0 empty; BK_GOLD) */
    uint32_t rng;                               /* classic: the chance to come */
} bk_pos;

/* what a placement did (the UI's animation) */
typedef struct {
    int cells, lines, gain, golds;              /* cells laid, lines cleared, points won, gold blocks cleared */
    uint16_t rows, cols;                        /* the rows and columns cleared (bit i) */
    int streak;
} bk_result;

typedef struct {
    int goal, target, par, pieces;              /* a level's (classic: BK_CLASSIC, 0...) */
    uint8_t seq[BK_SEQ];
    int nseq;
    uint16_t pat[BK_N];                         /* the pattern at the start */
    int golds;                                  /* the gold blocks at the start */
    uint8_t way[BK_SEQ][2];                     /* a way to the goal: each placement's tray place and cell */
    int nway;
    bk_pos p;
    struct { bk_pos p; uint8_t mv[2]; } hist[BK_HIST];
    int nh;
    uint8_t mv[BK_SEQ][2];                      /* a level: the placements made (the way followed while they match) */
    bk_result last;
} bk_game;

/* a level's data (blocks_levels.h): its grid (100 cells, rows from the top: '.' empty, '1'..'9' 'a'..'c' a block of
 * that colour, 'G' a gold block, 'o' a pattern cell), its pieces in order (shapes as letters: bk_shape_char), its way
 * (placements in the notation, without the spaces), its par */
typedef struct {
    uint8_t goal;
    uint16_t target;
    uint8_t par, pieces;
    const char *grid, *seq, *way;
} bk_level_def;
extern const bk_level_def bk_levels[BK_LEVELS];

char bk_shape_char(int shape);
int  bk_shape_of(char c);                       /* -1: none */

/* ---- the rules on a core -------------------------------------------------------------------------------------- */
int  bk_fits(const uint16_t fill[BK_N], int shape, int x, int y);
int  bk_fits_anywhere(const uint16_t fill[BK_N], int shape);
int  bk_can_move(const bk_core *k);             /* a piece of the tray fits somewhere */
int  bk_hand_empty(const bk_core *k);
/* lays tray place `slot`'s piece at (x, y) (1: done, 0: no piece there or it does not fit), clears the full lines,
 * scores; the tray's place empty (no deal) */
int  bk_lay(bk_core *k, int slot, int x, int y, bk_result *r);
/* a level's next three pieces into an empty tray (fewer at the end of the sequence) */
void bk_deal_seq(bk_core *k, const uint8_t *seq, int nseq);
int  bk_goal_left(const bk_core *k, int goal, int target);   /* how far from the goal (0: reached) */
int  bk_popcount(const uint16_t m[BK_N]);

/* ---- a game ---------------------------------------------------------------------------------------------------- */
void bk_new_classic(bk_game *g, uint32_t seed);
int  bk_level(bk_game *g, int level);           /* 0: no such level */
int  bk_load(bk_game *g, const bk_level_def *d);   /* 0: bad data */
/* a placement: lays it, deals what comes next; 0 refused */
int  bk_place(bk_game *g, int slot, int x, int y);
int  bk_undo(bk_game *g);
int  bk_solved(const bk_game *g);               /* a level's goal reached */
int  bk_over(const bk_game *g);                 /* the game is over: solved, failed, or (classic) nothing fits */
int  bk_left(const bk_game *g);                 /* a level's pieces still to come (the tray's too) */
int  bk_stars(const bk_game *g);
int  bk_on_way(const bk_game *g);               /* a level: the placements so far are its way's first ones */
/* the hint: a placement (the tray's place, x, y); 1 found, 0 nothing fits. A level: its way while on it, else a
 * search from here (a way found is kept as the level's way); classic: the best placement by the evaluation. */
int  bk_hint(bk_game *g, int *slot, int *x, int *y);
int  bk_script(bk_game *g, const char *moves);

/* ---- the search (the hint, the levels' tool) ------------------------------------------------------------------- */
/* the evaluation of a position for a goal (bigger: better) */
int  bk_eval(const bk_core *k, int goal, int target);
/* a beam search of `width` positions a step from k, for the goal within the sequence's pieces: the fewest placements
 * found that reach it (their places and cells in way), or -1 none found */
int  bk_search(const bk_core *k, const uint8_t *seq, int nseq, int goal, int target, int width, uint8_t way[][2]);
/* a player that sees only the tray: the best placement by the evaluation (noise: a random part of each value, from
 * *rng; 0 none), the tray's other pieces still fitting after it; 0 nothing fits */
int  bk_greedy(const bk_core *k, int goal, int target, int noise, uint32_t *rng, int *slot, int *x, int *y);

#endif
