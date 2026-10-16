/*
 * Diagonal Sudoku for one player: the rules. A grid of 9 x 9 cells in nine boxes of 3 x 3; some cells are given.
 * Fill the others so that every row, every column, every box and both long diagonals hold each digit 1..9 once.
 * Every puzzle has one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * What the Sudoku variants share is sudokuvar.h: the units (here the diagonals too), the graded solver, the
 * player's grid (sv_game) and the notation. A puzzle is made from a seed: a full grid filled by chance, then clues
 * taken away in pairs round the centre while the puzzle keeps its one solution within its level (sv_make_givens).
 * The levels as Sudoku's (sv_grade): easy singles, medium pairs and locked digits, hard triples, fish and the
 * XY-wing, expert a digit tried and followed. The diagonals give more away, so a medium puzzle keeps 24 clues at
 * least (Sudoku's 28).
 */
#ifndef DIAGONALSUDOKU_H
#define DIAGONALSUDOKU_H

#include <stdint.h>

#include "games/sudokuvar.h"

#define DSU_VARIANTS SV_LEVELS
#define DSU_LEVELS 150                          /* the designed levels (diagonalsudoku_levels.h) */
#define DSU_CHAPTERS 6

/* the clues a level keeps at least */
static inline int dsu_floor(int level) { return level == SV_MEDIUM ? 24 : sv_clues_floor(level); }

void dsu_shape(sv_shape *s);                    /* the boxes and the diagonals */
/* a puzzle of the level from the seed: its clues and solution; the level reached (the hardest found under it when
 * none was found at it within the tries) */
int  dsu_make_at(int level, uint32_t seed, uint8_t *clues, uint8_t *sol, int tries);
void dsu_new(sv_game *g, int level, uint32_t seed);   /* a puzzle made at random */
/* a designed level: 81 characters, '.' or a clue 1..9, row by row; 0: not one puzzle the reasoning solves */
int  dsu_decode(sv_game *g, const char *code);

#endif
