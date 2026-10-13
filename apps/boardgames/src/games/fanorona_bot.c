/*
 * Fanorona's bot (bots.h): alpha-beta (ai.h) over the rules' position, captures first and quiescence over them. A
 * turn of captures is a step a move, the same player still to move, and going on is a choice: stopping (FN_STOP) is
 * a move like the others, so the search weighs each further capture against ending there. The turn is searched step
 * by step and written as one word ("d3-e3w-e4a"; a word that ends stops the chain). The evaluation: the pieces (most
 * of it, and more as they get few: the last pieces decide), the strong points (where the diagonals cross: more ways
 * to move and to take), and the room to move.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation (fanorona.h), played by fanorona_show().
 */
#include <stdio.h>

#include "bots.h"
#include "games/capture_bots.h"
#include "games/fanorona.h"

static int encode(fnmove m) { return m.from | m.to << 6 | m.kind << 12; }
static fnmove decode(int m) { return (fnmove){(int8_t)(m & 63), (int8_t)(m >> 6 & 63), (int8_t)(m >> 12)}; }

typedef struct {
    fnpos p;                                    /* first: the adapter reads the position through st */
    cpb_rep rep;                                /* the positions before it, since the last capture */
} fns;

static uint64_t a_hash(const void *st);

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    const fnpos *p = st;
    fnmove l[FN_MAX_MOVES];
    int n = fn_legal(p, l);
    for (int i = 0; i < n; i++) out[i] = encode(l[i]);
    if (p->chain >= 0) out[n++] = encode((fnmove){p->chain, p->chain, FN_STOP});   /* going on is a choice */
    return n;
}

static void a_make(void *st, int m)
{
    fns *s = st;
    cpb_rep_push(&s->rep, a_hash(st));
    fn_make(&s->p, decode(m));
    if (!s->p.quiet) cpb_rep_clear(&s->rep);
}
static int a_side(const void *st) { return ((const fnpos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const fnpos *p = st;
    if (p->chain >= 0) return 0;
    if (!p->left[p->side]) { *score = -ai_win(p->left[!p->side]); return 1; }
    if (p->quiet >= FN_QUIET || cpb_rep_seen(&((const fns *)st)->rep, a_hash(st))) { *score = 0; return 1; }
    fnmove l[FN_MAX_MOVES];
    if (fn_legal(p, l)) return 0;
    *score = -ai_win(0);                        /* no move: lost */
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const fnpos *p = st;
    uint64_t h = cpb_hash(p->at, sizeof p->at, (uint64_t)p->side | (uint64_t)(p->chain + 1) << 1);
    return p->chain >= 0 ? ai_mix(h, p->seen * 8 + (uint64_t)(p->dir + 1)) : h;
}

/* the side to move's view; 100 a piece */
static int a_eval(const void *st)
{
    const fnpos *p = st;
    int me = p->side, mine = p->left[me], theirs = p->left[!me];
    int v = 100 * (mine - theirs);
    if (mine + theirs < 20) v += 600 * (mine - theirs) / (mine + theirs);   /* few left: each counts more */
    for (int i = 0; i < FN_POINTS; i++) {
        if (p->at[i] < 0) continue;
        int w = ((fn_x(i) + fn_y(i)) & 1) ? 0 : 4, room = 0;
        for (int d = 0; d < 8; d++) {
            int to = fn_next(i, d);
            room += to >= 0 && p->at[to] < 0;
        }
        v += (p->at[i] == me ? 1 : -1) * (w + room);
    }
    return v;
}

static int a_order(const void *st, int m)       /* captures first, the longer line first; stopping last */
{
    const fnpos *p = st;
    fnmove x = decode(m);
    if (x.kind == FN_PAIKA) return 0;
    if (x.kind == FN_STOP) return -10;
    int8_t t[FN_MAX_TAKE];
    return 100 + 10 * fn_taken(p, x, t);
}

static int a_noisy(const void *st, int *out)
{
    const fnpos *p = st;
    fnmove l[FN_MAX_MOVES];
    int n = fn_legal(p, l);
    if (p->chain < 0 && (!n || l[0].kind == FN_PAIKA)) return 0;   /* captures are compulsory: all, or none */
    for (int i = 0; i < n; i++) out[i] = encode(l[i]);
    if (p->chain >= 0) out[n++] = encode((fnmove){p->chain, p->chain, FN_STOP});
    return n;
}

static const ai_game ADAPTER = {
    .size = sizeof(fns), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};

static const cpb_levels LEVELS = {
    .easy_depth = 2, .easy_nodes = 4000, .easy_noise = 100,
    .medium_depth = 4, .medium_nodes = 40000, .medium_noise = 25,
    .hard_nodes = 400000, .tt_bits = 18, .chains = 1,
};

/* the whole turn of the side to move in the game g (captures, maybe a stop), at a level: its moves (encoded) in
 * out; their count, 0: none. g is read at once (its position and the ones before it that may come again). */
int fnb_turn(const fngame *g, int level, uint64_t seed, int *out, int max, ai_stats *stats)
{
    fns s = {.p = *fn_pos(g)};
    for (int i = g->ply - 1; i >= 0 && s.rep.n < CPB_REP && g->pos[i + 1].quiet; i--)
        cpb_rep_push(&s.rep, a_hash(&g->pos[i]));
    s.rep.ply = 0;                              /* the root: never a repetition itself */
    int score;
    if (a_over(&s, &score)) return 0;
    return cpb_turn(&ADAPTER, &s, &LEVELS, level, seed, out, max, stats);
}

/* a turn in the notation, one word: "d2-e3", "d3-e3w-e4a" (a stop: the word ends) */
void fnb_write(const int *moves, int n, char *out, int cap)
{
    int k = 0;
    out[0] = 0;
    for (int i = 0; i < n && k + 6 < cap; i++) {
        fnmove x = decode(moves[i]);
        if (x.kind == FN_STOP) break;
        if (!i) k += snprintf(out + k, (size_t)(cap - k), "%c%c", 'a' + fn_x(x.from), '1' + fn_y(x.from));
        k += snprintf(out + k, (size_t)(cap - k), "-%c%c", 'a' + fn_x(x.to), '1' + fn_y(x.to));
        if (x.kind != FN_PAIKA) out[k++] = x.kind == FN_APPROACH ? 'a' : 'w', out[k] = 0;
    }
}

static int think(int seat, int level, char *move, int cap)
{
    const fngame *g = fanorona_live();  /* read at entry by fnb_turn(): nothing else is */
    int mv[CPB_TURN], chain = fn_pos(g)->chain;
    if (fn_pos(g)->side != seat || cap < 8) return 0;
    int n = fnb_turn(g, level, ai_session_seed(), mv, CPB_TURN < (cap - 3) / 4 ? CPB_TURN : (cap - 3) / 4, NULL);
    if (!n || ai_cancelled()) return 0;
    if (chain >= 0 && decode(mv[0]).kind == FN_STOP) { snprintf(move, (size_t)cap, "stop"); return 1; }
    fnb_write(mv, n, move, cap);
    return 1;
}

static const bot fanorona_bot = {.think = think, .play = fanorona_show};
REGISTER_BOT(fanorona, &fanorona_bot)
