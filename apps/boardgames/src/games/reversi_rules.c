/*
 * Reversi's rules: the moves, the turned discs, the passes and the end (no SDL; tests/reversi_test.c).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include "games/reversi.h"

static const int DIR[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

uint64_t rv_flips(const rpos *p, int side, int s)
{
    uint64_t all = p->disc[0] | p->disc[1], mine = p->disc[side], theirs = p->disc[!side], out = 0;
    if (s < 0 || s >= 64 || (all >> s & 1)) return 0;
    for (int d = 0; d < 8; d++) {               /* a line of theirs, then one of mine: the line turns */
        uint64_t line = 0;
        int f = s % 8 + DIR[d][0], r = s / 8 + DIR[d][1];
        for (; f >= 0 && f < 8 && r >= 0 && r < 8 && (theirs >> RV_SQ(f, r) & 1); f += DIR[d][0], r += DIR[d][1])
            line |= 1ull << RV_SQ(f, r);
        if (line && f >= 0 && f < 8 && r >= 0 && r < 8 && (mine >> RV_SQ(f, r) & 1)) out |= line;
    }
    return out;
}

uint64_t rv_legal(const rpos *p, int side)
{
    uint64_t m = 0;
    for (int s = 0; s < 64; s++)
        if (rv_flips(p, side, s)) m |= 1ull << s;
    return m;
}

void rv_make(rpos *p, int s)
{
    uint64_t f = rv_flips(p, p->side, s);
    p->disc[p->side] |= f | 1ull << s;
    p->disc[!p->side] &= ~f;
    p->passed = 0;
    if (rv_legal(p, !p->side)) p->side = !p->side;
    else if (rv_legal(p, p->side)) p->passed = 1;     /* the other player cannot move: the same player again */
    else p->over = 1;
}

int rv_count(const rpos *p, int player)
{
    int n = 0;
    for (uint64_t b = p->disc[player]; b; b &= b - 1) n++;
    return n;
}

void rv_new(rgame *g)
{
    rpos *p = &g->pos[0];
    *p = (rpos){{0, 0}, 0, 0, 0};
    p->disc[0] = 1ull << RV_SQ(4, 3) | 1ull << RV_SQ(3, 4);   /* e4 d5 ebony */
    p->disc[1] = 1ull << RV_SQ(3, 3) | 1ull << RV_SQ(4, 4);   /* d4 e5 ivory */
    g->ply = 0;
}

int rv_play(rgame *g, int s)
{
    const rpos *p = rv_pos(g);
    if (p->over || g->ply >= RV_MAX || !rv_flips(p, p->side, s)) return 0;
    g->pos[g->ply + 1] = *p;
    rv_make(&g->pos[g->ply + 1], s);
    g->move[g->ply++] = s;
    return 1;
}

int rv_undo(rgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

int rv_status(const rgame *g)
{
    const rpos *p = rv_pos(g);
    if (!p->over) return RV_PLAY;
    int a = rv_count(p, 0), b = rv_count(p, 1);
    return a > b ? RV_WIN0 : b > a ? RV_WIN1 : RV_DRAW;
}
