/*
 * Truco's bot (bots.h): 2 or 4 players, games to 15 or 30, with or without flor. The cards are played by
 * information-set Monte Carlo tree search (ai_mcts) over the rest of the hand: each iteration deals the cards the seat
 * has not seen (the pack minus its hand and every card played) to the others, keeping to what the table showed (who
 * has a flor; the envido the winner declared). The calls are rule-based on the same sampling: the hand's chance
 * (the deals played out by the playouts' heuristic) for the truco, the envido's chance (the best envido of each side
 * in the deals) for the envido. Runs on the worker thread: it copies the game at entry and reads only what its seat
 * may know. No SDL. MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * - Envido: called with 27 or more (real envido from 31, falta envido with 33 when the game allows it), the mano a
 *   point sooner; answered by its chance against the stake and what a refusal gives away; raised when strong.
 * - Truco: called (retruco, vale cuatro) when the hand's chance is high; answered by the chance against the stake
 *   (a refusal gives away the hand at its worth); raised when strong; a truco answered with an envido first when the
 *   envido is good.
 * - The other side's calls tell: medium takes some chance off when the other side calls; hard weighs each sampled deal
 *   by how likely the call was in it (a truco from high cards, an envido from a good envido, now and then a bluff).
 * - Honest bluffing: now and then (easy 4 %, medium 8 %, hard 10 %) a call with a weak hand, from the bot's own coin,
 *   never a look at another hand.
 * - Levels: easy samples few deals and misjudges its chances (noise), reads nothing in the other side's calls, and
 *   plays a third of its cards by the playouts' rule; hard samples the most deals and searches the cards longest.
 * The playouts: the partner holding the round: the lowest card; else the lowest card that wins it, or the lowest.
 * The next hand waits for a human tap (actor() -1); think() answers "N" there all the same.
 * Its moves are the game's notation ("1e", "t", "e", "r", "f", "q", "n", "N"), played by truco_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/cardbot.h"
#include "games/spanish.h"
#include "games/truco.h"

#define TRUCO_HARD_ITERS 30000                  /* the card play's search (a small game) */

typedef struct { int samples, noise, bluff, discount; } tr_level;

static tr_level levels(int level)
{
    static const tr_level L[AI_LEVELS] = {
        [AI_EASY] = {30, 25, 4, 0}, [AI_MEDIUM] = {400, 5, 8, 10}, [AI_HARD] = {1200, 0, 10, 0},
    };
    return L[level < 0 ? 0 : level > AI_HARD ? AI_HARD : level];
}

typedef struct { tpos p; uint8_t me; } tr_st;

static int side(int pl) { return pl & 1; }

static void t_apply(tpos *p, int a)
{
    tgame g;
    g.pos = *p;
    g.n = 0;
    if (truco_play(&g, a)) *p = g.pos;
}

/* ---- the card play ---------------------------------------------------------------------------------------------- */
static int t_moves(const void *s, int *out)
{
    const tpos *p = &((const tr_st *)s)->p;
    if (p->phase != TP_PLAY || p->call || p->env == TE_CALLING) return 0;
    int k = 0, pl = p->turn;
    for (int i = 0; i < p->n[pl]; i++)
        if (p->hand[pl][i] < CDB_HIDDEN) out[k++] = TA_PLAY | p->hand[pl][i];
    return k;
}

static void t_make(void *s, int m) { t_apply(&((tr_st *)s)->p, m); }
static int t_side(const void *s) { return ((const tr_st *)s)->p.turn; }

static int t_result(const void *s, float *rw)
{
    const tpos *p = &((const tr_st *)s)->p;
    if (p->phase == TP_PLAY) return 0;
    int w = p->phase == TP_OVER ? p->winner : p->hand_side;
    for (int q = 0; q < p->players; q++) rw[q] = side(q) == w ? 1.0f : 0.0f;
    return 1;
}

