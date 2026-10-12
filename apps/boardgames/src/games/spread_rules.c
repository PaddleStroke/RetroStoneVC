/*
 * Spread's rules (spread.h): clones and jumps, the pieces turned, the end and the count. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/spread.h"

/* the blocked cells of each board (x, y; -1 ends), symmetric both ways so neither player is favoured */
static const int8_t BLOCKS[SPD_BOARDS][9][2] = {
    {{-1, 0}},
    {{2, 2}, {4, 2}, {2, 4}, {4, 4}, {-1, 0}},
    {{3, 2}, {2, 3}, {3, 3}, {4, 3}, {3, 4}, {-1, 0}},
    {{3, 0}, {0, 3}, {6, 3}, {3, 6}, {3, 2}, {3, 4}, {-1, 0}},
};

int spd_dist(int a, int b)
{
    int dx = abs(a % SPD_N - b % SPD_N), dy = abs(a / SPD_N - b / SPD_N);
    return dx > dy ? dx : dy;
}

static int moves_of(const spdpos *p, int player, spdmove *out)
{
    int n = 0;
    for (int s = 0; s < SPD_CELLS; s++) {
        if (p->c[s] != SPD_P0 + player) continue;
        int x = s % SPD_N, y = s / SPD_N;
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++) {
                int tx = x + dx, ty = y + dy;
                if ((!dx && !dy) || tx < 0 || ty < 0 || tx >= SPD_N || ty >= SPD_N) continue;
                if (p->c[SPD_AT(tx, ty)] != SPD_EMPTY) continue;
                if (out) out[n] = (spdmove){(uint8_t)s, (uint8_t)SPD_AT(tx, ty)};
                n++;
            }
    }
    return n;
}

int spd_legal(const spdpos *p, spdmove *out) { return moves_of(p, p->side, out); }
int spd_can_move(const spdpos *p, int player) { return moves_of(p, player, NULL) > 0; }

uint64_t spd_turns(const spdpos *p, spdmove m)
{
    uint64_t t = 0;
    int x = m.to % SPD_N, y = m.to / SPD_N, enemy = SPD_P0 + !p->side;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int tx = x + dx, ty = y + dy;
            if (tx < 0 || ty < 0 || tx >= SPD_N || ty >= SPD_N) continue;
            if (p->c[SPD_AT(tx, ty)] == enemy) t |= 1ull << SPD_AT(tx, ty);
        }
    return t;
}

void spd_make(spdpos *p, spdmove m)
{
    uint64_t t = spd_turns(p, m);
    uint8_t mine = (uint8_t)(SPD_P0 + p->side);
    if (spd_dist(m.from, m.to) == 2) {
        p->c[m.from] = SPD_EMPTY;
        p->idle++;
    } else p->idle = 0;
    p->c[m.to] = mine;
    for (int k = 0; k < SPD_CELLS; k++)
        if (t >> k & 1) p->c[k] = mine;
    p->side ^= 1;
}

int spd_count(const spdpos *p, int player)
{
    int n = 0;
    for (int s = 0; s < SPD_CELLS; s++) n += p->c[s] == SPD_P0 + player;
    return n;
}

int spd_cell(const char *s)
{
    int x = s[0] - 'a', y = s[1] - '1';
    return x >= 0 && y >= 0 && x < SPD_N && y < SPD_N ? SPD_AT(x, y) : -1;
}

void spd_new(spdgame *g, int board)
{
    spdpos *p = &g->pos[0];
    memset(p, 0, sizeof *p);
    g->ply = 0;
    g->board = board < 0 || board >= SPD_BOARDS ? 0 : board;
    for (int i = 0; BLOCKS[g->board][i][0] >= 0; i++) p->c[SPD_AT(BLOCKS[g->board][i][0], BLOCKS[g->board][i][1])] =
        SPD_BLOCK;
    p->c[SPD_AT(0, 0)] = p->c[SPD_AT(6, 6)] = SPD_P0;
    p->c[SPD_AT(0, 6)] = p->c[SPD_AT(6, 0)] = SPD_P1;
}

static int full(const spdpos *p)
{
    for (int s = 0; s < SPD_CELLS; s++)
        if (p->c[s] == SPD_EMPTY) return 0;
    return 1;
}

int spd_filler(const spdgame *g)
{
    const spdpos *p = spd_pos(g);
    if (full(p) || p->idle >= SPD_IDLE || spd_can_move(p, p->side)) return -1;
    return !p->side;
}

int spd_score(const spdgame *g, int player)
{
    const spdpos *p = spd_pos(g);
    int n = spd_count(p, player);
    if (spd_filler(g) == player)
        for (int s = 0; s < SPD_CELLS; s++) n += p->c[s] == SPD_EMPTY;
    return n;
}

int spd_status(const spdgame *g)
{
    const spdpos *p = spd_pos(g);
    if (!full(p) && p->idle < SPD_IDLE && spd_can_move(p, p->side)) return SPD_PLAY;
    int a = spd_score(g, 0), b = spd_score(g, 1);
    return a > b ? SPD_WIN0 : b > a ? SPD_WIN1 : SPD_DRAW;
}

int spd_play(spdgame *g, spdmove m)
{
    const spdpos *p = spd_pos(g);
    if (g->ply >= SPD_MAX_PLY || spd_status(g) != SPD_PLAY) return 0;
    if (m.from >= SPD_CELLS || m.to >= SPD_CELLS || p->c[m.from] != SPD_P0 + p->side || p->c[m.to] != SPD_EMPTY)
        return 0;
    int d = spd_dist(m.from, m.to);
    if (d < 1 || d > 2) return 0;
    g->pos[g->ply + 1] = *p;
    spd_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int spd_undo(spdgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

int spd_script(spdgame *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        int a = spd_cell(s), b = s[1] ? spd_cell(s + 2) : -1;
        if (a < 0 || b < 0 || !spd_play(g, (spdmove){(uint8_t)a, (uint8_t)b})) return 0;
        s += 4;
    }
    return 1;
}
