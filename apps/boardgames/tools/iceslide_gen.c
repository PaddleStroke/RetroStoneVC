/*
 * Ice Slide's levels, made by chance and chosen: src/games/iceslide_levels.h. Our own levels: no published set is used.
 * Each chapter has its sizes of ice, its range of pars and of rocks. A candidate:
 *   - rocks laid by chance, a start drawn among the free squares, and every other free square tried as the hole: one
 *     whose fewest pushes (the rules' search, ics_solve) lie in the chapter's range is drawn;
 *   - measured: its par, the places the puck can rest at from the start, the traps among them (places from which the
 *     hole can no longer be reached), and how many shortest ways there are (one way alone is harder to find);
 *   - kept unless it is a turn or a mirror of one kept already.
 * The difficulty: the par, more with every place to rest and every trap, less with many shortest ways. Each chapter
 * keeps 25 candidates spread evenly over its difficulties, from the easiest; the first level of all is the easiest of
 * the first chapter. Deterministic (fixed seeds): run it again, the same levels come out.
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
#define POOL 1500
#define TRIES 600000

/* each chapter: two sizes of ice (w, h), the par's range, the rocks' range */
static const struct { int w[2], h[2], lo, hi, rlo, rhi; } CH[ICS_CHAPTERS] = {
    {{5, 6}, {5, 5}, 2, 5, 3, 6},
    {{6, 7}, {6, 6}, 4, 7, 5, 9},
    {{7, 8}, {7, 6}, 6, 9, 7, 11},
    {{8, 8}, {7, 8}, 8, 12, 9, 14},
    {{9, 9}, {8, 9}, 10, 14, 12, 17},
    {{10, 10}, {9, 9}, 13, 18, 15, 21},
};

typedef struct {
    int w, h, par, places, traps;
    double ways, diff;
    char rows[ICS_CELLS + ICS_MAXH + 1], key[ICS_CELLS + 8];
} cand;

static cand pool[POOL];
static int npool;
static uint32_t rng;

/* the board as text, row by row: '#' rock, 'P' the puck, 'O' the hole, '.' ice */
static char cell_ch(const ics_board *b, int start, int c)
{
    return b->rock[c] ? '#' : c == start ? 'P' : c == b->goal ? 'O' : '.';
}

/* the key that a board and its turns and mirrors share: the smallest of their texts */
static void canon(const ics_board *b, int start, char *key)
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
                s[k++] = cell_ch(b, start, ux + uy * w);
            }
        s[k] = 0;
        if (!best[0] || strcmp(s, best) < 0) memcpy(best, s, (size_t)k + 1);
    }
    strcpy(key, best);
}

/* the par, the places to rest at, the traps, the shortest ways */
static void measure(const ics_board *b, int start, cand *c)
{
    int16_t dist[ICS_CELLS];
    double cnt[ICS_CELLS];
    int q[ICS_CELLS], head = 0, tail = 0;
    for (int i = 0; i < ICS_CELLS; i++) dist[i] = -1, cnt[i] = 0;
    dist[start] = 0;
    cnt[start] = 1;
    q[tail++] = start;
    while (head < tail) {
        int at = q[head++];
        if (at == b->goal) continue;
        for (int d = 0; d < 4; d++) {
            int len, to = ics_slide(b, at, d, &len);
            if (!len) continue;
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
    c->diff = c->par + 0.06 * c->places + 0.5 * (c->traps < 6 ? c->traps : 6) - 0.7 * log2(c->ways);
}

static int known(const char *key)
{
    for (int i = 0; i < npool; i++)
        if (!strcmp(pool[i].key, key)) return 1;
    return 0;
}

static int seen_before(const char *key, char (*keys)[ICS_CELLS + 8], int n)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(keys[i], key)) return 1;
    return 0;
}

static int by_diff(const void *a, const void *b)
{
    double d = ((const cand *)a)->diff - ((const cand *)b)->diff;
    return d < 0 ? -1 : d > 0;
}

int main(void)
{
    static char kept[ICS_LEVELS][ICS_CELLS + 8];
    static cand out[ICS_LEVELS];
    int nkept = 0;
    for (int ch = 0; ch < ICS_CHAPTERS; ch++) {
        rng = pr_seed(1000u + (uint32_t)ch);
        npool = 0;
        for (int tries = 0; tries < TRIES && npool < POOL; tries++) {
            ics_board b;
            memset(&b, 0, sizeof b);
            int sz = pr_below(&rng, 2);
            b.w = CH[ch].w[sz];
            b.h = CH[ch].h[sz];
            int cells = b.w * b.h, rocks = CH[ch].rlo + pr_below(&rng, CH[ch].rhi - CH[ch].rlo + 1);
            for (int k = 0; k < rocks;) {
                int c = pr_below(&rng, cells);
                if (!b.rock[c]) { b.rock[c] = 1; k++; }
            }
            int start;
            do start = pr_below(&rng, cells);
            while (b.rock[start]);
            b.goal = -1;
            if (ics_reach(&b, start) < CH[ch].lo + 3) continue;
            int goals[ICS_CELLS], ng = 0;
            for (int c = 0; c < cells; c++) {
                if (b.rock[c] || c == start) continue;
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
            measure(&b, start, &c);
            if (c.places < c.par + 2) continue;   /* hardly a choice on the way */
            canon(&b, start, c.key);
            if (known(c.key) || seen_before(c.key, kept, nkept)) continue;
            int k = 0;
            for (int y = 0; y < b.h; y++) {
                if (y) c.rows[k++] = '|';
                for (int x = 0; x < b.w; x++) c.rows[k++] = cell_ch(&b, start, x + y * b.w);
            }
            c.rows[k] = 0;
            pool[npool++] = c;
        }
        qsort(pool, (size_t)npool, sizeof *pool, by_diff);
        fprintf(stderr, "chapter %d: %d candidates, difficulty %.1f .. %.1f\n", ch + 1, npool, pool[0].diff,
                pool[npool - 1].diff);
        for (int i = 0, last = -1; i < PER; i++) {
            double q = ch == 0 && i == 0 ? 0 : 0.03 + 0.94 * i / (PER - 1);
            int at = (int)lround(q * (npool - 1));
            if (at <= last) at = last + 1;      /* never the same one twice */
            if (at >= npool) at = npool - 1;
            last = at;
            out[nkept] = pool[at];
            strcpy(kept[nkept++], pool[at].key);
        }
    }
    printf("/*\n"
           " * Ice Slide's levels: our own, made by tools/iceslide_gen.c (boards drawn by chance, measured by the rules'\n"
           " * search, spread over six chapters of 25 from the easiest). Each: the rows ('#' a rock, 'P' the puck, 'O' the\n"
           " * hole), then (in the comment) its par, the places to rest at, the traps and the shortest ways; the pars again\n"
           " * in ics_level_par (the tests check them).\n"
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
