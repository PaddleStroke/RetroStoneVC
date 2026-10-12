/*
 * Water Sort: the rules (watersort.h): the engine of sorting.h, a pour taking the whole top run. No SDL: tested by
 * tests/watersort_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/watersort.h"

int ws_new(srt_game *g, int variant, uint32_t seed, srt_move *way)
{
    int v = variant < 0 || variant >= WS_VARIANTS ? 0 : variant;
    return srt_generate(g, SRT_VARIANT_COLOURS[v], 0, seed, way);
}

int ws_pour(srt_game *g, int from, int to) { return srt_move_do(g, from, to); }

int ws_script(srt_game *g, const char *moves) { return srt_script(g, moves); }

#ifndef SRT_GEN                                 /* not in the levels' tool (tools/watersort_gen.c): it makes them */
#include "games/watersort_levels.h"

int ws_level(srt_game *g, int level)
{
    if (level < 0 || level >= WS_LEVELS || !srt_parse(g, ws_levels[level], 0)) return 0;
    g->par = ws_level_par[level];
    return 1;
}
#endif
