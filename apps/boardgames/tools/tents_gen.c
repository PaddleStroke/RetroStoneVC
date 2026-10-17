/*
 * Tents' levels, made by chance and chosen as by hand: src/games/tents_levels.h. Our own puzzles, from the game's own
 * maker (tents put by chance where they touch none, a tree beside each, fields drawn until the solver solves one at
 * its level; tents_rules.c). Each candidate:
 *   - its grade (bench.h: easy, medium, hard), checked to have one solution;
 *   - the work it asks of the solver: the passes of its reasoning (each a round of the ways allowed; a trial's count
 *     too), and how often the medium way was needed;
 *   - its look (taste): the trees spread over the four quarters, no blank strip, no clumps, trees answering each
 *     other across the centre;
 *   - near-duplicates dropped (the same trees turned or mirrored).
 * Each chapter has its makes (a size, a level, how many, and the part of the make's candidates, ranked by that work,
 * they are picked from): each pick takes the best-looking candidate of its own slice of that part, so the work
 * still climbs evenly. 5 x 5 at the easy level first, up to 12 x 12 at the hard level. Then each chapter goes from
 * its easiest to its hardest, and ends on a showpiece: a field whose trees answer each other across the centre (a
 * half turn), at the chapter's last size and level, a little harder than the chapter's hardest.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/tngen apps/boardgames/tools/tents_gen.c \
 *       apps/boardgames/src/games/tents_rules.c && /tmp/tngen > apps/boardgames/src/games/tents_levels.h
 * "/tmp/tngen survey N LEVEL": how the work spreads for that size and level (choosing the makes).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/tents.h"
#include "games/puzrand.h"

#define CANDS 400                               /* candidates made for each make */
#define SHOWS 60                                /* symmetric candidates looked at for a showpiece */

typedef struct {
    tn_puzzle p;
    int grade, numbers;                         /* numbers: the trees */
    long passes, mediums;
    double score, taste;
    char key[TN_CELLS + 8];
} cand;

typedef struct { int n, level, count; float lo, hi; } make;
/* a size, a level, how many levels, the part of its candidates (from the least work 0 to the most 1); the chapter's
 * last make gives one level fewer: the showpiece takes its place */
static const make CHAPTER[TN_CHAPTERS][3] = {
    {{5, BENCH_EASY, 10, 0, 0.9f}, {6, BENCH_EASY, 15, 0, 1}},
    {{7, BENCH_EASY, 10, 0.3f, 1}, {7, BENCH_MEDIUM, 15, 0, 0.6f}},
    {{8, BENCH_MEDIUM, 25, 0.1f, 1}},
    {{9, BENCH_MEDIUM, 10, 0.3f, 1}, {9, BENCH_HARD, 15, 0, 0.5f}},
    {{10, BENCH_HARD, 25, 0.1f, 0.9f}},
    {{12, BENCH_HARD, 25, 0.2f, 1}},
};

