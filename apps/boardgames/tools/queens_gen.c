/*
 * Eight Queens' levels, made by chance and chosen: src/games/queens_levels.h. Our own puzzles: no published set is
 * used. Each candidate:
 *   - a board of n x n (4 to 10) and one of its solutions drawn at random (the rules' own list of every solution);
 *   - clues added in a random order until that solution is the only one left (qn_count): holes on squares the
 *     solution leaves empty, always in symmetric sets (a square and its half-turn image, or its four quarter-turn
 *     images, as logic grids lay their givens out), and, when the holes are not enough, one fixed queen from the
 *     solution (a board needing more is dropped); a clue that rules nothing out is not kept;
 *   - then thinned: each clue taken away in turn, for good if the solution stays the only one (no clue too many);
 *   - solved by reasoning, as a player would, without ever guessing: a row or a column with one free square left
 *     takes its queen (1 point); a square whose queen would leave some row or column without a free square is
 *     ruled out (3 points); a square whose queen would, once the rows and columns it forces are filled in, leave one
 *     empty (8 points). A board this solver cannot finish needs guessing: dropped. The points are its grade.
 * Mirror and rotation duplicates are dropped. Six chapters by size (4 and 5, 6 and 7, 7 and 8, 8, 9, 10), 25 each,
 * chosen evenly over the chapter's grades, easiest first, never two boards with fixed queens in a row, each chapter
 * ending on its hardest quarter's board with holes in fours.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/qngen apps/boardgames/tools/queens_gen.c \
 *       apps/boardgames/src/games/queens_rules.c && /tmp/qngen > apps/boardgames/src/games/queens_levels.h
 * Deterministic: a fixed seed. MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/puzrand.h"
#include "games/queens.h"

#define CANDS 500                               /* a chapter's */
#define PER 25
#define TRIED (1 << 18)

typedef struct {
    int n, grade, deep, holes, givens, sym, used;
    char rows[QN_SQUARES + QN_MAX + 1];
} level;

static uint32_t rng;

/* ---- the reasoning solver ---------------------------------------------------------------------------------------- */
typedef struct { int n, queens; uint8_t queen[QN_SQUARES], out[QN_SQUARES]; } state;

static void put(state *s, int c)
{
    s->queen[c] = 1;
    s->queens++;
    for (int t = 0; t < s->n * s->n; t++)
        if (qn_attacks(s->n, c, t)) s->out[t] = 1;
    s->out[c] = 1;                              /* taken */
}

/* the free squares of a row (col 0) or a column (col 1) without a queen: how many, *only the last; -1: it has one */
static int line_free(const state *s, int i, int col, int *only)
{
    int k = 0;
    for (int j = 0; j < s->n; j++) {
        int c = col ? j * s->n + i : i * s->n + j;
        if (s->queen[c]) return -1;
        if (!s->out[c]) { k++; *only = c; }
    }
    return k;
}

static int broken(const state *s)               /* a row or a column left with no queen and no free square */
{
    int only;
    for (int i = 0; i < s->n; i++)
        for (int col = 0; col < 2; col++)
            if (line_free(s, i, col, &only) == 0) return 1;
    return 0;
}

/* one queen forced (a row or a column with one free square): 1; none: 0 */
static int single(state *s)
{
    for (int i = 0; i < s->n; i++)
        for (int col = 0; col < 2; col++) {
            int only;
            if (line_free(s, i, col, &only) == 1) {
                put(s, only);
                return 1;
            }
        }
    return 0;
}

/* the forced queens filled in until none is left, or something breaks: 1 broken */
static int propagate(state *s)
{
    while (!broken(s) && s->queens < s->n && single(s)) {}
    return broken(s);
}

/* the grade; -1: guessing needed. *deep: the deepest steps used. */
static int grade(const qn_game *g, int *deep)
{
    state s;
    memset(&s, 0, sizeof s);
    s.n = g->n;
    for (int c = 0; c < s.n * s.n; c++) s.out[c] |= g->hole[c];
    for (int c = 0; c < s.n * s.n; c++)
        if (g->fixed[c]) put(&s, c);
    int points = 0;
    *deep = 0;
    while (s.queens < s.n) {
        if (broken(&s)) return -1;
        if (single(&s)) { points += 1; continue; }
        int progress = 0;
        for (int c = 0; c < s.n * s.n; c++) {   /* a queen there leaves a line empty at once */
            if (s.out[c]) continue;
            state t = s;
            put(&t, c);
            if (broken(&t)) { s.out[c] = 1; points += 3; progress = 1; }
        }
        if (progress) { if (*deep < 1) *deep = 1; continue; }
        for (int c = 0; c < s.n * s.n; c++) {   /* ... once the queens it forces are filled in */
            if (s.out[c]) continue;
            state t = s;
            put(&t, c);
            if (propagate(&t)) { s.out[c] = 1; points += 8; progress = 1; }
        }
        if (progress) { *deep = 2; continue; }
        return -1;
    }
    return points;
}

