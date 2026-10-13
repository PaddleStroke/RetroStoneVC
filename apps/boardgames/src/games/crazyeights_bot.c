/*
 * Crazy Eights' bot (bots.h), for both variants (Crazy Eights, Last Card) and 2 to 6 players: information-set Monte
 * Carlo tree search (ai_mcts) over the round, each iteration dealing the cards the seat has not seen (the pack minus
 * its hand and the discard) at random into the others' hands and the stock; heuristic playouts to the round's end.
 * Runs on the worker thread: it copies the game at entry and reads only what its seat may know (its hand, the
 * discard, the counts). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The search's moves are the rules' actions (a card, a suit, draw, pass); "last card" is always called and a player
 * who forgot it always caught, in the search as at the table (the easy level forgets to call a third of the time). The
 * playouts: keep the eights and wilds for need, stay in the suit held most, shed the costly cards, hit the next player
 * with the action cards when they are close to going out. The reward: the round won, and for the others the fewer
 * points left in hand. The round's summary waits for a human tap (actor() -1); think() answers "n" there all the same.
 * Its moves are the game's notation ("p3", "c1", "d", "x", "l", "k", "n"), played by crazyeights_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/cardbot.h"
#include "games/crazyeights.h"

#define CE_HARD_ITERS 25000                     /* about 0.3 s a move on a PC */
#define CE_CUTOFF 120                           /* a playout this long: scored by the hands */

/* the view: the position (the hidden cards CDB_HIDDEN), the observer, and what the others' draws showed (crazy
 * eights): the cn[q] oldest cards of q's hand are none of suit csuit[q], rank crank[q], nor an eight */
typedef struct { cpos p; uint8_t me, cn[CE_MAXP], csuit[CE_MAXP], crank[CE_MAXP]; } ce_st;

static int lacks_ok(const ce_st *st, int q, int c)
{
    return CARD_RANK(c) != 8 && CARD_SUIT(c) != st->csuit[q] && CARD_RANK(c) != st->crank[q];
}

static int is_wild(const cpos *p, int c)
{
    return p->variant == CE_LAST ? CARD_SUIT(c) == SUIT_JOKER : CARD_RANK(c) == 8;
}

/* plays an action on a copy (the rules' own ce_play, on a game with no history) */
static void c_apply(cpos *p, int a)
{
    cgame g;
    g.pos = *p;
    g.n = g.floor = 0;
    if (ce_play(&g, a)) *p = g.pos;
}

static int c_moves(const void *s, int *out)
{
    const cpos *p = &((const ce_st *)s)->p;
    int k = 0;
    if (p->phase == CP_CHOOSE) {
        for (int c = 0; c < 4; c++) out[k++] = CA_CHOOSE | c;
        return k;
    }
    if (p->phase != CP_PLAY) return 0;
    if (ce_legal(p, CA_CATCH)) { out[0] = CA_CATCH; return 1; }
    if (ce_legal(p, CA_CALL)) { out[0] = CA_CALL; return 1; }
    const uint8_t *h = p->hand[p->turn];
    for (int i = 0; i < p->n[p->turn]; i++) {
        if (i && h[i] == h[i - 1]) continue;    /* the same card twice (the action deck): once */
        if (ce_playable(p, h[i])) out[k++] = CA_PLAY | h[i];
    }
    if (ce_legal(p, CA_DRAW)) out[k++] = CA_DRAW;
    if (ce_legal(p, CA_PASS)) out[k++] = CA_PASS;
    return k;
}

static void c_make(void *s, int m) { c_apply(&((ce_st *)s)->p, m); }
static int c_side(const void *s) { return ((const ce_st *)s)->p.turn; }

static void c_reward(const cpos *p, float *rw)
{
    float cap = p->variant == CE_LAST ? 150.0f : 60.0f;
    for (int q = 0; q < p->players; q++) {
        float left = p->left[q] > cap ? cap : p->left[q];
        rw[q] = q == p->winner ? 1.0f : 0.3f * (1.0f - left / cap);
    }
}

static int c_result(const void *s, float *rw)
{
    const cpos *p = &((const ce_st *)s)->p;
    if (p->phase != CP_ROUND && p->phase != CP_OVER) return 0;
    c_reward(p, rw);
    return 1;
}

/* a long playout: the fewer cards, the better */
static int c_estimate(const void *s, float *rw)
{
    const cpos *p = &((const ce_st *)s)->p;
    float sum = 0;
    for (int q = 0; q < p->players; q++) sum += rw[q] = 1.0f / (1.0f + p->n[q] * p->n[q]);
    for (int q = 0; q < p->players; q++) rw[q] /= sum;
    return 1;
}

