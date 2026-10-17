/*
 * Ice Slide's levels, made and chosen with taste: src/games/iceslide_levels.h. Our own levels: no published set is
 * used. Each chapter has its sizes of ice, its range of pars and of rocks. A candidate:
 *   - rocks laid as a designer would: in a pattern with a symmetry (a half turn, a mirror, both mirrors, a quarter
 *     turn on square ice, or pillars on a lattice), never four in a square block, the ice left in one piece and the
 *     rim not crowded;
 *   - a start drawn among the free squares, and every other free square (but the corners and those near the start)
 *     tried as the hole: one whose fewest pushes (the rules' search, ics_solve) lie in the chapter's range is drawn;
 *   - measured: its par, the places the puck can rest at, the traps among them (places from which the hole can no
 *     longer be reached), how many shortest ways there are (one way alone is harder to find), how much of the ice
 *     the slides cross (the board is used, not a corner of it) and how far the shortest way spreads;
 *   - kept unless it is a turn or a mirror of one kept already, or its rocks were used already.
 * The difficulty: the par, more with every place to rest and every trap, less with many shortest ways. Each chapter's
 * 25 levels follow a rising curve of pars (gentle in the first chapter), the easier ones of a par first, the patterns
 * taking turns; the last level of a chapter is its showpiece: of its hardest par, the richest pattern, the hardest.
 * Deterministic (fixed seeds): run it again, the same levels come out.
 *   gcc -O2 -Iapps/boardgames/src -o /tmp/icsgen apps/boardgames/tools/iceslide_gen.c -lm && \
 *       /tmp/icsgen > apps/boardgames/src/games/iceslide_levels.h
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#define ICS_GEN
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/iceslide_rules.c"

#define PER (ICS_LEVELS / ICS_CHAPTERS)
#define POOL 2500
#define TRIES 400000

/* each chapter: two sizes of ice (w, h), the par's range, the rocks' range */
static const struct { int w[2], h[2], lo, hi, rlo, rhi; } CH[ICS_CHAPTERS] = {
    {{5, 6}, {5, 5}, 2, 5, 4, 6},
    {{6, 7}, {6, 6}, 4, 7, 6, 9},
    {{7, 8}, {7, 6}, 6, 9, 8, 11},
    {{8, 8}, {7, 8}, 8, 12, 10, 14},
    {{9, 9}, {8, 9}, 10, 14, 12, 17},
    {{10, 10}, {9, 9}, 13, 18, 14, 21},
};

/* the rock patterns: their symmetry */
enum { S_TURN, S_MIRROR, S_MIRROR2, S_QUARTER, S_LATTICE, STYLES };
static const int RICH[STYLES] = {1, 1, 3, 3, 2};     /* how showy a pattern is (for the showpieces) */

typedef struct {
    int w, h, par, places, traps, style, cover;
    double ways, diff, spread;
    char rows[ICS_CELLS + ICS_MAXH + 1], key[ICS_CELLS + 8], rkey[ICS_CELLS + 8];
} cand;

static cand pool[POOL];
static int npool;
static uint32_t rng;

/* the board as text, row by row: '#' rock, 'P' the puck, 'O' the hole, '.' ice */
static char cell_ch(const ics_board *b, int start, int c)
{
    return b->rock[c] ? '#' : c == start ? 'P' : c == b->goal ? 'O' : '.';
}

/* the key that a board and its turns and mirrors share: the smallest of their texts (of the rocks alone if asked) */
static void canon(const ics_board *b, int start, int rocks_only, char *key)
{
    int w = b->w, h = b->h;
    char best[ICS_CELLS + 8] = {0}, s[ICS_CELLS + 8];
    for (int t = 0; t < 8; t++) {
        int sw = t & 4 ? h : w, sh = t & 4 ? w : h;
        if ((t & 4) && w != h) continue;        /* a quarter turn changes the size: only for a square */
        int k = snprintf(s, sizeof s, "%dx%d:", sw, sh);
        for (int y = 0; y < sh; y++)
            for (int x = 0; x < sw; x++) {
                int ux = t & 4 ? y : x, uy = t & 4 ? x : y;
                if (t & 1) ux = w - 1 - ux;
                if (t & 2) uy = h - 1 - uy;
                int c = ux + uy * w;
                s[k++] = rocks_only ? (b->rock[c] ? '#' : '.') : cell_ch(b, start, c);
            }
        s[k] = 0;
        if (!best[0] || strcmp(s, best) < 0) memcpy(best, s, (size_t)k + 1);
    }
    strcpy(key, best);
}

