/*
 * Koi-Koi's bot (bots.h), both lengths of game: information-set Monte Carlo tree search (ai_mcts) over the round,
 * each iteration dealing the cards the seat has not seen (the deck minus its hand, the field and every card captured)
 * at random into the other hand and the stock; heuristic playouts to the round's end. The koi-koi call is a move of
 * the search like the others: going on is weighed against stopping by the rounds played out. Runs on the worker
 * thread: it copies the game at entry and reads only what its seat may know. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The playouts: capture the field card worth most (brights, then the yaku's animals and ribbons, then the plains),
 * each card weighed more when it brings a yaku closer; with nothing to capture, lay down the card worth least to the
 * other player; stop at 7 points or more, or when the other player is close to a yaku, else call koi-koi now and
 * then. The reward: the round's points to its winner. The next round waits for a human tap (actor() -1); think()
 * answers "n" there all the same.
 * Its moves are the game's notation ("p8a", "t3c", "d", "k", "s", "n"), played by koikoi_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/cardbot.h"
#include "games/koikoi.h"

#define KK_HARD_ITERS 30000                     /* about 0.3 s a move on a PC */

typedef struct { kpos p; uint8_t me; } kk_st;

static void k_apply(kpos *p, int a)
{
    kgame g;
    g.pos = *p;
    g.n = 0;
    if (kk_play(&g, a)) *p = g.pos;
}

static int k_moves(const void *s, int *out)
{
    const kpos *p = &((const kk_st *)s)->p;
    int k = 0, pl = p->turn;
    switch (p->phase) {
    case KP_PLAY:
        for (int i = 0; i < p->nh[pl]; i++)
            if (p->hand[pl][i] < KK_CARDS) out[k++] = KA_PLAY | p->hand[pl][i];
        return k;
    case KP_CHOOSE: {
        int sl[4], n = kk_matches(p, p->held, sl);
        for (int i = 0; i < n; i++) out[k++] = KA_TAKE | p->field[sl[i]];
        return k;
    }
    case KP_DRAW: out[0] = KA_DRAW; return p->ndeck > 0;
    case KP_DECIDE: out[0] = KA_STOP, out[1] = KA_KOI; return 2;
    default: return 0;
    }
}

static void k_make(void *s, int m) { k_apply(&((kk_st *)s)->p, m); }
static int k_side(const void *s) { return ((const kk_st *)s)->p.turn; }

static int k_result(const void *s, float *rw)
{
    const kpos *p = &((const kk_st *)s)->p;
    if (p->phase != KP_ROUND && p->phase != KP_OVER) return 0;
    rw[0] = rw[1] = 0.5f;
    if (p->res.winner < 2) {
        float x = 0.6f + (float)p->res.points / 30.0f;
        x = x > 1 ? 1 : x;
        rw[p->res.winner] = x;
        rw[1 - p->res.winner] = 1 - x;
    }
    return 1;
}

/* ---- the playouts' heuristic ------------------------------------------------------------------------------------ */
static int value(int c)                         /* a card's worth to its taker */
{
    switch (kk_type(c)) {
    case HT_BRIGHT: return c == KK_RAINMAN ? 14 : 20;
    case HT_ANIMAL: return c == KK_SAKE ? 16 : c == KK_BOAR || c == KK_DEER || c == KK_BUTTERFLIES ? 11 : 6;
    case HT_RIBBON: return kk_ribbon(c) == HR_RED ? 5 : 9;
    default: return 2;
    }
}

/* more when the taker's captures make it a yaku (the yaku's points gained) */
static int gain(const kpos *p, int pl, int a, int b)
{
    uint8_t cap[KK_CARDS + 4];
    int n = p->ncap[pl];
    memcpy(cap, p->cap[pl], (size_t)n);
    int before = kk_yaku(cap, n, NULL);
    cap[n++] = (uint8_t)a;
    if (b < KK_CARDS) cap[n++] = (uint8_t)b;
    return (kk_yaku(cap, n, NULL) - before) * 12 + value(a) + (b < KK_CARDS ? value(b) : 0);
}

static int k_policy(const kpos *p, ai_rng *r, int eps)
{
    int pl = p->turn, sl[4];
    switch (p->phase) {
    case KP_PLAY: {
        int best = -1;
        long bs = -(1L << 30);
        for (int i = 0; i < p->nh[pl]; i++) {
            int c = p->hand[pl][i], n = kk_matches(p, c, sl);
            long sc;
            if (!n) sc = -value(c) * 2;
            else if (n == 3)
                sc = 100 + gain(p, pl, c, p->field[sl[0]]) + value(p->field[sl[1]]) + value(p->field[sl[2]]);
            else {
                sc = 0;
                for (int j = 0; j < n; j++) {
                    int g = gain(p, pl, c, p->field[sl[j]]);
                    if (g > sc) sc = g;
                }
                sc += 30;
            }
            if (eps && ai_rand_n(r, 100) < eps) sc += ai_rand_n(r, 60);
            if (sc > bs) bs = sc, best = c;
        }
        return best < 0 ? AI_NONE : KA_PLAY | best;
    }
    case KP_CHOOSE: {
        int n = kk_matches(p, p->held, sl), best = p->field[sl[0]];
        for (int j = 1; j < n; j++)
            if (value(p->field[sl[j]]) > value(best)) best = p->field[sl[j]];
        return KA_TAKE | best;
    }
    case KP_DRAW: return KA_DRAW;
    case KP_DECIDE: {
        int mine = kk_points(p, pl), theirs = kk_points(p, 1 - pl);
        if (mine >= 7 || theirs > 0 || p->nh[pl] <= 2 || p->koi[1 - pl]) return KA_STOP;
        return ai_rand_n(r, 100) < 35 ? KA_KOI : KA_STOP;
    }
    default: return AI_NONE;
    }
}

