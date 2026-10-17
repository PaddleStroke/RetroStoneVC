/*
 * Masyu's levels, made by chance and chosen: src/games/masyu_levels.h. Our own puzzles, from the game's own maker (a
 * loop by chance, its pearls, then pearls taken away while the solver finishes it at its level; masyu_rules.c). Each
 * candidate:
 *   - its grade (bench.h: easy, medium, hard), checked to have one solution;
 *   - the work it asks of the solver: the passes of its reasoning (each a round of the ways allowed; a trial's count
 *     too), and how often the medium ways were needed;
 *   - near-duplicates dropped (the same pearls turned or mirrored).
 * Each chapter has its makes (a size, a level, how many, and the part of the make's candidates, ranked by that work,
 * they are picked from evenly): 6 x 6 at the easy level first, up to 12 x 12 at the hard level. Then each chapter goes
 * from its smallest and easiest to its biggest and hardest.
 * Then the look, as a setter would place them: every level's loop given all the pearls it can take, then the pearls
 * taken away a half-turn pair at a time (a pearl whose partner cell can hold none first) while the solver finishes
 * it at no more than its grade; of a few orders (seeded by the loop), the one with the fewest pearls lacking their
 * half-turn partner, no big square without a pearl and the pearls spread over the four quarters, kept at its grade
 * with one solution (else the level stays as it was).
 * By default the tool takes the loops of the levels as they are (the maker's run is long) and only restyles them, so
 * running it again changes nothing; "fresh" makes new loops first (deterministic too: fixed seeds; long).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/msygen apps/boardgames/tools/masyu_gen.c \
 *       apps/boardgames/src/games/masyu_rules.c && /tmp/msygen > /tmp/m.h && mv /tmp/m.h \
 *       apps/boardgames/src/games/masyu_levels.h            (or "/tmp/msygen fresh")
 * "/tmp/msygen survey N LEVEL": how the work spreads for that size and level (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/masyu.h"
#include "games/masyu_levels.h"
#include "games/puzrand.h"

#define CANDS 80                                /* candidates made for each make */
#define BUDGET 200000L                          /* the solver's passes a candidate may cost */

typedef struct {
    msy_puzzle p;
    int grade, pearls;
    long passes, mediums;
    double score;
    char key[MSY_CELLS + 8];
} cand;

typedef struct { int n, level, count; float lo, hi; } make;
/* a size, a level, how many levels, the part of its candidates (from the least work 0 to the most 1) */
static const make CHAPTER[MSY_CHAPTERS][3] = {
    {{6, BENCH_EASY, 15, 0, 0.8f}, {7, BENCH_EASY, 10, 0, 1}},
    {{8, BENCH_EASY, 10, 0.2f, 1}, {8, BENCH_MEDIUM, 15, 0, 0.7f}},
    {{9, BENCH_MEDIUM, 25, 0.1f, 1}},
    {{9, BENCH_HARD, 10, 0, 0.6f}, {10, BENCH_MEDIUM, 15, 0.2f, 1}},
    {{10, BENCH_HARD, 25, 0.1f, 0.9f}},
    {{12, BENCH_MEDIUM, 10, 0.4f, 1}, {12, BENCH_HARD, 15, 0.2f, 1}},
};

