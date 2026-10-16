/*
 * Jigsaw Sudoku for one player: the rules. A grid of 9 x 9 cells cut into nine irregular regions of nine cells; some
 * cells are given. Fill the others so that every row, every column and every region holds each digit 1..9 once.
 * Every puzzle has one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * What the Sudoku variants share is sudokuvar.h: the units, the graded solver, the player's grid (sv_game) and the
 * notation. A puzzle is made from a seed: the regions first, the 3 x 3 boxes reshaped by swapping cells between
 * neighbouring regions (each swap made twice, the second its mirror through the centre, so the regions stay
 * symmetric; a swap that would cut a region in two is undone); then a full grid filled by chance (a shape no grid
 * fits is thrown away), and clues taken away in pairs round the centre while the puzzle keeps its one solution
 * within its level (sv_make_givens). The levels as Sudoku's (sv_grade): easy singles, medium pairs and locked digits,
 * hard triples, fish and the XY-wing, expert a digit tried and followed.
 */
#ifndef JIGSAWSUDOKU_H
#define JIGSAWSUDOKU_H

#include <stdint.h>

#include "games/sudokuvar.h"

#define JSU_VARIANTS SV_LEVELS
#define JSU_LEVELS 150                          /* the designed levels (jigsawsudoku_levels.h) */
#define JSU_CHAPTERS 6
#define JSU_CODE (2 * SV_CELLS + 2)             /* a level's code: the regions, ':', the clues */

/* nine regions of nine cells from the boxes, `swaps` swaps made (the regions in s->region) */
void jsu_regions(uint32_t *rng, sv_shape *s, int swaps);
/* a puzzle of the level from the seed (floor: the clues it keeps at least; -1 the level's own): its shape, clues and
 * solution; the level reached (the hardest found under it when none was found at it within the tries) */
int  jsu_make_at(int level, int floor, uint32_t seed, sv_shape *s, uint8_t *clues, uint8_t *sol, int tries);
void jsu_new(sv_game *g, int level, uint32_t seed);   /* a puzzle made at random */
/* a designed level's code: the regions (81 digits 1..9), ':', the clues (81, '.' or 1..9); 0: not one puzzle the
 * reasoning solves */
int  jsu_decode(sv_game *g, const char *code);
int  jsu_shape(sv_shape *s, uint8_t *clues, const char *code);   /* without solving (the pictures); 0: not one */
void jsu_encode(const sv_shape *s, const uint8_t *clues, char *out);   /* out: JSU_CODE */

#endif
