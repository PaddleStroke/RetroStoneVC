/*
 * The 15 Puzzle: the rules (taquin.h): the parity, the shuffle, the slides and undo, the search for the fewest moves,
 * the levels. No SDL: tested by tests/taquin_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/taquin.h"

#include <string.h>

#include "games/puzrand.h"

int tq_inversions(const int8_t *at, int n)
{
    int k = 0;
    for (int i = 0; i < n * n; i++)
        for (int j = i + 1; j < n * n; j++)
            if (at[i] && at[j] && at[i] > at[j]) k++;
    return k;
}

int tq_solvable(const int8_t *at, int n)
{
    int inv = tq_inversions(at, n), gap = 0;
    for (int i = 0; i < n * n; i++)
        if (!at[i]) gap = i;
    if (n & 1) return !(inv & 1);
    return ((inv + n - gap / n) & 1) == 1;      /* the gap's row from the bottom: n - row */
}

static int in_order(const int8_t *at, int n)
{
    for (int i = 0; i < n * n - 1; i++)
        if (at[i] != i + 1) return 0;
    return 1;
}

void tq_shuffle(int n, uint32_t seed, int8_t *at)
{
    uint32_t s = pr_seed(seed);
    int t[TQ_PLACES];
    do {
        for (int i = 0; i < n * n; i++) t[i] = i;
        pr_shuffle(&s, t, n * n);
        for (int i = 0; i < n * n; i++) at[i] = (int8_t)t[i];
        if (!tq_solvable(at, n)) {              /* the first two tiles swapped: the other parity */
            int a = -1, b = -1;
            for (int i = 0; i < n * n && b < 0; i++)
                if (at[i]) { if (a < 0) a = i; else b = i; }
            int8_t x = at[a];
            at[a] = at[b];
            at[b] = x;
        }
    } while (in_order(at, n));
}

void tq_load(tq_game *g, int n, const int8_t *at)
{
    int pic = g->picture;
    memset(g, 0, sizeof *g);
    g->picture = pic;
    g->n = n;
    for (int i = 0; i < n * n; i++) {
        g->at[i] = at[i];
        if (!at[i]) g->gap = i;
    }
}

void tq_new(tq_game *g, int variant, uint32_t seed)
{
    if (variant < 0 || variant >= TQ_VARIANTS) variant = 1;
    int n = 3 + variant % 3;
    int8_t at[TQ_PLACES];
    tq_shuffle(n, seed, at);
    g->picture = variant >= 3;
    tq_load(g, n, at);
}

int tq_line(const tq_game *g, int p)
{
    int n = g->n;
    if (p < 0 || p >= n * n || p == g->gap) return 0;
    int px = p % n, py = p / n, gx = g->gap % n, gy = g->gap / n;
    if (px == gx) return py > gy ? py - gy : gy - py;
    if (py == gy) return px > gx ? px - gx : gx - px;
    return 0;
}

/* the gap walks to p, one place at a time: each tile on the way slides into it */
static void walk(tq_game *g, int p)
{
    int n = g->n, step = p % n == g->gap % n ? (p > g->gap ? n : -n) : (p > g->gap ? 1 : -1);
    while (g->gap != p) {
        int next = g->gap + step;
        g->at[g->gap] = g->at[next];
        g->at[next] = 0;
        g->gap = next;
    }
}

int tq_tap(tq_game *g, int p)
{
    int k = tq_line(g, p);
    if (!k) return 0;
    if (g->nh >= TQ_HIST) {                     /* forget the oldest half */
        memmove(g->hist, g->hist + TQ_HIST / 2, TQ_HIST / 2);
        g->nh -= TQ_HIST / 2;
    }
    g->hist[g->nh++] = (int8_t)g->gap;
    walk(g, p);
    g->moves += k;
    return k;
}

int tq_undo(tq_game *g)
{
    if (!g->nh) return -1;
    int back = g->hist[--g->nh];
    g->moves -= tq_line(g, back);
    walk(g, back);
    return back;
}

int tq_solved(const tq_game *g) { return in_order(g->at, g->n); }

int tq_place_of(const tq_game *g, int tile)
{
    for (int i = 0; i < g->n * g->n; i++)
        if (g->at[i] == tile) return i;
    return -1;
}

int tq_script(tq_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == '@') {                        /* in order but the last tile, the gap beside it */
            int8_t at[TQ_PLACES];
            int n = g->n, moves = g->moves;
            for (int i = 0; i < n * n; i++) at[i] = (int8_t)(i + 1);
            at[n * n - 1] = (int8_t)(n * n - 1);
            at[n * n - 2] = 0;
            tq_load(g, n, at);
            g->moves = moves;
            s++;
            continue;
        }
        int t = 0;
        if (*s < '0' || *s > '9') return 0;
        for (; *s >= '0' && *s <= '9'; s++) t = t * 10 + *s - '0';
        if (!tq_tap(g, tq_place_of(g, t))) return 0;
    }
    return 1;
}

/* ---- the search -------------------------------------------------------------------------------------------------- */
typedef struct {
    int n, gap, md, wq, len;
    int8_t at[TQ_PLACES];
    int lc[2][TQ_MAX];                          /* each row's (0) and column's (1) linear conflicts */
    long nodes, budget;
} srch;

