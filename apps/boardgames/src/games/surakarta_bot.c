/*
 * Surakarta's bot (bots.h): alpha-beta (ai.h) over the rules' position, with quiescence over the captures (runs round
 * the loops). The evaluation: the pieces (the game's end by the move limit or a repetition counts them too), the
 * enemy pieces the side to move can take now, its own pieces the other can take (more when nothing guards them: a
 * piece of its own that would take the taker back), and the mobility.
 * Runs on the worker thread: it copies the position at entry and touches nothing else (bots.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Its moves are the game's notation ("b2-c3" a step, "b2xe5" a capture), played by surakarta_show().
 */
#include <stdio.h>

#include "bots.h"
#include "games/capture_bots.h"
#include "games/surakarta.h"

static int has_step(const skpos *p)             /* a piece of the side to move next to an empty point */
{
    for (int pt = 0; pt < SK_POINTS; pt++) {
        if (p->at[pt] != p->side) continue;
        for (int d = 0; d < 8; d++) {
            int x = sk_x(pt) + sk_dx[d], y = sk_y(pt) + sk_dy[d];
            if (x >= 0 && x < SK_SIDE && y >= 0 && y < SK_SIDE && p->at[x + SK_SIDE * y] < 0) return 1;
        }
    }
    return 0;
}

typedef struct {
    skpos p;                                    /* first: the adapter reads the position through st */
    cpb_rep rep;                                /* the positions before it, since the last capture */
} sks;

/* ---- the adapter ------------------------------------------------------------------------------------------------- */
static int a_moves(const void *st, int *out)
{
    skmove l[SK_MAX_MOVES];
    int n = sk_legal(st, l);
    for (int i = 0; i < n; i++) out[i] = l[i].from | l[i].to << 6;
    return n;
}

static uint64_t a_hash(const void *st);

static void a_make(void *st, int m)
{
    sks *s = st;
    cpb_rep_push(&s->rep, a_hash(st));
    sk_make(&s->p, (skmove){(int8_t)(m & 63), (int8_t)(m >> 6), -1, 0, 0, 0});
    if (!s->p.quiet) cpb_rep_clear(&s->rep);
}

static int a_side(const void *st) { return ((const skpos *)st)->side; }

static int a_over(const void *st, int *score)
{
    const skpos *p = st;
    int me = p->side, d = p->left[me] - p->left[!me];
    if (!p->left[me]) { *score = -ai_win(p->left[!me]); return 1; }
    if (p->quiet >= SK_QUIET || cpb_rep_seen(&((const sks *)st)->rep, a_hash(st))) {
        /* the move limit, a position again: the most pieces win */
        *score = d > 0 ? ai_win(d) : d < 0 ? -ai_win(-d) : 0;
        return 1;
    }
    if (has_step(p)) return 0;
    skmove l[SK_MAX_MOVES];
    if (sk_legal(p, l)) return 0;
    *score = -ai_win(0);                        /* no move: lost */
    return 1;
}

static uint64_t a_hash(const void *st)
{
    const skpos *p = st;
    return cpb_hash(p->at, sizeof p->at, p->side);
}

/* the pieces `side` can take (bits), and how many moves it has */
static uint64_t targets(const skpos *p, int side, int *moves)
{
    skpos q = *p;
    q.side = (uint8_t)side;
    skmove l[SK_MAX_MOVES];
    int n = sk_legal(&q, l);
    uint64_t t = 0;
    for (int i = 0; i < n; i++)
        if (l[i].circuit >= 0) t |= 1ull << l[i].to;
    *moves = n;
    return t;
}

/* the side to move's view; 100 a piece */
static int a_eval(const void *st)
{
    const skpos *p = st;
    int me = p->side, foe = !me, mm, mf;
    uint64_t mine = targets(p, me, &mm), theirs = targets(p, foe, &mf);
    int v = 100 * (p->left[me] - p->left[foe]);
    v += 2 * (mm - mf);
    /* what I can take now: about one piece at most (the other answers), a little for the rest */
    int t = __builtin_popcountll(mine);
    if (t) v += 45 + 10 * (t - 1);
    /* what they can take next: each piece of mine they attack, worse unguarded (I could not take the taker back:
     * roughly, none of my runs reaches that point) */
    for (uint64_t a = theirs; a; a &= a - 1) {
        int pt = __builtin_ctzll(a);
        skpos q = *p;                           /* guarded: with an enemy piece there, could I take it? */
        q.at[pt] = (int8_t)foe;
        int dummy;
        v -= targets(&q, me, &dummy) >> pt & 1 ? 12 : 30;
    }
    return v;
}

static int a_order(const void *st, int m)       /* captures first */
{
    const skpos *p = st;
    return p->at[m >> 6] >= 0 ? 100 : 0;
}

static int a_noisy(const void *st, int *out)
{
    const skpos *p = st;
    skmove l[SK_MAX_MOVES];
    int n = sk_legal(p, l), k = 0;
    for (int i = 0; i < n; i++)
        if (l[i].circuit >= 0) out[k++] = l[i].from | l[i].to << 6;
    return k;
}

static const ai_game ADAPTER = {
    .size = sizeof(sks), .moves = a_moves, .make = a_make, .side = a_side, .over = a_over, .eval = a_eval,
    .hash = a_hash, .order = a_order, .noisy = a_noisy, .unit = 100,
};

static const cpb_levels LEVELS = {
    .easy_depth = 2, .easy_nodes = 3000, .easy_noise = 120,
    .medium_depth = 4, .medium_nodes = 30000, .medium_noise = 25,
    .hard_nodes = 450000, .tt_bits = 18,
};

/* the move in the game g (its side to move), at a level: from | to << 6; -1: none. g is read at once (its position
 * and the ones before it that may come again). */
int skb_choose(const skgame *g, int level, uint64_t seed, ai_stats *stats)
{
    sks s = {.p = *sk_pos(g)};
    for (int i = g->ply - 1; i >= 0 && s.rep.n < CPB_REP && g->pos[i + 1].quiet; i--)
        cpb_rep_push(&s.rep, a_hash(&g->pos[i]));
    s.rep.ply = 0;                              /* the root: never a repetition itself */
    int m, score;
    if (a_over(&s, &score)) return -1;
    return cpb_turn(&ADAPTER, &s, &LEVELS, level, seed, &m, 1, stats) ? m : -1;
}

static int think(int seat, int level, char *move, int cap)
{
    const skgame *g = surakarta_live();         /* read at entry by skb_choose() and here: nothing else is */
    const skpos *p = sk_pos(g);
    if (p->side != seat || cap < 6) return 0;
    int m = skb_choose(g, level, ai_session_seed(), NULL);
    if (m < 0 || ai_cancelled()) return 0;
    int from = m & 63, to = m >> 6;
    snprintf(move, (size_t)cap, "%c%c%c%c%c", 'a' + sk_x(from), '1' + sk_y(from), p->at[to] >= 0 ? 'x' : '-',
             'a' + sk_x(to), '1' + sk_y(to));
    return 1;
}

static const bot surakarta_bot = {.think = think, .play = surakarta_show};
REGISTER_BOT(surakarta, &surakarta_bot)
