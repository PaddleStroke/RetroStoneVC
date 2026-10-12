/*
 * The Tower of Hanoi: the rules (hanoi.h). No SDL: tested by tests/hanoi_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/hanoi.h"

#include <string.h>

void hn_new(hn_game *g, int discs)
{
    memset(g, 0, sizeof *g);
    g->n = discs < 3 ? 3 : discs > HN_MAX ? HN_MAX : discs;
    for (int i = 0; i < g->n; i++) g->disc[0][i] = (int8_t)(g->n - i);
    g->h[0] = g->n;
}

int hn_optimum(int discs) { return (1 << discs) - 1; }

int hn_top(const hn_game *g, int p) { return p < 0 || p > 2 || !g->h[p] ? 0 : g->disc[p][g->h[p] - 1]; }

int hn_can(const hn_game *g, int from, int to)
{
    if (from < 0 || from > 2 || to < 0 || to > 2 || from == to || !g->h[from]) return 0;
    return !g->h[to] || hn_top(g, to) > hn_top(g, from);
}

static void shift(hn_game *g, int from, int to)
{
    g->disc[to][g->h[to]++] = g->disc[from][--g->h[from]];
    g->disc[from][g->h[from]] = 0;
}

int hn_move(hn_game *g, int from, int to)
{
    if (!hn_can(g, from, to)) return 0;
    shift(g, from, to);
    if (g->nh >= HN_HIST) {                     /* forget the oldest half */
        memmove(g->hist, g->hist + HN_HIST / 2, HN_HIST / 2);
        g->nh -= HN_HIST / 2;
    }
    g->hist[g->nh++] = (uint8_t)(from * 3 + to);
    g->moves++;
    return 1;
}

int hn_undo(hn_game *g)
{
    if (!g->nh) return 0;
    int m = g->hist[--g->nh];
    shift(g, m % 3, m / 3);
    g->moves--;
    return 1;
}

int hn_solved(const hn_game *g) { return g->h[1] == g->n || g->h[2] == g->n; }

/* the shortest way, recursively: n - 1 discs aside, the largest across, the n - 1 back on it */
void hn_way(int n, int a, int b, int k, int *from, int *to)
{
    int c = 3 - a - b, half = (1 << (n - 1)) - 1;
    if (k < half) hn_way(n - 1, a, c, k, from, to);
    else if (k == half) { *from = a; *to = b; }
    else hn_way(n - 1, c, b, k - half - 1, from, to);
}

int hn_script(hn_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ' || *s == ',') { s++; continue; }
        if (*s == '=') {
            int k = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            for (int i = 0; i < k; i++) {
                int f, t, done = g->moves;
                if (done >= hn_optimum(g->n)) return 0;
                hn_way(g->n, 0, 2, done, &f, &t);
                if (!hn_move(g, f, t)) return 0;
            }
            continue;
        }
        if (s[0] < 'a' || s[0] > 'c' || s[1] < 'a' || s[1] > 'c') return 0;
        if (!hn_move(g, s[0] - 'a', s[1] - 'a')) return 0;
        s += 2;
    }
    return 1;
}
