/*
 * The Knight's Tour's levels, made by chance and chosen: src/games/knightstour_levels.h. Our own boards: no published
 * set is used. Each candidate:
 *   - a board: a rectangle (3 to 10 squares a side), plain, or with holes cut at random (scattered, or in pairs and
 *     fours round the middle), its corners cut away (crosses, steps), a frame round a hole in the middle, or a
 *     diamond; every square reachable from every other by knight's moves;
 *   - a start: squares tried in turn, kept when the rules' solver finds a whole tour from it (kt_solve);
 *   - its grade: the squares, how far a knight jumping at random gets before it is stuck (the mean of 300 walks: a
 *     board that traps a careless knight is harder), and whether Warnsdorff's rule alone (no look ahead) gets round;
 *   - the hint followed from the start to the end (as the test does), so every level can be played by the hint alone.
 * Mirror and rotation duplicates are dropped (one start per board shape). Six chapters by the board's squares
 * (9..20, 20..30, 30..40, 40..52, 52..68, 68..100), 25 each, chosen evenly over the chapter's grades, easiest first.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/ktgen apps/boardgames/tools/knightstour_gen.c \
 *       apps/boardgames/src/games/knightstour_rules.c && /tmp/ktgen > apps/boardgames/src/games/knightstour_levels.h
 * Deterministic: a fixed seed, no clock in the choice. MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/knightstour.h"
#include "games/puzrand.h"

#define CANDS 1200
#define TRIED (1 << 19)              /* the shapes tried (a hash set) */
#define PER 25
#define WALKS 300

typedef struct {
    int w, h, start, cells, holes, wd_fail;
    uint8_t hole[KT_MAX];
    float cov, score;
    char rows[KT_MAX + KT_MAXW + 1], shape[KT_MAX + KT_MAXW + 8];
} level;

static uint32_t rng;
static level cand[CANDS];
static int ncand;

/* the rows from the top ('.' '#' 'N'; start -1: no knight) */
static void to_rows(int w, int h, const uint8_t *hole, int start, char *o)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int s = (h - 1 - y) * w + x;
            *o++ = hole[s] ? '#' : s == start ? 'N' : '.';
        }
        if (y < h - 1) *o++ = '|';
    }
    *o = 0;
}

/* the board turned (t: 0..7, the square's symmetries): its size and holes, and where a square goes */
static int turn_sq(int w, int h, int t, int s, int *tw, int *th)
{
    int x = s % w, y = s / w, X = x, Y = y;
    if (t & 1) X = w - 1 - x;                   /* mirrored */
    if (t & 2) Y = h - 1 - y;                   /* upside down */
    *tw = w;
    *th = h;
    if (t & 4) {                                /* transposed */
        int k = X;
        X = Y;
        Y = k;
        *tw = h;
        *th = w;
    }
    return Y * *tw + X;
}

/* the canonical shape: the smallest of the eight turns' rows (with the start or without) */
static void canonical(const level *L, int with_start, char *out)
{
    out[0] = 0;
    for (int t = 0; t < 8; t++) {
        uint8_t hole[KT_MAX];
        int tw = 0, th = 0, st = -1;
        for (int s = 0; s < L->w * L->h; s++) {
            int d = turn_sq(L->w, L->h, t, s, &tw, &th);
            hole[d] = L->hole[s];
            if (s == L->start) st = d;
        }
        char buf[KT_MAX + KT_MAXW + 8];
        snprintf(buf, sizeof buf, "%dx%d:", tw, th);
        to_rows(tw, th, hole, with_start ? st : -1, buf + strlen(buf));
        if (!out[0] || strcmp(buf, out) < 0) strcpy(out, buf);
    }
}