/* ---- the playouts' heuristic ------------------------------------------------------------------------------------ */
static int suit_count(const cpos *p, int pl, int suit)
{
    int k = 0;
    for (int i = 0; i < p->n[pl]; i++) k += !is_wild(p, p->hand[pl][i]) && CARD_SUIT(p->hand[pl][i]) == suit;
    return k;
}

static int c_policy(const cpos *p, ai_rng *r, int eps)
{
    int pl = p->turn;
    if (p->phase == CP_CHOOSE) {
        int best = ai_rand_n(r, 4), bc = -1;
        for (int s = 0; s < 4; s++) {
            int c = suit_count(p, pl, s);
            if (c > bc) bc = c, best = s;
        }
        return CA_CHOOSE | best;
    }
    if (p->phase != CP_PLAY) return AI_NONE;
    if (ce_legal(p, CA_CATCH)) return CA_CATCH;
    if (ce_legal(p, CA_CALL)) return CA_CALL;
    const uint8_t *h = p->hand[pl];
    int n = p->n[pl], last = p->variant == CE_LAST, next = ce_next(p, pl, 1), nn = p->n[next];
    int best = -1;
    long bs = -(1L << 30), count = 0;
    for (int i = 0; i < n; i++) {
        int c = h[i];
        if (!ce_playable(p, c)) continue;
        count++;
        long sc;
        if (is_wild(p, c)) {
            sc = n <= 2 ? 200 : -300;
            if (last && c == ACTION_WILD4) sc += nn <= 2 ? 400 : -100;
        } else {
            sc = suit_count(p, pl, CARD_SUIT(c)) * 12 + ce_points(p, c);
            if (last && CARD_RANK(c) >= ACTION_SKIP) sc += nn <= 2 ? 300 : 10;
        }
        if (eps && ai_rand_n(r, 100) < eps) sc += ai_rand_n(r, 400);
        if (sc > bs) bs = sc, best = c;
    }
    if (best >= 0) {
        if (last && p->drawn != CE_NONE && ai_rand_n(r, 100) < 10) return CA_PASS;   /* now and then: keep it */
        return CA_PLAY | best;
    }
    (void)count;
    if (ce_legal(p, CA_DRAW)) return CA_DRAW;
    return CA_PASS;
}

static int c_playout(const void *s, ai_rng *r) { return c_policy(&((const ce_st *)s)->p, r, 6); }

/* ---- the view and the deal of the unseen ------------------------------------------------------------------------ */
static void c_determinise(void *s, int seat, ai_rng *r)
{
    (void)seat;
    ce_st *st = s;
    cpos *p = &st->p;
    uint8_t pack[CARDS_MAX], seen[CARDS_MAX * 2], pool[CARDS_MAX];
    int npack = cards_pack(p->variant == CE_LAST ? PACK_ACTION : PACK_52, pack), ns = 0;
    for (int i = 0; i < p->n[st->me]; i++) seen[ns++] = p->hand[st->me][i];
    for (int i = 0; i < p->npile; i++) seen[ns++] = p->pile[i];
    int np = cdb_unseen(pack, npack, seen, ns, pool);
    cdb_shuffle(pool, np, r);
    for (int q = 0; q < p->players; q++)   /* the cards known to lack a suit, a rank and the eights first */
        for (int i = 0; q != st->me && i < st->cn[q] && i < p->n[q]; i++) {
            if (p->hand[q][i] != CDB_HIDDEN) continue;
            for (int j = np - 1; j >= 0; j--)
                if (lacks_ok(st, q, pool[j])) {
                    p->hand[q][i] = pool[j];
                    pool[j] = pool[--np];
                    break;
                }
        }
    for (int q = 0; q < p->players; q++)
        if (q != st->me) cdb_fill(p->hand[q], p->n[q], pool, &np);
    cdb_fill(p->stock, p->nstock, pool, &np);
    p->seed = ai_rand(r);
}

/* seat me's view: the others' hands and the stock hidden, the seed gone; the deal's draws replayed (crazy eights: a
 * player draws only when they cannot play: their hand then held no card of the suit, the rank, nor an eight) */
