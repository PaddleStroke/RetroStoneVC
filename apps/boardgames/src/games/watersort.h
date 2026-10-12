/*
 * Water Sort for one player: the rules. Glass tubes hold layers of coloured liquid, four at most. Pour from one tube
 * into another: the top layer, and every layer of its colour under it, go as far as the other tube holds them, onto
 * the same colour or into an empty tube. Solved when each colour fills a tube of its own (the engine: sorting.h).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The variants: 4 colours in 6 tubes, 7 in 9, 10 in 12, 12 in 14. The notation (script, the screenshots, the tests):
 * a pour is its two tubes, a b c... from the left (row by row), "ac bd"; "+" sets the extra tube; "=k" plays the next k
 * pours of the shortest way ("=": all of it).
 */
#ifndef WATERSORT_H
#define WATERSORT_H

#include "games/sorting.h"

#define WS_VARIANTS SRT_VARIANTS
#define WS_LEVELS 150
#define WS_CHAPTERS 6

/* a new puzzle from a seed: its shortest way's length (the way itself in way, when not NULL) */
int ws_new(srt_game *g, int variant, uint32_t seed, srt_move *way);
int ws_pour(srt_game *g, int from, int to);     /* the layers poured; 0: not allowed */
int ws_script(srt_game *g, const char *moves);

/* the levels: 150 of our own in six chapters, from 3 colours to 12, made by tools/watersort_gen.c
 * (watersort_levels.h: the trays as text, each one's shortest way, checked by the tests) */
extern const char *const ws_levels[WS_LEVELS];
extern const uint8_t ws_level_par[WS_LEVELS];
int ws_level(srt_game *g, int level);          /* 0: none */

#endif
