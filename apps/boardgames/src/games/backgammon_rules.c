/*
 * Backgammon's rules (see backgammon.h): the moves of one die (and what each variant allows: hitting or pinning,
 * points held by one checker, the start and the head, the six in a row), the must-use-the-dice rule, the turns, the
 * end, undo, the notation.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/backgammon.h"

/* ---- the variants' ways ------------------------------------------------------------------------------------------ */
static int same_way(int v) { return v == BG_FEVGA || v == BG_NARDY; }

int bg_opp(int variant, int point)
{
    if (point < 1 || point > 24) return point;
    return same_way(variant) ? (point > 12 ? point - 12 : point + 12) : 25 - point;
}

/* fevga: the first checker off the start has passed the other's start (the player's 12), or nothing has left yet */
static int passed_start(const signed char *me)
{
    if (me[24] == 15 || me[BG_OFF]) return 1;
    for (int i = 1; i <= 11; i++) if (me[i]) return 1;
    return 0;
}

/* long nardy: the checkers that may leave the head this turn (two on a first throw of 6-6, 4-4 or 3-3) */
static int head_limit(const bpos *p)
{
    int d = p->die[0], doubles = p->nd == 4;
    return p->throws[(int)p->side] == 1 && doubles && (d == 6 || d == 4 || d == 3) ? 2 : 1;
}

/* a checker of the player to move may leave point i (1..24) */
static int may_leave(const bpos *p, int i)
{
    const signed char *me = p->n[(int)p->side];
    switch (p->variant) {
    case BG_PLAKOTO: return !p->pin[(int)p->side][i];
    case BG_FEVGA: return i != 24 || passed_start(me);
    case BG_NARDY: return i != 24 || p->head < head_limit(p);
    default: return 1;
    }
}

/* a checker of the player to move may land on point t (1..24); *hit: it hits (backgammon) or pins (plakoto) */
static int may_land(const bpos *p, int t, int *hit)
{
    int me = p->side, q = bg_opp(p->variant, t), there = p->n[!me][q];
    *hit = 0;
    switch (p->variant) {
    case BG_BACKGAMMON:
        *hit = there == 1;
        return there <= 1;
    case BG_PLAKOTO:
        if (p->pin[me][t]) return 0;            /* the other player's point: they pin one of ours there */
        if (there == 1 && !p->pin[!me][q]) *hit = 1;   /* a single checker, free: pinned */
        return there == 0 || there == 1;        /* a pinned one: the point is ours already */
    default:
        return there == 0;                      /* one checker holds a point */
    }
}

/* fevga, long nardy: after a move, the player who made it holds six points in a row the rules forbid: with all 15 of
 * the other's checkers behind them, or (fevga) the player's own starting table */
static int bad_prime(const bpos *p, int player)
{
    const signed char *me = p->n[player], *op = p->n[!player];
    int v = p->variant, row = 0;
    if (v == BG_FEVGA) {
        int all = 1;
        for (int i = 19; i <= 24; i++) all &= me[i] > 0;
        if (all) return 1;
    }
    for (int q = 24; q >= 1; q--) {             /* the other's numbering: q .. q + 5 held, the checkers above them */
        row = me[bg_opp(v, q)] > 0 ? row + 1 : 0;
        if (row < 6) continue;
        int behind = 0;
        for (int i = q + 6; i <= 24; i++) behind += op[i];
        if (behind == 15) return 1;
    }
    return 0;
}

static void apply(bpos *p, bmove m)
{
    int s = p->side, v = p->variant;
    signed char *me = p->n[s], *op = p->n[!s];
    me[(int)m.from]--;
    me[(int)m.to]++;
    if (v == BG_BACKGAMMON && m.hit) {
        op[25 - m.to]--;
        op[BG_BAR]++;
    }
    if (v == BG_PLAKOTO) {
        if (m.hit) p->pin[!s][bg_opp(v, m.to)] = 1;
        if (m.from >= 1 && m.from <= 24 && !me[(int)m.from]) p->pin[!s][bg_opp(v, m.from)] = 0;   /* set free */
    }
    if (v == BG_NARDY && m.from == 24) p->head++;
    p->used |= (unsigned char)(1u << m.die);
}