static int t_policy(const tpos *p, ai_rng *r, int eps)
{
    int pl = p->turn, n = p->n[pl];
    if (!n) return AI_NONE;
    if (eps && ai_rand_n(r, 100) < eps) return TA_PLAY | p->hand[pl][ai_rand_n(r, n)];
    int top = 0, holder = -1, lo = 0, hi = 0, beat = -1;
    for (int q = 0; q < p->players; q++) {
        int c = p->played[p->round][q];
        if (c != CARD_NONE && truco_power(c) > top) top = truco_power(c), holder = q;
    }
    for (int i = 1; i < n; i++) {
        if (truco_power(p->hand[pl][i]) < truco_power(p->hand[pl][lo])) lo = i;
        if (truco_power(p->hand[pl][i]) > truco_power(p->hand[pl][hi])) hi = i;
    }
    for (int i = 0; i < n; i++) {
        int w = truco_power(p->hand[pl][i]);
        if (w > top && (beat < 0 || w < truco_power(p->hand[pl][beat]))) beat = i;
    }
    if (holder < 0) return TA_PLAY | p->hand[pl][p->round == 0 && ai_rand_n(r, 2) ? lo : hi];   /* a lead */
    if (side(holder) == side(pl)) return TA_PLAY | p->hand[pl][lo];
    return TA_PLAY | p->hand[pl][beat >= 0 ? beat : lo];
}

static int t_playout(const void *s, ai_rng *r) { return t_policy(&((const tr_st *)s)->p, r, 8); }

/* ---- the view and the deal of the unseen ------------------------------------------------------------------------ */
/* deals the others' unknown cards: every card played is known, the rest drawn from the unseen; a flor shown (or not)
 * and the envido declared by its winner are kept (tried a few times, then let go) */
static void t_determinise(void *s, int seat, ai_rng *r)
{
    (void)seat;
    tr_st *st = s;
    tpos *p = &st->p;
    uint8_t pack[48], seen[48], pool[48];
    int npack = cards_pack(PACK_ES40, pack), ns = 0, me = st->me;
    for (int i = 0; i < 3; i++) seen[ns++] = p->dealt[me][i];
    for (int k = 0; k <= p->round && k < 3; k++)
        for (int q = 0; q < p->players; q++)
            if (q != me && p->played[k][q] != CARD_NONE) seen[ns++] = p->played[k][q];
    int np = cdb_unseen(pack, npack, seen, ns, pool);
    for (int tries = 0; tries < 60; tries++) {
        cdb_shuffle(pool, np, r);
        int k = 0, ok = 1;
        for (int q = 0; q < p->players; q++) {
            if (q == me) continue;
            int j = 0;
            uint8_t d[3];
            for (int x = 0; x <= p->round && x < 3; x++)
                if (p->played[x][q] != CARD_NONE) d[j++] = p->played[x][q];
            for (int i = 0; i < p->n[q]; i++) p->hand[q][i] = d[j++] = pool[k++];
            memcpy(p->dealt[q], d, 3);
            if (p->flor) ok &= truco_has_flor(d) == (p->flor_mask >> q & 1);
            if (p->env_winner == q) ok &= truco_envido(d) == p->env_value[q];
            else p->env_value[q] = (uint8_t)truco_envido(d);
        }
        if (ok) break;
    }
    p->seed = ai_rand(r);
}

/* seat me's view: the others' cards in hand hidden (and what they were dealt), their envido unknown but the winner's */
static void t_view(const tpos *live, int me, tr_st *v)
{
    v->p = *live;
    v->me = (uint8_t)me;
    tpos *p = &v->p;
    for (int q = 0; q < p->players; q++) {
        if (q == me) continue;
        memset(p->hand[q], CDB_HIDDEN, 3);
        memset(p->dealt[q], CDB_HIDDEN, 3);
        if (p->env_winner != q) p->env_value[q] = 0;
    }
    p->seed = 0;
}

/* how likely the player who made the call on the table would have made it in this deal (hard reads the calls so:
 * each deal weighed by it): a truco from high cards, an envido from a good envido, a bluff now and then */
static float call_weight(const tpos *p, int caller, int envido)
{
    if (envido) return p->env_value[caller] >= 27 ? 1.0f : p->env_value[caller] >= 24 ? 0.5f : 0.2f;
    int high = 0;
    for (int i = 0; i < 3; i++) high += p->dealt[caller][i] < CDB_HIDDEN && truco_power(p->dealt[caller][i]) >= 10;
    for (int i = 0; i < 3; i++)
        if (p->dealt[caller][i] < CDB_HIDDEN && truco_power(p->dealt[caller][i]) >= 12) high = 2;
    return high >= 2 ? 1.0f : high ? 0.5f : 0.25f;
}

