/*
 * Fanorona: the rules (no SDL; tested by tests/fanorona_test.c). The board, the notation and the rules: fanorona.h.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/fanorona.h"

const int8_t fn_dx[8] = {0, 1, 1, 1, 0, -1, -1, -1}, fn_dy[8] = {1, 1, 0, -1, -1, -1, 0, 1};

int fn_next(int pt, int d)
{
    int x = fn_x(pt) + fn_dx[d], y = fn_y(pt) + fn_dy[d];
    if (fn_dx[d] && fn_dy[d] && ((fn_x(pt) + fn_y(pt)) & 1)) return -1;   /* no diagonal through this point */
    if (x < 0 || x >= FN_COLS || y < 0 || y >= FN_ROWS) return -1;
    return x + FN_COLS * y;
}

int fn_point(const char *s)
{
    if (s[0] < 'a' || s[0] > 'i' || s[1] < '1' || s[1] > '5') return -1;
    return (s[0] - 'a') + FN_COLS * (s[1] - '1');
}

int fn_dir(int from, int to)
{
    for (int d = 0; d < 8; d++)
        if (fn_next(from, d) == to) return d;
    return -1;
}

void fn_start(fnpos *p)
{
    memset(p, 0, sizeof *p);
    for (int i = 0; i < FN_POINTS; i++) {
        int x = fn_x(i), y = fn_y(i);
        p->at[i] = (int8_t)(y < 2 ? 0 : y > 2 ? 1 : x == 4 ? -1 : (x < 4) == (x % 2 == 0) ? 1 : 0);
    }
    p->chain = p->dir = -1;
    p->left[0] = p->left[1] = FN_PIECES;
}

/* the unbroken line of enemy pieces from pt on, in direction d */
static int line(const fnpos *p, int pt, int d, int8_t *out)
{
    int n = 0, foe = !p->side;
    for (; pt >= 0 && p->at[pt] == foe; pt = fn_next(pt, d)) out[n++] = (int8_t)pt;
    return n;
}

int fn_taken(const fnpos *p, fnmove m, int8_t *out)
{
    int d = fn_dir(m.from, m.to);
    if (d < 0) return 0;
    if (m.kind == FN_APPROACH) return line(p, fn_next(m.to, d), d, out);
    if (m.kind == FN_WITHDRAW) return line(p, fn_next(m.from, (d + 4) % 8), (d + 4) % 8, out);
    return 0;
}

/* the captures of the piece on pt (in a chain: not onto a point it stood on, not in its last direction) */
static int captures(const fnpos *p, int pt, fnmove *out)
{
    int n = 0, foe = !p->side, chained = p->chain >= 0;
    for (int d = 0; d < 8; d++) {
        int to = fn_next(pt, d);
        if (to < 0 || p->at[to] >= 0) continue;
        if (chained && ((p->seen >> to & 1) || d == p->dir)) continue;
        int ahead = fn_next(to, d), back = fn_next(pt, (d + 4) % 8);
        if (ahead >= 0 && p->at[ahead] == foe) out[n++] = (fnmove){(int8_t)pt, (int8_t)to, FN_APPROACH};
        if (back >= 0 && p->at[back] == foe) out[n++] = (fnmove){(int8_t)pt, (int8_t)to, FN_WITHDRAW};
    }
    return n;
}

int fn_legal(const fnpos *p, fnmove *out)
{
    if (p->chain >= 0) return captures(p, p->chain, out);
    int n = 0;
    for (int i = 0; i < FN_POINTS; i++)
        if (p->at[i] == p->side) n += captures(p, i, out + n);
    if (n) return n;                            /* capturing is compulsory */
    for (int i = 0; i < FN_POINTS; i++) {       /* paika: a step that takes nothing */
        if (p->at[i] != p->side) continue;
        for (int d = 0; d < 8; d++) {
            int to = fn_next(i, d);
            if (to >= 0 && p->at[to] < 0) out[n++] = (fnmove){(int8_t)i, (int8_t)to, FN_PAIKA};
        }
    }
    return n;
}

static void end_turn(fnpos *p)
{
    p->side = !p->side;
    p->chain = p->dir = -1;
    p->seen = 0;
}

