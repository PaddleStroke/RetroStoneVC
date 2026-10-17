/*
 * Skyscrapers' levels, made by chance and chosen: src/games/skyscrapers_levels.h. Our own puzzles, made here with the
 * game's solver and grader (skyscrapers_rules.c): a town by chance, every edge number told, then numbers and heights
 * taken away while the solution stays the only one and the grader still solves it within its level, always with the
 * clue's images under a symmetry (a half turn, a mirror; a quarter turn for each chapter's last level, its showpiece,
 * with no height given when one can be found), so the numbers stand balanced round the town, as a puzzle made by hand
 * does; every side keeps a number. Each candidate:
 *   - its grade (bench.h: easy, medium, hard), checked to have one solution;
 *   - the work it asks of the solver: the passes of its reasoning (each a round of the ways allowed; a trial's count
 *     too), and how often the medium way was needed;
 *   - near-duplicates dropped (the same town and numbers turned or mirrored).
 * Each chapter has its makes (a size, a level, how many, and the part of the make's candidates, ranked by that work,
 * they are picked from evenly): 4 x 4 at the easy level first, up to 7 x 7 at the hard level. Then each chapter
 * goes from its easiest to its hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/skygen apps/boardgames/tools/skyscrapers_gen.c \
 *       apps/boardgames/src/games/skyscrapers_rules.c && /tmp/skygen > apps/boardgames/src/games/skyscrapers_levels.h
 * "/tmp/skygen survey N LEVEL": how the work spreads for that size and level (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/skyscrapers.h"

#define CANDS 150                               /* candidates made for each make */

typedef struct {
    sky_puzzle p;
    int grade, numbers, sym;                    /* numbers: the clues (edge numbers and heights); sym: SYM_* */
    long passes, mediums;
    double score;
    char key[SKY_CELLS + 64];
} cand;

typedef struct { int n, level, count; float lo, hi; } make;
/* a size, a level, how many levels, the part of its candidates (from the least work 0 to the most 1) */
static const make CHAPTER[SKY_CHAPTERS][3] = {
    {{4, BENCH_EASY, 12, 0, 1}, {5, BENCH_EASY, 13, 0, 0.6f}},
    {{5, BENCH_EASY, 8, 0.5f, 1}, {5, BENCH_MEDIUM, 17, 0, 0.6f}},
    {{5, BENCH_MEDIUM, 8, 0.6f, 1}, {6, BENCH_MEDIUM, 17, 0, 0.8f}},
    {{6, BENCH_MEDIUM, 8, 0.7f, 1}, {6, BENCH_HARD, 17, 0, 0.4f}},
    {{6, BENCH_HARD, 25, 0.3f, 0.95f}},
    {{7, BENCH_HARD, 25, 0.15f, 1}},
};

/* the puzzle whichever way it lies: the smallest of its eight (turned, mirrored), drawn as a square of n + 2 with the
 * edge numbers round the heights given */
static void canon(const sky_puzzle *p, char *key)
{
    int n = p->n, m = n + 2;
    char sq[(SKY_MAX + 2) * (SKY_MAX + 2)], best[SKY_CELLS + 64], k[SKY_CELLS + 64];
    memset(sq, '.', sizeof sq);
    for (int i = 0; i < n; i++) {
        sq[i + 1] = (char)('0' + p->clue[SKY_TOP][i]);
        sq[(i + 1) * m + m - 1] = (char)('0' + p->clue[SKY_RIGHT][i]);
        sq[(m - 1) * m + i + 1] = (char)('0' + p->clue[SKY_BOTTOM][i]);
        sq[(i + 1) * m] = (char)('0' + p->clue[SKY_LEFT][i]);
    }
    for (int c = 0; c < n * n; c++) sq[(c / n + 1) * m + c % n + 1] = (char)('0' + p->given[c]);
    best[0] = 0;
    for (int t = 0; t < 8; t++) {
        for (int y = 0; y < m; y++)
            for (int x = 0; x < m; x++) {
                int a = t & 1 ? m - 1 - x : x, b = t & 2 ? m - 1 - y : y, sx = t & 4 ? b : a, sy = t & 4 ? a : b;
                k[y * m + x] = sq[sy * m + sx];
            }
        k[m * m] = 0;
        if (!best[0] || strcmp(k, best) < 0) strcpy(best, k);
    }
    strcpy(key, best);
}

