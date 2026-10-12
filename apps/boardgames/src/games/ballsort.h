/*
 * Ball Sort for one player: the rules. Glass tubes hold four balls at most. Move one ball at a time, the top ball of a
 * tube, onto a ball of its colour or into an empty tube. Solved when each colour fills a tube of its own (the engine:
 * sorting.h, Water Sort's, moving one at a time).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 4 colours in 6 tubes, 7 in 9, 10 in 12, 12 in 14. The notation (script, the screenshots, the tests):
 * a move is its two tubes, a b c... from the left (row by row), "ac bd"; "+" sets the extra tube; "=k" plays the next
 * k moves of the shortest way ("=": all of it).
 */
#ifndef BALLSORT_H
#define BALLSORT_H

#include "games/sorting.h"

#define BS_VARIANTS SRT_VARIANTS

/* a new puzzle from a seed: its shortest way's length (the way itself in way, when not NULL) */
int bs_new(srt_game *g, int variant, uint32_t seed, srt_move *way);
int bs_move(srt_game *g, int from, int to);     /* 1: moved; 0: not allowed */
int bs_script(srt_game *g, const char *moves);

#endif
