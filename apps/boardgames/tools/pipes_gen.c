/*
 * Pipes' levels, made by chance and chosen with taste: src/games/pipes_levels.h. Our own levels, from our own maker
 * (a tree grown by chance over the grid, kept when the deductions alone find it: one solution). The tool grows its
 * own trees, three ways, mixed in every chapter:
 *   - free, half of them; or symmetric, the network the same turned a half turn about the source (odd sizes), or
 *     mirrored left to right (a straight spine down the middle on odd sizes, one pipe across it on even ones);
 *   - from the last tile laid more often than not (long winding pipes with branches), else from any tile.
 * Taste: few dead ends (they clutter), no straight run across most of the board, the four quarters about as full
 * of turns and tees as each other. Each candidate:
 *   - its grade (pp_grade: the fewest kinds of deduction that solve it: a tile's own sides, no loop, a way out for
 *     every group, two groups never shut off together) and the passes of deduction it took;
 *   - its start: every tile turned by chance, most of them wrong; its par, the fewest quarter turns to the solution;
 *   - near-duplicates dropped (the same tree turned or mirrored).
 * Each chapter has its sizes (4 x 4 up to 11 x 11, the wrapped boards in the last two); a size's candidates are
 * ranked by difficulty (the grade, then the passes per tile) and picked evenly over a band of that ranking that moves
 * up with the chapters, the easiest band in chapter 1. Then each chapter goes from its easiest to its hardest, and
 * ends on a showpiece: its hardest symmetric network.
 * Deterministic (fixed seeds): run it again after a change to the maker and check the diff.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/ppgen apps/boardgames/tools/pipes_gen.c \
 *       apps/boardgames/src/games/pipes_rules.c && /tmp/ppgen > apps/boardgames/src/games/pipes_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/pipes.h"
#include "games/puzrand.h"

#define CANDS 160                               /* candidates made for each size of each chapter */

typedef struct {
    int n, wrap, grade, rounds, par, sym;
    double score, plain;
    uint8_t sol[PP_TILES], now[PP_TILES];
    char key[PP_TILES + 1];
} cand;

/* a chapter's sizes: n, wrapped, how many levels; and the band of the ranking they come from (0 easiest .. 1) */
typedef struct { int n, wrap, count; } part;
static const struct { part p[3]; float lo, hi; } CHAPTER[PP_CHAPTERS] = {
    {{{4, 0, 9}, {5, 0, 16}}, 0.0f, 0.45f},
    {{{5, 0, 8}, {6, 0, 17}}, 0.2f, 0.7f},
    {{{6, 0, 7}, {7, 0, 18}}, 0.45f, 0.95f},
    {{{7, 0, 5}, {8, 0, 20}}, 0.55f, 1.0f},
    {{{9, 0, 15}, {7, 1, 10}}, 0.6f, 1.0f},
    {{{10, 0, 9}, {11, 0, 8}, {9, 1, 8}}, 0.65f, 1.0f},
};

/* the sides of a tile seen in the board turned (r quarter turns clockwise) then mirrored (left-right) */
static uint8_t sides_of(uint8_t s, int r, int m)
{
    s = pp_turn(s, r);
    if (m) s = (uint8_t)((s & (PP_UP | PP_DOWN)) | (s & PP_RIGHT ? PP_LEFT : 0) | (s & PP_LEFT ? PP_RIGHT : 0));
    return s;
}