/* the cells a cell shares its place with under the pattern's symmetry */
static int orbit(int style, int w, int h, int x, int y, int *out)
{
    int n = 0, xs[4], ys[4];
    xs[n] = x, ys[n++] = y;
    switch (style) {
    case S_TURN: case S_LATTICE: xs[n] = w - 1 - x, ys[n++] = h - 1 - y; break;
    case S_MIRROR: xs[n] = w - 1 - x, ys[n++] = y; break;
    case S_MIRROR2:
        xs[n] = w - 1 - x, ys[n++] = y;
        xs[n] = x, ys[n++] = h - 1 - y;
        xs[n] = w - 1 - x, ys[n++] = h - 1 - y;
        break;
    case S_QUARTER:
        xs[n] = w - 1 - y, ys[n++] = x;
        xs[n] = w - 1 - x, ys[n++] = h - 1 - y;
        xs[n] = y, ys[n++] = w - 1 - x;
        break;
    }
    int m = 0;
    for (int i = 0; i < n; i++) {
        int c = xs[i] + ys[i] * w, dup = 0;
        for (int j = 0; j < m; j++) dup |= out[j] == c;
        if (!dup) out[m++] = c;
    }
    return m;
}

static int free_connected(const ics_board *b)
{
    int cells = b->w * b->h, seen[ICS_CELLS] = {0}, q[ICS_CELLS], h = 0, t = 0, n = 0, s = -1;
    for (int c = 0; c < cells; c++)
        if (!b->rock[c]) { n++; if (s < 0) s = c; }
    seen[s] = 1;
    q[t++] = s;
    while (h < t) {
        int c = q[h++], x = c % b->w, y = c / b->w;
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d], m = nx + ny * b->w;
            if (nx < 0 || ny < 0 || nx >= b->w || ny >= b->h || b->rock[m] || seen[m]) continue;
            seen[m] = 1;
            q[t++] = m;
        }
    }
    return t == n;
}

/* the rocks in their pattern: 0 when the taste rules refuse it */
static int lay(ics_board *b, int style, int rocks)
{
    int w = b->w, h = b->h, lx = pr_below(&rng, 2), ly = pr_below(&rng, 2), n = 0;
    for (int tries = 0; n < rocks && tries < 300; tries++) {
        int x = pr_below(&rng, w), y = pr_below(&rng, h), o[4];
        int m = orbit(style, w, h, x, y, o), ok = 1;
        if (n + m > rocks + 2) continue;
        for (int i = 0; i < m; i++) {
            if (b->rock[o[i]]) ok = 0;
            if (style == S_LATTICE && ((o[i] % w) % 2 != lx || (o[i] / w) % 2 != ly)) ok = 0;
        }
        if (!ok) continue;
        for (int i = 0; i < m; i++) b->rock[o[i]] = 1;
        n += m;
    }
    if (n < rocks) return 0;
    int edge = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int c = x + y * w;
            if (!b->rock[c]) continue;
            edge += x == 0 || y == 0 || x == w - 1 || y == h - 1;
            if (x + 1 < w && y + 1 < h && b->rock[c + 1] && b->rock[c + w] && b->rock[c + w + 1]) return 0;
        }
    if (edge * 2 > n + 1) return 0;             /* the rocks out on the ice, not along its rim */
    return free_connected(b);
}