/* the trees whichever way they lie: the smallest of their eight (turned, mirrored) */
static void canon(const tn_puzzle *p, char *key)
{
    int n = p->n;
    char best[TN_CELLS + 8], k[TN_CELLS + 8];
    best[0] = 0;
    for (int t = 0; t < 8; t++) {
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) {
                int a = t & 1 ? n - 1 - x : x, b = t & 2 ? n - 1 - y : y, sx = t & 4 ? b : a, sy = t & 4 ? a : b;
                k[y * n + x] = p->tree[sy * n + sx] ? 'T' : '.';
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
static cand chosen[TN_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

/* the look of a field, lower the better: the trees spread over the four quarters, no blank strip (a line with no
 * tree and no tent), no clumps (trees in a 2 x 2 block or three in a line), and trees answering each other across
 * the centre (the half-turn symmetry hand-set puzzles like) */
static double taste(const tn_puzzle *p)
{
    int n = p->n, t = 0, sym = 0, clump = 0, blank = 0;
    double q[4] = {0, 0, 0, 0};
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            int c = y * n + x;
            if (!p->tree[c]) continue;
            t++;
            for (int k = 0; k < 4; k++) {           /* a tree on the middle line is half in each */
                double wx = n % 2 && x == n / 2 ? 0.5 : (k & 1) == (x >= n / 2);
                double wy = n % 2 && y == n / 2 ? 0.5 : (k >> 1) == (y >= n / 2);
                q[k] += wx * wy;
            }
            sym += p->tree[n * n - 1 - c];
            if (x + 2 < n && p->tree[c + 1] && p->tree[c + 2]) clump++;
            if (y + 2 < n && p->tree[c + n] && p->tree[c + 2 * n]) clump++;
            if (x + 1 < n && y + 1 < n && p->tree[c + 1] && p->tree[c + n] && p->tree[c + n + 1]) clump += 2;
        }
    for (int way = 0; way < 2; way++)
        for (int i = 0; i < n; i++) {
            int trees = 0;
            for (int j = 0; j < n; j++) trees += p->tree[way ? j * n + i : i * n + j];
            blank += !trees && p->count[way][i] == 0;
        }
    double bal = 0;
    for (int k = 0; k < 4; k++) bal += q[k] > t / 4.0 ? q[k] - t / 4.0 : t / 4.0 - q[k];
    return t ? 3.0 * bal / t + 0.6 * blank + 0.5 * clump - 1.0 * sym / t : 99;
}

/* a showpiece's field: tents and their trees set in pairs answering each other across the centre (a half turn) */
static void symmetric(int n, uint32_t *s, tn_puzzle *p, uint8_t *sol)
{
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    int cells = n * n, want = cells / 5, tents = 0, order[TN_CELLS];
    memset(p, 0, sizeof *p);
    memset(sol, 0, (size_t)cells);
    p->n = n;
    for (int c = 0; c < cells; c++) order[c] = c;
    pr_shuffle(s, order, cells);
    for (int i = 0; i < cells && tents + 1 < want; i++) {
        int c = order[i], c2 = cells - 1 - c, ok = c != c2 && !p->tree[c] && !p->tree[c2];
        for (int h = 0; h < 2 && ok; h++) {     /* neither tent touches one, nor the other */
            int e = h ? c2 : c, x = e % n, y = e / n;
            for (int dy = -1; dy <= 1 && ok; dy++)
                for (int dx = -1; dx <= 1 && ok; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && ny >= 0 && nx < n && ny < n && (sol[ny * n + nx] || (!h && ny * n + nx == c2)))
                        ok = 0;
                }
        }
        if (!ok) continue;
        int spots[4], ns = 0, x = c % n, y = c / n;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d], e = ny * n + nx;
            if (nx < 0 || ny < 0 || nx >= n || ny >= n || e == c2 || cells - 1 - e == e) continue;
            if (!p->tree[e] && !sol[e] && !p->tree[cells - 1 - e] && !sol[cells - 1 - e]) spots[ns++] = e;
        }
        if (!ns) continue;
        int d = spots[pr_below(s, ns)];
        sol[c] = sol[c2] = 1;
        p->tree[d] = p->tree[cells - 1 - d] = 1;
        tents += 2;
    }
    for (int i = 0; i < n; i++) {
        int r = 0, k = 0;
        for (int j = 0; j < n; j++) r += sol[i * n + j], k += sol[j * n + i];
        p->count[0][i] = (int8_t)r;
        p->count[1][i] = (int8_t)k;
    }
}

/* one candidate from a seed; 1 when it is of the level and has one solution (show: a symmetric field) */
static int candidate(cand *c, int n, int level, uint32_t seed, int show)
{
    uint8_t sol[TN_CELLS];
    if (show) {
        uint32_t s = pr_seed(seed);
        symmetric(n, &s, &c->p, sol);
    } else {
        tn_make(n, level, seed, &c->p, sol);
    }
    c->grade = tn_effort(&c->p, &c->passes, &c->mediums);
    if (c->grade != level || tn_count(&c->p, 2) != 1) return 0;
    c->numbers = 0;
    for (int i = 0; i < n * n; i++) c->numbers += c->p.tree[i];
    /* the work: the passes, the medium finds weigh more; fewer trees, a little more */
    c->score = (double)c->passes + 4.0 * c->mediums - 0.2 * c->numbers;
    c->taste = taste(&c->p);
    canon(&c->p, c->key);
    return 1;
}