static void ceb_view(const cgame *g, int me, ce_st *v)
{
    const cpos *live = &g->pos;
    memset(v, 0, sizeof *v);
    v->p = *live;
    v->me = (uint8_t)me;
    cgame *t = live->variant == CE_EIGHTS ? malloc(sizeof *t) : NULL;
    if (t) {                                    /* only what everyone saw is read: turns, the top card, counts */
        ce_new(t, g->variant, g->players, g->seed);
        for (int i = 0; i < g->n; i++) {
            const cpos *q = &t->pos;
            int a = g->act[i], pl = q->turn;
            if (CA_KIND(a) == CA_NEXT) memset(v->cn, 0, sizeof v->cn);
            if (CA_KIND(a) == CA_DRAW && !q->drew) {
                v->cn[pl] = q->n[pl];
                v->csuit[pl] = q->suit;
                v->crank[pl] = (uint8_t)CARD_RANK(q->pile[q->npile - 1]);
            }
            if (CA_KIND(a) == CA_PLAY && v->cn[pl] && lacks_ok(v, pl, CA_ARG(a))) v->cn[pl]--;   /* old or new */
            ce_play(t, a);
            if (v->cn[pl] > t->pos.n[pl]) v->cn[pl] = t->pos.n[pl];
        }
        free(t);
    }
    for (int q = 0; q < live->players; q++)
        if (q != me) memset(v->p.hand[q], CDB_HIDDEN, live->n[q]);
    memset(v->p.stock, CDB_HIDDEN, (size_t)live->nstock);
    v->p.seed = 0;
}

/* the action for seat at a level (0: nothing to do) */
int ceb_choose(const cgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const cpos *live = &g->pos;
    if (stats) memset(stats, 0, sizeof *stats);
    if (live->phase == CP_ROUND) return CA_NEXT;
    if ((live->phase != CP_PLAY && live->phase != CP_CHOOSE) || live->turn != seat) return 0;
    ce_st *v = malloc(sizeof *v);
    if (!v) return ce_auto(live);
    ceb_view(g, seat, v);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)live->n[seat] * 977 + live->npile * 31 + live->nstock));
    int mv[AI_MAX_MOVES], n = c_moves(v, mv), a = n ? mv[0] : 0;
    if (n == 1 && mv[0] == CA_CALL && level <= AI_EASY && ai_rand_n(&r, 100) < 33) {
        /* the easy level forgets to call: plays on */
        ce_st w = *v;
        w.p.called[seat] = 1;                   /* (only to list its plays) */
        int m2[AI_MAX_MOVES], k = c_moves(&w, m2);
        if (k) a = m2[ai_rand_n(&r, k)];
    } else if (n > 1) {
        if (cdb_slip(level, &r)) {
            ce_st w = *v;
            c_determinise(&w, seat, &r);
            a = c_policy(&w.p, &r, CDB_SLIP_EPS);
        } else {
            ai_game g = {
                .size = sizeof(ce_st), .moves = c_moves, .make = c_make, .side = c_side,
                .players = live->players, .result = c_result, .playout = c_playout, .estimate = c_estimate,
                .cutoff = CE_CUTOFF, .determinise = c_determinise,
            };
            int m = cdb_search(&g, v, level, CE_HARD_ITERS, &r, stats);
            if (m != AI_NONE) a = m;
        }
    }
    free(v);
    return ce_legal(live, a) ? a : ce_auto(live);
}

static int think(int seat, int level, char *move, int cap)
{
    cgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *crazyeights_live();                   /* the copy, at entry: nothing else is read */
    const cpos *p = &g->pos;
    int a = ceb_choose(g, seat, level, ai_mix(ai_session_seed(), (uint64_t)g->n), NULL), ok = 1;
    if (!a || ai_cancelled() || cap < 8) ok = 0;
    else
        switch (CA_KIND(a)) {
        case CA_PLAY: {
            int i = 0;
            while (i < p->n[p->turn] && p->hand[p->turn][i] != CA_ARG(a)) i++;
            snprintf(move, (size_t)cap, "p%d", i);
            break;
        }
        case CA_CHOOSE: snprintf(move, (size_t)cap, "c%d", CA_ARG(a)); break;
        case CA_DRAW: snprintf(move, (size_t)cap, "d"); break;
        case CA_PASS: snprintf(move, (size_t)cap, "x"); break;
        case CA_CALL: snprintf(move, (size_t)cap, "l"); break;
        case CA_CATCH: snprintf(move, (size_t)cap, "k"); break;
        case CA_NEXT: snprintf(move, (size_t)cap, "n"); break;
        default: ok = 0;
        }
    free(g);
    return ok;
}

static int actor(void)
{
    const cpos *p = &crazyeights_live()->pos;
    return p->phase == CP_PLAY || p->phase == CP_CHOOSE ? p->turn : -1;
}

static const bot crazyeights_bot = {.think = think, .actor = actor, .play = crazyeights_show};
REGISTER_BOT(crazyeights, &crazyeights_bot)
