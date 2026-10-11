/*
 * Lasca: the rules (no SDL; tested by tests/lasca_test.c). The board, the columns, the notation and the rules:
 * lasca.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/lasca.h"

static const int8_t DX[4] = {1, -1, 1, -1}, DY[4] = {1, 1, -1, -1};   /* the diagonals: player 0's forwards first */

int ls_square(const char *s)
{
    if (s[0] < 'a' || s[0] > 'g' || s[1] < '1' || s[1] > '7') return -1;
    int sq = (s[0] - 'a') + LS_SIDE * (s[1] - '1');
    return ls_used(sq) ? sq : -1;
}

static int next(int s, int d)                   /* the next square along diagonal d; -1 off the board */
{
    int x = ls_x(s) + DX[d], y = ls_y(s) + DY[d];
    return x < 0 || x >= LS_SIDE || y < 0 || y >= LS_SIDE ? -1 : x + LS_SIDE * y;
}

/* the diagonals a piece may go along: a soldier forwards only (player 0 up, player 1 down), an officer all four */
static int ways(int pc, int8_t *d)
{
    if (LS_OFFICER(pc)) { for (int i = 0; i < 4; i++) d[i] = (int8_t)i; return 4; }
    d[0] = LS_OWNER(pc) ? 2 : 0;
    d[1] = (int8_t)(d[0] + 1);
    return 2;
}

static void put(lscol *c, int pc)               /* a piece on top of a column */
{
    c->pc = c->pc << 2 | (uint64_t)pc;
    c->h++;
}

void ls_start(lspos *p)
{
    memset(p, 0, sizeof *p);
    for (int s = 0; s < LS_SQUARES; s++) {
        if (!ls_used(s) || ls_y(s) == 3) continue;
        put(&p->col[s], ls_y(s) < 3 ? LS_SOLDIER0 : LS_SOLDIER1);
    }
    p->chain = -1;
}

/* the captures of the column on s (not over a square already jumped in this chain) */
static int captures(const lspos *p, int s, lsmove *out)
{
    int8_t d[4];
    int n = 0, top = ls_top(p->col[s]), k = ways(top, d);
    for (int i = 0; i < k; i++) {
        int over = next(s, d[i]), to = over >= 0 ? next(over, d[i]) : -1;
        if (to < 0 || !p->col[over].h || p->col[to].h || (p->jumped >> over & 1)) continue;
        if (LS_OWNER(ls_top(p->col[over])) != LS_OWNER(top)) out[n++] = (lsmove){(int8_t)s, (int8_t)to, (int8_t)over};
    }
    return n;
}

static int mine(const lspos *p, int s) { return p->col[s].h && LS_OWNER(ls_top(p->col[s])) == p->side; }

int ls_legal(const lspos *p, lsmove *out)
{
    if (p->chain >= 0) return captures(p, p->chain, out);
    int n = 0;
    for (int s = 0; s < LS_SQUARES; s++)
        if (mine(p, s)) n += captures(p, s, out + n);
    if (n) return n;                            /* capturing is compulsory */
    for (int s = 0; s < LS_SQUARES; s++) {
        if (!mine(p, s)) continue;
        int8_t d[4];
        int k = ways(ls_top(p->col[s]), d);
        for (int i = 0; i < k; i++) {
            int to = next(s, d[i]);
            if (to >= 0 && !p->col[to].h) out[n++] = (lsmove){(int8_t)s, (int8_t)to, -1};
        }
    }
    return n;
}

void ls_make(lspos *p, lsmove m)
{
    lscol c = p->col[m.from];
    int top = ls_top(c), crowned = 0;
    p->col[m.from] = (lscol){0, 0};
    if (m.over >= 0) {                          /* the jumped column's top piece goes under this one */
        lscol *v = &p->col[m.over];
        c.pc |= (uint64_t)(v->pc & 3) << (2 * c.h);
        c.h++;
        v->pc >>= 2;
        v->h--;
        p->jumped |= 1ull << m.over;
    }
    if (!LS_OFFICER(top) && ls_y(m.to) == (LS_OWNER(top) ? 0 : LS_SIDE - 1)) {
        c.pc |= 2;                              /* the soldier on top reaches the far row: an officer */
        crowned = 1;
    }
    p->col[m.to] = c;
    p->quiet = m.over >= 0 || !LS_OFFICER(top) ? 0 : p->quiet + 1;
    p->chain = -1;
    if (m.over >= 0 && !crowned) {
        lsmove more[4];
        if (captures(p, m.to, more)) { p->chain = m.to; return; }   /* it jumps on: still the same turn */
    }
    p->jumped = 0;
    p->side = !p->side;
}

