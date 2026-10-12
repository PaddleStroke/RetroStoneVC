/*
 * Nonograms for one player: the rules. A hidden picture on a grid of squares; the numbers beside each row and above
 * each column (its clues) give the runs of filled squares in that line, in order, with at least one empty square
 * between two runs. Fill the squares to find the picture; mark the squares known to be empty with a cross (the crosses
 * are only the player's notes). Solved when every line has its runs.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The pictures: 5 x 5, 10 x 10 or 15 x 15 (the variants). Half of the time one of the house's own little pictures
 * (a heart, a boat, a cat...), else one drawn by chance (blobs mirrored left to right, like an ink blot). Every
 * picture can be solved line by line: the line solver (ng_line) finds what a line's clues and its known squares force,
 * line after line; a picture is used only when that alone fills it all, so it has one solution and never needs a
 * guess. The house's pictures are checked the same way (tests/nonogram_test.c).
 * Squares: x + y * n, from the top left. The notation (script, the screenshots, the tests): a square is its column
 * letter a.. and its row 1.. from the top; "c4" fills it (or empties it if filled), "xc4" crosses it (or uncrosses
 * it), "c4-c9" fills the run from c4 to c9 (one action: the drag), "xc4-c9" crosses it; "=n" fills the next n
 * squares of the picture that are still empty (reading order).
 */
#ifndef NONOGRAM_H
#define NONOGRAM_H

#include <stdint.h>

#define NG_VARIANTS 3
#define NG_MAX 15
#define NG_SQ (NG_MAX * NG_MAX)
#define NG_RUNS 8                               /* a line of 15 has at most 8 runs */
#define NG_HIST 4096

enum { NG_EMPTY, NG_FILL, NG_CROSS };           /* a square as the player left it */
enum { NG_UNKNOWN = 0, NG_ON = 1, NG_OFF = 2 }; /* a square for the line solver */

typedef struct { uint8_t sq, was, first; } ng_change;

typedef struct {
    int n;                                      /* squares a side */
    int pic;                                    /* the house's picture (ng_pictures), or -1: drawn by chance */
    uint8_t sol[NG_SQ];                         /* 1: filled in the picture */
    uint8_t colour[NG_SQ];                      /* its colours (NG_COLOURS), shown once it is solved */
    uint8_t clue[2][NG_MAX][NG_RUNS];           /* [0] the rows', [1] the columns' runs */
    uint8_t runs[2][NG_MAX];                    /* how many (0: an empty line) */
    uint8_t sq[NG_SQ];                          /* NG_EMPTY, NG_FILL, NG_CROSS */
    ng_change hist[NG_HIST];
    int nh, moves;
} ng_game;

/* the colours of the pictures: 0 none, then ebony, red lacquer, amber, jade, lapis, ivory, walnut, plum, sky,
 * rose, leaf, sand */
#define NG_COLOURS 13
extern const uint8_t ng_rgb[NG_COLOURS][3];

typedef struct { int n; const char *rows[NG_MAX]; } ng_picture;   /* '.' empty, else a colour letter */
extern const ng_picture ng_pictures[];
extern const int ng_npictures;

/* one line: its length, its runs, its squares (NG_UNKNOWN / ON / OFF), filled in with what they force; how many
 * squares were found; -1: no way to place the runs */
int  ng_line(int len, const uint8_t *runs, int nruns, uint8_t *line);
/* a picture (n x n, 1: filled): 1 when line solving alone fills it all */
int  ng_solvable(int n, const uint8_t *pic);
/* a line's runs (out: NG_RUNS at most); how many */
int  ng_runs_of(const uint8_t *cells, int len, int step, uint8_t *out);

void ng_new(ng_game *g, int variant, uint32_t seed);
void ng_load(ng_game *g, int n, const uint8_t *pic, const uint8_t *colour, int index);
int  ng_load_picture(ng_game *g, int index);   /* one of the house's: 0 if it does not exist */
/* the squares from a to b (in one row or column) that are as a is, set to `to` (a fill run passes over crosses, a
 * cross run over filled squares): one action; 0: nothing changed */
int  ng_paint(ng_game *g, int a, int b, int to);
int  ng_undo(ng_game *g);                       /* the last action taken back: a square it changed; -1: none */
int  ng_line_done(const ng_game *g, int way, int i);   /* 1: the line (0 a row, 1 a column) has its runs */
int  ng_solved(const ng_game *g);
int  ng_square(const ng_game *g, const char *s);   /* "c4": its square; -1: none */
int  ng_script(ng_game *g, const char *moves);  /* 0 at the first bad one */

#endif
