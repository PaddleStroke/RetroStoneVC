/*
 * Kakuro for one player: the rules. A grid of n x n squares (6, 8, 10 or 12), some white, some dark; the top row and
 * the left column are dark. A run is a line of white squares, across or down, between dark ones; the dark square
 * before it holds its clue. Fill the white squares with digits 1..9 so that no digit repeats in a run and each run's
 * digits add up to its clue. Every puzzle has one solution.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: the four sizes, each at three levels (6 easy, 6 medium, 6 hard, 8 easy...). A puzzle is made from a
 * seed: a pattern of dark squares (turned half round it stays the same; every run two squares or more, nine at
 * most; the white squares all joined), digits put in by chance, the clues added up; then digits changed one at a
 * time where the reasoning is stuck until the solver finishes it, at its level and not below. The level is the
 * reasoning the solver needs (bench.h):
 *   0 easy: the ways of making each clue with its run's length (a square keeps the digits of those ways), a digit
 *     placed is not used again in its runs, a digit every way needs with one square left for it;
 *   1 medium: also every run worked out whole (the digits its squares can take together, without repeats);
 *   2 hard: also trials (a digit that leads, by the easy ways, to a contradiction).
 * The player's digits may be wrong; the clashes (a digit twice in a run, a full run with the wrong sum) can be shown.
 * Squares: 0..n*n-1, row by row from the top left. The notation (script, the screenshots, the tests): a square is its
 * column letter a.. and its row 1.. from the top; "c2=4" puts 4 in c2, "c2=0" empties it, "c2'4" turns the note 4 on
 * or off, "h" a hint, "+n" fills the next n squares (reading order) that are not right with their digits.
 */
#ifndef KAKURO_H
#define KAKURO_H

#include <stdint.h>

#include "games/bench.h"

#define KKR_MAX 12
#define KKR_CELLS (KKR_MAX * KKR_MAX)
#define KKR_SIZES 4
#define KKR_LEVELS 3
#define KKR_VARIANTS (KKR_SIZES * KKR_LEVELS)
#define KKR_RUNS KKR_CELLS

enum { KKR_EASY, KKR_MEDIUM, KKR_HARD };

static const int KKR_SIZE[KKR_SIZES] = {6, 8, 10, 12};
static inline int kkr_size_of(int variant) { return KKR_SIZE[variant / KKR_LEVELS % KKR_SIZES]; }
static inline int kkr_level_of(int variant) { return variant % KKR_LEVELS; }

typedef struct {
    int n;
    uint8_t white[KKR_CELLS];
    uint8_t across[KKR_CELLS], down[KKR_CELLS];  /* a dark square's clues: the run to its right, below (0: none) */
    uint8_t sol[KKR_CELLS];
} kkr_puzzle;

typedef struct {
    uint8_t len, sum, clue;                     /* clue: the dark square holding it */
    uint8_t cell[9];
} kkr_run;

typedef struct {
    int variant, level;
    uint32_t seed;
    kkr_puzzle p;
    kkr_run run[KKR_RUNS];
    int nruns;
    uint8_t run_of[KKR_CELLS][2];               /* a white square's runs: across, down */
    uint8_t val[KKR_CELLS];
    uint16_t notes[KKR_CELLS];
    bench_hist hist;
} kkr_game;

/* the runs of a puzzle: their count; run_of may be NULL */
int  kkr_runs(const kkr_puzzle *p, kkr_run *runs, uint8_t (*run_of)[2]);
int  kkr_count(const kkr_puzzle *p, int limit); /* the solutions, up to limit (a plain search) */
int  kkr_rate(const kkr_puzzle *p);             /* KKR_EASY..KKR_HARD; BENCH_GUESS stuck; BENCH_NONE none */
void kkr_make(int n, int level, uint32_t seed, kkr_puzzle *p);

void kkr_new(kkr_game *g, int variant, uint32_t seed);
int  kkr_put(kkr_game *g, int cell, int digit); /* a digit (0: empty); the note of it in its runs goes; 0: no */
int  kkr_note(kkr_game *g, int cell, int digit);
int  kkr_erase(kkr_game *g, int cell);
int  kkr_hint(kkr_game *g);                     /* a wrong digit put right, else a square found by reasoning */
int  kkr_undo(kkr_game *g);
int  kkr_clash(const kkr_game *g, int cell);    /* its digit twice in a run, or a full run of it with a wrong sum */
int  kkr_solved(const kkr_game *g);
int  kkr_script(kkr_game *g, const char *moves);

#endif
