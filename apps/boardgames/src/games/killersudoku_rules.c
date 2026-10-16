/*
 * Killer Sudoku: the rules (killersudoku.h): the maker and the levels' code, on the variants' shared rules
 * (sudokuvar.h). No SDL: tested by tests/killersudoku_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/killersudoku.h"

#include <string.h>

#include "games/puzrand.h"

int ksu_make_at(int level, int most, uint32_t seed, sv_shape *s, uint8_t *sol, int tries)
{
    if (level < SV_EASY || level > SV_EXPERT) level = SV_EASY;
    uint32_t rng = pr_seed(seed);
    sv_shape best;
    uint8_t full[SV_CELLS], best_sol[SV_CELLS];
    int best_g = -1;
    for (int t = 0; t < tries; t++) {
        memset(s, 0, sizeof *s);
        sv_boxes(s);
        sv_shape_init(s);
        if (!sv_fill(s, &rng, full, 0)) continue;
        /* the passes a grade may take, and (expert) all the trials' passes: well under a second */
        int g = sv_make_cages(s, level, most, &rng, full, 2000, level == SV_EXPERT ? 4000 : 0);
        if (g == level) {
            memcpy(sol, full, SV_CELLS);
            return g;
        }
        if (g >= SV_EASY && g < level && g > best_g) {
            best_g = g;
            best = *s;
            memcpy(best_sol, full, SV_CELLS);
        }
    }
    if (best_g < 0) {                           /* none at all (never seen): the last one */
        memcpy(sol, full, SV_CELLS);
        return sv_grade(s, NULL, SV_EXPERT, NULL, NULL, 0);
    }
    *s = best;
    memcpy(sol, best_sol, SV_CELLS);
    return best_g;
}

void ksu_new(sv_game *g, int level, uint32_t seed)
{
    sv_shape s;
    uint8_t sol[SV_CELLS];
    if (level < SV_EASY || level > SV_EXPERT) level = SV_EASY;
    int got = ksu_make_at(level, ksu_most(level), seed, &s, sol, level == SV_EXPERT ? 3 : 12);
    sv_load(g, &s, got < 0 ? level : got, NULL, sol);
    g->level = level;
    g->seed = seed;
}

int ksu_shape(sv_shape *s, const char *code)
{
    memset(s, 0, sizeof *s);
    sv_boxes(s);
    const char *end;
    if (!sv_cages_decode(s, code, &end) || *end) return 0;
    sv_shape_init(s);
    return 1;
}

int ksu_decode(sv_game *g, const char *code)
{
    sv_shape s;
    return ksu_shape(&s, code) && sv_load_level(g, &s, NULL);
}
