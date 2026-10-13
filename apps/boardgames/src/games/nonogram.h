/*
 * Nonograms for one player: the rules. A hidden picture on a grid of squares; the numbers beside each row and above
 * each column (its clues) give the runs of filled squares in that line, in order, with at least one empty square
 * between two runs. Fill the squares to find the picture; mark the squares known to be empty with a cross (the crosses
 * are only the player's notes). Solved when every line has its runs.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The levels: 150 of the house's own little pictures (nonogram_levels.h, drawn for this game: tools/
 * nonogram_pictures.txt), 5 x 5 to 15 x 15. The random puzzles (the variants: 5 x 5, 10 x 10 or 15 x 15): a picture
 * drawn by chance (blobs mirrored left to right, like an ink blot). Every picture can be solved line by line: the line
 * solver (ng_line) finds what a line's clues and its known squares force, line after line; a picture is used only when
 * that alone fills it all, so it has one solution and never needs a guess (the levels are checked the same way:
 * tests/nonogram_test.c). The hint: a wrong mark put right, else a square one line's clues and the right marks force.
 * Squares: x + y * n, from the top left. The notation (script, the screenshots, the tests): a square is its column
 * letter a.. and its row 1.. from the top; "c4" fills it (or empties it if filled), "xc4" crosses it (or uncrosses
 * it), "c4-c9" fills the run from c4 to c9 (one action: the drag), "xc4-c9" crosses it; "=n" fills the next n
 * squares of the picture that are still empty (reading order); "h" a hint.
 */
#ifndef NONOGRAM_H
#define NONOGRAM_H

#include <stdint.h>

#include "games/bench.h"

#define NG_VARIANTS 3
#define NG_MAX 15
#define NG_SQ (NG_MAX * NG_MAX)
#define NG_RUNS 8                               /* a line of 15 has at most 8 runs */
#define NG_LEVELS 150                           /* the designed levels (nonogram_levels.h) */
#define NG_CHAPTERS 6

enum { NG_EMPTY, NG_FILL, NG_CROSS };           /* a square as the player left it */
enum { NG_UNKNOWN = 0, NG_ON = 1, NG_OFF = 2 }; /* a square for the line solver */

typedef struct {
    int n;                                      /* squares a side */
    int pic;                                    /* the level (nonogram_levels.h), or -1: drawn by chance */
    uint8_t sol[NG_SQ];                         /* 1: filled in the picture */
    uint8_t colour[NG_SQ];                      /* its colours (NG_COLOURS), shown once it is solved */
    uint8_t clue[2][NG_MAX][NG_RUNS];           /* [0] the rows', [1] the columns' runs */
    uint8_t runs[2][NG_MAX];                    /* how many (0: an empty line) */
    uint8_t sq[NG_SQ];                          /* NG_EMPTY, NG_FILL, NG_CROSS */
    bench_hist hist;                            /* the actions played (hist.moves), undone one by one */
} ng_game;

/* the colours of the pictures: 0 none, then ebony, red lacquer, amber, jade, lapis, ivory, walnut, plum, sky,
 * rose, leaf, sand */
#define NG_COLOURS 13
extern const uint8_t ng_rgb[NG_COLOURS][3];
extern const char ng_letters[NG_COLOURS + 1];  /* their letters in a picture's rows: ".kryqbwnpsolm" */

typedef struct { int n; const char *rows[NG_MAX]; } ng_picture;   /* '.' empty, else a colour letter */

/* one line: its length, its runs, its squares (NG_UNKNOWN / ON / OFF), filled in with what they force; how many
 * squares were found; -1: no way to place the runs */
int  ng_line(int len, const uint8_t *runs, int nruns, uint8_t *line);
/* a picture (n x n, 1: filled): 1 when line solving alone fills it all */
int  ng_solvable(int n, const uint8_t *pic);
/* the same, graded: the sweeps over every line that line solving takes to fill it all (0: it never does); *left
 * (may be NULL): the squares still unknown after the first sweep */
int  ng_sweeps(int n, const uint8_t *pic, int *left);
/* a line's runs (out: NG_RUNS at most); how many */
int  ng_runs_of(const uint8_t *cells, int len, int step, uint8_t *out);

void ng_new(ng_game *g, int variant, uint32_t seed);
void ng_load(ng_game *g, int n, const uint8_t *pic, const uint8_t *colour, int index);
/* a picture of rows (a level: index): 0 if it is not well formed */
int  ng_load_picture(ng_game *g, const ng_picture *p, int index);
/* the squares from a to b (in one row or column) that are as a is, set to `to` (a fill run passes over crosses, a
 * cross run over filled squares): one action; 0: nothing changed */
int  ng_paint(ng_game *g, int a, int b, int to);
int  ng_undo(ng_game *g);                       /* the last action taken back: a square it changed; -1: none */
/* a hint: a wrong mark put right (a square filled off the picture crossed, a cross on it filled), else a square the
 * line solver finds from one line's clues and the right marks, filled (or crossed when the lines force only empty
 * squares): the square; -1: none (solved) */
int  ng_hint(ng_game *g);
int  ng_line_done(const ng_game *g, int way, int i);   /* 1: the line (0 a row, 1 a column) has its runs */
int  ng_solved(const ng_game *g);
int  ng_square(const ng_game *g, const char *s);   /* "c4": its square; -1: none */
int  ng_script(ng_game *g, const char *moves);  /* 0 at the first bad one */

#endif