/* every square reachable from every other by knight's moves */
static int connected(int w, int h, const uint8_t *hole)
{
    static const int J[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
    int seen[KT_MAX] = {0}, q[KT_MAX], hd = 0, tl = 0, n = 0, first = -1;
    for (int s = 0; s < w * h; s++)
        if (!hole[s]) { n++; if (first < 0) first = s; }
    if (first < 0) return 0;
    seen[first] = 1;
    q[tl++] = first;
    while (hd < tl) {
        int s = q[hd++];
        for (int i = 0; i < 8; i++) {
            int x = s % w + J[i][0], y = s / w + J[i][1];
            if (x < 0 || y < 0 || x >= w || y >= h) continue;
            int t = y * w + x;
            if (!hole[t] && !seen[t]) { seen[t] = 1; q[tl++] = t; }
        }
    }
    return tl == n;
}

/* ---- the boards -------------------------------------------------------------------------------------------------- */
static void board(int *w, int *h, uint8_t *hole)
{
    int fam = pr_below(&rng, 7);
    *w = 3 + pr_below(&rng, 8);
    *h = 3 + pr_below(&rng, 8);
    memset(hole, 0, KT_MAX);
    int W = *w, H = *h, n = W * H;
    switch (fam) {
    case 0:                                     /* plain */
        break;
    case 1:                                     /* holes scattered */
    case 2: {
        int k = 1 + pr_below(&rng, 1 + n / 9);
        for (int i = 0; i < k; i++) hole[pr_below(&rng, n)] = 1;
        break;
    }
    case 3: {                                   /* holes in pairs (a half turn), or in fours (a quarter turn) */
        int k = 1 + pr_below(&rng, 1 + n / 16), four = W == H && pr_below(&rng, 2);
        for (int i = 0; i < k; i++) {
            int x = pr_below(&rng, W), y = pr_below(&rng, H);
            hole[y * W + x] = hole[(H - 1 - y) * W + W - 1 - x] = 1;
            if (four) hole[x * W + W - 1 - y] = hole[(W - 1 - x) * W + y] = 1;
        }
        break;
    }
    case 4: {                                   /* corners cut away: crosses, steps */
        int cw = 1 + pr_below(&rng, W / 3 > 0 ? W / 3 : 1), ch = 1 + pr_below(&rng, H / 3 > 0 ? H / 3 : 1);
        int which = 1 + pr_below(&rng, 15), same = pr_below(&rng, 2);
        for (int c = 0; c < 4; c++) {
            if (!(which >> c & 1) && !same) continue;
            for (int y = 0; y < ch; y++)
                for (int x = 0; x < cw; x++) {
                    int X = c & 1 ? W - 1 - x : x, Y = c & 2 ? H - 1 - y : y;
                    hole[Y * W + X] = 1;
                }
        }
        break;
    }
    case 5: {                                   /* a frame round a hole in the middle */
        if (W < 5 || H < 5) break;
        int iw = 1 + pr_below(&rng, W - 4), ih = 1 + pr_below(&rng, H - 4), x0 = (W - iw) / 2, y0 = (H - ih) / 2;
        for (int y = y0; y < y0 + ih; y++)
            for (int x = x0; x < x0 + iw; x++) hole[y * W + x] = 1;
        break;
    }
    default: {                                  /* a diamond (its points cut off a little or not) */
        float cx = (W - 1) / 2.0f, cy = (H - 1) / 2.0f, r = (W + H) / 4.0f + 0.5f * pr_below(&rng, 3);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) hole[y * W + x] = fabsf(x - cx) + fabsf(y - cy) > r;
        break;
    }
    }
}

/* how far a knight jumping at random gets (the squares visited, a part of all): the mean of WALKS walks */
static float walks(const kt_game *g0)
{
    double sum = 0;
    for (int i = 0; i < WALKS; i++) {
        kt_game g = *g0;
        uint8_t to[8];
        int k;
        while ((k = kt_moves(&g, to)) > 0) kt_move(&g, to[pr_below(&rng, k)]);
        sum += (double)g.len / g.cells;
    }
    return (float)(sum / WALKS);
}

/* Warnsdorff's rule alone, no look ahead (ties: the first of the rules' order): 1 when it gets round */
static int warnsdorff(const kt_game *g0)
{
    kt_game g = *g0;
    while (!kt_done(&g)) {
        uint8_t to[8];
        int k = kt_moves(&g, to), best = -1, bd = 99;
        for (int i = 0; i < k; i++) {
            int d = kt_onward(&g, to[i]);
            if (d < bd) { bd = d; best = to[i]; }
        }
        if (best < 0) return 0;
        kt_move(&g, best);
    }
    return 1;
}

/* the hint followed from the start: 1 when it gets round without a doubt */
static int by_hint(const kt_game *g0)
{
    kt_game g = *g0;
    while (!kt_done(&g)) {
        int s = kt_hint(&g);
        if (s < 0 || !kt_move(&g, s)) return 0;
    }
    return 1;
}

/* 1: the shape was tried already; else it is noted */
static int tried(const char *shape)
{
    static uint32_t set[TRIED];
    uint32_t h = 2166136261u;
    for (const char *p = shape; *p; p++) h = (h ^ (uint8_t)*p) * 16777619u;
    h |= 1;
    for (uint32_t i = h & (TRIED - 1);; i = (i + 1) & (TRIED - 1)) {
        if (set[i] == h) return 1;          /* a hash alike: taken as the same shape (rare; it only skips one) */
        if (!set[i]) { set[i] = h; return 0; }
    }
}

