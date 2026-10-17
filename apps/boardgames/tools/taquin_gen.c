/*
 * The 15 Puzzle's levels: src/games/taquin_levels.h. Our own levels: no published set is used. Six chapters: 3 x 3,
 * its picture, 4 x 4, its picture, 4 x 4 longer, 5 x 5 (the picture every other level). A candidate: the tray in
 * order, then the gap walked at random (never straight back), as many steps as drawn; its exact par found by the
 * rules' search (tq_search, IDA*: Manhattan distances plus linear conflicts); kept when the par lies in the chapter's
 * range, the search stayed quick (the tests solve every level again on every run), and no level kept is the same.
 * The difficulty: the par, more when it lies far above the search's bound (the detours a player must see). The looks
 * (taste_of): the tiles out of place spread over the whole tray, not one corner stirred. Each chapter
 * keeps 25 candidates spread evenly over its difficulties, from the easiest: at each step the best looking of a few
 * close in difficulty, the gap starting elsewhere than in the level before; the last, its showpiece, the best looking
 * among its hardest tenth. Deterministic (fixed seeds).
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/tqgen apps/boardgames/tools/taquin_gen.c && \
 *       /tmp/tqgen > apps/boardgames/src/games/taquin_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define TQ_GEN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/taquin_rules.c"

#define PER (TQ_LEVELS / TQ_CHAPTERS)
#define POOL 300
#define TRIES 40000
#define BUDGET 2000000L                         /* the positions a level's search may look at */

/* each chapter: the tray's side, cut from the picture (2: every other level), the par's range */
static const struct { int n, picture, lo, hi; } CH[TQ_CHAPTERS] = {
    {3, 0, 2, 12}, {3, 1, 12, 22}, {4, 0, 10, 26}, {4, 1, 24, 36}, {4, 0, 34, 46}, {5, 2, 26, 42},
};

typedef struct {
    int n, par, bound, gap;
    double taste;
    long looked;
    double diff;
    char text[TQ_PLACES + TQ_MAX + 4];
} cand;

static cand pool[POOL];
static int npool;
static uint32_t rng;

/* how well a tray looks set out by hand (higher: better): the tiles out of place spread over the whole tray (every
 * row and column has some; their middle near the tray's), not one corner stirred and the rest left in order */
static double taste_of(const int8_t *at, int n)
{
    int row[TQ_MAX] = {0}, col[TQ_MAX] = {0}, off = 0, lines = 0;
    double cx = 0, cy = 0;
    for (int p = 0; p < n * n; p++) {
        if (!at[p] || at[p] == p + 1) continue;
        row[p / n] = col[p % n] = 1;
        cx += p % n;
        cy += p / n;
        off++;
    }
    if (!off) return 0;
    for (int i = 0; i < n; i++) lines += row[i] + col[i];
    cx = cx / off - (n - 1) / 2.0;
    cy = cy / off - (n - 1) / 2.0;
    return 2.0 * lines / (2 * n) - 0.8 * (cx * cx + cy * cy) / ((n - 1) * (n - 1) / 4.0);
}

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0;
}

/* the tiles as text, rows apart: 1..9 a..o, '.' the gap (the kind of picture is set later) */
static void text_of(const int8_t *at, int n, char *s)
{
    static const char TILES[] = ".123456789abcdefghijklmno";
    int k = 0;
    s[k++] = 'n';
    s[k++] = ':';
    for (int y = 0; y < n; y++) {
        if (y) s[k++] = '|';
        for (int x = 0; x < n; x++) s[k++] = TILES[at[x + y * n]];
    }
    s[k] = 0;
}

static int known(const char *t, const cand *list, int nlist)
{
    for (int i = 0; i < nlist; i++)
        if (!strcmp(list[i].text + 1, t + 1)) return 1;
    return 0;
}

