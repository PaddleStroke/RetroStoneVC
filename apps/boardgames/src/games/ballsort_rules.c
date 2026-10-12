/*
 * Ball Sort: the rules (ballsort.h): the engine of sorting.h, one ball a move. No SDL: tested by
 * tests/ballsort_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/ballsort.h"

int bs_new(srt_game *g, int variant, uint32_t seed, srt_move *way)
{
    int v = variant < 0 || variant >= BS_VARIANTS ? 0 : variant;
    return srt_generate(g, SRT_VARIANT_COLOURS[v], 1, seed, way);
}

int bs_move(srt_game *g, int from, int to) { return srt_move_do(g, from, to); }

int bs_script(srt_game *g, const char *moves) { return srt_script(g, moves); }