static int cmp(const void *a, const void *b)
{
    float d = ((const level *)a)->score - ((const level *)b)->score;
    return d < 0 ? -1 : d > 0;
}

int main(void)
{
    static const int lo[KT_CHAPTERS] = {8, 20, 30, 40, 52, 68}, hi[KT_CHAPTERS] = {20, 30, 40, 52, 68, 101};
    rng = pr_seed(20261012);
    int per[KT_CHAPTERS] = {0};
    for (long tries = 0; ncand < CANDS && tries < 400000; tries++) {
        level L;
        memset(&L, 0, sizeof L);
        board(&L.w, &L.h, L.hole);
        int cells = 0, holes = 0;
        for (int s = 0; s < L.w * L.h; s++) cells += !L.hole[s], holes += L.hole[s];
        int ch = -1;
        for (int c = 0; c < KT_CHAPTERS; c++)
            if (cells >= lo[c] && cells < hi[c]) ch = c;
        if (ch < 0 || per[ch] >= CANDS / KT_CHAPTERS) continue;
        if (!connected(L.w, L.h, L.hole)) continue;
        L.start = -1;
        canonical(&L, 0, L.shape);
        if (tried(L.shape)) continue;
        /* a start with a tour: the squares tried in a shuffled order */
        int sq[KT_MAX], n = 0;
        for (int s = 0; s < L.w * L.h; s++)
            if (!L.hole[s]) sq[n++] = s;
        pr_shuffle(&rng, sq, n);
        kt_game g;
        for (int i = 0; i < n && i < 6 && L.start < 0; i++) {
            kt_setup(&g, L.w, L.h, L.hole, sq[i]);
            if (kt_solve(&g, NULL, 200000L) == 1 && by_hint(&g)) L.start = sq[i];
        }
        if (L.start < 0) continue;
        kt_setup(&g, L.w, L.h, L.hole, L.start);
        L.cells = cells;
        L.holes = holes;
        L.cov = walks(&g);
        L.wd_fail = !warnsdorff(&g);
        L.score = cells * (1.6f - L.cov) + (L.wd_fail ? 6.0f : 0) + 0.3f * holes;
        to_rows(L.w, L.h, L.hole, L.start, L.rows);
        cand[ncand++] = L;
        per[ch]++;
        if (ncand % 100 == 0) fprintf(stderr, "%d candidates (%ld boards tried)\n", ncand, tries + 1);
    }
    static const char *const head[] = {
        "/*",
        " * The Knight's Tour's levels: our own boards, made by tools/knightstour_gen.c (rectangles plain, with holes,",
        " * corners cut, frames and diamonds, drawn by chance; each start checked by the rules' solver and by the hint",
        " * followed to the end), six chapters of 25 by the board's squares, each chapter from the easiest. Each: its",
        " * rows from the top ('.' a square, '#' a hole, 'N' the start), then (in the comment) its size, squares, holes,",
        " * the part of the board a knight jumping at random visits, whether Warnsdorff's rule alone fails, its grade.",
        " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).",
        " */",
        "const char *const kt_levels[KT_LEVELS] = {",
    };
    for (size_t i = 0; i < sizeof head / sizeof head[0]; i++) printf("%s\n", head[i]);
    int number = 0;
    for (int c = 0; c < KT_CHAPTERS; c++) {
        static level pool[CANDS];
        int np = 0;
        for (int i = 0; i < ncand; i++)
            if (cand[i].cells >= lo[c] && cand[i].cells < hi[c]) pool[np++] = cand[i];
        qsort(pool, (size_t)np, sizeof(level), cmp);
        fprintf(stderr, "chapter %d: %d candidates, grades %.1f..%.1f\n", c + 1, np, np ? pool[0].score : 0,
                np ? pool[np - 1].score : 0);
        if (np < PER) {
            fprintf(stderr, "chapter %d: too few candidates\n", c + 1);
            return 1;
        }
        for (int i = 0; i < PER; i++) {
            const level *L = &pool[(int)((long)(np - 1) * i / (PER - 1))];
            printf("    \"%s\", /* %d: %dx%d, %d squares, %d holes, %.0f%%%s, %.1f */\n", L->rows, ++number, L->w, L->h,
                   L->cells, L->holes, 100 * L->cov, L->wd_fail ? ", Warnsdorff fails" : "", L->score);
        }
    }
    printf("};\n");
    return 0;
}