/* ---- the candidates ---------------------------------------------------------------------------------------------- */
static int count(const qn_game *g) { return qn_count(g, NULL); }

static void to_rows(const qn_game *g, char *o)
{
    int n = g->n;
    for (int y = 0; y < n; y++) {
        for (int f = 0; f < n; f++) {
            int s = (n - 1 - y) * n + f;
            *o++ = g->hole[s] ? '#' : g->fixed[s] ? 'Q' : '.';
        }
        if (y < n - 1) *o++ = '|';
    }
    *o = 0;
}

/* the smallest of the eight turns' rows */
static void canonical(const qn_game *g, char *out)
{
    int n = g->n;
    out[0] = 0;
    for (int t = 0; t < 8; t++) {
        qn_game h;
        qn_empty(&h, n, -1);
        for (int s = 0; s < n * n; s++) {
            int x = s % n, y = s / n, X = t & 1 ? n - 1 - x : x, Y = t & 2 ? n - 1 - y : y;
            if (t & 4) { int k = X; X = Y; Y = k; }
            h.hole[Y * n + X] = g->hole[s];
            h.fixed[Y * n + X] = g->fixed[s];
        }
        char buf[QN_SQUARES + QN_MAX + 1];
        to_rows(&h, buf);
        if (!out[0] || strcmp(buf, out) < 0) strcpy(out, buf);
    }
}

