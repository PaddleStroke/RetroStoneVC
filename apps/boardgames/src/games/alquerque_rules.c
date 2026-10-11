/*
 * Alquerque: the rules (no SDL; tested by tests/alquerque_test.c). The board, the notation and the rules: alquerque.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/alquerque.h"

const int8_t aq_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1}, aq_dy[8] = {1, 1, 0, -1, -1, -1, 0, 1};

int aq_next(int pt, int d)
{
    int x = aq_x(pt) + aq_dx[d], y = aq_y(pt) + aq_dy[d];
    if (aq_dx[d] && aq_dy[d] && ((aq_x(pt) + aq_y(pt)) & 1)) return -1;   /* no diagonal through this point */
    if (x < 0 || x >= AQ_SIDE || y < 0 || y >= AQ_SIDE) return -1;
    return x + AQ_SIDE * y;
}

int aq_point(const char *s)
{
    if (s[0] < 'a' || s[0] > 'e' || s[1] < '1' || s[1] > '5') return -1;
    return (s[0] - 'a') + AQ_SIDE * (s[1] - '1');
}

void aq_start(aqpos *p)
{
    memset(p, 0, sizeof *p);
    for (int i = 0; i < AQ_POINTS; i++) {
        int x = aq_x(i), y = aq_y(i);
        p->at[i] = (int8_t)(y < 2 || (y == 2 && x > 2) ? 0 : y > 2 || (y == 2 && x < 2) ? 1 : -1);
    }
    p->chain = -1;
    p->left[0] = p->left[1] = AQ_PIECES;
}

/* the captures of the piece on pt */
static int captures(const aqpos *p, int pt, aqmove *out)
{
    int n = 0, me = p->at[pt];
    for (int d = 0; d < 8; d++) {
        int over = aq_next(pt, d), to = over >= 0 ? aq_next(over, d) : -1;
        if (to >= 0 && p->at[over] == !me && p->at[to] < 0) out[n++] = (aqmove){(int8_t)pt, (int8_t)to, (int8_t)over};
    }
    return n;
}

int aq_legal(const aqpos *p, aqmove *out)
{
    if (p->chain >= 0) return captures(p, p->chain, out);
    int n = 0, ahead = p->side ? -1 : 1;
    for (int i = 0; i < AQ_POINTS; i++)
        if (p->at[i] == p->side) n += captures(p, i, out + n);
    if (n) return n;                            /* capturing is compulsory */
    for (int i = 0; i < AQ_POINTS; i++) {       /* steps: forwards or sideways */
        if (p->at[i] != p->side) continue;
        for (int d = 0; d < 8; d++) {
            int to = aq_next(i, d);
            if (to >= 0 && aq_dy[d] != -ahead && p->at[to] < 0) out[n++] = (aqmove){(int8_t)i, (int8_t)to, -1};
        }
    }
    return n;
}

void aq_make(aqpos *p, aqmove m)
{
    p->at[m.to] = p->at[m.from];
    p->at[m.from] = -1;
    p->chain = -1;
    if (m.over >= 0) {
        p->at[m.over] = -1;
        p->left[!p->side]--;
        p->quiet = 0;
        aqmove more[8];
        if (captures(p, m.to, more)) { p->chain = m.to; return; }   /* it jumps on: still the same turn */
    } else {
        p->quiet++;
    }
    p->side = !p->side;
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void aq_new(aqgame *g)
{
    g->ply = 0;
    aq_start(&g->pos[0]);
}

int aq_play(aqgame *g, aqmove m)
{
    if (g->ply >= AQ_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    aq_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int aq_undo(aqgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

aqstatus aq_status(const aqgame *g)
{
    const aqpos *p = aq_pos(g);
    if (p->chain >= 0) return AS_PLAY;
    aqmove m[AQ_MAX_MOVES];
    if (!aq_legal(p, m)) return AS_STUCK;
    if (p->quiet >= AQ_QUIET) return AS_QUIET;
    int seen = 0;                               /* back to the last capture: nothing before it repeats */
    for (int i = g->ply; i >= 0 && i >= g->ply - p->quiet; i--) {
        const aqpos *q = &g->pos[i];
        seen += q->side == p->side && q->chain == p->chain && !memcmp(q->at, p->at, sizeof p->at);
    }
    return seen >= 3 ? AS_REPETITION : AS_PLAY;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static int play_one(aqgame *g, int from, int to)
{
    aqmove l[AQ_MAX_MOVES];
    int n = aq_legal(aq_pos(g), l);
    for (int i = 0; i < n; i++)
        if (l[i].from == from && l[i].to == to) return aq_play(g, l[i]);
    return 0;
}

int aq_script(aqgame *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        int a = aq_point(s);
        if (a < 0) return 0;
        s += 2;
        do {                                    /* c2-c3, c2c3, b2xd4xb4: a move to each next point */
            if (*s == '-' || *s == 'x') s++;
            int b = aq_point(s);
            if (b < 0 || !play_one(g, a, b)) return 0;
            s += 2;
            a = b;
        } while (*s && *s != ' ');
    }
    return 1;
}
