/*
 * Killer Sudoku's levels, made by chance and chosen: src/games/killersudoku_levels.h. Our own puzzles, from the
 * game's own maker (a full grid filled by chance, every cell its own cage, then neighbouring cages merged while the
 * puzzle keeps one solution within its level: ksu_make_at). Each candidate is graded by the solver (sv_grade: the
 * hardest way of reasoning it needs, and the solver's passes, its work) and counted (its cages, the cages of one cell).
 * The chapters: 1 easy, cages of at most three cells; 2 easy with cages up to four, then the gentlest medium ones;
 * 3 medium (pairs, locked digits, the rule of 45 on a unit); 4 hard with cages up to four; 5 hard with cages up to
 * five, the harder half; 6 expert (a digit tried and followed). Within a chapter the candidates are ranked by their
 * work and picked evenly, then go from the easiest to the hardest. Duplicates dropped.
 * The taste (sudoku_gen_craft.h): the cages are merged together with their mirror images, so the cages' pattern keeps a
 * setter's symmetry, mixed along a chapter (half and quarter turns, mirrors, the diagonal); each chapter ends with its
 * showpiece, its hardest with a quarter turn or both mirrors.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/ksugen apps/boardgames/tools/killersudoku_gen.c \
 *       apps/boardgames/src/games/killersudoku_rules.c -lm && /tmp/ksugen > apps/boardgames/src/games/killersudoku_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/killersudoku.h"
#include "sudoku_gen_craft.h"

typedef struct {
    char code[KSU_CODE];
    int level, cages, ones, most, mode;
    long passes;
    double score;
} cand;

#define POOL 120
static cand pool[POOL];
static int npool;
static cand chosen[KSU_LEVELS];
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

/* the pool: `want` puzzles of the level, cages at most `most` cells, few cages of one cell; the cages' pattern keeps a
 * setter's symmetry (cr_mode_for: half and quarter turns, mirrors, the diagonal, mixed along the pool) */
static void fill_pool(int level, int most, int want, uint32_t seed0, int chapter)
{
    static sv_shape s;
    uint8_t sol[SV_CELLS];
    npool = 0;
    for (uint32_t k = 0; npool < want && npool < POOL && k < 6000; k++) {
        cand c;
        uint32_t rng = pr_seed(seed0 + k * 7919u);
        int mode = cr_mode_for(npool + (int)k % 3, chapter, 0x1f);
        memset(&s, 0, sizeof s);
        sv_boxes(&s);
        sv_shape_init(&s);
        if (!sv_fill(&s, &rng, sol, 0)) continue;
        if (cr_cages(&s, level, most, mode, &rng, sol, 2000, level == SV_EXPERT ? 4000 : 0) != level) continue;
        c.level = level;
        c.most = most;
        c.mode = mode;
        c.cages = s.ncages;
        c.ones = 0;
        for (int q = 0; q < s.ncages; q++) c.ones += s.cage[q].n == 1;
        if (c.ones > 3 || !sv_cages_encode(&s, c.code, sizeof c.code) || seen(c.code)) continue;
        sv_grade(&s, NULL, SV_EXPERT, NULL, &c.passes, 0);
        c.score = c.passes + (40 - c.cages) * 6.0 + c.ones * -10.0;
        pool[npool++] = c;
    }
    fprintf(stderr, "pool level %d most %d: %d\n", level, most, npool);
}

/* k picked evenly over the pool ranked by score, from from% to (from + span)% */
static void pick(int k, int from, int span)
{
    qsort(pool, (size_t)npool, sizeof pool[0], by_score);
    int lo = from * npool / 100, hi = (from + span) * npool / 100 - 1;
    if (hi >= npool) hi = npool - 1;
    for (int i = 0; i < k && nchosen < KSU_LEVELS; i++) {
        int at = lo + (k > 1 ? i * (hi - lo) / (k - 1) : 0);
        chosen[nchosen++] = pool[at];
    }
}

/* the chapter from its easiest to its hardest; its showpiece last: the hardest of those with a quarter turn or both
 * mirrors (the setters' grandest symmetries), when there is one */
static void sort_chapter(int first)
{
    qsort(chosen + first, (size_t)(nchosen - first), sizeof chosen[0], by_score);
    int show = -1;
    for (int i = first; i < nchosen; i++)
        if (chosen[i].mode == CR_ROT4 || chosen[i].mode == CR_BOTH) show = i;
    if (show < 0) return;
    cand c = chosen[show];
    memmove(chosen + show, chosen + show + 1, (size_t)(nchosen - 1 - show) * sizeof chosen[0]);
    chosen[nchosen - 1] = c;
}

int main(void)
{
    fill_pool(SV_EASY, 3, 50, 101, 0);
    pick(25, 0, 70);
    sort_chapter(0);
    fill_pool(SV_EASY, 4, 30, 5001, 1);
    pick(12, 0, 100);
    fill_pool(SV_MEDIUM, 4, 50, 9001, 1);
    pick(13, 0, 40);
    sort_chapter(25);
    fill_pool(SV_MEDIUM, 5, 50, 13001, 2);
    pick(25, 20, 80);
    sort_chapter(50);
    fill_pool(SV_HARD, 4, 50, 20001, 3);
    pick(25, 0, 100);
    sort_chapter(75);
    fill_pool(SV_HARD, 5, 60, 40001, 4);
    pick(25, 40, 60);
    sort_chapter(100);
    fill_pool(SV_EXPERT, 5, 50, 60001, 5);
    pick(25, 0, 100);
    sort_chapter(125);
    if (nchosen != KSU_LEVELS) {
        fprintf(stderr, "only %d levels\n", nchosen);
        return 1;
    }
    static const char *const LEVEL[4] = {"easy", "medium", "hard", "expert"};
    printf("/*\n * Killer Sudoku's levels: our own, made by tools/killersudoku_gen.c (a grid filled by chance, its cells'\n"
           " * cages merged while the solution stays the only one), six chapters of 25 from the easiest. Each: the cages'\n"
           " * code (sv_cages_decode: each cell's joins, ':', the sums); in the comment its level (sv_grade), the most\n"
           " * cells a cage may have, its cages and the solver's passes.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("static const char *const ksu_levels[KSU_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        const cand *c = &chosen[i];
        printf("    \"%s\", /* %d: %s, cages of %d at most, %d cages, %ld passes */\n", c->code, i + 1, LEVEL[c->level],
               c->most, c->cages, c->passes);
    }
    printf("};\n");
    return 0;
}