/* the chance that the seat's side takes the hand (deals of the unseen played out by the heuristic); weigh: each deal
 * weighed by how likely the truco on the table was in it */
static float hand_chance(const tr_st *v, int samples, ai_rng *r, int weigh)
{
    tr_st w;
    int us = side(v->me), caller = (v->p.responder + v->p.players - 1) % v->p.players;
    float wins = 0, done = 0;
    for (int s = 0; s < samples && !ai_cancelled(); s++) {
        w = *v;
        t_determinise(&w, v->me, r);
        tpos *p = &w.p;
        float wt = weigh && p->call ? call_weight(p, caller, 0) : 1;
        p->call = 0;                            /* the cards alone decide */
        if (p->env == TE_CALLING) p->env = TE_DONE;
        for (int guard = 0; guard < 20 && p->phase == TP_PLAY; guard++) {
            int m = t_policy(p, r, 10);
            if (m == AI_NONE) break;
            t_apply(p, m);
        }
        if (p->phase == TP_PLAY) continue;
        done += wt;
        wins += (p->phase == TP_OVER ? p->winner : p->hand_side) == us ? wt : 0;
    }
    return done > 0 ? wins / done : 0.5f;
}

/* the chance that the seat's side wins the envido (each side's best, the mano's side on a tie); weigh: as above */
static float envido_chance(const tr_st *v, int samples, ai_rng *r, int weigh)
{
    tr_st w;
    int us = side(v->me), caller = (v->p.env_responder + v->p.players - 1) % v->p.players;
    float wins = 0, done = 0;
    for (int s = 0; s < samples; s++) {
        w = *v;
        t_determinise(&w, v->me, r);
        const tpos *p = &w.p;
        int best = -1;
        for (int k = 0; k < p->players; k++) {
            int q = (p->mano + k) % p->players;
            if (best < 0 || p->env_value[q] > p->env_value[best]) best = q;
        }
        float wt = weigh && p->env == TE_CALLING ? call_weight(p, caller, 1) : 1;
        done += wt;
        wins += side(best) == us ? wt : 0;
    }
    return done > 0 ? wins / done : 0.5f;
}

static float noisy(float x, tr_level L, ai_rng *r)
{
    if (L.noise) x += (float)(ai_rand_n(r, 2 * L.noise + 1) - L.noise) / 100.0f;
    return x;
}

/* the envido's stake if accepted, and what a refusal gives away */
static void envido_stake(const tpos *p, int us, int *stake, int *deje)
{
    int all = 0, before = 0;
    for (int i = 0; i < p->nenv; i++) {
        int v = p->chain[i] == TE_REAL ? 3 : 2;
        all += v;
        if (i < p->nenv - 1) before += v;
    }
    *stake = p->chain[p->nenv - 1] == TE_FALTA ? truco_falta(p, us) : all;
    *deje = p->nenv == 1 ? 1 : before;
}

