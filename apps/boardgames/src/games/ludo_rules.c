/*
 * Ludo for two to four players on one device, and the petits chevaux: the rules and the notation (ludo.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/ludo.h"

void ludo_new(lgame *g, int variant, int players)
{
    memset(g, 0, sizeof *g);
    memset(g->pos.at, -1, sizeof g->pos.at);
    g->pos.variant = (uint8_t)(variant == LUDO_CHEVAUX ? LUDO_CHEVAUX : LUDO_CLASSIC);
    g->pos.players = (uint8_t)(players >= 3 && players <= LUDO_PLAYERS ? players : 2);
    g->pos.winner = -1;
}

int ludo_goal(int variant) { return variant == LUDO_CHEVAUX ? CHEVAUX_HOME : LUDO_HOME; }

static const int8_t SEAT[LUDO_PLAYERS] = {0, 2, 1, 3};    /* a player's seat; a seat's player, the same */

int ludo_seat(int player) { return SEAT[player & 3]; }

int ludo_square(int player, int at)
{
    if (at < 0 || at >= LUDO_COLUMN) return -1;
    return (at + ludo_seat(player) * LUDO_TRACK / 4) % LUDO_TRACK;
}

int ludo_safe(int square) { return square >= 0 && square % (LUDO_TRACK / 4) % LUDO_STAR == 0; }

/* who stands where a token of player `me` at `at` would: bit 0 its own tokens, bit 1 other players' (the yards and
 * the centre: nobody) */
static int who(const lpos *p, int me, int at)
{
    if (at < 0 || at >= ludo_goal(p->variant)) return 0;
    int sq = ludo_square(me, at), r = 0;
    for (int t = 0; t < LUDO_TOKENS; t++) {
        if (p->at[me][t] == at) r |= 1;
        for (int o = 0; o < p->players && sq >= 0; o++)
            if (o != me && ludo_square(o, p->at[o][t]) == sq) r |= 2;
    }
    return r;
}

int ludo_dest(const lpos *p, int token)
{
    if (p->phase != LUDO_MOVE || token < 0 || token >= LUDO_TOKENS) return -1;
    int at = p->at[p->turn][token], d = p->die, to;
    if (at < 0) to = d == 6 ? 0 : -1;
    else if (p->variant == LUDO_CLASSIC) return at + d <= LUDO_HOME && at < LUDO_HOME ? at + d : -1;
    else if (at < LUDO_FOOT) to = at + d <= LUDO_FOOT ? at + d : -1;     /* the foot of the stairs: exactly */
    else if (at < CHEVAUX_HOME) to = d == (at < LUDO_FOOT + LUDO_STEPS ? at - LUDO_FOOT + 1 : 6) ? at + 1 : -1;
    else to = -1;
    if (to < 0 || p->variant == LUDO_CLASSIC) return to;
    for (int k = at < 0 ? 0 : at + 1; k <= to; k++)    /* no horse passed, none of its own where it stops */
        if (who(p, p->turn, k) & (k < to ? 3 : 1)) return -1;
    return to;
}

int ludo_hits(const lpos *p, int token)
{
    int to = ludo_dest(p, token), me = p->turn, sq = ludo_square(me, to), m = 0;
    if (sq < 0 || (p->variant == LUDO_CLASSIC && ludo_safe(sq))) return 0;
    for (int o = 0; o < p->players; o++)
        for (int t = 0; t < LUDO_TOKENS; t++)
            if (o != me && ludo_square(o, p->at[o][t]) == sq) m |= 1 << (LUDO_TOKENS * o + t);
    return m;
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
    for (int t = 0; t < LUDO_TOKENS; t++) n += p->at[player][t] == ludo_goal(p->variant);
    return n;
}

static void next_player(lpos *p)
{
    for (int s = ludo_seat(p->turn) + 1;; s++)  /* the next seat round the table that has a player */
        if (SEAT[s & 3] < p->players) {
            p->turn = (uint8_t)SEAT[s & 3];
            break;
        }
    p->sixes = 0;
    p->phase = LUDO_THROW;
}

/* a new event: the position before it, kept for undo. The history full, its oldest quarter is forgotten (undo goes
 * back that far at most): a game never stops for its length. */
static void record(lgame *g, int ev)
{
    if (g->n >= LUDO_MAX_EVENTS) {
        int drop = LUDO_MAX_EVENTS / 4;
        memmove(g->past, g->past + drop, (size_t)(LUDO_MAX_EVENTS - drop) * sizeof g->past[0]);
        memmove(g->ev, g->ev + drop, (size_t)(LUDO_MAX_EVENTS - drop) * sizeof g->ev[0]);
        g->n -= drop;
    }
    g->past[g->n] = g->pos;
    g->ev[g->n++] = (uint8_t)ev;
}


int ludo_throw(lgame *g, int value)
{
    lpos *p = &g->pos;
    if (p->phase != LUDO_THROW || value < 1 || value > 6) return 0;
    record(g, (uint8_t)value);
    p->die = (uint8_t)value;
    p->throws++;
    if (p->variant == LUDO_CLASSIC && value == 6 && ++p->sixes == 3) {    /* Ludo's third 6: lost, the turn ends */
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
    int to = ludo_dest(p, token), hits = ludo_hits(p, token), me = p->turn, caught = 0;
    if (to < 0) return 0;
    record(g, (uint8_t)(LUDO_MOVE_EV + token));
    p->at[me][token] = (int8_t)to;
    for (int i = 0; i < LUDO_PLAYERS * LUDO_TOKENS; i++)   /* the other players' tokens there go back to their yards */
        if (hits >> i & 1) {
            p->at[i / LUDO_TOKENS][i % LUDO_TOKENS] = -1;
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
