/*
 * Diagonal Sudoku: the rules (diagonalsudoku.h): the maker and the levels' code, on the variants' shared rules
 * (sudokuvar.h). No SDL: tested by tests/diagonalsudoku_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/diagonalsudoku.h"

#include <string.h>

#include "games/puzrand.h"

void dsu_shape(sv_shape *s)
{
    memset(s, 0, sizeof *s);
    sv_boxes(s);
    s->diag = 1;
    sv_shape_init(s);
}

int dsu_make_at(int level, uint32_t seed, uint8_t *clues, uint8_t *sol, int tries)
{
    if (level < SV_EASY || level > SV_EXPERT) level = SV_EASY;
    uint32_t rng = pr_seed(seed);
    sv_shape s;
    uint8_t full[SV_CELLS], p[SV_CELLS];
    int best_g = -1;
    dsu_shape(&s);
    for (int t = 0; t < tries; t++) {
        if (!sv_fill(&s, &rng, full, 0)) continue;
        /* the passes a grade may take, and (expert) all the trials' passes: well under a second */
        int g = sv_make_givens(&s, level, dsu_floor(level), &rng, full, p, 3000, level == SV_EXPERT ? 8000 : 0);
        if (g >= SV_EASY && g <= level && g > best_g) {   /* the nearest yet (never above the level) */
            best_g = g;
            memcpy(clues, p, SV_CELLS);
            memcpy(sol, full, SV_CELLS);
            if (g == level) break;
        }
    }
    return best_g;
}

void dsu_new(sv_game *g, int level, uint32_t seed)
{
    sv_shape s;
    uint8_t p[SV_CELLS], sol[SV_CELLS];
    if (level < SV_EASY || level > SV_EXPERT) level = SV_EASY;
    dsu_make_at(level, seed, p, sol, level == SV_EXPERT ? 5 : 12);
    dsu_shape(&s);
    sv_load(g, &s, level, p, sol);
    g->seed = seed;
}

int dsu_decode(sv_game *g, const char *code)
{
    sv_shape s;
    uint8_t p[SV_CELLS];
    if (strlen(code) != SV_CELLS || !sv_clues_decode(p, code)) return 0;
    dsu_shape(&s);
    return sv_load_level(g, &s, p);
}