/* the pearls whichever way they lie: the smallest of their eight (turned, mirrored) */
static void canon(const msy_puzzle *p, char *key)
{
    int n = p->n;
    char best[MSY_CELLS + 8], k[MSY_CELLS + 8];
    best[0] = 0;
    for (int t = 0; t < 8; t++) {
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int a = t & 1 ? n - 1 - x : x, b = t & 2 ? n - 1 - y : y, sx = t & 4 ? b : a, sy = t & 4 ? a : b;
                k[y * n + x] = (char)('.' + p->pearl[sy * n + sx]);
            }
        k[n * n] = 0;
        if (!best[0] || strcmp(k, best) < 0) strcpy(best, k);
    }
    strcpy(key, best);
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[MSY_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* one candidate from a seed; 1 when it is of the size and the level and has one solution */
static int candidate(cand *c, int n, int level, uint32_t seed)
{
    uint8_t sol[MSY_LINKS];
    msy_make_ex(n, level, seed, BUDGET, &c->p, sol);
    if (c->p.n != n) return 0;
    c->grade = msy_effort(&c->p, &c->passes, &c->mediums);
    if (c->grade != level || msy_count(&c->p, 2) != 1) return 0;
    c->pearls = 0;
    for (int i = 0; i < n * n; i++) c->pearls += c->p.pearl[i] > 0;
    /* the work: the passes, the medium finds weigh more; fewer pearls, a little more */
    c->score = (double)c->passes + 4.0 * c->mediums - 0.3 * c->pearls;
    canon(&c->p, c->key);
    return 1;
}

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 20 * CANDS; seed++) {
        cand *c = &pool[k];
        if (!candidate(c, m->n, m->level, seed)) continue;
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (!dup) k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

static void survey(int n, int level)
{
    make m = {n, level, 0, 0, 1};
    int k = make_pool(&m, 7);
    printf("%dx%d level %d: %d candidates; work (passes, medium finds, pearls) by tenths:\n", n, n, level, k);
    for (int i = 0; i < 10 && k; i++) {
        const cand *c = &pool[i * (k - 1) / 9];
        printf("  %ld %ld %d\n", c->passes, c->mediums, c->pearls);
    }
}

/* ---- the restyle: the same loops, their pearls chosen again for a hand-set look --------------------------------- */
#define TRIES 8                                 /* the takings-away tried for each level */

/* the pearls a loop can take: a black one where it turns and runs on straight through both next cells, a white one
 * where it goes straight and turns in a next cell (as the maker puts them) */
static void pearls_from(int n, const uint8_t *sol, uint8_t *pearl)
{
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};   /* east, west, south, north */
    int dirs[MSY_CELLS];
    for (int c = 0; c < n * n; c++) {
        int x = c % n, y = c / n, d = 0;
        for (int k = 0; k < 4; k++) {
            int ax = x + DX[k], ay = y + DY[k];
            if (ax < 0 || ay < 0 || ax >= n || ay >= n) continue;
            if (sol[msy_link(n, c < ay * n + ax ? c : ay * n + ax, c < ay * n + ax ? ay * n + ax : c)]) d |= 1 << k;
        }
        dirs[c] = d;
    }
    for (int c = 0; c < n * n; c++) {
        int d = dirs[c], x = c % n, y = c / n;
        pearl[c] = MSY_NO_PEARL;
        if (!d) continue;
        int straight = d == 3 || d == 12;
        if (!straight) {
            int ok = 1;
            for (int k = 0; k < 4; k++)
                if (d >> k & 1) ok &= (dirs[(y + DY[k]) * n + x + DX[k]] >> k & 1) != 0;
            if (ok) pearl[c] = MSY_BLACK;
        } else {
            int turns = 0;
            for (int k = 0; k < 4; k++)
                if (d >> k & 1) turns |= dirs[(y + DY[k]) * n + x + DX[k]] != d;
            if (turns) pearl[c] = MSY_WHITE;
        }
    }
}

/* how the pearls look (0 best): pearls whose half-turn partner has none, then the biggest square without a pearl
 * and the quarters' counts far from even */
static double pearl_look(const msy_puzzle *p)
{
    int n = p->n, lone = 0, big = 0, q[4] = {0}, tot = 0;
    for (int c = 0; c < n * n; c++)
        if (p->pearl[c]) {
            lone += !p->pearl[n * n - 1 - c];
            int x = c % n, y = c / n;
            if (2 * x + 1 != n && 2 * y + 1 != n) q[(2 * x >= n) + 2 * (2 * y >= n)]++, tot++;
        }
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            for (int s = big + 1; x + s <= n && y + s <= n; s++) {
                int empty = 1;
                for (int j = 0; j < s && empty; j++)
                    for (int i = 0; i < s && empty; i++) empty = !p->pearl[(y + j) * n + x + i];
                if (!empty) break;
                big = s;
            }
    double spread = 0;
    for (int i = 0; i < 4 && tot; i++) spread += abs(4 * q[i] - tot) / (double)tot;
    return lone / 2.0 + 3.0 * big / n + spread;
}

static int fits(const msy_puzzle *p, int level)
{
    int g = msy_grade(p, NULL);
    return g >= BENCH_EASY && g <= level;
}

/* the level's loop given new pearls: every pearl it can take, then taken away a half-turn pair at a time (the lone
 * ones first) while the solver finishes it at no more than its grade; the best-looking of a few orders at its grade
 * with one solution, or the level as it was */
