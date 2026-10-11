/*
 * The Game of the Amazons: the rules (amazons.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/amazons.h"

static const int DX[8] = {0, 1, 1, 1, 0, -1, -1, -1}, DY[8] = {1, 1, 0, -1, -1, -1, 0, 1};

int am_square(const char *s)
{
    if (!s || s[0] < 'a' || s[0] >= 'a' + AM_N || s[1] < '1' || s[1] > '9') return -1;
    int r = s[1] - '0';
    if (s[2] >= '0' && s[2] <= '9') r = r * 10 + s[2] - '0';
    if (r < 1 || r > AM_N) return -1;
    return (s[0] - 'a') + (r - 1) * AM_N;
}

void am_start(ampos *p)
{
    static const char *home[2][4] = {{"a4", "d1", "g1", "j4"}, {"a7", "d10", "g10", "j7"}};
    memset(p, 0, sizeof *p);
    for (int c = 0; c < 2; c++)
        for (int i = 0; i < 4; i++) p->sq[am_square(home[c][i])] = (uint8_t)(AM_AMAZON + c);
    p->at = -1;
}

int am_line(const ampos *p, int from, uint8_t *out)
{
    int n = 0, x0 = from % AM_N, y0 = from / AM_N;
    for (int d = 0; d < 8; d++)
        for (int x = x0 + DX[d], y = y0 + DY[d]; x >= 0 && x < AM_N && y >= 0 && y < AM_N; x += DX[d], y += DY[d]) {
            if (p->sq[x + y * AM_N] != AM_EMPTY) break;
            out[n++] = (uint8_t)(x + y * AM_N);
        }
    return n;
}

int am_full_moves(const ampos *p)
{
    if (p->stage != AM_MOVE) return 0;
    ampos q = *p;
    uint8_t to[AM_MAX_LINE], arrow[AM_MAX_LINE];
    int total = 0;
    for (int s = 0; s < AM_SQUARES; s++) {
        if (p->sq[s] != AM_AMAZON + p->side) continue;
        int n = am_line(p, s, to);
        q.sq[s] = AM_EMPTY;                     /* the amazon is gone from its square while it shoots */
        for (int i = 0; i < n; i++) {
            q.sq[to[i]] = p->sq[s];
            total += am_line(&q, to[i], arrow);
            q.sq[to[i]] = AM_EMPTY;
        }
        q.sq[s] = p->sq[s];
    }
    return total;
}

/* every move has an arrow (back to the square left): a whole turn exists if an amazon can move */
int am_can_move(const ampos *p)
{
    uint8_t to[AM_MAX_LINE];
    for (int s = 0; s < AM_SQUARES; s++)
        if (p->sq[s] == AM_AMAZON + p->side && am_line(p, s, to)) return 1;
    return 0;
}

void am_new(amgame *g)
{
    g->ply = 0;
    am_start(&g->pos[0]);
}

static int reaches(const ampos *p, int from, int to)
{
    uint8_t line[AM_MAX_LINE];
    int n = am_line(p, from, line);
    for (int i = 0; i < n; i++)
        if (line[i] == to) return 1;
    return 0;
}

int am_move(amgame *g, int from, int to)
{
    const ampos *p = am_pos(g);
    if (g->ply >= AM_MAX_PLY || p->stage != AM_MOVE || from < 0 || from >= AM_SQUARES || to < 0 ||
        to >= AM_SQUARES || p->sq[from] != AM_AMAZON + p->side || !reaches(p, from, to))
        return 0;
    ampos *q = &g->pos[g->ply + 1];
    *q = *p;
    q->sq[to] = q->sq[from];
    q->sq[from] = AM_EMPTY;
    q->stage = AM_SHOOT;
    q->at = (int8_t)to;
    g->step[g->ply++] = (amstep){(int8_t)from, (int8_t)to};
    return 1;
}

int am_shoot(amgame *g, int to)
{
    const ampos *p = am_pos(g);
    if (g->ply >= AM_MAX_PLY || p->stage != AM_SHOOT || to < 0 || to >= AM_SQUARES || !reaches(p, p->at, to))
        return 0;
    ampos *q = &g->pos[g->ply + 1];
    *q = *p;
    q->sq[to] = AM_ARROW;
    q->stage = AM_MOVE;
    q->side ^= 1;
    q->at = -1;
    g->step[g->ply++] = (amstep){p->at, (int8_t)to};
    return 1;
}

int am_undo(amgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

int am_winner(const amgame *g)
{
    const ampos *p = am_pos(g);
    if (p->stage != AM_MOVE || am_can_move(p)) return -1;
    return !p->side;
}

int am_script(amgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s != '/') {                        /* "d1-d7": the move */
            int from = am_square(s);
            if (from < 0) return 0;
            s += from >= 9 * AM_N ? 3 : 2;
            if (*s == '-') s++;
            int to = am_square(s);
            if (to < 0 || !am_move(g, from, to)) return 0;
            s += to >= 9 * AM_N ? 3 : 2;
        }
        if (*s == '/') {                        /* "/g7": the arrow */
            int to = am_square(++s);
            if (to < 0 || !am_shoot(g, to)) return 0;
            s += to >= 9 * AM_N ? 3 : 2;
        }
        if (*s && *s != ' ') return 0;
    }
    return 1;
}
