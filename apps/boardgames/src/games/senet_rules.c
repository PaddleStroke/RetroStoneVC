/*
 * Senet: the rules and the notation (senet.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/senet.h"

void senet_new(senet_game *g, int variant)
{
    memset(g, 0, sizeof *g);
    g->variant = variant == SENET_SEVEN ? SENET_SEVEN : SENET_FIVE;
    senet_pos *p = &g->pos;
    p->pieces = g->variant == SENET_SEVEN ? 7 : 5;
    for (int s = 1; s <= 2 * p->pieces; s++) p->sq[s] = (uint8_t)(1 + !(s & 1));   /* ivory on the odd squares */
    p->winner = -1;
}

int senet_again(int value) { return value == 1 || value == 4 || value == 5; }

int senet_cell(int square, int *col, int *row)
{
    if (square < 1 || square > SENET_SQ) return 0;
    int k = square - 1;
    *row = k / 10;
    *col = *row == 1 ? 9 - k % 10 : k % 10;
    return 1;
}

static int owner(const senet_pos *p, int s) { return s >= 1 && s <= SENET_SQ ? p->sq[s] - 1 : -1; }

int senet_protected(const senet_pos *p, int s)
{
    int o = owner(p, s);
    return o >= 0 && (owner(p, s - 1) == o || owner(p, s + 1) == o);
}

/* square s holds a piece of `who` in a run of three or more of them */
static int in_block(const senet_pos *p, int s, int who)
{
    if (owner(p, s) != who) return 0;
    int lo = s, hi = s;
    while (owner(p, lo - 1) == who) lo--;
    while (owner(p, hi + 1) == who) hi++;
    return hi - lo >= 2;
}

/* the piece on `from` moved by the throw, forwards (dir 1) or back (-1); -1: not allowed */
static int dest_dir(const senet_pos *p, int from, int dir)
{
    int me = p->turn, to = from + dir * p->roll;
    if (owner(p, from) != me || !p->roll) return -1;
    if (dir > 0 && from < SENET_BEAUTY && to > SENET_BEAUTY) return -1;   /* every piece stops on 26 */
    if (to < 1 || to > SENET_OFF) return -1;    /* off the board with the exact throw only */
    for (int s = from + dir; s != to; s += dir)  /* three in a row of the other player bar the way */
        if (in_block(p, s, !me)) return -1;
    if (to == SENET_OFF) return to;
    int o = owner(p, to);
    if (o == me) return -1;
    if (o >= 0 && senet_protected(p, to)) return -1;
    return to;
}

static int has(const senet_pos *p, int dir)
{
    for (int s = 1; s <= SENET_SQ; s++)
        if (dest_dir(p, s, dir) >= 0) return 1;
    return 0;
}

int senet_dest(const senet_pos *p, int from)
{
    if (p->phase != SENET_MOVE) return -1;
    return dest_dir(p, from, p->back ? -1 : 1);
}

int senet_takes(const senet_pos *p, int from)
{
    int to = senet_dest(p, from);
    return to >= 1 && to <= SENET_SQ && owner(p, to) == !p->turn;
}

int senet_landing(const senet_pos *p, int to)
{
    if (to != SENET_WATER) return to;
    int s = SENET_REBIRTH;
    while (s > 1 && p->sq[s]) s--;              /* 15 taken: the first free square before it */
    return s;
}

uint32_t senet_movable(const senet_pos *p)
{
    uint32_t m = 0;
    for (int s = 1; s <= SENET_SQ; s++)
        if (senet_dest(p, s) >= 0) m |= 1u << s;
    return m;
}

void senet_roll(senet_pos *p, int value)
{
    p->roll = (uint8_t)value;
    p->throws++;
    p->phase = SENET_MOVE;
    p->back = 0;
    if (has(p, 1)) return;
    p->back = 1;                                /* nothing goes forwards: backwards then */
    if (has(p, -1)) return;
    p->back = 0;
    p->turn ^= 1;                               /* no move at all: the throw is lost */
    p->phase = SENET_THROW;
}

void senet_step(senet_pos *p, int from)
{
    int to = senet_dest(p, from), me = p->turn;
    if (to < 0) return;
    p->sq[from] = 0;
    if (to == SENET_OFF) {
        p->off[me]++;
    } else {
        if (p->sq[to]) p->sq[from] = p->sq[to]; /* a swap: the other piece to the square the mover left */
        p->sq[to] = 0;
        p->sq[senet_landing(p, to)] = (uint8_t)(1 + me);
    }
    p->back = 0;
    if (p->off[me] == p->pieces) {
        p->winner = (int8_t)me;
        p->phase = SENET_OVER;
        return;
    }
    p->phase = SENET_THROW;
    if (!senet_again(p->roll)) p->turn ^= 1;
}

/* a new event: the position before it, kept for undo. The history full, its oldest quarter is forgotten (undo goes
 * back that far at most): a game never stops for its length. */
static void record(senet_game *g, int ev)
{
    if (g->n >= SENET_MAX_EVENTS) {
        int drop = SENET_MAX_EVENTS / 4;
        memmove(g->past, g->past + drop, (size_t)(SENET_MAX_EVENTS - drop) * sizeof g->past[0]);
        memmove(g->ev, g->ev + drop, (size_t)(SENET_MAX_EVENTS - drop) * sizeof g->ev[0]);
        g->n -= drop;
    }
    g->past[g->n] = g->pos;
    g->ev[g->n++] = (uint8_t)ev;
}

int senet_throw(senet_game *g, int value)
{
    if (g->pos.phase != SENET_THROW || value < 1 || value > 5) return 0;
    record(g, value);
    senet_roll(&g->pos, value);
    return g->pos.phase == SENET_MOVE ? SENET_T_MOVE : SENET_T_PASS;
}

int senet_move(senet_game *g, int from)
{
    if (senet_dest(&g->pos, from) < 0) return 0;
    int swap = senet_takes(&g->pos, from);
    record(g, SENET_MOVE_EV + from);
    senet_step(&g->pos, from);
    return 1 + swap;
}

int senet_undo(senet_game *g)
{
    if (!g->n) return 0;
    while (g->n) {                              /* the throws after the last move, then the move itself */
        g->pos = g->past[--g->n];
        if (g->ev[g->n] >= SENET_MOVE_EV) break;
    }
    return 1;
}

int senet_script(senet_game *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        if (*s >= '1' && *s <= '5' && (s[1] == ' ' || !s[1])) {
            if (!senet_throw(g, *s - '0')) return 0;
            s++;
        } else if (*s == 's') {
            char *end;
            long sq = strtol(s + 1, &end, 10);
            if (end == s + 1 || !senet_move(g, (int)sq)) return 0;
            s = end;
        } else {
            return 0;
        }
    }
    return 1;
}