static void restyle(cand *c)
{
    int n = c->p.n;
    uint8_t sol[MSY_LINKS], full[MSY_CELLS];
    if (msy_grade(&c->p, sol) != c->grade) return;
    pearls_from(n, sol, full);
    uint32_t h = 2166136261u;
    for (int e = 0; e < msy_links(n); e++) h = (h ^ sol[e]) * 16777619u;
    double best = pearl_look(&c->p);
    for (int t = 0; t < TRIES; t++) {
        uint32_t rng = pr_seed(h + (uint32_t)t * 7919u);
        int lone[MSY_CELLS], pair[MSY_CELLS], nl = 0, np = 0;
        for (int i = 0; i < n * n; i++) {
            int j = n * n - 1 - i;
            if (j < i || (!full[i] && !full[j])) continue;
            if (full[i] && full[j]) pair[np++] = i;
            else lone[nl++] = full[i] ? i : j;
        }
        pr_shuffle(&rng, lone, nl);
        pr_shuffle(&rng, pair, np);
        msy_puzzle q = {0};
        q.n = n;
        memcpy(q.pearl, full, sizeof full);
        if (!fits(&q, c->grade)) continue;
        for (int k = 0; k < nl + np; k++) {
            int i = k < nl ? lone[k] : pair[k - nl], j = n * n - 1 - i;
            uint8_t a = q.pearl[i], b = q.pearl[j];
            q.pearl[i] = q.pearl[j] = MSY_NO_PEARL;
            if (!fits(&q, c->grade)) q.pearl[i] = a, q.pearl[j] = b;
        }
        double lk = pearl_look(&q);
        if (lk >= best || msy_grade(&q, NULL) != c->grade || msy_count(&q, 2) != 1) continue;
        best = lk;
        c->p = q;
    }
    msy_effort(&c->p, &c->passes, &c->mediums);
    c->pearls = 0;
    for (int i = 0; i < n * n; i++) c->pearls += c->p.pearl[i] > 0;
    c->score = (double)c->passes + 4.0 * c->mediums - 0.3 * c->pearls;
    canon(&c->p, c->key);
}

static const char *const GRADE[] = {"easy", "medium", "hard"};

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "survey")) {
        survey(atoi(argv[2]), atoi(argv[3]));
        return 0;
    }
    int fresh = argc == 2 && !strcmp(argv[1], "fresh");
    for (int i = 0; !fresh && i < MSY_LEVELS; i++) {   /* the loops of the levels as they are */
        cand *c = &chosen[nchosen++];
        if (!msy_decode(msy_levels[i], &c->p)) return 1;
        c->grade = msy_grade(&c->p, NULL);
    }
    for (int ch = 0; fresh && ch < MSY_CHAPTERS; ch++) {
        for (int p = 0; p < 3 && CHAPTER[ch][p].count; p++) {
            const make *m = &CHAPTER[ch][p];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 3 + p + 1));
            fprintf(stderr, "chapter %d, make %d (%dx%d %s): %d candidates\n", ch + 1, p + 1, m->n, m->n,
                    GRADE[m->level], k);
            for (int i = 0; i < m->count; i++) {
                int at = (int)((m->lo + (m->hi - m->lo) * (i + 0.5f) / m->count) * (k - 1));
                while (at < k && seen(pool[at].key)) at++;
                if (at >= k) {
                    fprintf(stderr, "chapter %d: not enough of make %d\n", ch + 1, p + 1);
                    return 1;
                }
                chosen[nchosen++] = pool[at];
            }
        }
    }
    for (int i = 0; i < nchosen; i++) {
        restyle(&chosen[i]);
        fprintf(stderr, "\rrestyled %d", i + 1);
    }
    fprintf(stderr, "\n");
    for (int ch = 0; ch < MSY_CHAPTERS; ch++) {
        int first = ch * MSY_LEVELS / MSY_CHAPTERS;
        for (int i = first + 1; i < first + MSY_LEVELS / MSY_CHAPTERS; i++)   /* from its easiest to its hardest */
            for (int j = i; j > first; j--) {
                cand *a = &chosen[j - 1], *b = &chosen[j];
                if (a->p.n * 1e6 + a->grade * 1e4 + a->score <= b->p.n * 1e6 + b->grade * 1e4 + b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
    }
    printf("/*\n"
           " * Masyu's levels: our own, made by tools/masyu_gen.c (the game's maker; each checked to have one\n"
           " * solution), six chapters of 25 from the easiest. Each: \"<n>:\" and a character a cell, row by row\n"
           " * (masyu.h, msy_decode); in the comment its size, its grade and the work it asks of the solver (its\n"
           " * passes, the medium finds).\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const msy_levels[MSY_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[MSY_CELLS + 64];
        if (!msy_encode(&chosen[i].p, code, sizeof code)) return 1;
        printf("    \"%s\", /* %d: %dx%d %s, %ld passes, %ld medium */\n", code, i + 1, chosen[i].p.n, chosen[i].p.n,
               GRADE[chosen[i].grade], chosen[i].passes, chosen[i].mediums);
    }
    printf("};\n");
    return nchosen == MSY_LEVELS ? 0 : 1;
}
