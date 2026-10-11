/*
 * Backgammon's rules (see backgammon.h): the moves of one die, the must-use-the-dice rule, the turns, undo, the
 * notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "games/backgammon.h"

/* ---- one die ----------------------------------------------------------------------------------------------------- */
/* the moves of die di for the player to move, added to out from k: the bar first, bearing off once all are home */
static int singles(const bpos *p, int di, bmove *out, int k)
{
    const signed char *me = p->n[p->side], *op = p->n[!p->side];
    int d = p->die[di];
    if (me[BG_BAR]) {
        int t = BG_BAR - d;
        if (op[25 - t] <= 1) out[k++] = (bmove){BG_BAR, (signed char)t, (signed char)di, op[25 - t] == 1};
        return k;
    }
    int home = 1, high = 0;
    for (int i = 7; i <= 24; i++) if (me[i]) home = 0;
    for (int i = 24; i >= 1 && !high; i--) if (me[i]) high = i;
    for (int i = 1; i <= 24; i++) {
        if (!me[i]) continue;
        int t = i - d;
        if (t >= 1) {
            if (op[25 - t] <= 1) out[k++] = (bmove){(signed char)i, (signed char)t, (signed char)di, op[25 - t] == 1};
        } else if (home && (t == 0 || i == high)) {
            out[k++] = (bmove){(signed char)i, BG_OFF, (signed char)di, 0};
        }
    }
    return k;
}

static void apply(bpos *p, bmove m)
{
    signed char *me = p->n[p->side], *op = p->n[!p->side];
    me[(int)m.from]--;
    me[(int)m.to]++;
    if (m.hit) {
        op[25 - m.to]--;
        op[BG_BAR]++;
    }
    p->used |= (unsigned char)(1u << m.die);
}

/* the most dice the player to move can still play (the dice of one value tried once) */
static int most(const bpos *p)
{
    int left = 0, best = 0;
    unsigned tried = 0;
    for (int i = 0; i < p->nd; i++) left += !(p->used >> i & 1);
    for (int i = 0; i < p->nd && best < left; i++) {
        if ((p->used >> i & 1) || (tried >> p->die[i] & 1)) continue;
        tried |= 1u << p->die[i];
        bmove m[BG_MAXMOVES];
        int k = singles(p, i, m, 0);
        for (int j = 0; j < k && best < left; j++) {
            bpos q = *p;
            apply(&q, m[j]);
            int r = 1 + most(&q);
            if (r > best) best = r;
        }
    }
    return best;
}

int bg_legal(const bpos *p, bmove *out)
{
    if (p->phase != BG_MOVE) return 0;
    int full = most(p), n = 0;
    if (!full) return 0;
    unsigned tried = 0;
    for (int i = 0; i < p->nd; i++) {           /* a move counts if the most dice can still be played after it */
        if ((p->used >> i & 1) || (tried >> p->die[i] & 1)) continue;
        tried |= 1u << p->die[i];
        bmove m[BG_MAXMOVES];
        int k = singles(p, i, m, 0);
        for (int j = 0; j < k; j++) {
            bpos q = *p;
            apply(&q, m[j]);
            if (1 + most(&q) == full) out[n++] = m[j];
        }
    }
    if (full == 1 && p->nd == 2 && !p->used && p->die[0] != p->die[1]) {   /* only one: the larger, if it can */
        int big = p->die[0] > p->die[1] ? 0 : 1, any = 0, k = 0;
        for (int i = 0; i < n; i++) any |= out[i].die == big;
        if (any) {
            for (int i = 0; i < n; i++)
                if (out[i].die == big) out[k++] = out[i];
            n = k;
        }
    }
    return n;
}

int bg_find(const bpos *p, int from, int to, bmove *m)
{
    bmove l[BG_MAXMOVES];
    int n = bg_legal(p, l), best = -1;
    for (int i = 0; i < n; i++)
        if (l[i].from == from && l[i].to == to && (best < 0 || p->die[(int)l[i].die] < p->die[(int)l[best].die]))
            best = i;
    if (best >= 0) *m = l[best];
    return best >= 0;
}

int bg_pips(const bpos *p, int player)
{
    int s = 0;
    for (int i = 1; i <= 25; i++) s += i * p->n[player][i];
    return s;
}

