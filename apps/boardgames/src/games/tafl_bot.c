/*
 * Tafl's bot (bots.h), for either side and every board (Copenhagen 11 x 11, Tablut 9 x 9, Brandubh 7 x 7): alpha-beta
 * (ai.h) over the rules' position, with quiescence over the captures and the king's runs to safety. One evaluation,
 * from the defenders' side (the side to move's view is its sign), with what each side plays for:
 *   the defenders: the king's escape routes (a clear line to a corner, Tablut: to the edge; the defenders to move
 *   with one open have won, two open at once cannot both be shut), the squares one king's move away that would open
 *   one, the king's room, the pieces;
 *   the attackers: the king hemmed in (attackers on its sides: four take it, two away from the throne in Tablut and
 *   Brandubh), the squares next to the corners held (the king's last steps), the pieces.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("a4-d4": from, to; files a.., ranks 1..), played by tafl_show().
 */
#include <stdio.h>

#include "bots.h"
#include "games/capture_bots.h"
#include "games/tafl.h"

#define RUNNER 20000                            /* the king through next move: a near win (eval scale) */

typedef struct {
    tpos p;
    int8_t variant, n, king;                    /* the king's square (-1 taken), kept by make() */
    cpb_rep rep;                                /* the positions before it, since the last capture */
} tbs;

static uint64_t key(const tpos *p, int n) { return cpb_hash(p->sq, (size_t)n * n, p->side); }

static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};

static int step(const tbs *s, int q, int d)    /* the square next to q in direction d; -1 off the board */
{
    int x = q % s->n + DX[d], y = q / s->n + DY[d];
    return x < 0 || y < 0 || x >= s->n || y >= s->n ? -1 : y * s->n + x;
}

/* the escape squares the king on k reaches in one move (along clear lines; it may cross the empty throne) */
static int routes(const tbs *s, int k)
{
    int r = 0;
    for (int d = 0; d < 4; d++)
        for (int t = step(s, k, d); t >= 0 && !s->p.sq[t]; t = step(s, t, d))
            if (tf_escape(s->variant, t)) { r++; break; }
    return r;
}

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    const tbs *s = st;
    tmove l[TF_MAX_MOVES];
    int n = tf_legal(&s->p, s->variant, l);
    for (int i = 0; i < n; i++) out[i] = l[i].from | l[i].to << 7;
    return n;
}

static void a_make(void *st, int m)
{
    tbs *s = st;
    tmove x = {(uint8_t)(m & 127), (uint8_t)(m >> 7), 0, {0}};
    cpb_rep_push(&s->rep, key(&s->p, s->n));
    tf_make(&s->p, s->variant, &x);
    if (x.ncap) cpb_rep_clear(&s->rep);
    if (x.from == s->king) s->king = (int8_t)x.to;
    for (int i = 0; i < x.ncap; i++)
        if (x.cap[i] == s->king) s->king = -1;
}

static int a_side(const void *st) { return ((const tbs *)st)->p.side; }

/* 1: a piece of the side to move next to an empty square it may stop on (so it has a move) */
static int has_step(const tbs *s)
{
    for (int q = 0; q < s->n * s->n; q++) {
        int pc = s->p.sq[q];
        if (!pc || TF_OWNER(pc) != s->p.side) continue;
        for (int d = 0; d < 4; d++) {
            int t = step(s, q, d);
            if (t >= 0 && !s->p.sq[t] && (pc == TF_KING || !tf_restricted(s->variant, t))) return 1;
        }
    }
    return 0;
}

static int a_over(const void *st, int *score)
{
    const tbs *s = st;
    int defenders = s->p.side == 1, won = s->king < 0 ? 0 : tf_escape(s->variant, s->king) ? 1 : -1;
    if (won >= 0) { *score = (won == 1) == defenders ? ai_win(0) : -ai_win(0); return 1; }
    if (cpb_rep_seen(&s->rep, key(&s->p, s->n))) { *score = 0; return 1; }   /* a position again: a draw */
    if (has_step(s)) return 0;
    tmove l[TF_MAX_MOVES];
    if (tf_legal(&s->p, s->variant, l)) return 0;
    *score = -ai_win(0);                        /* no move: lost */
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const tbs *s = st;
    return key(&s->p, s->n);
}

