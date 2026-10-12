/*
 * Tracks for one player: the rules. A square grid; one railway line runs through it from an entry on the board's edge
 * to an exit on another edge, cell to cell (a straight piece or a curve in each cell it uses, never crossing itself
 * nor branching). The numbers above the columns and beside the rows say how many cells of that line the track uses;
 * a few pieces are given. Lay the whole track.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The puzzles are made from a seed: a line drawn by chance from one edge to another (a third to two thirds of the
 * cells), its counts, the two end pieces given; then a solver that reasons the way a person does (tk_deduce) works
 * on it, and while it gets stuck a piece of the line is given where it was stuck; the pieces not needed are then taken
 * away again. A puzzle the solver finishes has one solution only. The ways of reasoning, by level:
 *   0: a cell's own sides (two ends at most, a dead end impossible) and the counts of its row and column;
 *   1: also no loop (two ends of one piece of line never joined, and the line not closed before it has all its cells);
 *   2: also the crossings of a cut (between two columns, or two rows, the line crosses an odd number of times when the
 *      entry and the exit are on either side of it, else an even number);
 *   3: also trying a side and following it to a contradiction.
 * The sizes 6, 8 and 10 (the variants) also want more of these: 6 needs at most level 1, 8 at least level 1, 10 at
 * least level 2.
 * Cells: x + y * n from the top left. A piece is its two ends, as bits: TK_N up, TK_E right, TK_S down, TK_W left.
 * The notation (script, the screenshots, the tests): a cell is its column letter a.. and its row 1.. from the top;
 * "c4" taps it (the next piece; an X is cleared), "c4ne" lays that piece (any two of n e s w), "c4x" marks it X,
 * "c4." clears it, "c4-c7-f7" lays the track along the cells (straight runs between the named ones: one drag); "h" a
 * hint; "=" hints to the end.
 */
#ifndef TRACKS_H
#define TRACKS_H

#include <stdint.h>

#define TK_VARIANTS 3
#define TK_MAX 10
#define TK_CELLS (TK_MAX * TK_MAX)
#define TK_HIST 4096

enum { TK_N = 1, TK_E = 2, TK_S = 4, TK_W = 8, TK_X = 16 };

typedef struct { uint8_t cell, was, first; } tk_change;

typedef struct {
    int n, grade;                               /* grade: the level of reasoning the puzzle needs */
    int a, b;                                   /* the entry and the exit cells */
    uint8_t a_side, b_side;                     /* their sides on the board's edge (TK_N...) */
    uint8_t rows[TK_MAX], cols[TK_MAX];         /* the counts */
    uint8_t sol[TK_CELLS];                      /* the solution: each cell's piece (0: none) */
    uint8_t given[TK_CELLS];                    /* 1: a given piece */
    uint8_t cell[TK_CELLS];                     /* the player's: a piece, TK_X, or 0 */
    tk_change hist[TK_HIST];
    int nh, moves;
} tk_game;

static inline int tk_opp(int side) { return ((side << 2) | (side >> 2)) & 15; }
static inline int tk_piece(int v) { return v & 15; }           /* the piece in a cell (0: none, an X) */

/* the solver, reasoning up to level (0..3) from the counts and the given pieces: 1 when it lays the whole track
 * (then out: each cell's piece); *grade: the highest level it used */
int  tk_deduce(const tk_game *g, int level, int *grade, uint8_t *out);
/* the tracks that fit the counts and the given pieces, counted up to limit (a search, for the tests); -1: the search
 * gave up (too long) */
int  tk_count(const tk_game *g, int limit);
void tk_new(tk_game *g, int variant, uint32_t seed);

int  tk_tap(tk_game *g, int cell);              /* the cell's next piece (an X cleared); 0: a given cell */
int  tk_set(tk_game *g, int cell, int v);       /* a piece, TK_X or 0; 0: not allowed or no change */
/* the track laid along cells (each next to the one before): one action; 0: nothing changed */
int  tk_lay(tk_game *g, const int *cells, int len);
/* crosses along cells: every one without a piece crossed (or, when the first is crossed, every cross cleared): one
 * action; 0: nothing changed */
int  tk_mark(tk_game *g, const int *cells, int len);
int  tk_hint(tk_game *g);                       /* one cell set right: the cell; -1: none */
int  tk_undo(tk_game *g);                       /* the last action taken back: a cell it changed; -1: none */
int  tk_solved(const tk_game *g);
int  tk_line_count(const tk_game *g, int way, int i);   /* the cells of a row (0) or column (1) holding a piece */
int  tk_cell(const tk_game *g, const char *s);  /* "c4": its cell; -1: none */
int  tk_script(tk_game *g, const char *moves);  /* 0 at the first bad one */

#endif
