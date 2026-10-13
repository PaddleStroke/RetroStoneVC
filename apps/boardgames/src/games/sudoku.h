/*
 * Sudoku for one player: the rules. A grid of 9 x 9 cells in nine boxes of 3 x 3; some cells are given. Fill the
 * others so that every row, every column and every box holds each digit 1..9 once. Every puzzle has one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The puzzles are made from a seed: a full grid filled by chance, then clues taken away (two at a time, opposite each
 * other round the centre) as long as the solution stays the only one and the puzzle stays within its level. The
 * level is graded by the ways of reasoning a solver needs (su_rate), not by the clues' count:
 *   0 easy: singles only (a cell with one digit left; a digit with one place left in a row, column or box), and at
 *     least 36 clues;
 *   1 medium: also locked candidates (a digit's places in a box all in one row or column, or the other way round),
 *     naked and hidden pairs; at least 28 clues;
 *   2 hard: also naked and hidden triples and quads, the X-wing, the swordfish, the XY-wing;
 *   3 expert: needs more than all of these (chains of reasoning, or trying a digit and following it through).
 * A puzzle is kept only when it needs its level's own ways (an easy one nothing more than singles, a medium one at
 * least a pair or locked candidates, a hard one at least a triple or an X-wing). The grader grades 0..4: easy 0,
 * medium 1, hard 2 (triples, the X-wing) or 3 (quads, the swordfish, the XY-wing), expert 4.
 * The player's entries may be wrong; nothing says so (the option of showing clashes: two same digits in a row, column
 * or box). The puzzle is solved when the grid is full and right.
 * Cells: 0..80, row by row from the top left. The notation (script, the screenshots, the tests): a cell is its column
 * letter a..i and its row 1..9 from the top; "e5=7" puts 7 in e5, "e5=0" empties it, "e5'7" turns the note 7 on or
 * off, "h" a hint, "+n" fills the next n empty cells (reading order) with their digits.
 */
#ifndef SUDOKU_H
#define SUDOKU_H

#include <stdint.h>

#include "games/bench.h"

#define SU_VARIANTS 4
#define SU_CELLS 81
#define SU_LEVELS 150                           /* the designed levels (sudoku_levels.h) */
#define SU_CHAPTERS 6

enum { SU_EASY, SU_MEDIUM, SU_HARD, SU_EXPERT };

typedef struct {
    int level;
    uint32_t seed;
    uint8_t given[SU_CELLS];                    /* 1: a clue */
    uint8_t sol[SU_CELLS];                      /* the solution */
    uint8_t val[SU_CELLS];                      /* the grid now (0: empty) */
    uint16_t notes[SU_CELLS];                   /* the pencil marks: bit d for the digit d */
    bench_hist hist;                            /* the actions played (hist.moves), undone one by one */
} su_game;

/* the solutions of a grid (0: empty), counted up to limit; the first found in sol (may be NULL) */
int  su_count(const uint8_t *grid, int limit, uint8_t *sol);
/* the grade of a grid (0..4 as above: the hardest way its solving needs; 4: more than every way known here) */
int  su_rate(const uint8_t *grid);
/* the same, and the steps each grade's ways took (steps[0..3]: singles, pairs and locked candidates, triples and the
 * X-wing, quads, the swordfish and the XY-wing; may be NULL) */
int  su_rate_steps(const uint8_t *grid, int *steps);
/* the bench's grade (bench.h): BENCH_EASY singles, BENCH_MEDIUM pairs and locked candidates, BENCH_HARD the harder
 * ways or a digit tried and followed (with the easy and medium ways) to a contradiction, BENCH_GUESS beyond, BENCH_NONE
 * no solution; *passes (may be NULL): the solver's passes, its work */
int  su_bench(const uint8_t *grid, long *passes);
/* a puzzle of the level from the seed: its clues (0 elsewhere) and its solution */
void su_make(int level, uint32_t seed, uint8_t *puzzle, uint8_t *sol);

void su_new(su_game *g, int level, uint32_t seed);
void su_load(su_game *g, int level, const uint8_t *puzzle, const uint8_t *sol);
/* a designed level (sudoku_levels.h): 81 characters, '.' empty or a clue 1..9, row by row; 0: not one puzzle with
 * one solution */
int  su_decode(su_game *g, const char *s);
int  su_put(su_game *g, int cell, int digit);   /* a digit (0: empty the cell); the notes of that digit in its row,
                                                 * column and box go; 0: not allowed (a clue, the same digit) */
int  su_note(su_game *g, int cell, int digit);  /* a note on or off (an empty cell only); 0: not allowed */
int  su_erase(su_game *g, int cell);            /* the cell's digit, or else its notes, taken out; 0: nothing */
int  su_hint(su_game *g);                       /* a wrong digit put right, else the easiest cell filled: the cell;
                                                 * -1: none */
int  su_undo(su_game *g);                       /* the last action taken back: a cell it changed; -1: none */
int  su_clash(const su_game *g, int cell);      /* 1: its digit is also in its row, column or box */
int  su_solved(const su_game *g);
int  su_count_digit(const su_game *g, int digit);   /* the cells holding it */
int  su_cell(const char *s);                    /* "e5": its cell; -1: none */
int  su_script(su_game *g, const char *moves);  /* 0 at the first bad one */

#endif
