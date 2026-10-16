/*
 * Jigsaw Sudoku: the rules (jigsawsudoku.h): the regions' maker, the puzzles' and the levels' code, on the variants'
 * shared rules (sudokuvar.h). No SDL: tested by tests/jigsawsudoku_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/jigsawsudoku.h"

#include <string.h>

#include "games/puzrand.h"

/* the region r's cells all joined side to side */
static int joined(const uint8_t *reg, int r)
{
    int stack[SV_CELLS], n = 0, seen = 0, first = -1;
    uint8_t mark[SV_CELLS] = {0};
    for (int c = 0; c < SV_CELLS && first < 0; c++)
        if (reg[c] == r) first = c;
    if (first < 0) return 0;
    stack[n++] = first;
    mark[first] = 1;
    while (n) {
        int c = stack[--n];
        seen++;
        int nb[4] = {c % 9 ? c - 1 : -1, c % 9 < 8 ? c + 1 : -1, c >= 9 ? c - 9 : -1, c < 72 ? c + 9 : -1};
        for (int i = 0; i < 4; i++)
            if (nb[i] >= 0 && !mark[nb[i]] && reg[nb[i]] == r) mark[nb[i]] = 1, stack[n++] = nb[i];
    }
    return seen == 9;
}

void jsu_regions(uint32_t *rng, sv_shape *s, int swaps)
{
    uint8_t *reg = s->region, keep[SV_CELLS];
    sv_boxes(s);
    for (int done = 0, tries = 0; done < swaps && tries < 40 * swaps; tries++) {
        int a = pr_below(rng, SV_CELLS), ra = reg[a];
        /* a cell of a neighbouring region, beside region ra: it takes a's place there, a takes its */
        int cand[SV_CELLS], nc = 0, rb = -1;
        int nb[4] = {a % 9 ? a - 1 : -1, a % 9 < 8 ? a + 1 : -1, a >= 9 ? a - 9 : -1, a < 72 ? a + 9 : -1};
        int k = pr_below(rng, 4);
        for (int i = 0; i < 4 && rb < 0; i++)
            if (nb[(k + i) % 4] >= 0 && reg[nb[(k + i) % 4]] != ra) rb = reg[nb[(k + i) % 4]];
        if (rb < 0) continue;
        for (int c = 0; c < SV_CELLS; c++) {
            if (reg[c] != rb) continue;
            int o[4] = {c % 9 ? c - 1 : -1, c % 9 < 8 ? c + 1 : -1, c >= 9 ? c - 9 : -1, c < 72 ? c + 9 : -1};
            for (int i = 0; i < 4; i++)
                if (o[i] >= 0 && o[i] != a && reg[o[i]] == ra) { cand[nc++] = c; break; }
        }
        if (!nc) continue;
        int b = cand[pr_below(rng, nc)], ma = SV_CELLS - 1 - a, mb = SV_CELLS - 1 - b;
        int self = ma == b;                     /* the swap is its own mirror */
        if (!self && (ma == a || mb == b || ma == b || mb == a)) continue;
        memcpy(keep, reg, sizeof keep);
        reg[a] = (uint8_t)rb;
        reg[b] = (uint8_t)ra;
        if (!self) {
            reg[ma] = (uint8_t)(8 - rb);
            reg[mb] = (uint8_t)(8 - ra);
        }
        int ok = 1;
        for (int r = 0; r < 9 && ok; r++) ok = joined(reg, r);
        for (int c = 0; c < SV_CELLS && ok; c++) ok = reg[SV_CELLS - 1 - c] == 8 - reg[c];
        if (!ok) memcpy(reg, keep, sizeof keep);
        else done++;
    }
}

int jsu_make_at(int level, int floor, uint32_t seed, sv_shape *s, uint8_t *clues, uint8_t *sol, int tries)
{
    if (level < SV_EASY || level > SV_EXPERT) level = SV_EASY;
    if (floor < 0) floor = level == SV_MEDIUM ? 24 : sv_clues_floor(level);   /* at 28 clues most stay easy */
    uint32_t rng = pr_seed(seed);
    sv_shape best;
    uint8_t full[SV_CELLS], p[SV_CELLS], best_p[SV_CELLS], best_sol[SV_CELLS];
    int best_g = -1;
    for (int t = 0; t < tries; t++) {
        int filled = 0;
        for (int k = 0; k < 40 && !filled; k++) {   /* regions some grid fits (most do; a failure is soon seen) */
            memset(s, 0, sizeof *s);
            jsu_regions(&rng, s, 40);
            sv_shape_init(s);
            filled = sv_fill(s, &rng, full, 20000);
        }
        if (!filled) continue;
        /* the passes a grade may take, and (expert) all the trials' passes: well under a second */
        int g = sv_make_givens(s, level, floor, &rng, full, p, 3000, level == SV_EXPERT ? 8000 : 0);
        if (g == level || (t == tries - 1 && best_g < 0 && g >= SV_EASY && g <= level)) {
            memcpy(clues, p, SV_CELLS);
            memcpy(sol, full, SV_CELLS);
            return g;
        }
        if (g >= SV_EASY && g < level && g > best_g) {
            best_g = g;
            best = *s;
            memcpy(best_p, p, SV_CELLS);
            memcpy(best_sol, full, SV_CELLS);
        }
    }
    if (best_g < 0) {                           /* none (never seen): the plain boxes, every clue */
        memset(s, 0, sizeof *s);
        sv_boxes(s);
        sv_shape_init(s);
        sv_fill(s, &rng, sol, 0);
        memcpy(clues, sol, SV_CELLS);
        return SV_EASY;
    }
    *s = best;
    memcpy(clues, best_p, SV_CELLS);
    memcpy(sol, best_sol, SV_CELLS);
    return best_g;
}

void jsu_new(sv_game *g, int level, uint32_t seed)
{
    sv_shape s;
    uint8_t p[SV_CELLS], sol[SV_CELLS];
    if (level < SV_EASY || level > SV_EXPERT) level = SV_EASY;
    jsu_make_at(level, -1, seed, &s, p, sol, level == SV_EXPERT ? 4 : 12);
    sv_load(g, &s, level, p, sol);
    g->seed = seed;
}

int jsu_shape(sv_shape *s, uint8_t *clues, const char *code)
{
    memset(s, 0, sizeof *s);
    if (!sv_regions_decode(s, code) || code[SV_CELLS] != ':' || !sv_clues_decode(clues, code + SV_CELLS + 1) ||
        code[2 * SV_CELLS + 1])
        return 0;
    for (int r = 0; r < 9; r++)
        if (!joined(s->region, r)) return 0;
    sv_shape_init(s);
    return 1;
}

int jsu_decode(sv_game *g, const char *code)
{
    sv_shape s;
    uint8_t p[SV_CELLS];
    return jsu_shape(&s, p, code) && sv_load_level(g, &s, p);
}

void jsu_encode(const sv_shape *s, const uint8_t *clues, char *out)
{
    sv_regions_encode(s, out);
    out[SV_CELLS] = ':';
    sv_clues_encode(clues, out + SV_CELLS + 1);
}
