/*
 * The Game of the Goose: the rules (goose.h). No SDL: tested by tests/goose_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/goose.h"

int goo_kind(int s)
{
    switch (s) {
    case 6: return GK_BRIDGE;
    case 19: return GK_INN;
    case 31: return GK_WELL;
    case 42: return GK_MAZE;
    case 52: return GK_PRISON;
    case 58: return GK_DEATH;
    case 26: case 53: return GK_DICE;
    case GOO_LAST: return GK_GARDEN;
    }
    return s > 0 && s < GOO_LAST && (s % 9 == 0 || s % 9 == 5) ? GK_GOOSE : GK_PLAIN;
}

void goo_new(goo_game *g, int players)
{
    memset(g, 0, sizeof *g);
    g->players = players < 2 ? 2 : players > GOO_PLAYERS ? GOO_PLAYERS : players;
    g->winner = -1;
}

int goo_turn(const goo_game *g) { return g->turn; }
int goo_status(const goo_game *g) { return g->winner < 0 ? GOO_PLAY : GOO_WIN0 + g->winner; }

static void step(goo_move *m, int sq, int how)
{
    if (m->n < GOO_STEPS) m->step[m->n++] = (goo_step){(uint8_t)sq, (uint8_t)how};
}

/* the next to throw after `p`: the players at the inn miss their turns (one less to wait each), those in the well or
 * the prison are passed over; two held and nobody free to come: the next one is freed */
static int next_player(goo_game *g, int p)
{
    int c = p;
    for (int guard = 0; guard < 64 * GOO_PLAYERS; guard++) {
        c = (c + 1) % g->players;
        if (g->wait[c] > 0) { g->wait[c]--; continue; }
        if (g->wait[c] == GOO_HELD) {
            int free_one = 0;
            for (int q = 0; q < g->players; q++) free_one |= q != c && g->wait[q] != GOO_HELD;
            if (free_one) continue;
            g->wait[c] = 0;                     /* nobody can come: freed */
            g->last.freed = c;
        }
        return c;
    }
    return c;
}

/* one throw on the position (the history is the caller's) */
static void apply(goo_game *g, int d1, int d2)
{
    int p = g->turn, from = g->pos[p], n = d1 + d2;
    goo_move m = {.player = p, .from = from, .d1 = d1, .d2 = d2, .swap = -1, .freed = -1};
    int s = from;
    if (from == 0 && n == 9) {                  /* the opening throws */
        s = d1 == 6 || d2 == 6 ? 26 : 53;
        step(&m, s, GOO_FLY);
    } else {
        int dir = 1;
        for (int chain = 0; chain < 32; chain++) {
            for (int k = 0; k < n; k++) {       /* square by square, bouncing off 63 */
                if (s == GOO_LAST) dir = -1;
                if (s + dir < 0) break;
                s += dir;
                step(&m, s, GOO_HOP);
            }
            int kind = goo_kind(s);
            if (kind == GK_GOOSE) continue;     /* again, the same way */
            if (kind == GK_BRIDGE) s = 12, step(&m, s, GOO_GLIDE);
            else if (kind == GK_MAZE) s = 30, step(&m, s, GOO_GLIDE);
            else if (kind == GK_DEATH) s = 0, step(&m, s, GOO_FLY);
            break;
        }
    }
    m.to = s;
    if (s != 0 && s != from)                    /* a taken square: that pawn goes where the mover came from */
        for (int q = 0; q < g->players; q++)
            if (q != p && g->pos[q] == s) {
                g->pos[q] = from;
                g->wait[q] = 0;
                m.swap = q;
            }
    g->pos[p] = s;
    int kind = goo_kind(s);
    g->wait[p] = kind == GK_INN ? 2 : kind == GK_WELL || kind == GK_PRISON ? GOO_HELD : 0;
    g->last = m;
    if (s == GOO_LAST) g->winner = p;
    else g->turn = next_player(g, p);
}

static int valid(int d) { return d >= 1 && d <= 6; }

/* back to the position before the first throw kept (the start, or the base of a long game), no throw played */
static void restart(goo_game *g)
{
    g->n = 0;
    memcpy(g->pos, g->base_pos, sizeof g->pos);
    memcpy(g->wait, g->base_wait, sizeof g->wait);
    g->turn = g->base_turn;
    g->winner = -1;
    memset(&g->last, 0, sizeof g->last);
}

/* the throws full: the oldest quarter goes into the base position (undo goes back that far at most), so a game
 * never stops for its length */
static void fold(goo_game *g)
{
    int drop = GOO_MAX_THROWS / 4, n = g->n, pos[GOO_PLAYERS], wait[GOO_PLAYERS], turn = g->turn;
    goo_move last = g->last;
    memcpy(pos, g->pos, sizeof pos);
    memcpy(wait, g->wait, sizeof wait);
    restart(g);
    while (g->n < drop) apply(g, g->dice[g->n][0], g->dice[g->n][1]), g->n++;
    memcpy(g->base_pos, g->pos, sizeof g->base_pos);
    memcpy(g->base_wait, g->wait, sizeof g->base_wait);
    g->base_turn = g->turn;
    memmove(g->dice, g->dice + drop, (size_t)(g->known - drop) * sizeof g->dice[0]);
    memmove(g->who, g->who + drop, (size_t)(g->known - drop) * sizeof g->who[0]);
    g->n = n - drop;
    g->known -= drop;
    memcpy(g->pos, pos, sizeof pos);
    memcpy(g->wait, wait, sizeof wait);
    g->turn = turn;
    g->last = last;
}

int goo_play(goo_game *g, int d1, int d2)
{
    if (!valid(d1) || !valid(d2) || g->winner >= 0) return 0;
    if (g->n >= GOO_MAX_THROWS) fold(g);
    if (g->n >= g->known || g->dice[g->n][0] != d1 || g->dice[g->n][1] != d2) g->known = g->n + 1;
    g->dice[g->n][0] = (uint8_t)d1;
    g->dice[g->n][1] = (uint8_t)d2;
    g->who[g->n++] = (uint8_t)g->turn;
    apply(g, d1, d2);
    return 1;
}

int goo_undo(goo_game *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    restart(g);
    for (int i = 0; i < n; i++) {
        g->who[i] = (uint8_t)g->turn;
        apply(g, g->dice[i][0], g->dice[i][1]);
        g->n = i + 1;
    }
    return 1;
}

int goo_next(const goo_game *g, int *d1, int *d2)
{
    if (g->n >= g->known) return 0;
    *d1 = g->dice[g->n][0];
    *d2 = g->dice[g->n][1];
    return 1;
}

int goo_last_throw(const goo_game *g, int player, int *d1, int *d2)
{
    for (int i = g->n - 1; i >= 0; i--)
        if (g->who[i] == player) {
            *d1 = g->dice[i][0];
            *d2 = g->dice[i][1];
            return 1;
        }
    return 0;
}

int goo_script(goo_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (s[0] < '1' || s[0] > '6' || s[1] < '1' || s[1] > '6') return 0;
        if (!goo_play(g, s[0] - '0', s[1] - '0')) return 0;
        s += 2;
    }
    return 1;
}
