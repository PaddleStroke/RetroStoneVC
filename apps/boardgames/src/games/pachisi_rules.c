/*
 * Pachisi for two or four players on one device: the rules and the notation (pachisi.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/pachisi.h"

void pa_new(pgame *g, int players)
{
    memset(g, 0, sizeof *g);
    g->pos.players = (uint8_t)(players == 4 ? 4 : 2);
    g->pos.winner = -1;
}

int pa_value(int mouths)
{
    static const int v[7] = {25, 10, 2, 3, 4, 5, 6};
    return mouths >= 0 && mouths <= 6 ? v[mouths] : 0;
}

int pa_mouths(int value)
{
    return value == 25 ? 0 : value == 10 ? 1 : value >= 2 && value <= 6 ? value : -1;
}

int pa_grace(int value) { return value == 6 || value == 10 || value == 25; }
int pa_army_home(const ppos *p, int army)
{
    int n = 0;
    for (int i = 0; i < PA_PIECES; i++) n += p->at[army][i] == PA_HOME;
    return n;
}

int pa_army(const ppos *p, int piece)
{
    if (piece < 0 || piece >= 2 * PA_PIECES) return -1;
    if (p->players == 2) return p->turn + (piece >= PA_PIECES ? 2 : 0);
    if (piece >= PA_PIECES) return -1;          /* four: one army, its own, or its partner's once its own is home */
    return pa_army_home(p, p->turn) == PA_PIECES ? (p->turn + 2) % PA_ARMIES : p->turn;
}

int pa_square(int army, int place)
{
    if (place <= 0 || place >= PA_HOME) return -1;
    if (place < PA_END) return PA_LAP + army * 7 + place - 1;                  /* down the middle column */
    if (place > PA_UP) return PA_LAP + army * 7 + PA_HOME - place - 1;         /* up it, home */
    return (17 * army + place - PA_END) % PA_LAP;
}

int pa_castle(int square)
{
    int j = square % 17;
    return square >= 0 && square < PA_LAP && (j == 0 || j == 4 || j == 13);
}

void pa_cell(int square, int *x, int *y)
{
    int arm, dx, dy;                            /* an arm's own frame: arm 0's (the bottom), dy outwards */
    if (square < 0 || square >= PA_SQUARES) {
        *x = *y = 9;
        return;
    }
    if (square >= PA_LAP) {                     /* a middle column: row 1..7 from the charkoni */
        arm = (square - PA_LAP) / 7;
        dx = 0;
        dy = (square - PA_LAP) % 7 + 2;
    } else {
        int j = square % 17;
        arm = square / 17;
        if (j == 0) dx = 0, dy = 9;             /* the arm's end */
        else if (j <= 8) dx = 1, dy = 10 - j;   /* its next column, inwards (anticlockwise) */
        else arm = (arm + 1) % 4, dx = -1, dy = j - 7;   /* the next arm's first column, outwards */
    }
    for (int k = 0; k < arm; k++) {             /* turned anticlockwise (on the screen, y down) */
        int t = dx;
        dx = dy;
        dy = -t;
    }
    *x = 9 + dx;
    *y = 9 + dy;
}

/* the other player's pieces on a square */
static int enemies(const ppos *p, int player, int square)
{
    int n = 0;
    for (int a = !player; a < PA_ARMIES; a += 2)
        for (int i = 0; i < PA_PIECES; i++) n += pa_square(a, p->at[a][i]) == square && square >= 0;
    return n;
}

int pa_dest(const ppos *p, int piece)
{
    int a = pa_army(p, piece);
    if (p->phase != PA_MOVE || a < 0) return -1;
    int at = p->at[a][piece % PA_PIECES], to = at + p->roll;
    if (at == PA_HOME || to > PA_HOME || to == PA_HOME - 1) return -1;   /* no throw is a 1: never one short */
    if (at == 0 && !pa_grace(p->roll) && (p->out >> a & 1)) return -1;   /* out of the charkoni: a grace */
    int sq = pa_square(a, to);
    if (pa_castle(sq) && enemies(p, p->turn & 1, sq)) return -1;
    return to;
}

