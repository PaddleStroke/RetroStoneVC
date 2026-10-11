/*
 * Breakthrough rules (breakthrough.h): forward steps, diagonal captures, the race to the far row.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "breakthrough.h"

int bt_square(const char *s)
{
    if (s[0] < 'a' || s[0] > 'h' || s[1] < '1' || s[1] > '8') return -1;
    return (s[1] - '1') * 8 + (s[0] - 'a');
}

int bt_forward(int player) { return player ? -8 : 8; }

void bt_start(btpos *p)
{
    memset(p->at, -1, sizeof p->at);
    for (int s = 0; s < 16; s++) {
        p->at[s] = 0;
        p->at[63 - s] = 1;
    }
    p->side = 0;
    p->left[0] = p->left[1] = BT_PIECES;
}

int bt_winner(const btpos *p)
{
    for (int f = 0; f < 8; f++) {
        if (p->at[56 + f] == 0) return 0;      /* ivory on rank 8 */
        if (p->at[f] == 1) return 1;           /* ebony on rank 1 */
    }
    if (!p->left[1]) return 0;
    if (!p->left[0]) return 1;
    return -1;
}

int bt_legal(const btpos *p, btmove *out)
{
    if (bt_winner(p) >= 0) return 0;
    int me = p->side, fwd = bt_forward(me), n = 0;
    for (int s = 0; s < BT_SQUARES; s++) {
        if (p->at[s] != me) continue;
        int to = s + fwd, f = s & 7;            /* never off the board: a piece on the far row has already won */
        for (int df = -1; df <= 1; df++) {
            if (f + df < 0 || f + df > 7) continue;
            int t = to + df, there = p->at[t];
            if (there == me || (df == 0 && there >= 0)) continue;   /* straight on: only to an empty square */
            out[n++] = (btmove){(int8_t)s, (int8_t)t, (int8_t)(there >= 0)};
        }
    }
    return n;
}

void bt_make(btpos *p, btmove m)
{
    if (m.capture) p->left[!p->side]--;
    p->at[m.to] = (int8_t)p->side;
    p->at[m.from] = -1;
    p->side = !p->side;
}

void bt_new(btgame *g)
{
    g->ply = 0;
    bt_start(&g->pos[0]);
}

int bt_play(btgame *g, btmove m)
{
    btmove legal[BT_MAX_MOVES];
    int n = bt_legal(bt_pos(g), legal), k = 0;
    while (k < n && !(legal[k].from == m.from && legal[k].to == m.to)) k++;
    if (k == n || g->ply >= BT_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    bt_make(&g->pos[g->ply + 1], legal[k]);
    g->move[g->ply++] = legal[k];
    return 1;
}

int bt_undo(btgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

int bt_script(btgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int from = bt_square(s), sep = s[2] == '-' || s[2] == 'x';
        if (from < 0 || !s[2]) return 0;
        int to = bt_square(s + 2 + sep);
        if (to < 0) return 0;
        s += 4 + sep;
        if ((*s && *s != ' ') || !bt_play(g, (btmove){(int8_t)from, (int8_t)to, 0})) return 0;
    }
    return 1;
}