/* the par, the places to rest at, the traps, the shortest ways, the ice crossed, the way's spread */
static void measure(const ics_board *b, int start, cand *c)
{
    int16_t dist[ICS_CELLS];
    double cnt[ICS_CELLS];
    uint8_t crossed[ICS_CELLS] = {0};
    int q[ICS_CELLS], head = 0, tail = 0, cells = b->w * b->h;
    for (int i = 0; i < ICS_CELLS; i++) dist[i] = -1, cnt[i] = 0;
    dist[start] = 0;
    cnt[start] = 1;
    q[tail++] = start;
    while (head < tail) {
        int at = q[head++];
        crossed[at] = 1;
        if (at == b->goal) continue;
        for (int d = 0; d < 4; d++) {
            int len, to = ics_slide(b, at, d, &len);
            if (!len) continue;
            for (int k = 1; k <= len; k++) crossed[(at % b->w + k * DX[d]) + (at / b->w + k * DY[d]) * b->w] = 1;
            if (dist[to] < 0) {
                dist[to] = (int16_t)(dist[at] + 1);
                cnt[to] = cnt[at];
                q[tail++] = to;
            } else if (dist[to] == dist[at] + 1) cnt[to] += cnt[at];
        }
    }
    c->par = dist[b->goal];
    c->ways = cnt[b->goal];
    c->places = 0;
    c->traps = 0;
    for (int i = 0; i < tail; i++) {
        if (q[i] == b->goal) continue;
        c->places++;
        if (ics_solve(b, q[i], NULL) < 0) c->traps++;
    }
    int fr = 0, cr = 0;
    for (int i = 0; i < cells; i++) fr += !b->rock[i], cr += crossed[i];
    c->cover = 100 * cr / fr;
    /* the shortest way's spread: the box its rests fill, against the board's */
    int8_t way[ICS_LONGEST];
    ics_solve(b, start, way);
    int x0 = start % b->w, x1 = x0, y0 = start / b->w, y1 = y0, at = start;
    for (int k = 0; k < c->par && k < ICS_LONGEST; k++) {
        int len;
        at = ics_slide(b, at, way[k], &len);
        int x = at % b->w, y = at / b->w;
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
    }
    c->spread = (double)(x1 - x0) / (b->w - 1) + (double)(y1 - y0) / (b->h - 1);
    c->diff = c->par + 0.06 * c->places + 0.5 * (c->traps < 6 ? c->traps : 6) - 0.7 * log2(c->ways);
}

static int seen_before(const char *key, char (*keys)[ICS_CELLS + 8], int n)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(keys[i], key)) return 1;
    return 0;
}

static int known(const char *key, const char *rkey)
{
    for (int i = 0; i < npool; i++)
        if (!strcmp(pool[i].key, key) || !strcmp(pool[i].rkey, rkey)) return 1;
    return 0;
}

static int by_par_diff(const void *a, const void *b)
{
    const cand *p = a, *q = b;
    if (p->par != q->par) return p->par - q->par;
    double d = p->diff - q->diff;
    return d < 0 ? -1 : d > 0;
}