/* ---- the handmade look: clues placed with a symmetry ---------------------------------------------------------------
 * The town's edge numbers and given heights drawn as a square of n + 2 (the numbers round the heights); a clue is
 * taken away or kept with all its images under the puzzle's symmetry (a half turn, a mirror, or a quarter turn for the
 * chapters' showpieces), so the numbers stand balanced on all four sides, as in a puzzle made by hand. */
enum { SYM_HALF, SYM_MIRROR, SYM_QUARTER, SYMS };

/* a clue (0 .. 4n - 1 edge * n + i, then 4n + cell) to its place on the square, and back (-1: a corner) */
static void place_of(int n, int id, int *x, int *y)
{
    int m = n + 2;
    if (id >= 4 * n) { *x = (id - 4 * n) % n + 1; *y = (id - 4 * n) / n + 1; return; }
    int e = id / n, i = id % n;
    switch (e) {
    case SKY_TOP: *x = i + 1; *y = 0; break;
    case SKY_RIGHT: *x = m - 1; *y = i + 1; break;
    case SKY_BOTTOM: *x = i + 1; *y = m - 1; break;
    default: *x = 0; *y = i + 1; break;
    }
}

static int id_at(int n, int x, int y)
{
    int m = n + 2;
    if (x > 0 && x < m - 1 && y > 0 && y < m - 1) return 4 * n + (y - 1) * n + x - 1;
    if (y == 0 && x > 0 && x < m - 1) return SKY_TOP * n + x - 1;
    if (y == m - 1 && x > 0 && x < m - 1) return SKY_BOTTOM * n + x - 1;
    if (x == 0 && y > 0 && y < m - 1) return SKY_LEFT * n + y - 1;
    if (x == m - 1 && y > 0 && y < m - 1) return SKY_RIGHT * n + y - 1;
    return -1;
}

static int image(int n, int sym, int id)
{
    int m = n + 2, x, y;
    place_of(n, id, &x, &y);
    switch (sym) {
    case SYM_HALF: return id_at(n, m - 1 - x, m - 1 - y);
    case SYM_MIRROR: return id_at(n, m - 1 - x, y);
    default: return id_at(n, m - 1 - y, x);
    }
}

/* the clue's images (itself first); their count */
static int orbit(int n, int sym, int id, int *out)
{
    int k = 0;
    for (int c = id; k == 0 || c != id; c = image(n, sym, c)) out[k++] = c;
    return k;
}

static void clue_on(sky_puzzle *q, const uint8_t *full_clue, const uint8_t *sol, int id, int on)
{
    int n = q->n;
    if (id < 4 * n) q->clue[id / n][id % n] = on ? full_clue[id] : 0;
    else q->given[id - 4 * n] = on ? sol[id - 4 * n] : 0;
}

/* a Latin square by chance (a cell at a time, its digits in a random order, back when stuck) */
static int latin_fill(int n, uint8_t *g, int c, uint32_t *s)
{
    if (c == n * n) return 1;
    int ds[SKY_MAX];
    for (int i = 0; i < n; i++) ds[i] = i + 1;
    pr_shuffle(s, ds, n);
    for (int i = 0; i < n; i++) {
        int ok = 1;
        for (int j = 0; j < c % n && ok; j++) ok = g[c - c % n + j] != ds[i];
        for (int j = c % n; j < c && ok; j += n) ok = g[j] != ds[i];
        if (!ok) continue;
        g[c] = (uint8_t)ds[i];
        if (latin_fill(n, g, c + 1, s)) return 1;
    }
    g[c] = 0;
    return 0;
}

