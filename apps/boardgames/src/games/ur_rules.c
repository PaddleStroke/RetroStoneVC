/*
 * The Royal Game of Ur for two players on one device: the rules and the notation (ur.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/ur.h"

void ur_new(ugame *g)
{
    memset(g, 0, sizeof *g);
    g->pos.n[0][0] = g->pos.n[1][0] = UR_PIECES;
    g->pos.winner = -1;
}

int ur_rosette(int place) { return place == 4 || place == UR_CENTRE || place == 14; }
int ur_shared(int place) { return place >= UR_SHARED_LO && place <= UR_SHARED_HI; }

int ur_cell(int player, int place, int *col, int *row)
{
    if (place < 1 || place > 14) return 0;
    if (ur_shared(place)) {
        *col = place - UR_SHARED_LO;
        *row = 1;
    } else {                                    /* 1..4: cols 3..0; 13, 14: cols 7, 6 */
        *col = place <= 4 ? 4 - place : 20 - place;
        *row = player ? 0 : 2;
    }
    return 1;
}

int ur_dest(const upos *p, int from)
{
    int me = p->turn, to = from + p->roll;
    if (p->phase != UR_MOVE || from < 0 || from >= UR_OFF || !p->roll || !p->n[me][from] || to > UR_OFF) return -1;
    if (to < UR_OFF && p->n[me][to]) return -1; /* its own piece there */
    if (to == UR_CENTRE && p->n[!me][to]) return -1;   /* the other's piece is safe on the central rosette */
    return to;
}

int ur_takes(const upos *p, int from)
{
    int to = ur_dest(p, from);
    return to >= 0 && ur_shared(to) && p->n[!p->turn][to];
}

int ur_movable(const upos *p)
{
    int m = 0;
    for (int f = 0; f < UR_OFF; f++)
        if (ur_dest(p, f) >= 0) m |= 1 << f;
    return m;
}

/* a new event: the position before it, kept for undo. The history full, its oldest quarter is forgotten (undo goes
 * back that far at most): a game never stops for its length. */
static void record(ugame *g, int ev)
{
    if (g->n >= UR_MAX_EVENTS) {
        int drop = UR_MAX_EVENTS / 4;
        memmove(g->past, g->past + drop, (size_t)(UR_MAX_EVENTS - drop) * sizeof g->past[0]);
        memmove(g->ev, g->ev + drop, (size_t)(UR_MAX_EVENTS - drop) * sizeof g->ev[0]);
        g->n -= drop;
    }
    g->past[g->n] = g->pos;
    g->ev[g->n++] = (uint8_t)ev;
}


int ur_throw(ugame *g, int value)
{
    upos *p = &g->pos;
    if (p->phase != UR_THROW || value < 0 || value > 4) return 0;
    record(g, (uint8_t)value);
    p->roll = (uint8_t)value;
    p->throws++;
    p->phase = UR_MOVE;
    if (ur_movable(p)) return UT_MOVE;
    p->turn ^= 1;                               /* a 0, or nothing can move: lost */
    p->phase = UR_THROW;
    return UT_PASS;
}

int ur_move(ugame *g, int from)
{
    upos *p = &g->pos;
    int to = ur_dest(p, from), me = p->turn, caught = 0;
    if (to < 0) return 0;
    record(g, (uint8_t)(UR_MOVE_EV + from));
    p->n[me][from]--;
    p->n[me][to]++;
    if (ur_shared(to) && p->n[!me][to]) {       /* sent back off the board */
        p->n[!me][to] = 0;
        p->n[!me][0]++;
        caught = 1;
    }
    if (p->n[me][UR_OFF] == UR_PIECES) {
        p->winner = (int8_t)me;
        p->phase = UR_OVER;
        return 1 + caught;
    }
    p->phase = UR_THROW;
    if (!ur_rosette(to)) p->turn ^= 1;          /* a rosette: the same player throws again */
    return 1 + caught;
}

int ur_undo(ugame *g)
{
    if (!g->n) return 0;
    while (g->n) {                              /* the throws after the last move, then the move itself */
        g->pos = g->past[--g->n];
        if (g->ev[g->n] >= UR_MOVE_EV) break;
    }
    return 1;
}

int ur_script(ugame *g, const char *s)
{
    for (; *s; s++) {
        if (*s == ' ') continue;
        if (*s >= '0' && *s <= '4') {
            if (!ur_throw(g, *s - '0')) return 0;
        } else if (*s >= 'a' && *s < 'a' + UR_OFF) {
            if (!ur_move(g, *s - 'a')) return 0;
        } else {
            return 0;
        }
    }
    return 1;
}