static int tried(const char *rows)
{
    static uint32_t set[TRIED];
    uint32_t h = 2166136261u;
    for (const char *p = rows; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    h |= 1;
    for (uint32_t i = h & (TRIED - 1);; i = (i + 1) & (TRIED - 1)) {
        if (set[i] == h) return 1;
        if (!set[i]) { set[i] = h; return 0; }
    }
}

/* a puzzle of n x n with a unique solution, its clues thinned; 0: none this time. The holes come in symmetric
 * sets: a square and its image by a half turn (sym 2), or its four images by quarter turns (sym 4), every one of
 * them a square the solution leaves empty; a set is added and taken away as one clue. Fixed queens are rare. */
static int make(int n, int sym, qn_game *g)
{
    uint64_t key;
    if (!qn_solution(n, pr_below(&rng, qn_solutions(n)), &key)) return 0;
    qn_empty(g, n, -1);
    int in[QN_SQUARES] = {0}, gives[QN_MAX], ng = 0, nset = 0, set[QN_SQUARES][4], size[QN_SQUARES], seen[QN_SQUARES];
    int order[QN_SQUARES];
    memset(seen, 0, sizeof seen);
    for (int r = 0; r < n; r++) in[r * n + (int)(key >> (4 * r) & 15)] = 1;
    for (int s = 0; s < n * n; s++) {
        if (in[s]) { gives[ng++] = s; continue; }
        if (seen[s]) continue;
        int x = s % n, y = s / n, im[4] = {s, (n - 1 - y) * n + n - 1 - x, x * n + n - 1 - y, (n - 1 - x) * n + y};
        int k = 0, ok = 1;
        for (int i = 0; i < sym; i++) {
            int dup = 0;
            for (int j = 0; j < k; j++) dup |= set[nset][j] == im[i];
            if (dup) continue;
            if (in[im[i]]) ok = 0;
            set[nset][k++] = im[i];
        }
        for (int j = 0; j < k; j++) seen[set[nset][j]] = 1;
        if (!ok) continue;
        size[nset] = k;
        order[nset] = nset;
        nset++;
    }
    pr_shuffle(&rng, order, nset);
    pr_shuffle(&rng, gives, ng);
    static const int give_pct[4] = {0, 0, 0, 10};   /* the share of fixed queens among the clues */
    int pct = give_pct[pr_below(&rng, 4)], ih = 0, ig = 0, clue[QN_SQUARES + QN_MAX], nc = 0;
    int now = count(g);
    #define SET_HOLES(i, v) for (int j_ = 0; j_ < size[i]; j_++) g->hole[set[i][j_]] = (v)
    while (now > 1 && (ih < nset || ig < ng)) {
        int give = ig < ng && (ih >= nset || pr_below(&rng, 100) < pct);
        int c = give ? gives[ig++] : order[ih++];
        if (give) { g->fixed[c] = g->q[c] = 1; g->count++; }
        else SET_HOLES(c, 1);
        int k = count(g);
        if (k < now) { now = k; clue[nc++] = give ? -1 - c : c; continue; }
        if (give) { g->fixed[c] = g->q[c] = 0; g->count--; }
        else SET_HOLES(c, 0);
    }
    if (now != 1) return 0;
    pr_shuffle(&rng, clue, nc);
    for (int i = 0; i < nc; i++) {              /* thinned */
        int give = clue[i] < 0, c = give ? -1 - clue[i] : clue[i];
        if (give) { g->fixed[c] = g->q[c] = 0; g->count--; }
        else SET_HOLES(c, 0);
        if (count(g) == 1) continue;
        if (give) { g->fixed[c] = g->q[c] = 1; g->count++; }
        else SET_HOLES(c, 1);
    }
    #undef SET_HOLES
    g->given = -1;
    for (int s = 0; s < n * n && g->given < 0; s++)
        if (g->fixed[s]) g->given = s;
    return 1;
}

static int cmp(const void *a, const void *b) { return ((const level *)a)->grade - ((const level *)b)->grade; }

int main(void)
{
    static const int lo[QN_CHAPTERS] = {4, 6, 7, 8, 9, 10}, hi[QN_CHAPTERS] = {5, 7, 8, 8, 9, 10};
    static level cand[CANDS];
    rng = pr_seed(20261012);
    static const char *const head[] = {
        "/*",
        " * Eight Queens' levels: our own puzzles, made by tools/queens_gen.c (a solution drawn at random, symmetric holes",
        " * and at most one fixed queen added until it is the only one, then thinned; each solved by reasoning alone), six",
        " * chapters of 25 by size (4 and 5, 6 and 7, 7 and 8, 8, 9, 10), each from the easiest. Each: its rows from the top ('.' a square,",
        " * '#' a hole, 'Q' a fixed queen), then (in the comment) its size, holes, fixed queens, the deepest reasoning",
        " * it needs (0 a row or column's last square, 1 a queen that would empty a line, 2 the same some moves on), its",
        " * grade.",
        " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).",
        " */",
        "const char *const qn_levels[QN_LEVELS] = {",
    };
    for (size_t i = 0; i < sizeof head / sizeof head[0]; i++) printf("%s\n", head[i]);
    int number = 0;
    for (int c = 0; c < QN_CHAPTERS; c++) {
        int nc = 0;
        for (long tries = 0; nc < CANDS && tries < 400000; tries++) {
            int n = lo[c] + pr_below(&rng, hi[c] - lo[c] + 1);
            qn_game g;
            int sym = pr_below(&rng, 3) ? 2 : 4;
            if (!make(n, sym, &g)) continue;
            level L;
            memset(&L, 0, sizeof L);
            canonical(&g, L.rows);
            if (tried(L.rows)) continue;
            L.grade = grade(&g, &L.deep);
            if (L.grade < 0) continue;
            L.n = n;
            L.sym = sym;
            for (int s = 0; s < n * n; s++) L.holes += g.hole[s], L.givens += g.fixed[s];
            if (L.givens > 1 || (c && L.holes < n / 2)) continue;  /* one fixed queen at most; holes that show */
            L.grade += 2 * n;
            to_rows(&g, L.rows);
            cand[nc++] = L;
        }
        qsort(cand, (size_t)nc, sizeof(level), cmp);
        fprintf(stderr, "chapter %d: %d candidates, grades %d..%d\n", c + 1, nc, nc ? cand[0].grade : 0,
                nc ? cand[nc - 1].grade : 0);
        if (nc < PER) {
            fprintf(stderr, "chapter %d: too few candidates\n", c + 1);
            return 1;
        }
        int from = c && nc - nc / 4 >= 2 * PER ? nc / 4 : 0;   /* past the first chapter, the plainest quarter out */
        /* evenly over the grades, never two with fixed queens in a row; the chapter ends on a showpiece: the
         * hardest quarter's best board with holes in fours and no fixed queen */
        int prev = 0;
        for (int i = 0; i < PER; i++) {
            int at = from + (int)((long)(nc - 1 - from) * i / (PER - 1)), pick = -1;
            if (i == PER - 1)
                for (int j = nc - 1; j >= nc - nc / 4 && pick < 0; j--)
                    if (!cand[j].used && cand[j].sym == 4 && !cand[j].givens) pick = j;
            for (int d = 0; pick < 0 && d <= 6; d++)
                for (int sgn = -1; sgn <= 1 && pick < 0; sgn += 2) {
                    int j = at + sgn * d;
                    if (j >= from && j < nc && !cand[j].used && !(prev && cand[j].givens)) pick = j;
                }
            for (int d = 0; pick < 0 && d < nc; d++)
                for (int sgn = -1; sgn <= 1 && pick < 0; sgn += 2) {
                    int j = at + sgn * d;
                    if (j >= 0 && j < nc && !cand[j].used) pick = j;
                }
            cand[pick].used = 1;
            prev = cand[pick].givens;
            const level *L = &cand[pick];
            printf("    \"%s\", /* %d: %dx%d, %d holes, %d fixed, depth %d, grade %d */\n", L->rows, ++number, L->n,
                   L->n, L->holes, L->givens, L->deep, L->grade);
        }
    }
    printf("};\n");
    return 0;
}