static int sym_make(int n, int level, int sym, uint32_t seed, sky_puzzle *q)
{
    uint32_t s = pr_seed(seed);
    uint8_t sol[SKY_CELLS] = {0}, full[4 * SKY_MAX];
    if (!latin_fill(n, sol, 0, &s)) return 0;
    memset(q, 0, sizeof *q);
    q->n = n;
    for (int e = 0; e < 4; e++)
        for (int i = 0; i < n; i++) {
            uint8_t hs[SKY_MAX];
            for (int j = 0; j < n; j++) hs[j] = sol[sky_line(n, e, i, j)];
            q->clue[e][i] = full[e * n + i] = (uint8_t)sky_seen(hs, n);
        }
    int lv = sky_grade(q, NULL);
    int cells[SKY_CELLS], orb[4];
    for (int c = 0; c < n * n; c++) cells[c] = 4 * n + c;
    pr_shuffle(&s, cells, n * n);
    for (int i = 0; i < n * n && (lv < BENCH_EASY || lv > level); i++) {   /* heights given, if need be */
        int k = orbit(n, sym, cells[i], orb);
        for (int j = 0; j < k; j++) clue_on(q, full, sol, orb[j], 1);
        lv = sky_grade(q, NULL);
    }
    if (lv < BENCH_EASY || lv > level) return 0;
    /* then taken away, orbit by orbit: the heights first, then the edge numbers */
    int order[4 * SKY_MAX + SKY_CELLS], no = 0, mark[4 * SKY_MAX + SKY_CELLS] = {0};
    for (int i = 0; i < n * n; i++)
        if (q->given[cells[i] - 4 * n]) order[no++] = cells[i];
    int edges = no;
    for (int i = 0; i < 4 * n; i++) order[no++] = i;
    pr_shuffle(&s, order + edges, 4 * n);
    for (int i = 0; i < no; i++) {
        if (mark[order[i]]) continue;
        int k = orbit(n, sym, order[i], orb);
        for (int j = 0; j < k; j++) mark[orb[j]] = 1;
        for (int j = 0; j < k; j++) clue_on(q, full, sol, orb[j], 0);
        int g = sky_count(q, 2) == 1 ? sky_grade(q, NULL) : -1;
        if (g < BENCH_EASY || g > level)
            for (int j = 0; j < k; j++) clue_on(q, full, sol, orb[j], 1);
    }
    /* the numbers on every side */
    for (int e = 0; e < 4; e++) {
        int any = 0;
        for (int i = 0; i < n; i++) any |= q->clue[e][i] != 0;
        if (!any) return 0;
    }
    return sky_grade(q, NULL) == level;
}

static int by_score(const void *a, const void *b)
{
    const cand *x = a, *y = b;
    return x->score < y->score ? -1 : x->score > y->score;
}

static cand pool[CANDS];
static cand chosen[SKY_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* one candidate from a seed; 1 when it is of the level and has one solution */
static int candidate(cand *c, int n, int level, uint32_t seed)
{
    /* mostly a half turn, some mirrors, a few quarter turns (the showpieces' pick) */
    c->sym = seed % 6 < 3 ? SYM_HALF : seed % 6 < 5 ? SYM_MIRROR : SYM_QUARTER;
    if (!sym_make(n, level, c->sym, seed, &c->p)) return 0;
    c->grade = sky_effort(&c->p, &c->passes, &c->mediums);
    if (c->grade != level || sky_count(&c->p, 2) != 1) return 0;
    c->numbers = 0;
    for (int i = 0; i < n * n; i++) c->numbers += c->p.given[i] != 0;
    for (int e = 0; e < 4; e++)
        for (int i = 0; i < n; i++) c->numbers += c->p.clue[e][i] != 0;
    /* the work: the passes, the medium finds weigh more; fewer clues ask a little more */
    c->score = (double)c->passes + 4.0 * c->mediums - 0.2 * c->numbers;
    canon(&c->p, c->key);
    return 1;
}

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 40 * CANDS; seed++) {
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
    printf("%dx%d level %d: %d candidates; work (passes, medium finds, numbers) by tenths:\n", n, n, level, k);
    for (int i = 0; i < 10 && k; i++) {
        const cand *c = &pool[i * (k - 1) / 9];
        printf("  %ld %ld %d\n", c->passes, c->mediums, c->numbers);
    }
}