static int dist_of(int n, int tile, int place)
{
    int goal = tile - 1, dx = goal % n - place % n, dy = goal / n - place / n;
    return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

/* a row (col 0) or a column's conflicts: of its tiles that belong to it, all but the longest run in order must leave
 * it and come back (two moves each) */
static int line_lc(const int8_t *at, int n, int col, int line)
{
    int v[TQ_MAX], k = 0;
    for (int i = 0; i < n; i++) {
        int place = col ? line + i * n : line * n + i, t = at[place];
        if (!t) continue;
        int gr = (t - 1) / n, gc = (t - 1) % n;
        if (col ? gc == line : gr == line) v[k++] = col ? gr : gc;
    }
    int best = 0, run[TQ_MAX];
    for (int i = 0; i < k; i++) {
        run[i] = 1;
        for (int j = 0; j < i; j++)
            if (v[j] < v[i] && run[j] + 1 > run[i]) run[i] = run[j] + 1;
        if (run[i] > best) best = run[i];
    }
    return 2 * (k - best);
}

int tq_bound(const int8_t *at, int n)
{
    int h = 0;
    for (int p = 0; p < n * n; p++)
        if (at[p]) h += dist_of(n, at[p], p);
    for (int l = 0; l < n; l++) h += line_lc(at, n, 0, l) + line_lc(at, n, 1, l);
    return h;
}

static int lc_all(const srch *s)
{
    int h = 0;
    for (int l = 0; l < s->n; l++) h += s->lc[0][l] + s->lc[1][l];
    return h;
}

/* depth first within the bound (quarters): 1 found (its length in s->len), 0 not within it (*next: the least f
 * beyond), -1 out of budget */
static int dfs(srch *s, int g, int bound, int prev, int lc, int *next)
{
    int h = s->md + lc, f = 4 * g + s->wq * h;
    if (f > bound) {
        if (f < *next) *next = f;
        return 0;
    }
    if (!h) {
        s->len = g;
        return 1;
    }
    if (++s->nodes > s->budget) return -1;
    int n = s->n, gap = s->gap, gx = gap % n, gy = gap / n;
    static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
    for (int d = 0; d < 4; d++) {
        int x = gx + DX[d], y = gy + DY[d];
        if (x < 0 || y < 0 || x >= n || y >= n) continue;
        int p = x + y * n;
        if (p == prev) continue;
        int t = s->at[p], dmd = dist_of(n, t, gap) - dist_of(n, t, p);
        /* the tile goes from p into the gap: across (its column changes) or up and down (its row changes) */
        int col = DY[d] == 0, a = col ? x : y, b = col ? gx : gy, oa = s->lc[col][a], ob = s->lc[col][b];
        s->at[gap] = (int8_t)t;
        s->at[p] = 0;
        s->gap = p;
        s->md += dmd;
        s->lc[col][a] = line_lc(s->at, n, col, a);
        s->lc[col][b] = line_lc(s->at, n, col, b);
        int r = dfs(s, g + 1, bound, gap, lc - oa - ob + s->lc[col][a] + s->lc[col][b], next);
        s->lc[col][a] = oa;
        s->lc[col][b] = ob;
        s->md -= dmd;
        s->gap = gap;
        s->at[p] = (int8_t)t;
        s->at[gap] = 0;
        if (r) return r;
    }
    return 0;
}

int tq_search(const int8_t *at, int n, int wq, long budget, long *looked)
{
    srch s;
    memset(&s, 0, sizeof s);
    s.n = n;
    s.wq = wq < 4 ? 4 : wq;
    s.budget = budget;
    memcpy(s.at, at, (size_t)n * n);
    for (int p = 0; p < n * n; p++) {
        if (!at[p]) s.gap = p;
        else s.md += dist_of(n, at[p], p);
    }
    for (int l = 0; l < n; l++) {
        s.lc[0][l] = line_lc(at, n, 0, l);
        s.lc[1][l] = line_lc(at, n, 1, l);
    }
    int lc = lc_all(&s), bound = s.wq * (s.md + lc), r = 0;
    for (;;) {
        int next = 1 << 30;
        r = dfs(&s, 0, bound, -1, lc, &next);
        if (r || next == 1 << 30) break;
        bound = next;
    }
    if (looked) *looked = s.nodes;
    return r == 1 ? s.len : -1;
}

int tq_par(const tq_game *g)
{
    if (g->n <= 3) return tq_search(g->at, g->n, 4, 50000000L, NULL);
    static const int W4[] = {4, 6, 8, 12}, W5[] = {8, 12, 16, 24};
    const int *w = g->n == 4 ? W4 : W5;
    for (int i = 0; i < 4; i++) {
        int len = tq_search(g->at, g->n, w[i], i ? 300000L : 600000L, NULL);
        if (len >= 0) return len;
    }
    return 0;
}

/* ---- the levels -------------------------------------------------------------------------------------------------- */
int tq_parse(tq_game *g, const char *level)
{
    static const char TILES[] = ".123456789abcdefghijklmno";
    if ((level[0] != 'n' && level[0] != 'p') || level[1] != ':') return 0;
    const char *s = level + 2;
    int n = 0;
    while (s[n] && s[n] != '|') n++;
    if (n < 3 || n > TQ_MAX) return 0;
    int8_t at[TQ_PLACES];
    int k = 0, seen = 0;
    for (; *s; s++) {
        if (*s == '|') continue;
        const char *c = strchr(TILES, *s);
        if (!c || k >= n * n || c - TILES >= n * n || (seen >> (c - TILES) & 1)) return 0;
        seen |= 1 << (c - TILES);
        at[k++] = (int8_t)(c - TILES);
    }
    if (k != n * n || !tq_solvable(at, n)) return 0;
    g->picture = level[0] == 'p';
    tq_load(g, n, at);
    return 1;
}

#ifndef TQ_GEN                                  /* not in the levels' tool (tools/taquin_gen.c): it makes them */
#include "games/taquin_levels.h"

int tq_level(tq_game *g, int level)
{
    if (level < 0 || level >= TQ_LEVELS || !tq_parse(g, tq_levels[level])) return 0;
    g->par = tq_level_par[level];
    return 1;
}
#endif
