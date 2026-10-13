/*
 * Scopa's bot (bots.h), 2 players or 4 in partnerships: information-set Monte Carlo tree search (ai_mcts) over the
 * round, each iteration dealing the cards the seat has not seen (the pack minus its hand, the table and every card
 * taken this round) at random into the others' hands and the stock; heuristic playouts to the round's end. Runs on
 * the worker thread: it copies the game at entry and reads only what its seat may know. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The search's moves are the rules' own: a card and the table cards it takes (each possible capture). The playouts:
 * sweep the table when a card can (a scopa); else the capture worth most (the settebello, the coins, the 7s and 6s of
 * the primiera, the most cards); with nothing to take, the card whose discard leaves the opponents no sweep and gives
 * away the least. The reward: the round's points of the seat's side against the other side's. The next round waits
 * for a human tap (actor() -1); think() answers "n" there all the same.
 * Its moves are the game's notation ("7d:3c4s", "Rb"), played by scopa_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/cardbot.h"
#include "games/scopa.h"

#define SCOPA_HARD_ITERS 50000                  /* about 0.3 s a move on a PC */

typedef struct { scpos p; uint8_t me; } sc_st;

static void s_apply(scpos *p, uint32_t a)
{
    scgame g;
    g.pos = *p;
    g.n = 0;
    if (scopa_play(&g, a)) *p = g.pos;
}

static int s_moves(const void *s, int *out)
{
    const scpos *p = &((const sc_st *)s)->p;
    if (p->phase != SC_PLAY) return 0;
    int k = 0, pl = p->turn;
    for (int i = 0; i < p->n[pl]; i++) {
        int c = p->hand[pl][i];
        if (c >= CDB_HIDDEN) continue;
        uint32_t m[64];
        int n = scopa_captures(p, c, m, 64);
        if (!n) out[k++] = (int)SCA(c, 0);
        for (int j = 0; j < n && k < AI_MAX_MOVES; j++) out[k++] = (int)SCA(c, m[j]);
    }
    return k;
}

static void s_make(void *s, int m) { s_apply(&((sc_st *)s)->p, (uint32_t)m); }
static int s_side(const void *s) { return ((const sc_st *)s)->p.turn; }

static int s_result(const void *s, float *rw)
{
    const scpos *p = &((const sc_st *)s)->p;
    if (p->phase == SC_PLAY) return 0;
    float d = (float)(p->pts[0] - p->pts[1]), x = 0.5f + d / 10.0f;
    x = x < 0 ? 0 : x > 1 ? 1 : x;
    for (int q = 0; q < p->players; q++) rw[q] = scopa_side(p, q) ? 1 - x : x;
    return 1;
}

/* ---- the playouts' heuristic ------------------------------------------------------------------------------------ */
static int worth(int c)                         /* a card's worth to its taker */
{
    int w = 10;
    if (CARD_SUIT(c) == IT_COINS) w += 9;
    if (c == CARD(IT_COINS, 7)) w += 60;
    if (CARD_RANK(c) == 7) w += 14;
    if (CARD_RANK(c) == 6) w += 6;
    return w;
}

static int s_policy(const scpos *p, ai_rng *r, int eps)
{
    int pl = p->turn, best = -1;
    long bs = -(1L << 30);
    int sum = 0;
    for (int t = 0; t < p->ntable; t++) sum += scopa_value(p->table[t]);
    for (int i = 0; i < p->n[pl]; i++) {
        int c = p->hand[pl][i];
        uint32_t m[64];
        int n = scopa_captures(p, c, m, 64);
        if (!n) {                               /* a discard: no sweep for the next one, little given away */
            long sc = -worth(c) * 3 + (sum + scopa_value(c) <= 10 ? -60 : 0) + scopa_value(c);
            if (eps && ai_rand_n(r, 100) < eps) sc += ai_rand_n(r, 200);
            if (sc > bs) bs = sc, best = (int)SCA(c, 0);
            continue;
        }
        for (int j = 0; j < n; j++) {
            long sc = 200 + worth(c);
            int cnt = 0;
            for (int t = 0; t < p->ntable; t++)
                if (m[j] >> t & 1) sc += worth(p->table[t]), cnt++;
            if (cnt == p->ntable) sc += 400;    /* a scopa */
            if (eps && ai_rand_n(r, 100) < eps) sc += ai_rand_n(r, 200);
            if (sc > bs) bs = sc, best = (int)SCA(c, m[j]);
        }
    }
    return best < 0 ? AI_NONE : best;
}

