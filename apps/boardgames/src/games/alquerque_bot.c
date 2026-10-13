/*
 * Alquerque's bot (bots.h): alpha-beta (ai.h) over the rules' position, captures first and quiescence over them (a
 * chain of captures is a jump a move, the same player still to move: the turn is searched jump by jump and written
 * whole, "b2xd4xb4"). The evaluation: the pieces, the steps each side has left (a piece never steps back: the side
 * that runs out of steps first must give pieces away), pieces backed by a piece of their own or the edge behind them
 * (no jump over them), and the centre.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("c2-c3" a step, "b2xd4" a capture, "b2xd4xb4" a chain), played by
 * alquerque_show().
 */
#include <stdio.h>

#include "bots.h"
#include "games/alquerque.h"
#include "games/capture_bots.h"

static int encode(aqmove m) { return m.from | m.to << 5 | (m.over + 1) << 10; }
static aqmove decode(int m) { return (aqmove){(int8_t)(m & 31), (int8_t)(m >> 5 & 31), (int8_t)((m >> 10) - 1)}; }

typedef struct {
    aqpos p;                                    /* first: the adapter reads the position through st */
    cpb_rep rep;                                /* the positions before it, since the last capture */
} aqs;

static uint64_t a_hash(const void *st);

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    aqmove l[AQ_MAX_MOVES];
    int n = aq_legal(st, l);
    for (int i = 0; i < n; i++) out[i] = encode(l[i]);
    return n;
}

static void a_make(void *st, int m)
{
    aqs *s = st;
    cpb_rep_push(&s->rep, a_hash(st));
    aq_make(&s->p, decode(m));
    if (!s->p.quiet) cpb_rep_clear(&s->rep);
}
static int a_side(const void *st) { return ((const aqpos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const aqpos *p = st;
    if (p->chain >= 0) return 0;
    if (!p->left[p->side]) { *score = -ai_win(p->left[!p->side]); return 1; }
    if (p->quiet >= AQ_QUIET || cpb_rep_seen(&((const aqs *)st)->rep, a_hash(st))) { *score = 0; return 1; }
    aqmove l[AQ_MAX_MOVES];
    if (aq_legal(p, l)) return 0;
    *score = -ai_win(0);                        /* no move: lost */
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const aqpos *p = st;
    return cpb_hash(p->at, sizeof p->at, (uint64_t)p->side | (uint64_t)(p->chain + 1) << 1);
}

/* one player's position: its steps (forwards or sideways, to an empty point), its pieces safe from a jump along every
 * line (backed by its own piece or the edge, or nothing next to it in front), the centre */
static int side_view(const aqpos *p, int pl)
{
    int v = 0, ahead = pl ? -1 : 1;
    for (int i = 0; i < AQ_POINTS; i++) {
        if (p->at[i] != pl) continue;
        int x = aq_x(i), y = aq_y(i), safe = 1;
        for (int d = 0; d < 8; d++) {
            int to = aq_next(i, d);
            if (to < 0) continue;
            if (p->at[to] < 0 && aq_dy[d] != -ahead) v += 3;   /* a step */
            int back = aq_next(i, (d + 4) % 8);
            if (p->at[to] == !pl && back >= 0 && p->at[back] < 0) safe = 0;   /* jumpable from `to` */
        }
        v += safe ? 10 : 0;
        v += 4 - (x > 2 ? x - 2 : 2 - x) - (y > 2 ? y - 2 : 2 - y);
    }
    return v;
}

/* the side to move's view; 100 a piece */
static int a_eval(const void *st)
{
    const aqpos *p = st;
    int me = p->side;
    return 100 * (p->left[me] - p->left[!me]) + side_view(p, me) - side_view(p, !me);
}

static int a_order(const void *st, int m)       /* captures first */
{
    (void)st;
    return m >> 10 ? 100 : 0;
}

static int a_noisy(const void *st, int *out)
{
    const aqpos *p = st;
    aqmove l[AQ_MAX_MOVES];
    int n = aq_legal(p, l);
    if (!n || l[0].over < 0) return 0;          /* captures are compulsory: all of them, or none */
    for (int i = 0; i < n; i++) out[i] = encode(l[i]);
    return n;
}

static const ai_game ADAPTER = {
    .size = sizeof(aqs), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};

static const cpb_levels LEVELS = {
    .easy_depth = 2, .easy_nodes = 3000, .easy_noise = 100,
    .medium_depth = 4, .medium_nodes = 40000, .medium_noise = 25,
    .hard_nodes = 600000, .tt_bits = 18, .chains = 1,
};

/* the whole turn of the side to move in the game g (a chain: several captures), at a level: its moves (encoded)
 * in out; their count, 0: none. g is read at once (its position and the ones before it that may come again). */
int aqb_turn(const aqgame *g, int level, uint64_t seed, int *out, int max, ai_stats *stats)
{
    aqs s = {.p = *aq_pos(g)};
    for (int i = g->ply - 1; i >= 0 && s.rep.n < CPB_REP && g->pos[i + 1].quiet; i--)
        cpb_rep_push(&s.rep, a_hash(&g->pos[i]));
    s.rep.ply = 0;                              /* the root: never a repetition itself */
    int score;
    if (a_over(&s, &score)) return 0;
    return cpb_turn(&ADAPTER, &s, &LEVELS, level, seed, out, max, stats);
}

/* a turn in the notation: "c2-c3", "b2xd4xb4" */
void aqb_write(const int *moves, int n, char *out, int cap)
{
    int k = 0;
    for (int i = 0; i < n && k + 6 < cap; i++) {
        aqmove x = decode(moves[i]);
        if (!i) k += snprintf(out + k, (size_t)(cap - k), "%c%c", 'a' + aq_x(x.from), '1' + aq_y(x.from));
        k += snprintf(out + k, (size_t)(cap - k), "%c%c%c", x.over >= 0 ? 'x' : '-',
                      'a' + aq_x(x.to), '1' + aq_y(x.to));
    }
}

static int think(int seat, int level, char *move, int cap)
{
    const aqgame *g = alquerque_live();  /* read at entry by aqb_turn(): nothing else is */
    int mv[CPB_TURN];
    if (aq_pos(g)->side != seat || cap < 6) return 0;
    int n = aqb_turn(g, level, ai_session_seed(), mv, CPB_TURN < (cap - 2) / 3 ? CPB_TURN : (cap - 2) / 3, NULL);
    if (!n || ai_cancelled()) return 0;
    aqb_write(mv, n, move, cap);
    return 1;
}

static const bot alquerque_bot = {.think = think, .play = alquerque_show};
REGISTER_BOT(alquerque, &alquerque_bot)