int pa_takes(const ppos *p, int piece)
{
    int to = pa_dest(p, piece);
    if (to < 0) return 0;
    int sq = pa_square(pa_army(p, piece), to);
    return pa_castle(sq) ? 0 : enemies(p, p->turn & 1, sq);
}

int pa_movable(const ppos *p)
{
    int m = 0;
    for (int i = 0; i < 2 * PA_PIECES; i++)
        if (pa_dest(p, i) >= 0) m |= 1 << i;
    return m;
}

int pa_home(const ppos *p, int side)
{
    return pa_army_home(p, side) + pa_army_home(p, side + 2);
}

static void next(ppos *p, int again)
{
    p->phase = PA_THROW;
    if (!again) p->turn = (uint8_t)((p->turn + 1) % p->players);   /* round the table */
}

int pa_throw(pgame *g, int mouths)
{
    ppos *p = &g->pos;
    if (p->phase != PA_THROW || mouths < 0 || mouths > 6 || g->n >= PA_MAX_EVENTS) return 0;
    g->past[g->n] = *p;
    g->ev[g->n++] = (uint8_t)mouths;
    p->roll = (uint8_t)pa_value(mouths);
    p->throws++;
    p->phase = PA_MOVE;
    if (pa_movable(p)) return PT_MOVE;
    next(p, pa_grace(p->roll));                 /* nothing can move: a grace still throws again */
    return pa_grace(p->roll) ? PT_AGAIN : PT_PASS;
}

int pa_move(pgame *g, int piece)
{
    ppos *p = &g->pos;
    int to = pa_dest(p, piece), me = p->turn, caught = 0;
    if (to < 0 || g->n >= PA_MAX_EVENTS) return 0;
    g->past[g->n] = *p;
    g->ev[g->n++] = (uint8_t)(PA_MOVE_EV + piece);
    int a = pa_army(p, piece), sq = pa_square(a, to);
    p->at[a][piece % PA_PIECES] = (uint8_t)to;
    p->out |= (uint8_t)(1 << a);
    if (!pa_castle(sq))                         /* the other player's pieces there go back to the charkoni */
        for (int e = !(me & 1); e < PA_ARMIES; e += 2)
            for (int i = 0; i < PA_PIECES; i++)
                if (sq >= 0 && pa_square(e, p->at[e][i]) == sq) {
                    p->at[e][i] = 0;
                    caught++;
                }
    if (pa_home(p, me & 1) == 2 * PA_PIECES) {   /* both armies of its side home */
        p->winner = (int8_t)me;
        p->phase = PA_OVER;
    } else {
        next(p, pa_grace(p->roll) || caught);
    }
    return 1 + caught;
}

int pa_decline(pgame *g)
{
    ppos *p = &g->pos;
    if (p->phase != PA_MOVE || g->n >= PA_MAX_EVENTS) return 0;
    g->past[g->n] = *p;
    g->ev[g->n++] = PA_DECLINE_EV;
    next(p, pa_grace(p->roll));
    return 1;
}

int pa_undo(pgame *g)
{
    if (!g->n) return 0;
    while (g->n) {                              /* the throws after the last choice, then the choice itself */
        g->pos = g->past[--g->n];
        if (g->ev[g->n] >= PA_DECLINE_EV) break;
    }
    return 1;
}

int pa_script(pgame *g, const char *s)
{
    while (*s) {
        if (*s == ' ') {
            s++;
        } else if (*s >= '0' && *s <= '9') {
            int v = 0;
            while (*s >= '0' && *s <= '9') v = v * 10 + *s++ - '0';
            if (pa_mouths(v) < 0 || !pa_throw(g, pa_mouths(v))) return 0;
        } else if (*s >= 'a' && *s < 'a' + 2 * PA_PIECES) {
            if (!pa_move(g, *s++ - 'a')) return 0;
        } else if (*s == '-') {
            s++;
            if (!pa_decline(g)) return 0;
        } else {
            return 0;
        }
    }
    return 1;
}