static int make_pool(const make *m, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 40 * CANDS; seed++) {
        cand *c = &pool[k];
        if (!candidate(c, m->n, m->level, seed, 0)) continue;
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (!dup) k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

/* the chapter's showpiece: of the first SHOWS symmetric fields of the size and level, the one whose work is nearest
 * a little past the chapter's hardest (target): a crown, not a wall; 0: none */
static int showpiece(const make *m, uint32_t seed0, double target, cand *out)
{
    int found = 0;
    cand c;
    for (uint32_t seed = seed0; found < SHOWS && seed < seed0 + 200000u; seed++) {
        if (!candidate(&c, m->n, m->level, seed, 1) || seen(c.key)) continue;
        double d = c.score - 1.15 * target, e = out->score - 1.15 * target;
        if (!found++ || (d < 0 ? -d : d) < (e < 0 ? -e : e)) *out = c;
    }
    return found > 0;
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

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "survey")) {
        survey(atoi(argv[2]), atoi(argv[3]));
        return 0;
    }
    for (int ch = 0; ch < TN_CHAPTERS; ch++) {
        int first = nchosen, last = 0;
        while (last + 1 < 3 && CHAPTER[ch][last + 1].count) last++;
        for (int p = 0; p <= last; p++) {
            const make *m = &CHAPTER[ch][p];
            int k = make_pool(m, 1000u * (uint32_t)(ch * 3 + p + 1)), count = m->count - (p == last);
            fprintf(stderr, "chapter %d, make %d (%dx%d %s): %d candidates\n", ch + 1, p + 1, m->n, m->n,
                    GRADE[m->level], k);
            for (int i = 0; i < count; i++) {   /* the best-looking of the pick's own slice of the part */
                int a = (int)((m->lo + (m->hi - m->lo) * i / count) * (k - 1));
                int b = (int)((m->lo + (m->hi - m->lo) * (i + 1) / count) * (k - 1));
                int at = -1;
                for (int j = a; j <= b && j < k; j++)
                    if (!seen(pool[j].key) && (at < 0 || pool[j].taste < pool[at].taste)) at = j;
                for (int j = b + 1; at < 0 && j < k; j++)
                    if (!seen(pool[j].key)) at = j;
                if (at < 0) {
                    fprintf(stderr, "chapter %d: not enough of make %d\n", ch + 1, p + 1);
                    return 1;
                }
                chosen[nchosen++] = pool[at];
            }
        }
        for (int i = first + 1; i < nchosen; i++)   /* the chapter from its easiest to its hardest */
            for (int j = i; j > first; j--) {
                cand *a = &chosen[j - 1], *b = &chosen[j];
                if (a->grade * 1e6 + a->p.n * 1e4 + a->score <= b->grade * 1e6 + b->p.n * 1e4 + b->score) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
        if (!showpiece(&CHAPTER[ch][last], 0x5400000u + 100000u * (uint32_t)ch, chosen[nchosen - 1].score,
                       &chosen[nchosen])) {
            fprintf(stderr, "chapter %d: no showpiece\n", ch + 1);
            return 1;
        }
        fprintf(stderr, "chapter %d showpiece: %ld passes\n", ch + 1, chosen[nchosen].passes);
        nchosen++;
    }
    printf("/*\n"
           " * Tents' levels: our own, made by tools/tents_gen.c (the game's maker; each checked to have one\n"
           " * solution, chosen for its look), six chapters of 25 from the easiest, each ending on a showpiece whose\n"
           " * trees answer each other across the centre. Each: \"<n>:\" and a character a cell, row by row\n"
           " * (tents.h, tn_decode); in the comment its size, its grade and the work it asks of the solver (its\n"
           " * passes, the medium finds).\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const tn_levels[TN_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        char code[TN_CELLS + 64];
        if (!tn_encode(&chosen[i].p, code, sizeof code)) return 1;
        printf("    \"%s\", /* %d: %dx%d %s, %ld passes, %ld medium%s */\n", code, i + 1, chosen[i].p.n,
               chosen[i].p.n, GRADE[chosen[i].grade], chosen[i].passes, chosen[i].mediums,
               i % 25 == 24 ? ", showpiece" : "");
    }
    printf("};\n");
    return nchosen == TN_LEVELS ? 0 : 1;
}
