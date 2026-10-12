/*
 * The 15 Puzzle: the rules (taquin.h): the parity, the shuffle, the slides and undo. No SDL: tested by
 * tests/taquin_test.c.
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