/* the smallest of the tree's eight pictures (turned, mirrored): the same tree whichever way it lies */
static void canon(const cand *c, char *key)
{
    int n = c->n;
    char best[PP_TILES + 1], k[PP_TILES + 1];
    best[0] = 0;
    for (int r = 0; r < 4; r++)
        for (int m = 0; m < 2; m++) {
            for (int y = 0; y < n; y++)
                for (int x = 0; x < n; x++) {
                    int sx = x, sy = y;         /* the cell that lands at (x, y) */
                    if (m) sx = n - 1 - sx;
                    for (int i = 0; i < r; i++) {   /* undo r clockwise turns: (x, y) <- (y, n-1-x) */
                        int tx = sy, ty = n - 1 - sx;
                        sx = tx, sy = ty;
                    }
                    k[y * n + x] = PP_CODE[sides_of(c->sol[sy * n + sx], r, m)];
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
static cand chosen[PP_LEVELS];
static int nchosen;

static int seen(const char *key)
{
    for (int i = 0; i < nchosen; i++)
        if (!strcmp(chosen[i].key, key)) return 1;
    return 0;
}

static int bits(unsigned m) { return __builtin_popcount(m); }

/* a tile's and a side's image: sym 1 a half turn about the middle, 2 mirrored left to right */
static int img_t(int n, int sym, int t)
{
    int x = t % n, y = t / n;
    return sym == 1 ? (n - 1 - x) + (n - 1 - y) * n : (n - 1 - x) + y * n;
}
static int img_d(int sym, int d) { return sym == 1 ? (d + 2) & 3 : (4 - d) & 3; }

static void link(int n, int wrap, uint8_t *sol, int *in, int t, int d)
{
    int u = pp_next(n, wrap, t, d);
    sol[t] |= (uint8_t)(1u << d);
    sol[u] |= (uint8_t)(1u << ((d + 2) & 3));
    in[u] = 1;
}

/* a tree over the grid (no tile with four pipes): sym 0 free, 1 the same turned a half turn (n odd), 2 mirrored left
 * to right; tip: the chance in 100 of growing from the last tile laid. 0: stuck (try again) */
static int grow(int n, int wrap, int sym, int tip, uint32_t *rng, uint8_t *sol)
{
    int nt = n * n, in[PP_TILES] = {0}, have, last = -1;
    static int cnd[PP_TILES * 4];
    memset(sol, 0, (size_t)nt);
    if (sym == 2 && n % 2) {                    /* the spine down the middle column */
        int c = n / 2;
        in[c] = 1;
        for (int y = 0; y + 1 < n; y++) link(n, 0, sol, in, c + y * n, 2);
        have = n;
    } else if (sym == 2) {                      /* one pipe across the middle */
        int a = n / 2 - 1 + pr_below(rng, n) * n;
        in[a] = 1;
        link(n, 0, sol, in, a, 1);
        have = 2;
        last = a + 1;
    } else {
        in[n / 2 * n + n / 2] = 1;
        have = 1;
    }
    while (have < nt) {
        int nc = 0, from_tip = last >= 0 && pr_below(rng, 100) < tip;
        for (int pass = from_tip ? 0 : 1; pass < 2 && !nc; pass++)
            for (int t = pass ? 0 : last; t < (pass ? nt : last + 1); t++) {
                if (!in[t] || bits(sol[t]) >= 3) continue;
                for (int d = 0; d < 4; d++) {
                    int u = pp_next(n, wrap, t, d);
                    if (u < 0 || in[u]) continue;
                    if (sym) {
                        int t2 = img_t(n, sym, t), u2 = img_t(n, sym, u);
                        if (u2 == u || (t2 == t && bits(sol[t]) >= 2)) continue;
                    }
                    cnd[nc++] = t * 4 + d;
                }
            }
        if (!nc) return 0;
        int e = cnd[pr_below(rng, nc)], t = e / 4, d = e % 4;
        link(n, wrap, sol, in, t, d);
        last = pp_next(n, wrap, t, d);
        have++;
        if (sym) {
            link(n, wrap, sol, in, img_t(n, sym, t), img_d(sym, d));
            have++;
        }
    }
    return 1;
}

/* how plain a network looks: 0 fine; more for many dead ends, a long straight run, lopsided quarters */
static double plainness(int n, const uint8_t *sol)
{
    int ends = 0, run = 0, best = 0, q[4] = {0};
    for (int t = 0; t < n * n; t++) {
        int b = bits(sol[t]), x = t % n, y = t / n;
        ends += b == 1;
        if (b == 3 || (b == 2 && sol[t] != (PP_UP | PP_DOWN) && sol[t] != (PP_LEFT | PP_RIGHT)))
            if (2 * x + 1 != n && 2 * y + 1 != n) q[(2 * x >= n) + 2 * (2 * y >= n)]++;
    }
    for (int y = 0; y < n; y++, run = 0)        /* straight runs, across then down */
        for (int x = 0; x < n; x++) {
            run = sol[y * n + x] == (PP_LEFT | PP_RIGHT) ? run + 1 : 0;
            if (run > best) best = run;
        }
    for (int x = 0; x < n; x++, run = 0)
        for (int y = 0; y < n; y++) {
            run = sol[y * n + x] == (PP_UP | PP_DOWN) ? run + 1 : 0;
            if (run > best) best = run;
        }
    int lo = q[0], hi = q[0];
    for (int i = 1; i < 4; i++) lo = q[i] < lo ? q[i] : lo, hi = q[i] > hi ? q[i] : hi;
    double fe = (double)ends / (n * n), p = 0;
    if (fe > 0.3) p += (fe - 0.3) * 20;
    if (best > n / 2 + 1) p += best - (n / 2 + 1);
    if (hi > 0) p += 2.0 * (hi - lo) / hi;
    return p;
}

/* candidates of one size: made, graded, ranked */
static int make_pool(int n, int wrap, uint32_t seed0)
{
    int k = 0;
    for (uint32_t seed = seed0; k < CANDS && seed < seed0 + 50 * CANDS; seed++) {
        cand *c = &pool[k];
        memset(c, 0, sizeof *c);
        c->n = n;
        c->wrap = wrap;
        uint32_t rng = pr_seed(seed);
        int sym = seed % 4 < 2 ? 0 : seed % 4 == 2 ? (n % 2 ? 1 : 2) : 2, tip = 40 + (int)(seed % 5) * 10, found = 0;
        for (int tries = 0; tries < PP_TRIES && !found; tries++) {
            while (!grow(n, wrap, sym, tip, &rng, c->sol)) {}
            found = pp_deduce(n, wrap, c->sol, NULL) == n * n;
        }
        if (!found) continue;
        c->sym = sym;
        c->plain = plainness(n, c->sol);
        if (c->plain > 1.5) continue;
        c->grade = pp_grade(n, wrap, c->sol, &c->rounds);
        if (c->grade < 0 || pp_count(n, wrap, c->sol, 2) != 1) continue;
        uint32_t r = pr_seed(seed ^ 0x2b3c4d5eu);
        int wrong;
        do {                                    /* scrambled: at least two tiles in three wrong */
            wrong = 0;
            for (int t = 0; t < n * n; t++) {
                c->now[t] = pp_turn(c->sol[t], pr_below(&r, 4));
                wrong += c->now[t] != c->sol[t];
            }
        } while (3 * wrong < 2 * n * n);
        c->par = pp_par(n, c->sol, c->now);
        canon(c, c->key);
        int dup = 0;
        for (int i = 0; i < k && !dup; i++) dup = !strcmp(pool[i].key, c->key);
        if (dup) continue;
        c->score = c->grade + (double)c->rounds / (n * n) * 4 + 0.01 * (seed % 7);
        k++;
    }
    qsort(pool, (size_t)k, sizeof pool[0], by_score);
    return k;
}

int main(void)
{
    for (int ch = 0; ch < PP_CHAPTERS; ch++) {
        int first = nchosen;
        for (int p = 0; p < 3 && CHAPTER[ch].p[p].n; p++) {
            const part *pt = &CHAPTER[ch].p[p];
            int k = make_pool(pt->n, pt->wrap, 1000u * (uint32_t)(ch * 3 + p + 1));
            float lo = CHAPTER[ch].lo, hi = CHAPTER[ch].hi;
            for (int i = 0; i < pt->count; i++) {   /* evenly over the band, the next unseen one */
                int at = (int)((lo + (hi - lo) * (i + 0.5f) / pt->count) * (k - 1));
                while (at < k && seen(pool[at].key)) at++;
                if (at >= k) {
                    fprintf(stderr, "chapter %d: not enough %dx%d\n", ch + 1, pt->n, pt->n);
                    return 1;
                }
                chosen[nchosen++] = pool[at];
            }
        }
        /* the chapter from its easiest to its hardest: the size first, then the score */
        for (int i = first + 1; i < nchosen; i++)
            for (int j = i; j > first; j--) {
                cand *a = &chosen[j - 1], *b = &chosen[j];
                double ka = a->n * a->n * (1 + 0.6 * a->wrap) + 8 * a->score;
                double kb = b->n * b->n * (1 + 0.6 * b->wrap) + 8 * b->score;
                if (ka <= kb) break;
                cand t = *a;
                *a = *b;
                *b = t;
            }
        /* the showpiece last: the hardest symmetric network of the chapter's last quarter */
        int show = -1;
        for (int i = nchosen - 1; i >= nchosen - (nchosen - first) / 4 && show < 0; i--)
            if (chosen[i].sym) show = i;
        if (show >= 0) {
            cand t = chosen[show];
            memmove(&chosen[show], &chosen[show + 1], (size_t)(nchosen - 1 - show) * sizeof t);
            chosen[nchosen - 1] = t;
        }
        int ns = 0;
        for (int i = first; i < nchosen; i++) ns += chosen[i].sym != 0;
        fprintf(stderr, "chapter %d: %d symmetric of %d\n", ch + 1, ns, nchosen - first);
    }
    if (nchosen != PP_LEVELS) {
        fprintf(stderr, "%d levels, not %d\n", nchosen, PP_LEVELS);
        return 1;
    }
    printf("/*\n * Pipes' levels: our own, made by tools/pipes_gen.c (trees grown by chance from the source, kept when the\n"
           " * deductions alone solve them, ranked by how hard those deductions are), six chapters of 25 from the easiest.\n"
           " * Each: \"<n>[w]:\" and a character a tile (pipes.h, PP_CODE); in the comment its grade, its passes of\n"
           " * deduction and its par (the fewest quarter turns).\n * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\nstatic const char *const pp_levels[PP_LEVELS] = {\n");
    for (int i = 0; i < nchosen; i++) {
        pp_game g;
        char code[PP_TILES + 8];
        pp_load(&g, chosen[i].n, chosen[i].wrap, chosen[i].n / 2 * chosen[i].n + chosen[i].n / 2, chosen[i].sol,
                chosen[i].now);
        pp_encode(&g, code, sizeof code);
        printf("    \"%s\", /* %d: %dx%d%s, grade %d, %d passes, par %d */\n", code, i + 1, chosen[i].n, chosen[i].n,
               chosen[i].wrap ? " wrapped" : "", chosen[i].grade, chosen[i].rounds, chosen[i].par);
    }
    printf("};\n");
    return 0;
}
