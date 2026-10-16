/*
 * Row Shift's levels: src/games/loopshift_levels.h. Our own levels: no published set is used. Six chapters of 25,
 * from 3 x 3 colour bands to 6 x 6 numbered tiles (the parts below). A candidate: the solved grid scrambled by random
 * moves (the rules' lsh_scramble, each move changing the grid); its fewest moves found by the rules' search (a
 * meeting in the middle, deeper here than in play: exact up to a + b, else the scramble's length, the best known, at
 * least a + b + 1); kept when they lie in the part's range and no level kept is the same grid turned, mirrored or
 * moved round the torus (tiles renamed alike). The difficulty: the fewest moves, then the tiles out of place. Each part
 * keeps its levels spread evenly over its range, the easiest first. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/lshgen apps/boardgames/tools/loopshift_gen.c && \
 *       /tmp/lshgen > apps/boardgames/src/games/loopshift_levels.h
 *   /tmp/lshgen -spread: how the fewest moves of random scrambles spread, for each size and kind
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define LSH_GEN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/loopshift_rules.c"

#define POOL 400
#define TRIES 6000

/* each part: the side, colour bands, how many levels, the range of the fewest moves */
static const struct { int n, colour, count, lo, hi; } PART[] = {
    {3, 1, 15, 1, 4}, {3, 0, 10, 2, 4},                     /* chapter 1 */
    {3, 0, 25, 4, 7},                                       /* 2 */
    {4, 1, 10, 3, 6}, {4, 0, 15, 3, 6},                     /* 3 */
    {4, 0, 25, 5, 9},                                       /* 4 */
    {5, 1, 8, 5, 8}, {5, 0, 17, 5, 9},                      /* 5 */
    {6, 1, 8, 6, 9}, {6, 0, 17, 6, 10},                     /* 6 */
};
#define PARTS ((int)(sizeof PART / sizeof PART[0]))

static void gen_depths(int n, int *a, int *b)
{
    static const int A[LSH_MAX + 1] = {0, 0, 0, 7, 5, 5, 4}, B[LSH_MAX + 1] = {0, 0, 0, 7, 5, 4, 4};
    *a = A[n];
    *b = B[n];
}

/* the least hash of the grid's look under the symmetries kept by the puzzle: the square's eight (rows and columns
 * swapped only for numbered tiles, colour bands keep their rows) and every move round the torus */
static uint64_t look_key(const int8_t *at, int n, int colour)
{
    uint64_t best = ~0ull;
    for (int sym = 0; sym < 8; sym++) {
        if (colour && sym >= 4) continue;       /* the transposes */
        for (int tx = 0; tx < n; tx++)
            for (int ty = 0; ty < n; ty++) {
                int8_t t[LSH_CELLS];
                for (int p = 0; p < n * n; p++) {
                    int map[2];
                    for (int w = 0; w < 2; w++) {   /* w 0: the place, 1: the tile's home */
                        int v = w ? at[p] : p, x = v % n, y = v / n;
                        if (sym & 1) x = n - 1 - x;
                        if (sym & 2) y = n - 1 - y;
                        if (sym & 4) { int s = x; x = y; y = s; }
                        x = (x + tx) % n;
                        y = (y + ty) % n;
                        map[w] = x + y * n;
                    }
                    t[map[0]] = (int8_t)map[1];
                }
                uint64_t h = hash_of(t, n, colour);
                if (h < best) best = h;
            }
    }
    return best;
}

static int misplaced(const int8_t *at, int n, int colour)
{
    int m = 0;
    for (int p = 0; p < n * n; p++) m += colour ? at[p] / n != p / n : at[p] != p;
    return m;
}

typedef struct {
    int par, exact, off;
    int8_t at[LSH_CELLS];
    uint8_t way[LSH_SOL];
    uint64_t key;
} cand;

static cand pool[POOL];
static uint64_t used[LSH_LEVELS];
static int nused;

/* a candidate from a seed: its fewest moves and a shortest way (or the scramble's inverse) */
static int make(cand *c, int n, int colour, int depth, uint32_t seed)
{
    lsh_game g;
    lsh_scramble(&g, n, colour, depth, seed);
    if (g.nsol < depth) return 0;
    int a, b;
    gen_depths(n, &a, &b);
    uint8_t path[LSH_SOL];
    int len = lsh_solve(g.start, n, colour, a, b, g.nsol, path, LSH_SOL);
    memcpy(c->at, g.start, sizeof c->at);
    if (len > 0) {
        c->par = len;
        c->exact = 1;
        memcpy(c->way, path, (size_t)len);
    } else {
        c->par = g.nsol;
        c->exact = g.nsol - 1 <= a + b;
        memcpy(c->way, g.sol, (size_t)g.nsol);
    }
    /* the way checked: it solves the grid in par moves */
    int8_t t[LSH_CELLS];
    memcpy(t, c->at, sizeof t);
    for (int i = 0; i < c->par; i++) lsh_apply(t, n, c->way[i]);
    if (!lsh_solved_at(t, n, colour)) { fprintf(stderr, "a way that does not solve\n"); exit(1); }
    c->off = misplaced(c->at, n, colour);
    c->key = look_key(c->at, n, colour);
    return 1;
}