/* the defenders' view; 100 an attacker */
static int defenders_view(const tbs *s)
{
    const tpos *p = &s->p;
    int n = s->n, k = s->king, att = 0, def = 0;
    for (int q = 0; q < n * n; q++) att += p->sq[q] == TF_ATT, def += p->sq[q] == TF_DEF;
    int v = 170 * def - 100 * att;
    /* the king: its routes now, the squares one move away that would open one, its room */
    int r = routes(s, k), room = 0, near = 0;
    if (r && p->side == 1) return RUNNER;       /* it runs now */
    if (r >= 2) return RUNNER / 2;              /* two at once: one stays open */
    v += 160 * r;
    tbs t = *s;
    t.p.sq[k] = TF_EMPTY;
    for (int d = 0; d < 4; d++)
        for (int q = step(s, k, d); q >= 0 && !p->sq[q]; q = step(s, q, d)) {
            room++;
            t.p.sq[q] = TF_KING;
            near += routes(&t, q) > 0;
            t.p.sq[q] = TF_EMPTY;
        }
    v += 4 * room + 45 * near;
    /* hemmed in: attackers (or the empty throne) on its sides */
    int th = tf_throne(s->variant), sides = 0;
    for (int d = 0; d < 4; d++) {
        int q = step(s, k, d);
        sides += q >= 0 && (p->sq[q] == TF_ATT || (q == th && !p->sq[q]));
    }
    v -= (s->variant == TF_HNEFATAFL ? 25 : 45) * sides;
    if (s->variant != TF_TABLUT) {              /* the corners' approaches held by attackers */
        int guard = 0;
        for (int c = 0; c < 4; c++) {
            int cx = c & 1 ? n - 1 : 0, cy = c & 2 ? n - 1 : 0, sx = cx ? -1 : 1, sy = cy ? -1 : 1;
            int g[3] = {(cy + sy) * n + cx + sx, cy * n + cx + 2 * sx, (cy + 2 * sy) * n + cx};
            for (int i = 0; i < 3; i++) guard += p->sq[g[i]] == TF_ATT;
        }
        v -= 12 * guard;
    }
    return v;
}

static int a_eval(const void *st)
{
    const tbs *s = st;
    int v = defenders_view(s);
    return s->p.side == 1 ? v : -v;
}

/* the enemy pieces next to `to` that a piece of `me` stopping there would take (the king aside); for the order */
static int takes(const tbs *s, int from, int to)
{
    int me = TF_OWNER(s->p.sq[from]), c = 0;
    for (int d = 0; d < 4; d++) {
        int a = step(s, to, d), b = a >= 0 ? step(s, a, d) : -1;
        if (b < 0 || !s->p.sq[a] || s->p.sq[a] == TF_KING || TF_OWNER(s->p.sq[a]) == me) continue;
        int pb = b == from ? TF_EMPTY : s->p.sq[b];
        c += pb ? TF_OWNER(pb) == me : tf_restricted(s->variant, b);
    }
    return c;
}

static int a_order(const void *st, int m)
{
    const tbs *s = st;
    int from = m & 127, to = m >> 7;
    if (from == s->king) return tf_escape(s->variant, to) ? 1000 : routes(s, to) ? 300 : 0;
    int c = takes(s, from, to);
    if (c) return 200 * c;
    if (s->p.side == 0 && s->king >= 0) {      /* attackers: next to the king, or onto its lines */
        for (int d = 0; d < 4; d++)
            if (step(s, s->king, d) == to) return 50;
    }
    return 0;
}

/* quiescence: the captures, and the king's moves to safety or onto a route */
static int a_noisy(const void *st, int *out)
{
    const tbs *s = st;
    tmove l[TF_MAX_MOVES];
    int n = tf_legal(&s->p, s->variant, l), k = 0;
    for (int i = 0; i < n; i++) {
        int from = l[i].from, to = l[i].to;
        if (from == s->king ? tf_escape(s->variant, to) : takes(s, from, to) > 0) out[k++] = from | to << 7;
    }
    return k;
}

static const ai_game ADAPTER = {
    .size = sizeof(tbs), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};

static const cpb_levels LEVELS = {
    .easy_depth = 2, .easy_nodes = 6000, .easy_noise = 100,
    .medium_depth = 3, .medium_nodes = 30000, .medium_noise = 25,
    .hard_nodes = 250000, .tt_bits = 18,
};

/* the move in the game g (its side to move), at a level: from | to << 7; -1: none. g is read at once (its position
 * and the ones before it that may come again). */
int tfb_choose(const tgame *g, int level, uint64_t seed, ai_stats *stats)
{
    const tpos *p = tf_pos(g);
    int v = g->variant;
    tbs s = {.p = *p, .variant = (int8_t)v, .n = (int8_t)tf_size(v), .king = (int8_t)tf_king(p, v)};
    for (int i = g->ply - 1; i >= 0 && s.rep.n < CPB_REP && !g->move[i].ncap; i--)
        cpb_rep_push(&s.rep, key(&g->pos[i], s.n));
    s.rep.ply = 0;                              /* the root: never a repetition itself */
    int m;
    if (s.king < 0) return -1;
    return cpb_turn(&ADAPTER, &s, &LEVELS, level, seed, &m, 1, stats) ? m : -1;
}

static int think(int seat, int level, char *move, int cap)
{
    const tgame *g = tafl_live();               /* read at entry by tfb_choose() and here: nothing else is */
    int n = tf_size(g->variant);
    if (tf_pos(g)->side != seat || cap < 8) return 0;
    int m = tfb_choose(g, level, ai_session_seed(), NULL);
    if (m < 0 || ai_cancelled()) return 0;
    int a = m & 127, b = m >> 7;
    snprintf(move, (size_t)cap, "%c%d-%c%d", 'a' + a % n, a / n + 1, 'a' + b % n, b / n + 1);
    return 1;
}

static const bot tafl_bot = {.think = think, .play = tafl_show};
REGISTER_BOT(tafl, &tafl_bot)
