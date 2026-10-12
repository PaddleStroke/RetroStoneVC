/*
 * Ball Sort's levels: src/games/ballsort_levels.h. The same tool as Water Sort's (tools/watersort_gen.c, read its
 * comment), the balls moved one at a time.
 *   gcc -O2 -Iapps/boardgames/src -Iapps/boardgames/tools -o /tmp/bsgen apps/boardgames/tools/ballsort_gen.c -lm && \
 *       /tmp/bsgen > apps/boardgames/src/games/ballsort_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define SRT_GEN
#define SRT_ONE 1
#define NAME "Ball Sort"
#define FILE_ID "ballsort"
#define PRE "bs"
#define UP "BS"
#define WHAT "moves"
#include "games/ballsort_rules.c"

#include "watersort_gen.c"