static int by_difficulty(const void *x, const void *y)
{
    const cand *a = x, *b = y;
    return a->par != b->par ? a->par - b->par : a->off - b->off;
}

static void spread(void)
{
    for (int n = LSH_MIN; n <= LSH_MAX; n++)
        for (int colour = 0; colour < 2; colour++) {
            printf("%d x %d %s:", n, n, colour ? "colours" : "numbers");
            for (int depth = 2; depth <= 12; depth += 2) {
                int hist[LSH_SOL + 1] = {0}, inexact = 0, k = n >= 6 ? 6 : 20;
                for (int s = 0; s < k; s++) {
                    cand c;
                    if (!make(&c, n, colour, depth, 1000u * depth + s)) continue;
                    hist[c.par]++;
                    inexact += !c.exact;
                }
                printf("  d%d:", depth);
                for (int p = 0; p <= LSH_SOL; p++)
                    if (hist[p]) printf(" %d*%d", p, hist[p]);
                if (inexact) printf(" (~%d)", inexact);
            }
            printf("\n");
            fflush(stdout);
        }
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "-spread")) { spread(); return 0; }
    printf("/*\n * Row Shift's levels: our own, made by tools/loopshift_gen.c (grids scrambled by chance, their fewest"
           " moves\n * found by the rules' search, six chapters of 25 from the easiest). Each: the side, 'n' numbered"
           " tiles or\n * 'c' colour bands, 'x' the fewest moves proven or 'b' the best known; the tiles by home place"
           " ('a'..);\n * then a shortest way (a line 'a'.. and the steps, a move), its length the par.\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n */\n");
    printf("const char *const lsh_levels[LSH_LEVELS] = {\n");
    int level = 0;
    for (int part = 0; part < PARTS; part++) {
        int n = PART[part].n, colour = PART[part].colour, lo = PART[part].lo, hi = PART[part].hi, np = 0;
        int want = PART[part].count, span = hi - lo + 1;
        for (int t = 0; t < TRIES && np < POOL && np < 3 * want + 12; t++) {
            cand c;
            int depth = lo + (int)(pr_seed(77u + part * 100003u + t) % (unsigned)(span + 2));
            if (!make(&c, n, colour, depth, 5000u + part * 1000003u + t)) continue;
            if (c.par < lo || c.par > hi) continue;
            int dup = 0;
            for (int i = 0; i < nused && !dup; i++) dup = used[i] == c.key;
            for (int i = 0; i < np && !dup; i++) dup = pool[i].key == c.key;
            if (!dup) pool[np++] = c;
        }
        qsort(pool, (size_t)np, sizeof *pool, by_difficulty);
        /* as many of each par as fits evenly, the more of the harder ones */
        int per[LSH_SOL + 1] = {0}, have[LSH_SOL + 1] = {0}, taken[LSH_SOL + 1] = {0};
        for (int i = 0; i < np; i++) have[pool[i].par]++;
        int left = want;
        for (int pass = 0; left > 0 && pass < 50; pass++)
            for (int p = hi; p >= lo && left > 0; p--)
                if (per[p] < have[p] && per[p] <= pass) { per[p]++; left--; }
        if (left) { fprintf(stderr, "part %d: %d levels short\n", part, left); return 1; }
        for (int i = 0; i < np; i++) {
            cand *c = &pool[i];
            /* spread over the par's candidates, from the fewest out of place */
            int p = c->par, k = taken[p];
            if (k >= per[p]) continue;
            int first = 0;
            while (first < np && pool[first].par != p) first++;
            if (i - first != (int)((long)k * have[p] / per[p])) continue;
            taken[p]++;
            used[nused++] = c->key;
            printf("    \"%d%c%c:", n, colour ? 'c' : 'n', c->exact ? 'x' : 'b');
            for (int q = 0; q < n * n; q++) putchar(c->at[q] < 26 ? 'a' + c->at[q] : 'A' + c->at[q] - 26);
            putchar(':');
            for (int q = 0; q < c->par; q++) printf("%c%d", 'a' + lsh_line(c->way[q]), lsh_amount(c->way[q]));
            printf("\", /* %d: par %d%s, %d out of place */\n", ++level, c->par, c->exact ? "" : " (best known)",
                   c->off);
        }
        fprintf(stderr, "part %d: %d candidates\n", part, np);
    }
    printf("};\n");
    if (level != LSH_LEVELS) { fprintf(stderr, "%d levels, not %d\n", level, LSH_LEVELS); return 1; }
    return 0;
}