int ls_columns(const lspos *p, int player)
{
    int n = 0;
    for (int s = 0; s < LS_SQUARES; s++) n += p->col[s].h && LS_OWNER(ls_top(p->col[s])) == player;
    return n;
}

int ls_prisoners(const lspos *p, int player)
{
    int n = 0;
    for (int s = 0; s < LS_SQUARES; s++) {
        lscol c = p->col[s];
        if (!c.h || LS_OWNER(ls_top(c)) != player) continue;
        for (int i = 1; i < c.h; i++) n += LS_OWNER(ls_piece(c, i)) != player;
    }
    return n;
}

int ls_setup(lspos *p, int side, const char *s)
{
    memset(p, 0, sizeof *p);
    p->chain = -1;
    p->side = (uint8_t)side;
    while (*s) {
        if (*s == ' ') { s++; continue; }
        int sq = ls_square(s);
        if (sq < 0 || s[2] != ':') return 0;
        s += 3;
        char list[24];
        int k = 0;
        while (*s && *s != ' ' && k < 22) list[k++] = *s++;
        if (!k) return 0;
        for (int i = k - 1; i >= 0; i--) {      /* written from the top: put from the bottom */
            int pc = list[i] == 'w' ? LS_SOLDIER0 : list[i] == 'b' ? LS_SOLDIER1 : list[i] == 'W' ? LS_OFFICER0
                   : list[i] == 'B' ? LS_OFFICER1 : -1;
            if (pc < 0) return 0;
            put(&p->col[sq], pc);
        }
    }
    return 1;
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void ls_new(lsgame *g)
{
    g->ply = 0;
    ls_start(&g->pos[0]);
}

int ls_play(lsgame *g, lsmove m)
{
    if (g->ply >= LS_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    ls_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int ls_undo(lsgame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

static int same(const lspos *a, const lspos *b)
{
    if (a->side != b->side || a->chain != b->chain || a->jumped != b->jumped) return 0;
    for (int s = 0; s < LS_SQUARES; s++)
        if (a->col[s].h != b->col[s].h || a->col[s].pc != b->col[s].pc) return 0;
    return 1;
}

lsstatus ls_status(const lsgame *g)
{
    const lspos *p = ls_pos(g);
    if (p->chain >= 0) return LS_PLAY;
    lsmove m[LS_MAX_MOVES];
    if (!ls_legal(p, m)) return LS_STUCK;
    if (p->quiet >= LS_QUIET) return LS_QUIET_DRAW;
    int seen = 0;                               /* back to the last capture or soldier move: nothing before repeats */
    for (int i = g->ply; i >= 0 && i >= g->ply - p->quiet; i--) seen += same(&g->pos[i], p);
    return seen >= 3 ? LS_REPETITION : LS_PLAY;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static int play_one(lsgame *g, int from, int to)
{
    lsmove l[LS_MAX_MOVES];
    int n = ls_legal(ls_pos(g), l);
    for (int i = 0; i < n; i++)
        if (l[i].from == from && l[i].to == to) return ls_play(g, l[i]);
    return 0;
}

int ls_script(lsgame *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        int a = ls_square(s);
        if (a < 0) return 0;
        s += 2;
        do {                                    /* c3-d4, c3d4, c3xe5xc7: a move to each next square */
            if (*s == '-' || *s == 'x') s++;
            int b = ls_square(s);
            if (b < 0 || !play_one(g, a, b)) return 0;
            s += 2;
            a = b;
        } while (*s && *s != ' ');
    }
    return 1;
}
