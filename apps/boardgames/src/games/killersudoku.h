/*
 * Killer Sudoku for one player: the rules. A grid of 9 x 9 cells in nine boxes of 3 x 3, cut into cages, each with
 * its sum; no digit is given. Fill the grid so that every row, every column and every box holds each digit 1..9
 * once, the digits of each cage add up to its sum, and no digit comes twice in a cage. Every puzzle has one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * What the Sudoku variants share is sudokuvar.h: the shape (units, peers, the cages' sums and those the rule of 45
 * gives), the graded solver, the player's grid (sv_game) and the notation. A puzzle is made from a seed: a full grid
 * filled by chance, every cell its own cage, then neighbouring cages merged (into pairs first, then threes...; at
 * most 4 cells easy, 5 above) while the puzzle keeps its one solution within its level (sv_make_cages). The levels
 * (sv_grade): 0 easy, a cage's ways and the singles; 1 medium, also pairs, locked digits and the rule of 45 on a
 * unit; 2 hard, also triples, fish, the XY-wing and the rule of 45 on rows or columns side by side; 3 expert, a digit
 * tried and followed to a contradiction.
 */
#ifndef KILLERSUDOKU_H
#define KILLERSUDOKU_H

#include <stdint.h>

#include "games/sudokuvar.h"

#define KSU_VARIANTS SV_LEVELS
#define KSU_LEVELS 150                          /* the designed levels (killersudoku_levels.h) */
#define KSU_CHAPTERS 6
#define KSU_CODE 260                            /* a level's code, at most */

/* the cages' most cells at a level */
static inline int ksu_most(int level) { return level == SV_EASY ? 4 : 5; }

/* a puzzle of the level (cages at most `most` cells) from the seed: its shape (s) and solution; the level it reached
 * (the hardest found at or under the level when none was found at it within the tries) */
int  ksu_make_at(int level, int most, uint32_t seed, sv_shape *s, uint8_t *sol, int tries);
void ksu_new(sv_game *g, int level, uint32_t seed);   /* a puzzle made at random */
/* a designed level's code (sv_cages_encode: the joins, ':', the sums); 0: not one puzzle the reasoning solves */
int  ksu_decode(sv_game *g, const char *code);
int  ksu_shape(sv_shape *s, const char *code);  /* the shape alone (no solving: the pictures); 0: not one */

#endif
