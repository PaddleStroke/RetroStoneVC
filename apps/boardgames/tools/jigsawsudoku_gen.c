/*
 * Jigsaw Sudoku's levels, made by chance and chosen: src/games/jigsawsudoku_levels.h. Our own puzzles, from the
 * game's own maker (the boxes reshaped into irregular regions, a grid filled by chance, clues taken away in pairs
 * round the centre while the solution stays the only one: jsu_make_at). Each candidate is graded by the solver
 * (sv_grade: the hardest way of reasoning it needs, and the solver's passes, its work) and its clues counted.
 * The chapters: 1 singles with many clues (46 down to 38: clues put back); 2 singles with fewer clues, then the
 * gentlest pairs; 3 pairs and locked digits; 4 and 5 triples, fish and the XY-wing (the easier half, the harder
 * half); 6 a digit tried and followed. Within a chapter the candidates are ranked by work and picked evenly, then go
 * from the easiest to the hardest. Duplicates dropped.
 * The taste (sudoku_gen_craft.h): the regions chosen among several for few one-cell spurs, no long snakes and few plain
 * boxes, gently bent or wild; the clues spread evenly (no bare row, column or region, none crowded).
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/jsugen apps/boardgames/tools/jigsawsudoku_gen.c \
 *       apps/boardgames/src/games/jigsawsudoku_rules.c -lm && /tmp/jsugen > apps/boardgames/src/games/jigsawsudoku_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/jigsawsudoku.h"
#include "sudoku_gen_craft.h"

typedef struct {
    char code[JSU_CODE];
    int level, clues;
    long passes;
    double score;
} cand;

#define POOL 160
static cand pool[POOL];
static int npool;
static cand chosen[JSU_LEVELS];
static int nchosen;

static int seen(const char *code)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].code, code)) return 1;
    for (int i = 0; i < npool; i++)
        if (!strcmp(pool[i].code, code)) return 1;
    return 0;
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static void grade(cand *c, const sv_shape *s, const uint8_t *p)
{
    c->clues = 0;
    for (int i = 0; i < SV_CELLS; i++) c->clues += p[i] != 0;
    c->level = sv_grade(s, p, SV_EXPERT, NULL, &c->passes, 0);
    c->score = c->level * 1000.0 + c->passes + (SV_CELLS - c->clues) * 2.0;
    jsu_encode(s, p, c->code);
}

/* regions with taste: the least ugly of several (cr_regions_ugly: spurs, snakes, boxes left plain), their swaps varied
 * so some are gently bent and some wild; a grid fitted to them. 0: none */
static int nice_regions(sv_shape *s, uint32_t *rng, uint8_t *full)
{
    sv_shape best;
    int best_u = 1 << 30;
    for (int t = 0; t < 12; t++) {
        memset(s, 0, sizeof *s);
        jsu_regions(rng, s, 24 + pr_below(rng, 40));
        int u = cr_regions_ugly(s->region);
        if (u < best_u) best_u = u, best = *s;
    }
    if (best_u > 30) return 0;    /* most come at 40-50; the best of twelve near 30 */
    *s = best;
    sv_shape_init(s);
    return sv_fill(s, rng, full, 20000);
}

/* the pool: `want` puzzles of the level on nice regions, the clues opposite each other round the centre as the
 * regions are, balanced (no bare row, column or region, none crowded); extra: clues put back up to 46 down to 38 */
static void fill_pool(int level, int want, uint32_t seed0, int extra)
{
    static sv_shape s;
    uint8_t p[SV_CELLS], full[SV_CELLS];
    int lop = 0, floor = level == SV_MEDIUM ? 24 : sv_clues_floor(level);
    npool = 0;
    for (uint32_t k = 0; npool < want && npool < POOL && k < 9000; k++) {
        cand c;
        uint32_t rng = pr_seed(seed0 + k * 7919u);
        if (!nice_regions(&s, &rng, full)) continue;
        if (cr_givens(&s, level, floor, CR_ROT2, &rng, full, p, 3000, level == SV_EXPERT ? 8000 : 0) != level)
            continue;
        if (extra) cr_more_clues(CR_ROT2, p, full, 46 - npool * 8 / (want > 1 ? want - 1 : 1), &rng);
        grade(&c, &s, p);
        if (c.level != level || seen(c.code)) continue;
        int bare;
        double bal = cr_balance(s.region, p, &bare);
        if (bare < (level == SV_EASY ? 2 : 1) || bal > 1.45) { lop++; continue; }
        if (extra) c.score = 1000 - c.clues * 10 + c.passes / 50.0;
        pool[npool++] = c;
    }
    fprintf(stderr, "pool level %d: %d (%d lopsided dropped)\n", level, npool, lop);
}
/* k picked evenly over the pool ranked by score, from from% to (from + span)% */
static void pick(int k, int from, int span)
{
    qsort(pool, (size_t)npool, sizeof pool[0], by_score);
    int lo = from * npool / 100, hi = (from + span) * npool / 100 - 1;
    if (hi >= npool) hi = npool - 1;
    for (int i = 0; i < k && nchosen < JSU_LEVELS; i++) {
        int at = lo + (k > 1 ? i * (hi - lo) / (k - 1) : 0);
        chosen[nchosen++] = pool[at];
    }
}

static void sort_chapter(int first)
{
    qsort(chosen + first, (size_t)(nchosen - first), sizeof chosen[0], by_score);
}

int main(void)
{
    fill_pool(SV_EASY, 25, 101, 1);
    pick(25, 0, 100);
    sort_chapter(0);
    fill_pool(SV_EASY, 40, 5001, 0);
    pick(10, 0, 100);
    fill_pool(SV_MEDIUM, 120, 9001, 0);
    pick(15, 0, 40);
    sort_chapter(25);
    pick(25, 42, 58);
    sort_chapter(50);
    fill_pool(SV_HARD, 100, 20001, 0);
    pick(25, 0, 50);
    sort_chapter(75);
    pick(25, 50, 50);
    sort_chapter(100);
    fill_pool(SV_EXPERT, 60, 60001, 0);
    pick(25, 0, 100);
    sort_chapter(125);
    if (nchosen != JSU_LEVELS) {
        fprintf(stderr, "only %d levels\n", nchosen);
        return 1;
    }
    static const char *const LEVEL[4] = {"easy", "medium", "hard", "expert"};
    printf("/*\n * Jigsaw Sudoku's levels: our own, made by tools/jigsawsudoku_gen.c (irregular regions, a grid filled by\n"
           " * chance, clues taken away in pairs round the centre while the solution stays the only one), six chapters of\n"
           " * 25 from the easiest. Each: the regions (81 digits 1..9), ':', the clues (81, '.' or 1..9), row by row\n"
           " * (jsu_decode); in the comment its level (sv_grade), the solver's passes and its clues.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("static const char *const jsu_levels[JSU_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        const cand *c = &chosen[i];
        printf("    \"%s\", /* %d: %s, %ld passes, %d clues */\n", c->code, i + 1, LEVEL[c->level], c->passes,
               c->clues);
    }
    printf("};\n");
    return 0;
}
