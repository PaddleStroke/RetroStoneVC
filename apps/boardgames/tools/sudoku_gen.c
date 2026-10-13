/*
 * Sudoku's levels, made by chance and chosen: src/games/sudoku_levels.h. Our own puzzles, from the game's own maker
 * (a full grid filled by chance, clues taken away in pairs round the centre while the solution stays the only one).
 * Each candidate is graded twice, as a person would solve it:
 *   - by the ways of reasoning it needs (su_rate_steps): singles; pairs and locked candidates; triples and the
 *     X-wing; quads, the swordfish and the XY-wing; and how many steps of each;
 *   - by the bench (su_bench): its grade (an expert puzzle must need no more than a digit tried and followed to a
 *     contradiction: never a guess) and the solver's passes, its work;
 *   and by its clues.
 * The chapters: 1 singles only, with many clues (46 down to 38); 2 singles with fewer clues, then the gentlest pairs;
 * 3 pairs and locked candidates; 4 triples and the X-wing; 5 quads, the swordfish and the XY-wing; 6 a digit tried
 * and followed. Within a make the candidates are ranked by difficulty and picked evenly, then each chapter goes from
 * its easiest to its hardest. Duplicates dropped (the same clues).
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/sugen apps/boardgames/tools/sudoku_gen.c \
 *       apps/boardgames/src/games/sudoku_rules.c && /tmp/sugen > apps/boardgames/src/games/sudoku_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/sudoku.h"

typedef struct {
    uint8_t p[SU_CELLS], sol[SU_CELLS];
    int rate, bench, clues, steps[4];
    long passes;
    double score;
    char key[SU_CELLS + 1];
} cand;

#define POOL 700
static cand pool[POOL];
static int npool;
static cand chosen[SU_LEVELS];
static int nchosen;

static void grade(cand *c)
{
    c->rate = su_rate_steps(c->p, c->steps);
    c->bench = su_bench(c->p, &c->passes);
    c->clues = 0;
    for (int i = 0; i < SU_CELLS; i++) {
        c->clues += c->p[i] != 0;
        c->key[i] = c->p[i] ? (char)('0' + c->p[i]) : '.';
    }
    c->key[SU_CELLS] = 0;
}

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    for (int i = 0; i < npool; i++)
        if (!strcmp(pool[i].key, key)) return 1;
    return 0;
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

/* clues put back from the solution, in pairs round the centre, until there are `target` */
static void more_clues(cand *c, int target, uint32_t *rng)
{
    int order[41];
    for (int i = 0; i < 41; i++) order[i] = i;
    pr_shuffle(rng, order, 41);
    int n = 0;
    for (int i = 0; i < SU_CELLS; i++) n += c->p[i] != 0;
    for (int i = 0; i < 41 && n < target; i++) {
        int a = order[i], b = SU_CELLS - 1 - a;
        if (c->p[a]) continue;
        c->p[a] = c->sol[a];
        c->p[b] = c->sol[b];
        n += a == b ? 1 : 2;
    }
}

/* the pool made: `want` candidates of the maker's level whose rate is in rlo..rhi (and bench not a guess) */
static void fill_pool(int level, int rlo, int rhi, int want, uint32_t seed0)
{
    npool = 0;
    for (uint32_t k = 0; npool < want && npool < POOL && k < 40000; k++) {
        cand c;
        su_make(level, seed0 + k * 7919u, c.p, c.sol);
        grade(&c);
        if (c.rate < rlo || c.rate > rhi || c.bench < BENCH_EASY || c.bench == BENCH_GUESS || seen(c.key)) continue;
        pool[npool++] = c;
    }
    fprintf(stderr, "pool level %d rate %d-%d: %d\n", level, rlo, rhi, npool);
}

