/*
 * Ludo for two players on one device: the rules and the notation (ludo.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/ludo.h"

void ludo_new(lgame *g)
{
    memset(g, 0, sizeof *g);
    memset(g->pos.at, -1, sizeof g->pos.at);
    g->pos.winner = -1;
}

int ludo_square(int player, int at)
{
    if (at < 0 || at >= LUDO_COLUMN) return -1;
    return (at + player * LUDO_TRACK / 2) % LUDO_TRACK;
}

int ludo_safe(int square) { return square >= 0 && square % (LUDO_TRACK / 4) % LUDO_STAR == 0; }

int ludo_dest(const lpos *p, int token)
{
    if (p->phase != LUDO_MOVE || token < 0 || token >= LUDO_TOKENS) return -1;
    int at = p->at[p->turn][token];
    if (at < 0) return p->die == 6 ? 0 : -1;
    return at + p->die <= LUDO_HOME && at < LUDO_HOME ? at + p->die : -1;
}

int ludo_movable(const lpos *p)
{
    int m = 0;
    for (int t = 0; t < LUDO_TOKENS; t++)
        if (ludo_dest(p, t) >= 0) m |= 1 << t;
    return m;
}

int ludo_home(const lpos *p, int player)
{
    int n = 0;
    for (int t = 0; t < LUDO_TOKENS; t++) n += p->at[player][t] == LUDO_HOME;
    return n;
}

static void next_player(lpos *p)
{
    p->turn ^= 1;
    p->sixes = 0;
    p->phase = LUDO_THROW;
}

int ludo_throw(lgame *g, int value)
{
    lpos *p = &g->pos;
    if (p->phase != LUDO_THROW || value < 1 || value > 6 || g->n >= LUDO_MAX_EVENTS) return 0;
    g->past[g->n] = *p;
    g->ev[g->n++] = (uint8_t)value;
    p->die = (uint8_t)value;
    p->throws++;
    if (value == 6 && ++p->sixes == 3) {        /* the third 6 in a row: lost, the turn ends */
        next_player(p);
        return LT_SIXES;
    }
    p->phase = LUDO_MOVE;
    if (ludo_movable(p)) return LT_MOVE;
    if (value == 6) {                           /* nothing to move, but a 6 still throws again */
        p->phase = LUDO_THROW;
        return LT_AGAIN;
    }
    next_player(p);
    return LT_PASS;
}

int ludo_move(lgame *g, int token)
{
    lpos *p = &g->pos;
    int to = ludo_dest(p, token), me = p->turn, caught = 0;
    if (to < 0 || g->n >= LUDO_MAX_EVENTS) return 0;
    g->past[g->n] = *p;
    g->ev[g->n++] = (uint8_t)(LUDO_MOVE_EV + token);
    p->at[me][token] = (int8_t)to;
    int sq = ludo_square(me, to);
    if (sq >= 0 && !ludo_safe(sq))              /* the other player's tokens there go back to their yard */
        for (int t = 0; t < LUDO_TOKENS; t++)
            if (ludo_square(!me, p->at[!me][t]) == sq) {
                p->at[!me][t] = -1;
                caught++;
            }
    if (ludo_home(p, me) == LUDO_TOKENS) {
        p->winner = (int8_t)me;
        p->phase = LUDO_OVER;
    } else if (p->die == 6) {
        p->phase = LUDO_THROW;
    } else {
        next_player(p);
    }
    return 1 + caught;
}

int ludo_undo(lgame *g)
{
    if (!g->n) return 0;
    while (g->n) {                              /* the throws after the last move, then the move itself */
        g->pos = g->past[--g->n];
        if (g->ev[g->n] >= LUDO_MOVE_EV) break;
    }
    return 1;
}

int ludo_script(lgame *g, const char *s)
{
    for (; *s; s++) {
        if (*s == ' ') continue;
        if (*s >= '1' && *s <= '6') {
            if (!ludo_throw(g, *s - '0')) return 0;
        } else if (*s >= 'a' && *s < 'a' + LUDO_TOKENS) {
            if (!ludo_move(g, *s - 'a')) return 0;
        } else {
            return 0;
        }
    }
    return 1;
}
