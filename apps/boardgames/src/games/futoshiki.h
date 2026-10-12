/*
 * Futoshiki for one player: the rules. A square of n x n cells (4 to 7); fill it with the digits 1..n so that every
 * row and every column holds each digit once (a Latin square). Some digits are given; between some neighbouring cells
 * a sign says which of the two holds the greater digit. Every puzzle has one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: the four sizes, each at three levels (4 easy, 4 medium, 4 hard, 5 easy...). A puzzle is made from a
 * seed: a Latin square filled by chance, every digit and every sign of it given, then taken away one by one (in a
 * random order) as long as the solution stays the only one and the puzzle stays within its level. The level is
 * graded by the ways of reasoning a solver needs (fsk_rate), not by the clues' count:
 *   0 easy: singles (a cell with one digit left; a digit with one place left in a row or column) and the signs'
 *     bounds (a cell less than its neighbour is less than the greatest digit the neighbour can still hold; chains of
 *     signs follow from it);
 *   1 medium: also naked and hidden pairs in a row or column;
 *   2 hard: also triples, quads, the X-wing, and one-step contradictions (a digit that, with the easy ways alone,
 *     leaves some cell with nothing).
 * A puzzle needing more than all of these is not used; one is kept only when it needs its level's own ways.
 * The player's digits may be wrong; the clashes (the same digit twice in a line, a sign not holding) can be shown.
 * Cells: 0..n*n-1, row by row from the top left. The notation (script, the screenshots, the tests): a cell is its
 * column letter a.. and its row 1.. from the top; "c2=4" puts 4 in c2, "c2=0" empties it, "c2'4" turns the note 4 on
 * or off, "h" a hint, "+n" fills the next n empty cells (reading order) with their digits.
 */
#ifndef FUTOSHIKI_H
#define FUTOSHIKI_H

#include <stdint.h>

#include "games/logicgrid2.h"

#define FSK_MIN 4
#define FSK_MAX 7
#define FSK_CELLS (FSK_MAX * FSK_MAX)
#define FSK_LEVELS 3
#define FSK_VARIANTS ((FSK_MAX - FSK_MIN + 1) * FSK_LEVELS)

enum { FSK_EASY, FSK_MEDIUM, FSK_HARD, FSK_BEYOND };
enum { FSK_NONE, FSK_LT, FSK_GT };              /* a sign: the first cell (left, top) less / greater */

static inline int fsk_size_of(int variant) { return FSK_MIN + variant / FSK_LEVELS; }
static inline int fsk_level_of(int variant) { return variant % FSK_LEVELS; }

typedef struct {
    int n;
    uint8_t g[FSK_CELLS];                       /* the given digits (0: none) */
    uint8_t rel[2][FSK_CELLS];                  /* the signs: [0] between a cell and the one on its right, [1] the one
                                                 * below it */
} fsk_puz;

typedef struct {
    int n, level;
    uint32_t seed;
    fsk_puz p;
    uint8_t sol[FSK_CELLS];                     /* the solution */
    uint8_t val[FSK_CELLS];                     /* the grid now (0: empty) */
    uint8_t notes[FSK_CELLS];                   /* the pencil marks: bit d for the digit d */
    lg2_hist hist;
} fsk_game;

/* the solutions of a puzzle, counted up to limit; the first found in sol (may be NULL) */
int  fsk_count(const fsk_puz *p, int limit, uint8_t *sol);
/* the grade of a puzzle (FSK_EASY..FSK_HARD: the hardest way its solving needs; FSK_BEYOND: more than these) */
int  fsk_rate(const fsk_puz *p);
/* a puzzle of the size and level from the seed, and its solution */
void fsk_make(int n, int level, uint32_t seed, fsk_puz *p, uint8_t *sol);

void fsk_new(fsk_game *g, int variant, uint32_t seed);
void fsk_load(fsk_game *g, int level, const fsk_puz *p, const uint8_t *sol);
int  fsk_put(fsk_game *g, int cell, int digit); /* a digit (0: empty the cell); its notes in the row and column go;
                                                 * 0: not allowed (a given cell, the same digit) */
int  fsk_note(fsk_game *g, int cell, int digit);   /* a note on or off (an empty cell only); 0: not allowed */
int  fsk_erase(fsk_game *g, int cell);          /* the cell's digit, or else its notes, taken out; 0: nothing */
int  fsk_hint(fsk_game *g);                     /* a wrong digit put right, else the easiest cell filled: the cell;
                                                 * -1: none */
int  fsk_undo(fsk_game *g);                     /* the last action taken back: a cell it changed; -1: none */
int  fsk_clash(const fsk_game *g, int cell);    /* 1: its digit is also in its line, or a sign by it does not hold */
int  fsk_solved(const fsk_game *g);
int  fsk_count_digit(const fsk_game *g, int digit);
int  fsk_script(fsk_game *g, const char *moves);   /* 0 at the first bad one */

#endif
