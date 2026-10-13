/*
 * Rikiki's bot (bots.h): the card play searched by the trick bots' core (trickbot.h: information-set Monte Carlo over
 * deals of the unseen cards that keep to the voids shown), each player for himself, the reward the deal's score
 * (the bid made exactly: 10 and a point a trick; missed: minus the difference). Runs on the worker thread: it copies
 * the game at entry and reads only what its seat may know (its hand, the turned card, the bids, the cards played).
 * No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The bid: the tricks the hand can expect, from simulated deals of the unseen cards played out by the core's
 * heuristic (those who have bid play to their bids, the others for tricks), rounded to the nearest; the dealer, when
 * that number is forbidden, takes the nearer of the numbers on either side. In play it takes tricks until it has its
 * bid, then ducks. The deal's summary waits for a human tap (actor() -1); think() answers "n" there all the same.
 * Its moves are the game's notation ("b2", "Qs", "n"), played by rikiki_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/rikiki.h"
#include "games/trickbot.h"

#define RIKIKI_HARD_ITERS 16000                 /* about 0.3 s a card on a PC */

typedef struct { uint8_t bid[RIKIKI_MAXP], size; } rkx;

static void k_reward(const tb_state *st, float *rw)
{
    const rkx *x = TB_CXS(st, rkx);
    int size = x->size;
    for (int s = 0; s < st->players; s++) {
        int bid = x->bid[s] == RIKIKI_NOBID ? st->won[s] : x->bid[s];
        rw[s] = (float)(rikiki_points(bid, st->won[s]) + size) / (float)(10 + 2 * size);
    }
}

static int k_want(const tb_state *st, int seat)
{
    const rkx *x = TB_CXS(st, rkx);
    return x->bid[seat] == RIKIKI_NOBID || st->won[seat] < x->bid[seat] ? 1 : -1;
}

static const tb_rules RULES = {.reward = k_reward, .want = k_want};

/* the view of seat me (bidding: the deal's start, the first player to lead) */
void rkb_view(const rikiki_game *g, int me, tb_state *v, rikiki_game *t)
{
    const rikiki_pos *p = &g->pos;
    tb_init(v, &RULES, p->players, me);
    v->trump = p->trump < 0 ? TB_NONE : (uint8_t)p->trump;
    v->turn = p->phase == RK_PLAY ? p->turn : p->first;
    v->leader = p->phase == RK_PLAY ? p->leader : p->first;
    v->tricks = p->tricks;
    rkx *x = TB_XS(v, rkx);
    x->size = p->size;
    for (int s = 0; s < p->players; s++) {
        x->bid[s] = p->bid[s];
        v->won[s] = p->won[s];
        if (s != me) tb_hidden(v, s, p->n[s]);
    }
    tb_mine(v, p->hand[me], p->n[me]);
    if (p->turned != CARD_NONE) tb_seen(v, p->turned);   /* face up: out of play */
    int start = 0;
    for (int i = 0; i < g->n; i++)
        if (RKA_KIND(g->act[i]) == RKA_NEXT) start = i + 1;
    rikiki_new(t, g->players, g->seed);
    int plays = p->tricks * p->players;
    for (int i = 0, k = 0; i < g->n; i++) {
        int a = g->act[i];
        if (i >= start && RKA_KIND(a) == RKA_PLAY) {
            const rikiki_pos *q = &t->pos;
            uint8_t who[RIKIKI_MAXP];
            for (int j = 0; j < q->ntrick; j++) who[j] = (uint8_t)((q->leader + j) % q->players);
            tb_note(v, q->turn, RKA_ARG(a), q->trick, who, q->ntrick);
            if (k++ < plays) tb_seen(v, RKA_ARG(a));
        }
        rikiki_play(t, a);
    }
    for (int j = 0; j < p->ntrick; j++) tb_trick_add(v, (p->leader + j) % p->players, p->trick[j]);
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_52, pack));
}

static int rkb_bid(const rikiki_game *g, int me, int level, ai_rng *r, rikiki_game *scratch)
{
    const rikiki_pos *p = &g->pos;
    tb_state *v = malloc(sizeof *v);
    float won[TB_MAXP] = {0};
    if (v) {
        rkb_view(g, me, v, scratch);
        v->eps = 5;
        tb_simulate(v, level <= AI_EASY ? 24 : level == AI_MEDIUM ? 48 : 96, r, won, NULL, NULL);
        free(v);
    }
    float e = won[me];
    int bid = (int)(e + 0.5f);
    if (bid > p->size) bid = p->size;
    if (!rikiki_can_bid(p, bid)) {
        int lo = bid - 1, hi = bid + 1;
        if (lo < 0 || (hi <= p->size && hi - e < e - lo)) bid = hi;
        else bid = lo;
        if (!rikiki_can_bid(p, bid)) bid = bid == hi ? lo : hi;
    }
    return bid;
}

/* the action for seat at a level (0: nothing to do) */
int rkb_choose(const rikiki_game *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const rikiki_pos *p = &g->pos;
    if (p->phase == RK_ROUND) return RKA_NEXT;
    if (p->phase == RK_OVER || p->turn != seat) return 0;
    rikiki_game *t = malloc(sizeof *t);
    tb_state *v = malloc(sizeof *v);
    int a = 0;
    if (t && v) {
        if (p->phase == RK_BID) {
            ai_rng r;
            ai_seed(&r, ai_mix(seed, (uint64_t)p->deal * 8 + seat));
            int b = rkb_bid(g, seat, level, &r, t);
            if (rikiki_can_bid(p, b)) a = RKA_BID | b;
        } else {
            rkb_view(g, seat, v, t);
            int c = tb_choose(v, level, seed, RIKIKI_HARD_ITERS, stats);
            if (c != TB_NONE && rikiki_can_play(p, c)) a = RKA_PLAY | c;
        }
    }
    free(t);
    free(v);
    return a ? a : rikiki_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    rikiki_game *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *rikiki_live();                        /* the copy, at entry: nothing else is read */
    int a = rkb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    char c[3];
    switch (RKA_KIND(a)) {
    case RKA_BID: snprintf(move, (size_t)cap, "b%d", RKA_ARG(a)); return 1;
    case RKA_PLAY: trick_name(RKA_ARG(a), c); snprintf(move, (size_t)cap, "%s", c); return 1;
    case RKA_NEXT: snprintf(move, (size_t)cap, "n"); return 1;
    default: return 0;
    }
}

static int actor(void)
{
    const rikiki_pos *p = &rikiki_live()->pos;
    return p->phase == RK_BID || p->phase == RK_PLAY ? p->turn : -1;
}

static const bot rikiki_bot = {.think = think, .actor = actor, .play = rikiki_show};
REGISTER_BOT(rikiki, &rikiki_bot)