/* the calls: an action, or 0 to play a card */
static int call_choice(const tr_st *v, int level, ai_rng *r)
{
    const tpos *p = &v->p;
    tr_level L = levels(level);
    int me = v->me, us = side(me), ev = p->env_value[me];
    float d = L.discount / 100.0f;
    if (p->env == TE_CALLING) {                 /* answering an envido */
        float c = noisy(envido_chance(v, L.samples, r, level >= AI_HARD), L, r) * (1 - d);
        int stake, deje;
        envido_stake(p, us, &stake, &deje);
        if (c >= 0.85f && truco_legal(p, TA_REAL) && level >= AI_MEDIUM) return TA_REAL;
        if (c >= 0.93f && truco_legal(p, TA_FALTA) && level >= AI_MEDIUM) return TA_FALTA;
        return (2 * c - 1) * (float)stake > -(float)deje ? TA_QUIERO : TA_NOQUIERO;
    }
    int bonus = p->mano == me;
    if (p->call) {                              /* answering a truco */
        if (truco_legal(p, TA_ENVIDO) && ev + bonus >= 28) return ev >= 31 ? TA_REAL : TA_ENVIDO;
        float c = noisy(hand_chance(v, L.samples, r, level >= AI_HARD), L, r) * (1 - d);
        int V = p->call, lv = p->level;
        if (c >= 0.8f && p->call < 4 && level >= AI_MEDIUM) return TA_TRUCO;
        return (2 * c - 1) * (float)V > -(float)lv ? TA_QUIERO : TA_NOQUIERO;
    }
    if (truco_can_envido(p) && truco_legal(p, TA_ENVIDO)) {   /* calling an envido */
        int adj = ev + bonus + (p->players == 4);
        if (adj >= 33 && truco_legal(p, TA_FALTA) && level >= AI_HARD) return TA_FALTA;
        if (adj >= 31 && truco_legal(p, TA_REAL)) return TA_REAL;
        if (adj >= 27) return TA_ENVIDO;
        if (ev < 22 && ai_rand_n(r, 100) < L.bluff) return TA_ENVIDO;   /* a bluff */
    }
    if (truco_can_truco(p)) {                   /* calling truco, retruco, vale cuatro */
        float c = noisy(hand_chance(v, L.samples / 2 + 20, r, 0), L, r);
        float need = p->level == 1 ? 0.7f : p->level == 2 ? 0.76f : 0.84f;
        if (c >= need) return TA_TRUCO;
        if (c < 0.35f && ai_rand_n(r, 100) < L.bluff) return TA_TRUCO;   /* a bluff */
    }
    return 0;
}

int trcb_choose(const tgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const tpos *live = &g->pos;
    if (stats) memset(stats, 0, sizeof *stats);
    if (live->phase == TP_HAND) return TA_NEXT;
    if (live->phase != TP_PLAY || truco_actor(live) != seat) return 0;
    tr_st *v = malloc(sizeof *v);
    if (!v) return truco_auto(live);
    t_view(live, seat, v);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)g->n * 131 + (uint64_t)seat));
    int a = call_choice(v, level, &r);
    if (!a) {
        int mv[AI_MAX_MOVES], n = t_moves(v, mv);
        a = n ? mv[0] : 0;
        if (n > 1) {
            if (cdb_slip(level, &r)) {
                tr_st w = *v;
                t_determinise(&w, seat, &r);
                a = t_policy(&w.p, &r, CDB_SLIP_EPS);
            } else {
                ai_game G = {
                    .size = sizeof(tr_st), .moves = t_moves, .make = t_make, .side = t_side,
                    .players = live->players, .result = t_result, .playout = t_playout,
                    .determinise = t_determinise,
                };
                int x = cdb_search(&G, v, level, TRUCO_HARD_ITERS, &r, stats);
                if (x != AI_NONE) a = x;
            }
        }
    }
    free(v);
    return a && truco_legal(live, a) ? a : truco_auto(live);
}

static int think(int seat, int level, char *move, int cap)
{
    tgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *truco_live();                         /* the copy, at entry: nothing else is read */
    int a = trcb_choose(g, seat, level, ai_mix(ai_session_seed(), (uint64_t)g->n), NULL), ok = 1;
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    switch (TA_KIND(a)) {
    case TA_PLAY: es_name(TA_ARG(a), move); break;
    case TA_TRUCO: snprintf(move, (size_t)cap, "t"); break;
    case TA_ENVIDO: snprintf(move, (size_t)cap, "e"); break;
    case TA_REAL: snprintf(move, (size_t)cap, "r"); break;
    case TA_FALTA: snprintf(move, (size_t)cap, "f"); break;
    case TA_QUIERO: snprintf(move, (size_t)cap, "q"); break;
    case TA_NOQUIERO: snprintf(move, (size_t)cap, "n"); break;
    case TA_MAZO: snprintf(move, (size_t)cap, "m"); break;
    case TA_NEXT: snprintf(move, (size_t)cap, "N"); break;
    default: ok = 0;
    }
    return ok;
}

static int actor(void)
{
    const tpos *p = &truco_live()->pos;
    return p->phase == TP_PLAY ? truco_actor(p) : -1;
}

static const bot truco_bot = {.think = think, .actor = actor, .play = truco_show};
REGISTER_BOT(truco, &truco_bot)
