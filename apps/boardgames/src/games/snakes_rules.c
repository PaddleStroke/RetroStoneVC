/*
 * Snakes and ladders: the rules (snakes.h). No SDL: tested by tests/snakes_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/snakes.h"

/* the board: long ladders low and in the middle, a snake by the top, none chained to another */
const sn_link sn_links[SN_LINKS] = {
    {3, 22}, {8, 30}, {28, 76}, {38, 57}, {42, 63}, {50, 69}, {71, 92}, {80, 99},
    {97, 78}, {93, 73}, {86, 24}, {74, 53}, {60, 40}, {49, 11}, {35, 15}, {17, 7},
};

void sn_cell(int square, int *col, int *row)
{
    int i = square - 1;
    *row = i / SN_SIDE;
    *col = *row & 1 ? SN_SIDE - 1 - i % SN_SIDE : i % SN_SIDE;   /* odd rows run right to left */
}

int sn_hop(int from, int k)
{
    int s = from + k;
    return s > SN_LAST ? 2 * SN_LAST - s : s;
}

int sn_link_at(int square)
{
    for (int i = 0; i < SN_LINKS; i++)
        if (sn_links[i].from == square) return i;
    return -1;
}

void sn_new_players(sn_game *g, int players)
{
    memset(g, 0, sizeof *g);
    g->players = players < 2 ? 2 : players > SN_PLAYERS ? SN_PLAYERS : players;
    g->winner = -1;
}

void sn_new(sn_game *g) { sn_new_players(g, 2); }

int sn_turn(const sn_game *g) { return g->turn; }

int sn_status(const sn_game *g) { return g->winner < 0 ? SN_PLAY : SN_WIN0 + g->winner; }

/* one throw on the position (the history is the caller's) */
static void apply(sn_game *g, int roll)
{
    sn_move m = {.player = g->turn, .from = g->pos[g->turn], .roll = roll, .again = roll == 6};
    m.land = sn_hop(m.from, roll);
    int l = sn_link_at(m.land);
    m.to = l < 0 ? m.land : sn_links[l].to;
    g->pos[m.player] = m.to;
    if (m.to == SN_LAST) g->winner = m.player;
    else if (!m.again) g->turn = (m.player + 1) % g->players;   /* round the table */
    g->last = m;
}

int sn_play(sn_game *g, int roll)
{
    if (roll < 1 || roll > 6 || g->winner >= 0 || g->n >= SN_MAX_THROWS) return 0;
    if (g->n >= g->known || g->roll[g->n] != roll) g->known = g->n + 1;   /* a new throw: what was undone is gone */
    g->roll[g->n++] = (uint8_t)roll;
    apply(g, roll);
    return 1;
}

int sn_undo(sn_game *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    g->n = 0;
    memset(g->pos, 0, sizeof g->pos);
    g->turn = 0;
    g->winner = -1;
    memset(&g->last, 0, sizeof g->last);
    while (g->n < n) apply(g, g->roll[g->n++]);
    return 1;
}

int sn_next(const sn_game *g) { return g->n < g->known ? g->roll[g->n] : 0; }