int main(void)
{
    static char kept[ICS_LEVELS][ICS_CELLS + 8], rkept[ICS_LEVELS][ICS_CELLS + 8];
    static cand out[ICS_LEVELS];
    static uint8_t used[POOL];
    int nkept = 0;
    for (int ch = 0; ch < ICS_CHAPTERS; ch++) {
        rng = pr_seed(2000u + (uint32_t)ch);
        npool = 0;
        for (int tries = 0; tries < TRIES && npool < POOL; tries++) {
            ics_board b;
            memset(&b, 0, sizeof b);
            int sz = pr_below(&rng, 2);
            b.w = CH[ch].w[sz];
            b.h = CH[ch].h[sz];
            int style = pr_below(&rng, STYLES);
            if (style == S_QUARTER && b.w != b.h) style = S_MIRROR2;
            int cells = b.w * b.h, rocks = CH[ch].rlo + pr_below(&rng, CH[ch].rhi - CH[ch].rlo + 1);
            if (!lay(&b, style, rocks)) continue;
            int start;
            do start = pr_below(&rng, cells);
            while (b.rock[start]);
            b.goal = -1;
            if (ics_reach(&b, start) < CH[ch].lo + 3) continue;
            int goals[ICS_CELLS], ng = 0, sx = start % b.w, sy = start / b.w;
            for (int c = 0; c < cells; c++) {
                int x = c % b.w, y = c / b.w;
                if (b.rock[c] || c == start) continue;
                if ((x == 0 || x == b.w - 1) && (y == 0 || y == b.h - 1)) continue;   /* a corner: too plain */
                if (abs(x - sx) + abs(y - sy) < (b.w + b.h) / 3) continue;           /* too near the start */
                b.goal = c;
                int d = ics_solve(&b, start, NULL);
                if (d >= CH[ch].lo && d <= CH[ch].hi) goals[ng++] = c;
            }
            if (!ng) continue;
            b.goal = goals[pr_below(&rng, ng)];
            cand c;
            memset(&c, 0, sizeof c);
            c.w = b.w;
            c.h = b.h;
            c.style = style;
            measure(&b, start, &c);
            if (c.places < c.par + 2) continue;   /* hardly a choice on the way */
            if (c.cover < 70) continue;           /* the slides leave much of the ice unused */
            if (c.spread < (c.par <= 2 ? 0.7 : 1.1)) continue;   /* the way stays in a corner */
            canon(&b, start, 0, c.key);
            canon(&b, start, 1, c.rkey);
            if (known(c.key, c.rkey) || seen_before(c.key, kept, nkept) || seen_before(c.rkey, rkept, nkept)) continue;
            int k = 0;
            for (int y = 0; y < b.h; y++) {
                if (y) c.rows[k++] = '|';
                for (int x = 0; x < b.w; x++) c.rows[k++] = cell_ch(&b, start, x + y * b.w);
            }
            c.rows[k] = 0;
            pool[npool++] = c;
        }
        qsort(pool, (size_t)npool, sizeof *pool, by_par_diff);
        memset(used, 0, sizeof used);
        int bypar[64] = {0};
        for (int i = 0; i < npool; i++) bypar[pool[i].par]++;
        fprintf(stderr, "chapter %d: %d candidates; by par:", ch + 1, npool);
        for (int p = CH[ch].lo; p <= CH[ch].hi; p++) fprintf(stderr, " %d:%d", p, bypar[p]);
        fprintf(stderr, "\n");
        /* the showpiece (the chapter's last level): of the hardest par, the richest pattern, the hardest */
        int show = -1, toppar = CH[ch].hi;
        while (toppar > CH[ch].lo && bypar[toppar] < 3) toppar--;
        for (int i = 0; i < npool; i++) {
            if (pool[i].par != toppar) continue;
            if (show < 0 || RICH[pool[i].style] > RICH[pool[show].style] ||
                (RICH[pool[i].style] == RICH[pool[show].style] && pool[i].diff > pool[show].diff))
                show = i;
        }
        used[show] = 1;
        int prev = -1, prev2 = -1;
        for (int i = 0; i < PER; i++) {
            cand *c;
            if (i == PER - 1) c = &pool[show];
            else {
                /* the curve of pars: from the chapter's lowest to one below its showpiece, gentle at first */
                double t = (double)i / (PER - 1);
                if (ch == 0) t = pow(t, 1.15);
                double want = CH[ch].lo + t * (toppar - CH[ch].lo);
                int p = (int)floor(want);
                while (p > CH[ch].lo && !bypar[p]) p--;
                /* within the par, the easier ones first: where `want` lies between p and p + 1 */
                int first = 0;
                while (first < npool && pool[first].par < p) first++;
                int at = first + (int)((want - p) * (bypar[p] - 1) * 0.8);
                int pick = -1;
                for (int pass = 0; pass < 2 && pick < 0; pass++)
                    for (int dk = 0; dk < npool && pick < 0; dk++)
                        for (int sgn = 0; sgn < 2 && pick < 0; sgn++) {
                            int j = sgn ? at - dk : at + dk;
                            if (j < 0 || j >= npool || used[j] || pool[j].par != p) continue;
                            if (!pass && (pool[j].style == prev || pool[j].style == prev2)) continue;
                            pick = j;
                        }
                if (pick < 0) { fprintf(stderr, "chapter %d: no level for par %d\n", ch + 1, p); return 1; }
                if (ch == 0 && i == 0) {        /* the very first: the plainest of all */
                    for (int j = 0; j < npool && pool[j].par == p; j++)
                        if (!used[j] && pool[j].places < pool[pick].places) pick = j;
                }
                used[pick] = 1;
                c = &pool[pick];
                prev2 = prev;
                prev = c->style;
            }
            out[nkept] = *c;
            strcpy(kept[nkept], c->key);
            strcpy(rkept[nkept++], c->rkey);
        }
    }
    printf("/*\n"
           " * Ice Slide's levels: our own, made by tools/iceslide_gen.c (rocks laid in symmetric patterns, measured by\n"
           " * the rules' search, six chapters of 25 on a rising curve of pars, each ending with a showpiece). Each: the\n"
           " * rows ('#' a rock, 'P' the puck, 'O' the hole), then (in the comment) its par, the places to rest at, the\n"
           " * traps and the shortest ways; the pars again in ics_level_par (the tests check them).\n"
           " * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).\n"
           " */\n"
           "const char *const ics_levels[ICS_LEVELS] = {\n");
    for (int i = 0; i < ICS_LEVELS; i++)
        printf("    \"%s\", /* %d: par %d, %d places, %d traps, %.0f ways */\n", out[i].rows, i + 1, out[i].par,
               out[i].places, out[i].traps, out[i].ways);
    printf("};\n\nconst uint8_t ics_level_par[ICS_LEVELS] = {");
    for (int i = 0; i < ICS_LEVELS; i++) printf("%s%d,", i % 25 ? " " : "\n    ", out[i].par);
    printf("\n};\n");
    return 0;
}