void fn_make(fnpos *p, fnmove m)
{
    if (m.kind == FN_STOP) { end_turn(p); return; }
    int8_t took[FN_MAX_TAKE];
    int n = fn_taken(p, m, took);
    p->at[m.to] = p->at[m.from];
    p->at[m.from] = -1;
    if (m.kind == FN_PAIKA) {
        p->quiet++;
        end_turn(p);
        return;
    }
    for (int i = 0; i < n; i++) p->at[took[i]] = -1;
    p->left[!p->side] -= (uint8_t)n;
    p->quiet = 0;
    if (p->chain < 0) p->seen = 1ull << m.from;
    p->seen |= 1ull << m.to;
    p->chain = m.to;
    p->dir = (int8_t)fn_dir(m.from, m.to);
    fnmove more[16];
    if (!p->left[!p->side] || !captures(p, m.to, more)) end_turn(p);   /* nothing more to take: the turn ends */
}

/* ---- a game ------------------------------------------------------------------------------------------------------ */
void fn_new(fngame *g)
{
    g->ply = 0;
    fn_start(&g->pos[0]);
}

int fn_play(fngame *g, fnmove m)
{
    if (g->ply >= FN_MAX_PLY) return 0;
    g->pos[g->ply + 1] = g->pos[g->ply];
    fn_make(&g->pos[g->ply + 1], m);
    g->move[g->ply++] = m;
    return 1;
}

int fn_undo(fngame *g)
{
    if (!g->ply) return 0;
    g->ply--;
    return 1;
}

fnstatus fn_status(const fngame *g)
{
    const fnpos *p = fn_pos(g);
    if (p->chain >= 0) return FS_PLAY;
    fnmove m[FN_MAX_MOVES];
    if (!p->left[p->side] || !fn_legal(p, m)) return FS_LOST;
    if (p->quiet >= FN_QUIET) return FS_QUIET;
    int seen = 0;                               /* back to the last capture: nothing before it repeats */
    for (int i = g->ply; i >= 0; i--) {
        const fnpos *q = &g->pos[i];
        if (q->left[0] != p->left[0] || q->left[1] != p->left[1]) break;
        seen += q->chain < 0 && q->side == p->side && !memcmp(q->at, p->at, sizeof p->at);
    }
    return seen >= 3 ? FS_REPETITION : FS_PLAY;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
/* the step from a to b (kind: FN_APPROACH, FN_WITHDRAW, or -1: whichever, if only one fits) */
static int play_one(fngame *g, int a, int b, int kind)
{
    fnmove l[FN_MAX_MOVES];
    int n = fn_legal(fn_pos(g), l), found = -1;
    for (int i = 0; i < n; i++) {
        if (l[i].from != a || l[i].to != b || (kind >= 0 && l[i].kind != kind)) continue;
        if (found >= 0) return 0;               /* approach or withdrawal: say which */
        found = i;
    }
    return found >= 0 && fn_play(g, l[found]);
}

int fn_script(fngame *g, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        const fnpos *p = fn_pos(g);
        if (!strncmp(s, "stop", 4)) {
            if (p->chain < 0 || !fn_play(g, (fnmove){p->chain, p->chain, FN_STOP})) return 0;
            s += 4;
            continue;
        }
        int a = fn_point(s);
        if (a < 0 || (p->chain >= 0 && a != p->chain)) return 0;
        s += 2;
        do {                                    /* d3-e3w-e4: a step to each next point */
            if (*s == '-' || *s == 'x') s++;
            int b = fn_point(s), kind = -1;
            if (b < 0) return 0;
            s += 2;
            if ((*s == 'a' || *s == 'w') && !(s[1] >= '0' && s[1] <= '9'))   /* "a3a" approach, not "a3-a4" */
                kind = *s++ == 'a' ? FN_APPROACH : FN_WITHDRAW;
            if (!play_one(g, a, b, kind)) return 0;
            a = b;
        } while (*s && *s != ' ' && *s != '+');
        if (*s == '+') s++;                     /* the chain goes on in the next word */
        else if (fn_pos(g)->chain >= 0 && !fn_play(g, (fnmove){fn_pos(g)->chain, fn_pos(g)->chain, FN_STOP}))
            return 0;
    }
    return 1;
}