static int k_playout(const void *s, ai_rng *r) { return k_policy(&((const kk_st *)s)->p, r, 8); }

/* ---- the view and the deal of the unseen ------------------------------------------------------------------------ */
static void k_determinise(void *s, int seat, ai_rng *r)
{
    (void)seat;
    kk_st *st = s;
    kpos *p = &st->p;
    uint8_t seen[KK_CARDS * 3], pool[KK_CARDS], pack[KK_CARDS];
    int ns = 0;
    for (int i = 0; i < KK_CARDS; i++) pack[i] = (uint8_t)i;
    for (int q = 0; q < 2; q++) {
        for (int i = 0; i < p->nh[q]; i++) seen[ns++] = p->hand[q][i];
        for (int i = 0; i < p->ncap[q]; i++) seen[ns++] = p->cap[q][i];
    }
    for (int i = 0; i < KK_FIELD; i++)
        if (p->field[i] < KK_CARDS) seen[ns++] = p->field[i];
    if (p->held < KK_CARDS) seen[ns++] = p->held;
    int np = cdb_unseen(pack, KK_CARDS, seen, ns, pool);
    cdb_shuffle(pool, np, r);
    for (int q = 0; q < 2; q++) cdb_fill(p->hand[q], p->nh[q], pool, &np);
    cdb_fill(p->deck, p->ndeck, pool, &np);
    p->seed = ai_rand(r);
}

static void k_view(const kpos *live, int me, kk_st *v)
{
    v->p = *live;
    v->me = (uint8_t)me;
    memset(v->p.hand[1 - me], CDB_HIDDEN, live->nh[1 - me]);
    memset(v->p.deck, CDB_HIDDEN, live->ndeck);
    v->p.seed = 0;
}

int kkb_choose(const kpos *live, int seat, int level, uint64_t seed, ai_stats *stats)
{
    if (stats) memset(stats, 0, sizeof *stats);
    if (live->phase == KP_ROUND) return KA_NEXT;
    if (live->phase == KP_OVER || live->turn != seat) return 0;
    kk_st *v = malloc(sizeof *v);
    if (!v) return kk_auto(live);
    k_view(live, seat, v);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)live->ndeck * 131 + live->nh[seat] * 7 + live->phase));
    int mv[AI_MAX_MOVES], n = k_moves(v, mv), m = n ? mv[0] : AI_NONE;
    if (n > 1) {
        if (cdb_slip(level, &r)) m = k_policy(&v->p, &r, CDB_SLIP_EPS);
        else {
            ai_game G = {
                .size = sizeof(kk_st), .moves = k_moves, .make = k_make, .side = k_side, .players = 2,
                .result = k_result, .playout = k_playout, .determinise = k_determinise,
            };
            int x = cdb_search(&G, v, level, KK_HARD_ITERS, &r, stats);
            if (x != AI_NONE) m = x;
        }
    }
    free(v);
    return m != AI_NONE && kk_legal(live, m) ? m : kk_auto(live);
}

static void card_name(int c, char *out) { snprintf(out, 4, "%d%c", HANA_MONTH(c) + 1, 'a' + HANA_INDEX(c)); }

static int think(int seat, int level, char *move, int cap)
{
    kgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *koikoi_live();                        /* the copy, at entry: nothing else is read */
    int a = kkb_choose(&g->pos, seat, level, ai_mix(ai_session_seed(), (uint64_t)g->n), NULL), ok = 1;
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    char c[4];
    switch (KA_KIND(a)) {
    case KA_PLAY: card_name(KA_CARD(a), c), snprintf(move, (size_t)cap, "p%s", c); break;
    case KA_TAKE: card_name(KA_CARD(a), c), snprintf(move, (size_t)cap, "t%s", c); break;
    case KA_DRAW: snprintf(move, (size_t)cap, "d"); break;
    case KA_KOI: snprintf(move, (size_t)cap, "k"); break;
    case KA_STOP: snprintf(move, (size_t)cap, "s"); break;
    case KA_NEXT: snprintf(move, (size_t)cap, "n"); break;
    default: ok = 0;
    }
    return ok;
}

static int actor(void)
{
    const kpos *p = &koikoi_live()->pos;
    return p->phase == KP_ROUND || p->phase == KP_OVER ? -1 : p->turn;
}

static const bot koikoi_bot = {.think = think, .actor = actor, .play = koikoi_show};
REGISTER_BOT(koikoi, &koikoi_bot)