/* k picked evenly over the pool ranked by score (from its easiest) */
static void pick(int k, int from, int span)
{
    qsort(pool, (size_t)npool, sizeof pool[0], by_score);
    int lo = from * npool / 100, hi = (from + span) * npool / 100 - 1;
    if (hi >= npool) hi = npool - 1;
    for (int i = 0; i < k; i++) {
        int at = lo + (k > 1 ? i * (hi - lo) / (k - 1) : 0);
        chosen[nchosen++] = pool[at];
    }
}

static void sort_chapter(int first)
{
    qsort(chosen + first, (size_t)(nchosen - first), sizeof chosen[0], by_score);
}

static double score_of(const cand *c)
{
    return c->rate * 1000.0 + c->steps[3] * 40 + c->steps[2] * 25 + c->steps[1] * 8 + (81 - c->clues) * 2 +
           c->passes / 20.0;
}

int main(void)
{
    uint32_t rng = pr_seed(2026);
    /* chapter 1: singles, 46 down to 38 clues */
    fill_pool(SU_EASY, 0, 0, 60, 101);
    for (int i = 0; i < npool; i++) pool[i].score = (double)pool[i].passes;
    qsort(pool, (size_t)npool, sizeof pool[0], by_score);
    for (int i = 0; i < 25; i++) {
        cand c = pool[i];
        more_clues(&c, 46 - i * 8 / 24, &rng);
        grade(&c);
        c.score = 1000 - c.clues * 10 + c.passes / 50.0;
        chosen[nchosen++] = c;
    }
    sort_chapter(0);
    /* chapter 2: singles with fewer clues, then the gentlest pairs; chapter 3 the rest of the pairs */
    fill_pool(SU_EASY, 0, 0, 80, 5001);
    for (int i = 0; i < npool; i++) pool[i].score = score_of(&pool[i]);
    pick(10, 0, 100);
    fill_pool(SU_MEDIUM, 1, 1, 200, 9001);
    for (int i = 0; i < npool; i++) pool[i].score = score_of(&pool[i]);
    pick(15, 0, 40);
    sort_chapter(25);
    pick(25, 42, 58);
    sort_chapter(50);
    /* chapter 4: triples and the X-wing */
    fill_pool(SU_HARD, 2, 2, 100, 20001);
    for (int i = 0; i < npool; i++) pool[i].score = score_of(&pool[i]);
    pick(25, 0, 100);
    sort_chapter(75);
    /* chapter 5: quads, the swordfish, the XY-wing */
    fill_pool(SU_HARD, 3, 3, 120, 60001);
    for (int i = 0; i < npool; i++) pool[i].score = score_of(&pool[i]);
    pick(25, 0, 100);
    sort_chapter(100);
    /* chapter 6: a digit tried and followed */
    fill_pool(SU_EXPERT, 4, 4, 120, 90001);
    for (int i = 0; i < npool; i++) pool[i].score = score_of(&pool[i]);
    pick(25, 0, 100);
    sort_chapter(125);
    if (nchosen != SU_LEVELS) {
        fprintf(stderr, "only %d levels\n", nchosen);
        return 1;
    }
    static const char *const RATE[5] = {"singles", "pairs", "triples, X-wing", "quads, swordfish, XY-wing",
                                        "a digit tried"};
    static const char *const BENCH[4] = {"easy", "medium", "hard", "guess"};
    printf("/*\n * Sudoku's levels: our own, made by tools/sudoku_gen.c (a grid filled by chance, clues taken away in\n"
           " * pairs round the centre while the solution stays the only one), six chapters of 25 from the easiest.\n"
           " * Each: 81 characters, row by row ('.' empty, else a clue; su_decode); in the comment the hardest way of\n"
           " * reasoning it needs (su_rate_steps), the bench's grade and passes (su_bench), its clues.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("static const char *const su_levels[SU_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        const cand *c = &chosen[i];
        printf("    \"%s\", /* %d: %s, bench %s %ld, %d clues */\n", c->key, i + 1, RATE[c->rate], BENCH[c->bench],
               c->passes, c->clues);
    }
    printf("};\n");
    return 0;
}
