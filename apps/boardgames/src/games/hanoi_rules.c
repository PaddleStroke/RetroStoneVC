/*
 * The Tower of Hanoi: the rules (hanoi.h): positions, moves, undo, the fewest moves from anywhere and the hint, the
 * random positions, the levels. No SDL: tested by tests/hanoi_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/hanoi.h"

#include <string.h>

#include "games/puzrand.h"

/* the levels a chapter: 3 discs, 4, 5, 6, 7 */
static const int CHAPTER_LEVELS[HN_CHAPTERS] = {12, 18, 20, 25, 25};

static void where(const hn_game *g, int8_t *pos)   /* pos[d]: disc d's peg (d 1..n) */
{
    for (int p = 0; p < 3; p++)
        for (int i = 0; i < g->h[p]; i++) pos[g->disc[p][i]] = (int8_t)p;
}

/* the fewest moves gathering discs 1..k onto peg t: the largest one not there moves once, after every smaller one went
 * to the third peg, and the smaller ones then follow it (2^(k-1) - 1 more) */
static int gather(const int8_t *pos, int k, int t)
{
    int m = 0;
    for (; k > 0; k--) {
        if (pos[k] == t) continue;
        m += 1 << (k - 1);
        t = 3 - t - pos[k];
    }
    return m;
}

int hn_target(const hn_game *g)
{
    if (g->goal >= 0) return g->goal;
    int8_t pos[HN_MAX + 1];
    where(g, pos);
    return gather(pos, g->n, 2) <= gather(pos, g->n, 1) ? 2 : 1;
}

int hn_left(const hn_game *g)
{
    int8_t pos[HN_MAX + 1];
    where(g, pos);
    return gather(pos, g->n, hn_target(g));
}

int hn_hint(const hn_game *g, int *from, int *to)
{
    if (hn_solved(g)) return 0;
    int8_t pos[HN_MAX + 1];
    where(g, pos);
    int t = hn_target(g);
    for (int k = g->n; k > 0; k--) {
        if (pos[k] == t) continue;
        int other = 3 - t - pos[k];
        if (!gather(pos, k - 1, other)) {       /* the smaller ones out of the way: disc k goes */
            *from = pos[k];
            *to = t;
            return 1;
        }
        t = other;
    }
    return 0;
}

void hn_set(hn_game *g, int discs, const int8_t *peg, int goal)
{
    memset(g, 0, sizeof *g);
    g->n = discs < 3 ? 3 : discs > HN_MAX ? HN_MAX : discs;
    for (int d = g->n; d >= 1; d--) {
        int p = peg[d - 1] < 0 || peg[d - 1] > 2 ? 0 : peg[d - 1];
        g->disc[p][g->h[p]++] = (int8_t)d;
    }
    g->goal = goal < -1 || goal > 2 ? -1 : goal;
    g->par = hn_left(g);
}

void hn_new(hn_game *g, int discs)
{
    int8_t peg[HN_MAX] = {0};
    hn_set(g, discs, peg, -1);
}

void hn_random(hn_game *g, int discs, uint32_t seed)
{
    uint32_t s = pr_seed(seed);
    int n = discs < 3 ? 3 : discs > HN_MAX ? HN_MAX : discs;
    int8_t peg[HN_MAX];
    do {
        for (int d = 0; d < n; d++) peg[d] = (int8_t)pr_below(&s, 3);
        hn_set(g, n, peg, pr_below(&s, 3));
    } while (g->par < 1 << (n - 1));            /* the largest disc must move */
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

int hn_solved(const hn_game *g)
{
    if (g->goal >= 0) return g->h[g->goal] == g->n;
    return g->h[1] == g->n || g->h[2] == g->n;
}

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
            int k = 0, all = s[1] < '0' || s[1] > '9';
            for (s++; *s >= '0' && *s <= '9'; s++) k = k * 10 + *s - '0';
            if (all) k = hn_left(g);
            for (int i = 0; i < k; i++) {
                int f, t;
                if (!hn_hint(g, &f, &t) || !hn_move(g, f, t)) return 0;
            }
            continue;
        }
        if (s[0] < 'a' || s[0] > 'c' || s[1] < 'a' || s[1] > 'c') return 0;
        if (!hn_move(g, s[0] - 'a', s[1] - 'a')) return 0;
        s += 2;
    }
    return 1;
}

int hn_parse(hn_game *g, const char *level)
{
    if (level[0] < 'a' || level[0] > 'c' || level[1] != ':') return 0;
    const char *p = level + 2;
    int n = (int)strlen(p);
    if (n < 3 || n > HN_MAX) return 0;
    int8_t peg[HN_MAX];
    for (int i = 0; i < n; i++) {
        if (p[i] < 'a' || p[i] > 'c') return 0;
        peg[n - 1 - i] = (int8_t)(p[i] - 'a');  /* from the largest */
    }
    hn_set(g, n, peg, level[0] - 'a');
    return 1;
}

int hn_chapter_of(int level)
{
    int c = 0;
    for (int first = 0; c < HN_CHAPTERS - 1 && level >= first + CHAPTER_LEVELS[c]; c++) first += CHAPTER_LEVELS[c];
    return c;
}

int hn_chapter_start(int chapter)
{
    int first = 0;
    for (int c = 0; c < chapter && c < HN_CHAPTERS; c++) first += CHAPTER_LEVELS[c];
    return first;
}

#ifndef HN_GEN                                  /* not in the levels' tool (tools/hanoi_gen.c): it makes them */
#include "games/hanoi_levels.h"

int hn_level(hn_game *g, int level)
{
    return level >= 0 && level < HN_LEVELS && hn_parse(g, hn_levels[level]);
}
#endif