int main(void)
{
    static cand out[TQ_LEVELS];
    int nout = 0;
    static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
    for (int ch = 0; ch < TQ_CHAPTERS; ch++) {
        int n = CH[ch].n;
        rng = pr_seed(3000u + (uint32_t)ch);
        npool = 0;
        for (int tries = 0; tries < TRIES && npool < POOL; tries++) {
            int8_t at[TQ_PLACES];
            for (int i = 0; i < n * n; i++) at[i] = (int8_t)((i + 1) % (n * n));
            int gap = n * n - 1, prev = -1, steps = CH[ch].lo + pr_below(&rng, 3 * CH[ch].hi - CH[ch].lo);
            for (int k = 0; k < steps;) {
                int d = pr_below(&rng, 4), x = gap % n + DX[d], y = gap / n + DY[d];
                if (x < 0 || y < 0 || x >= n || y >= n || x + y * n == prev) continue;
                prev = gap;
                at[gap] = at[x + y * n];
                at[x + y * n] = 0;
                gap = x + y * n;
                k++;
            }
            int bound = tq_bound(at, n);
            if (bound > CH[ch].hi || bound < 1) continue;
            cand c;
            memset(&c, 0, sizeof c);
            text_of(at, n, c.text);
            if (known(c.text, pool, npool) || known(c.text, out, nout)) continue;
            c.par = tq_search(at, n, 4, BUDGET, &c.looked);
            if (c.par < CH[ch].lo || c.par > CH[ch].hi) continue;
            c.n = n;
            c.bound = bound;
            c.diff = c.par + 0.5 * (c.par - bound);
            c.taste = taste_of(at, n);
            c.gap = gap;
            pool[npool++] = c;
        }
        qsort(pool, (size_t)npool, sizeof *pool, by_diff);
        fprintf(stderr, "chapter %d (%d x %d): %d candidates, par %d .. %d\n", ch + 1, n, n, npool, pool[0].par,
                pool[npool - 1].par);
        for (int i = 0, last = -1, gap = -1; i < PER; i++) {
            double q = ch == 0 && i == 0 ? 0 : 0.03 + 0.94 * i / (PER - 1);
            int at = (int)(q * (npool - 1) + 0.5), to, best = -1;
            if (at <= last) at = last + 1;
            if (at >= npool) at = npool - 1;
            to = ch == 0 && i == 0 ? at : at + npool / (3 * PER);
            if (i == PER - 1 && at < npool * 9 / 10) at = npool * 9 / 10;   /* the showpiece: among the hardest */
            if (i == PER - 1) to = npool - 1;
            if (to >= npool) to = npool - 1;
            double best_score = -1e9;
            for (int k = at; k <= to; k++) {    /* the best looking of a few close in difficulty */
                double score = pool[k].taste - (pool[k].gap == gap ? 0.6 : 0);
                if (score > best_score) { best_score = score; best = k; }
            }
            last = best;
            gap = pool[best].gap;
            out[nout] = pool[best];
            out[nout].text[0] = CH[ch].picture == 1 || (CH[ch].picture == 2 && i % 2) ? 'p' : 'n';
            nout++;
        }
    }
    printf("/*\n"
           " * The 15 Puzzle's levels: our own, made by tools/taquin_gen.c (trays slid out of order by chance, the exact\n"
           " * par found by the rules' search, six chapters of 25 from the easiest). Each: 'n' numbered tiles or 'p' cut\n"
           " * from the picture, then the rows (tiles 1..9 a..o, '.' the gap); in the comment its par, the search's first\n"
           " * bound and the positions it looked at; the pars again in tq_level_par (the tests search them again).\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "const char *const tq_levels[TQ_LEVELS] = {\n");
    for (int i = 0; i < nout; i++)
        printf("    \"%s\", /* %d: par %d, bound %d, %ld looked at */\n", out[i].text, i + 1, out[i].par, out[i].bound,
               out[i].looked);
    printf("};\n\nconst uint8_t tq_level_par[TQ_LEVELS] = {");
    for (int i = 0; i < nout; i++) printf("%s%d,", i % 25 ? " " : "\n    ", out[i].par);
    printf("\n};\n");
    return nout == TQ_LEVELS ? 0 : 1;
}