static int s_playout(const void *s, ai_rng *r) { return s_policy(&((const sc_st *)s)->p, r, 8); }

/* ---- the view and the deal of the unseen ------------------------------------------------------------------------ */
static void s_determinise(void *s, int seat, ai_rng *r)
{
    (void)seat;
    sc_st *st = s;
    scpos *p = &st->p;
    uint8_t pack[IT_CARDS], seen[160], pool[IT_CARDS];
    int npack = cards_pack(PACK_40, pack), ns = 0;
    for (int q = 0; q < p->players; q++) {
        for (int i = 0; i < p->n[q]; i++) seen[ns++] = p->hand[q][i];
        for (int i = 0; i < p->ntook[q]; i++) seen[ns++] = p->took[q][i];
    }
    for (int t = 0; t < p->ntable; t++) seen[ns++] = p->table[t];
    int np = cdb_unseen(pack, npack, seen, ns, pool);
    cdb_shuffle(pool, np, r);
    for (int q = 0; q < p->players; q++) cdb_fill(p->hand[q], p->n[q], pool, &np);
    cdb_fill(p->stock, p->nstock, pool, &np);
    p->seed = ai_rand(r);
}

static void s_view(const scpos *live, int me, sc_st *v)
{
    v->p = *live;
    v->me = (uint8_t)me;
    for (int q = 0; q < live->players; q++)
        if (q != me) memset(v->p.hand[q], CDB_HIDDEN, live->n[q]);
    memset(v->p.stock, CDB_HIDDEN, live->nstock);
    v->p.seed = 0;
}

uint32_t scob_choose(const scpos *live, int seat, int level, uint64_t seed, ai_stats *stats)
{
    if (stats) memset(stats, 0, sizeof *stats);
    if (live->phase == SC_ROUND) return SCA_NEXT;
    if (live->phase != SC_PLAY || live->turn != seat) return 0;
    sc_st *v = malloc(sizeof *v);
    if (!v) return scopa_auto(live);
    s_view(live, seat, v);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)live->nstock * 131 + live->ntable * 7 + live->n[seat]));
    int mv[AI_MAX_MOVES], n = s_moves(v, mv), m = n ? mv[0] : AI_NONE;
    if (n > 1) {
        if (cdb_slip(level, &r)) m = s_policy(&v->p, &r, CDB_SLIP_EPS);
        else {
            ai_game G = {
                .size = sizeof(sc_st), .moves = s_moves, .make = s_make, .side = s_side, .players = live->players,
                .result = s_result, .playout = s_playout, .determinise = s_determinise,
            };
            int x = cdb_search(&G, v, level, SCOPA_HARD_ITERS, &r, stats);
            if (x != AI_NONE) m = x;
        }
    }
    free(v);
    uint32_t a = m == AI_NONE ? 0 : (uint32_t)m;
    return a && scopa_legal(live, a) ? a : scopa_auto(live);
}

static int think(int seat, int level, char *move, int cap)
{
    scgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *scopa_live();                         /* the copy, at entry: nothing else is read */
    const scpos *p = &g->pos;
    uint32_t a = scob_choose(p, seat, level, ai_mix(ai_session_seed(), (uint64_t)g->n), NULL);
    int ok = a && !ai_cancelled() && cap >= 64;
    if (ok && a == SCA_NEXT) snprintf(move, (size_t)cap, "n");
    else if (ok) {
        int k = 0;
        it_name(SCA_CARD(a), move);
        k = 2;
        if (SCA_TAKE(a)) {
            move[k++] = ':';
            for (int t = 0; t < p->ntable && k + 3 < cap; t++)
                if (SCA_TAKE(a) >> t & 1) it_name(p->table[t], move + k), k += 2;
        }
        move[k] = 0;
    }
    free(g);
    return ok;
}

static int actor(void)
{
    const scpos *p = &scopa_live()->pos;
    return p->phase == SC_PLAY ? p->turn : -1;
}

static const bot scopa_bot = {.think = think, .actor = actor, .play = scopa_show};
REGISTER_BOT(scopa, &scopa_bot)