static const char *const GRADE[] = {"easy", "medium", "hard"};
static const char *const SYMNAME[] = {"half turn", "mirror", "quarter turn"};

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "survey")) {
        survey(atoi(argv[2]), atoi(argv[3]));
        return 0;
    }
    for (int ch = 0; ch < SKY_CHAPTERS; ch++) {
        int first = nchosen, k = 0;
        for (int p = 0; p < 3 && CHAPTER[ch][p].count; p++) {
            const make *m = &CHAPTER[ch][p];
            k = make_pool(m, 1000u * (uint32_t)(ch * 3 + p + 1));
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
        /* the chapter's showpiece, its last level: the hardest quarter-turn town of its last make (still in the
         * pool), with no height given when there is one, in place of that make's hardest pick */
        int last = -1;
        for (int pass = 0; pass < 2 && last < 0; pass++)
            for (int i = k - 1; i >= 0 && last < 0; i--) {
                int givens = 0;
                for (int c = 0; c < pool[i].p.n * pool[i].p.n; c++) givens += pool[i].p.given[c] != 0;
                if (pool[i].sym == SYM_QUARTER && (pass || !givens) && !seen(pool[i].key)) last = i;
            }
        if (last >= 0) {
            int worst = -1;
            for (int i = first; i < nchosen; i++)
                if (chosen[i].p.n == pool[last].p.n && chosen[i].grade == pool[last].grade &&
                    (worst < 0 || chosen[i].score >= chosen[worst].score)) worst = i;
            if (worst >= 0) chosen[worst] = pool[last];
            else last = -1;
        }
        for (int i = first + 1; i < nchosen; i++)   /* the chapter from its easiest to its hardest */
            for (int j = i; j > first; j--) {
                cand *a = &chosen[j - 1], *b = &chosen[j];
                if (a->grade * 1e6 + a->p.n * 1e4 + a->score <= b->grade * 1e6 + b->p.n * 1e4 + b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
        for (int i = first; last >= 0 && i < nchosen - 1; i++)   /* the showpiece last */
            if (!strcmp(chosen[i].key, pool[last].key)) {
                cand t = chosen[i];
                memmove(&chosen[i], &chosen[i + 1], (size_t)(nchosen - 1 - i) * sizeof t);
                chosen[nchosen - 1] = t;
                break;
            }
    }
    printf("/*\n"
           " * Skyscrapers' levels: our own, made by tools/skyscrapers_gen.c (the game's maker; each checked to have\n"
           " * one solution), six chapters of 25 from the easiest. Each: \"<n>:\", the edge numbers, then a character a\n"
           " * cell, row by row (skyscrapers.h, sky_decode); in the comment its size, its grade and the work it asks of\n"
           " * the solver (its passes, the medium finds).\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const sky_levels[SKY_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[SKY_CELLS + 64];
        if (!sky_encode(&chosen[i].p, code, sizeof code)) return 1;
        printf("    \"%s\", /* %d: %dx%d %s, %ld passes, %ld medium, %s */\n", code, i + 1, chosen[i].p.n,
               chosen[i].p.n, GRADE[chosen[i].grade], chosen[i].passes, chosen[i].mediums, SYMNAME[chosen[i].sym]);
    }
    printf("};\n");
    return nchosen == SKY_LEVELS ? 0 : 1;
}
