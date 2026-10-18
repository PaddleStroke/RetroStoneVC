/*
 * Twist Five's rules (twistfive.h): the marble, the quarter turn, rows of five, the end. No SDL: the tests and the bot
 * link it alone.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/twistfive.h"

#define H(x, y) ((x) + TV_N * (y))
const uint8_t tv_line[TV_LINES][5] = {
    /* across: each rank, from a or b */
    {H(0, 0), H(1, 0), H(2, 0), H(3, 0), H(4, 0)}, {H(1, 0), H(2, 0), H(3, 0), H(4, 0), H(5, 0)},
    {H(0, 1), H(1, 1), H(2, 1), H(3, 1), H(4, 1)}, {H(1, 1), H(2, 1), H(3, 1), H(4, 1), H(5, 1)},
    {H(0, 2), H(1, 2), H(2, 2), H(3, 2), H(4, 2)}, {H(1, 2), H(2, 2), H(3, 2), H(4, 2), H(5, 2)},
    {H(0, 3), H(1, 3), H(2, 3), H(3, 3), H(4, 3)}, {H(1, 3), H(2, 3), H(3, 3), H(4, 3), H(5, 3)},
    {H(0, 4), H(1, 4), H(2, 4), H(3, 4), H(4, 4)}, {H(1, 4), H(2, 4), H(3, 4), H(4, 4), H(5, 4)},
    {H(0, 5), H(1, 5), H(2, 5), H(3, 5), H(4, 5)}, {H(1, 5), H(2, 5), H(3, 5), H(4, 5), H(5, 5)},
    /* up and down: each file, from rank 1 or 2 */
    {H(0, 0), H(0, 1), H(0, 2), H(0, 3), H(0, 4)}, {H(0, 1), H(0, 2), H(0, 3), H(0, 4), H(0, 5)},
    {H(1, 0), H(1, 1), H(1, 2), H(1, 3), H(1, 4)}, {H(1, 1), H(1, 2), H(1, 3), H(1, 4), H(1, 5)},
    {H(2, 0), H(2, 1), H(2, 2), H(2, 3), H(2, 4)}, {H(2, 1), H(2, 2), H(2, 3), H(2, 4), H(2, 5)},
    {H(3, 0), H(3, 1), H(3, 2), H(3, 3), H(3, 4)}, {H(3, 1), H(3, 2), H(3, 3), H(3, 4), H(3, 5)},
    {H(4, 0), H(4, 1), H(4, 2), H(4, 3), H(4, 4)}, {H(4, 1), H(4, 2), H(4, 3), H(4, 4), H(4, 5)},
    {H(5, 0), H(5, 1), H(5, 2), H(5, 3), H(5, 4)}, {H(5, 1), H(5, 2), H(5, 3), H(5, 4), H(5, 5)},
    /* the diagonals up to the right, then down to the right */
    {H(0, 0), H(1, 1), H(2, 2), H(3, 3), H(4, 4)}, {H(1, 1), H(2, 2), H(3, 3), H(4, 4), H(5, 5)},
    {H(1, 0), H(2, 1), H(3, 2), H(4, 3), H(5, 4)}, {H(0, 1), H(1, 2), H(2, 3), H(3, 4), H(4, 5)},
    {H(0, 5), H(1, 4), H(2, 3), H(3, 2), H(4, 1)}, {H(1, 4), H(2, 3), H(3, 2), H(4, 1), H(5, 0)},
    {H(0, 4), H(1, 3), H(2, 2), H(3, 1), H(4, 0)}, {H(1, 5), H(2, 4), H(3, 3), H(4, 2), H(5, 1)},
};

int tv_square(const char *s)
{
    if (s[0] < 'a' || s[0] > 'f' || s[1] < '1' || s[1] > '6') return -1;
    return (s[0] - 'a') + (s[1] - '1') * TV_N;
}

int tv_quarter_of(int hole)
{
    return (hole % TV_N >= 3) + 2 * (hole / TV_N < 3);
}