/* ---- one die ----------------------------------------------------------------------------------------------------- */
/* the moves of die di for the player to move, added to out from k: the bar first, bearing off once all are home */
static int singles(const bpos *p, int di, bmove *out, int k)
{
    const signed char *me = p->n[(int)p->side], *op = p->n[!p->side];
    int d = p->die[di], hit;
    if (me[BG_BAR]) {
        int t = BG_BAR - d;
        if (op[25 - t] <= 1) out[k++] = (bmove){BG_BAR, (signed char)t, (signed char)di, op[25 - t] == 1};
        return k;
    }
    int home = 1, high = 0;
    for (int i = 7; i <= 24; i++) if (me[i]) home = 0;
    for (int i = 24; i >= 1 && !high; i--) if (me[i]) high = i;
    for (int i = 1; i <= 24; i++) {
        if (!me[i] || !may_leave(p, i)) continue;
        int t = i - d;
        if (t >= 1) {
            if (!may_land(p, t, &hit)) continue;
            bmove m = {(signed char)i, (signed char)t, (signed char)di, (signed char)hit};
            if (same_way(p->variant)) {
                bpos q = *p;
                apply(&q, m);
                if (bad_prime(&q, p->side)) continue;
            }
            out[k++] = m;
        } else if (home && (t == 0 || i == high)) {
            out[k++] = (bmove){(signed char)i, BG_OFF, (signed char)di, 0};
        }
    }
    return k;
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
    if (p->phase != BG_OVER || p->tie) return 0;
    const signed char *lo = p->n[!p->side];
    if (p->variant == BG_PLAKOTO && p->pin[!p->side][24]) return 2;   /* the mother pinned */
    if (lo[BG_OFF]) return 1;
    if (p->variant != BG_BACKGAMMON) return 2;
    for (int i = 19; i <= 25; i++) if (lo[i]) return 3;    /* in the winner's home board, or on the bar */
    return 2;
}

/* ---- the game ---------------------------------------------------------------------------------------------------- */
void bg_new(bgame *g, int variant)
{
    memset(g, 0, sizeof *g);
    bpos *p = &g->pos[0];
    p->variant = (signed char)(variant >= 0 && variant < BG_VARIANTS ? variant : BG_BACKGAMMON);
    for (int s = 0; s < 2; s++) {
        if (p->variant != BG_BACKGAMMON) {      /* all fifteen on the player's 24 */
            p->n[s][24] = 15;
            continue;
        }
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
        g->forgot += drop;
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
        if (p->variant != BG_BACKGAMMON) {      /* the winner throws again for the first move */
            p->phase = BG_THROW;
            return 1;
        }
    } else {
        if (a == b) {
            p->die[2] = p->die[3] = (signed char)a;
            p->nd = 4;
        }
        p->head = 0;
        if (p->throws[(int)p->side] < 2) p->throws[(int)p->side]++;
    }
    turn_starts(p);
    return 1;
}

/* plakoto: a pinned mother ends the game once the pinner has no checker left on their own start (the winner made the
 * player to move); both mothers pinned: a draw */
static int mother_end(bpos *p)
{
    int pinned[2];
    for (int s = 0; s < 2; s++) pinned[s] = p->n[s][24] == 1 && p->pin[s][24];
    if (pinned[0] && pinned[1]) {
        p->tie = 1;
        return 1;
    }
    for (int s = 0; s < 2; s++)
        if (pinned[s] && !p->n[!s][24]) {
            p->side = (signed char)!s;
            return 1;
        }
    return 0;
}

/* ---- for the bot (backgammon_bot.c): one die's moves before the rule of the most dice, a move on the position
 * alone, the end it may make --------------------------------------------------------------------------------------- */
int bg_singles(const bpos *p, int di, bmove *out) { return singles(p, di, out, 0); }
void bg_apply(bpos *p, bmove m) { apply(p, m); }

int bg_settle(bpos *p)
{
    if (p->n[(int)p->side][BG_OFF] == 15 || (p->variant == BG_PLAKOTO && mother_end(p))) {
        p->phase = BG_OVER;
        return 1;
    }
    return 0;
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
    if (bg_settle(p)) {
    } else if (!most(p)) {                        /* the dice used, or the rest cannot be: the other player's turn */
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
    return p->phase != BG_OVER ? BG_PLAY : p->tie ? BG_DRAW : p->side ? BG_WIN1 : BG_WIN0;
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

static void place_text(int point, char *out)
{
    if (point == BG_BAR) strcpy(out, "bar");
    else if (point == BG_OFF) strcpy(out, "off");
    else snprintf(out, 4, "%d", point < 0 ? 0 : point > 24 ? 24 : point);
}

int bg_event_text(const bgame *g, int i, char *out, int cap)
{
    int k = i - g->forgot;
    if (!out || cap < 12 || k < 0 || k >= g->n) return 0;
    const bevent *e = &g->ev[k];
    if (e->kind == BG_EV_THROW) {
        const bpos *p = &g->pos[k + 1];
        snprintf(out, (size_t)cap, "%d%d:", p->die[0], p->die[1]);
        return 1;
    }
    char a[4], b[4];
    place_text(e->m.from, a);
    place_text(e->m.to, b);
    snprintf(out, (size_t)cap, "%s/%s", a, b);
    return 1;
}