int bg_points(const bpos *p)
{
    if (p->phase != BG_OVER) return 0;
    const signed char *lo = p->n[!p->side];
    if (lo[BG_OFF]) return 1;
    for (int i = 19; i <= 25; i++) if (lo[i]) return 3;    /* in the winner's home board, or on the bar */
    return 2;
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
void bg_new(bgame *g)
{
    memset(g, 0, sizeof *g);
    bpos *p = &g->pos[0];
    for (int s = 0; s < 2; s++) {
        p->n[s][24] = 2;
        p->n[s][13] = 5;
        p->n[s][8] = 3;
        p->n[s][6] = 5;
    }
    p->phase = BG_OPENING;
    p->passed = -1;
}

/* a new event: the position after it is a copy of the one before, to change */
static bpos *push(bgame *g, int kind, int player, bmove m)
{
    if (g->n == BG_MAX) {                       /* full: forget the oldest */
        int drop = BG_MAX / 4;
        memmove(g->pos, g->pos + drop, (size_t)(BG_MAX + 1 - drop) * sizeof g->pos[0]);
        memmove(g->ev, g->ev + drop, (size_t)(BG_MAX - drop) * sizeof g->ev[0]);
        g->n -= drop;
    }
    g->ev[g->n] = (bevent){(signed char)kind, (signed char)player, m};
    g->pos[g->n + 1] = g->pos[g->n];
    return &g->pos[++g->n];
}

/* the dice thrown: if they cannot be played, the turn passes */
static void turn_starts(bpos *p)
{
    p->passed = -1;
    p->phase = BG_MOVE;
    if (!most(p)) {
        p->passed = p->side;
        p->side = !p->side;
        p->phase = BG_THROW;
    }
}

int bg_throw(bgame *g, int a, int b)
{
    const bpos *now = bg_pos(g);
    if ((now->phase != BG_OPENING && now->phase != BG_THROW) || a < 1 || a > 6 || b < 1 || b > 6) return 0;
    int opening = now->phase == BG_OPENING;
    bpos *p = push(g, BG_EV_THROW, opening ? -1 : now->side, (bmove){0, 0, 0, 0});
    p->used = 0;
    p->die[0] = (signed char)a;
    p->die[1] = (signed char)b;
    p->nd = 2;
    if (opening) {
        p->passed = -1;
        if (a == b) return 1;                   /* a tie: throw again */
        p->side = a > b ? 0 : 1;
    } else if (a == b) {
        p->die[2] = p->die[3] = (signed char)a;
        p->nd = 4;
    }
    turn_starts(p);
    return 1;
}

int bg_play(bgame *g, bmove m)
{
    bmove l[BG_MAXMOVES];
    int n = bg_legal(bg_pos(g), l), ok = 0;
    for (int i = 0; i < n && !ok; i++) ok = l[i].from == m.from && l[i].to == m.to && l[i].die == m.die;
    if (!ok) return 0;
    m.hit = 0;
    for (int i = 0; i < n; i++)
        if (l[i].from == m.from && l[i].to == m.to) m.hit = l[i].hit;
    bpos *p = push(g, BG_EV_MOVE, bg_pos(g)->side, m);
    apply(p, m);
    p->passed = -1;
    if (p->n[p->side][BG_OFF] == 15) p->phase = BG_OVER;
    else if (!most(p)) {                        /* the dice used, or the rest cannot be: the other player's turn */
        p->side = !p->side;
        p->phase = BG_THROW;
    }
    return 1;
}

int bg_undo(bgame *g)
{
    int k = g->n - 1;
    while (k >= 0 && g->ev[k].kind != BG_EV_MOVE) k--;
    if (k < 0) return 0;
    g->n = k;                                   /* the later throws go: the next ones are thrown again */
    return 1;
}

int bg_status(const bgame *g)
{
    const bpos *p = bg_pos(g);
    return p->phase != BG_OVER ? BG_PLAY : p->side ? BG_WIN1 : BG_WIN0;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
/* one checker from -> to with one die or several (the dice in either order) */
static int path(bgame *g, int from, int to)
{
    bmove m, l[BG_MAXMOVES];
    if (bg_find(bg_pos(g), from, to, &m)) return bg_play(g, m);
    int n = bg_legal(bg_pos(g), l), side = bg_pos(g)->side;
    for (int i = 0; i < n; i++) {
        if (l[i].from != from || l[i].to <= to) continue;
        bg_play(g, l[i]);
        if (bg_pos(g)->phase == BG_MOVE && bg_pos(g)->side == side && path(g, l[i].to, to)) return 1;
        bg_undo(g);
    }
    return 0;
}

static int point(const char **s)
{
    if (!strncmp(*s, "bar", 3)) { *s += 3; return BG_BAR; }
    if (!strncmp(*s, "off", 3)) { *s += 3; return BG_OFF; }
    if (!isdigit((unsigned char)**s)) return -1;
    int v = (int)strtol(*s, (char **)s, 10);
    return v >= 1 && v <= 24 ? v : -1;
}

int bg_moves(bgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (isdigit((unsigned char)s[0]) && isdigit((unsigned char)s[1]) && s[2] == ':') {
            if (!bg_throw(g, s[0] - '0', s[1] - '0')) return 0;
            s += 3;
            continue;
        }
        int hop[8], n = 0;                      /* 24/18/13*: the points it goes through */
        if ((hop[n++] = point(&s)) < 0) return 0;
        while (*s == '/' && n < 8) {
            s++;
            if ((hop[n++] = point(&s)) < 0) return 0;
            if (*s == '*') s++;
        }
        int times = 1;
        if (*s == '(') {
            times = (int)strtol(s + 1, (char **)&s, 10);
            if (*s++ != ')' || times < 1 || times > 4) return 0;
        }
        if (n < 2) return 0;
        for (int t = 0; t < times; t++)
            for (int i = 1; i < n; i++)
                if (!path(g, hop[i - 1], hop[i])) return 0;
    }
    return 1;
}