int tv_turned(int hole, int q, int dir)
{
    if (tv_quarter_of(hole) != q) return hole;
    int ox = (q & 1) * 3, oy = q & 2 ? 0 : 3, i = hole % TV_N - ox, j = hole / TV_N - oy, ni, nj;
    if (dir == TV_CW) ni = j, nj = 2 - i;       /* (y up) the top left corner goes to the top right */
    else ni = 2 - j, nj = i;
    return H(ox + ni, oy + nj);
}

void tv_rotate(uint8_t *cell, int q, int dir)
{
    uint8_t old[TV_CELLS];
    memcpy(old, cell, sizeof old);
    int ox = (q & 1) * 3, oy = q & 2 ? 0 : 3;
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++) {
            int h = H(ox + i, oy + j);
            cell[tv_turned(h, q, dir)] = old[h];
        }
}

int tv_five(const uint8_t *cell, int player)
{
    for (int l = 0; l < TV_LINES; l++) {
        int k = 0;
        while (k < 5 && cell[tv_line[l][k]] == player + 1) k++;
        if (k == 5) return 1;
    }
    return 0;
}

int tv_marks(const uint8_t *cell, uint8_t *mark)
{
    int n = 0;
    memset(mark, 0, TV_CELLS);
    for (int l = 0; l < TV_LINES; l++) {
        int v = cell[tv_line[l][0]], k = 1;
        while (v && k < 5 && cell[tv_line[l][k]] == v) k++;
        if (v && k == 5)
            for (k = 0; k < 5; k++) n += !mark[tv_line[l][k]], mark[tv_line[l][k]] = 1;
    }
    return n;
}

void tv_start(tvpos *p)
{
    memset(p, 0, sizeof *p);
    p->last = -1;
    p->winner = -1;
}

void tv_place(tvpos *p, int hole)
{
    p->cell[hole] = (uint8_t)(p->side + 1);
    p->count++;
    p->last = (int8_t)hole;
    if (tv_five(p->cell, p->side)) { p->winner = (int8_t)p->side; return; }   /* five at once: no twist */
    p->stage = TV_TWIST;
}

void tv_twist(tvpos *p, int q, int dir)
{
    tv_rotate(p->cell, q, dir);
    int me = tv_five(p->cell, p->side), other = tv_five(p->cell, !p->side);
    p->winner = (int8_t)(me && other ? TV_DRAW : me ? p->side : other ? !p->side : p->count == TV_CELLS ? TV_DRAW : -1);
    p->stage = TV_PLACE;
    p->last = -1;
    p->side ^= 1;
}

void tv_new(tvgame *g)
{
    g->ply = 0;
    tv_start(&g->pos[0]);
}

int tv_play_place(tvgame *g, int hole)
{
    const tvpos *p = tv_pos(g);
    if (g->ply >= TV_MAX_PLY || p->winner >= 0 || p->stage != TV_PLACE || hole < 0 || hole >= TV_CELLS ||
        p->cell[hole])
        return 0;
    g->pos[g->ply + 1] = *p;
    tv_place(&g->pos[g->ply + 1], hole);
    g->step[g->ply++] = (int8_t)hole;
    return 1;
}

int tv_play_twist(tvgame *g, int q, int dir)
{
    const tvpos *p = tv_pos(g);
    if (g->ply >= TV_MAX_PLY || p->winner >= 0 || p->stage != TV_TWIST || q < 0 || q > 3 || (dir != TV_CW && dir != TV_CCW))
        return 0;
    g->pos[g->ply + 1] = *p;
    tv_twist(&g->pos[g->ply + 1], q, dir);
    g->step[g->ply++] = (int8_t)(64 + q * 2 + dir);
    return 1;
}

int tv_undo(tvgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

int tv_script(tvgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s != '/') {                        /* "c4": the marble */
            int h = tv_square(s);
            if (h < 0 || !tv_play_place(g, h)) return 0;
            s += 2;
        }
        if (*s == '/') {                        /* "/2+": the twist */
            if (s[1] < '1' || s[1] > '4' || (s[2] != '+' && s[2] != '-')) return 0;
            if (!tv_play_twist(g, s[1] - '1', s[2] == '+' ? TV_CW : TV_CCW)) return 0;
            s += 3;
        }
        if (*s && *s != ' ') return 0;
    }
    return 1;
}
