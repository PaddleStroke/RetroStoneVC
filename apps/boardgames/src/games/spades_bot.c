/*
 * Spades' bot (bots.h): the card play searched by the trick bots' core (trickbot.h: information-set Monte Carlo over
 * deals of the unseen cards that keep to the voids shown), partners playing for their side, the reward the deal's
 * score for the side (contract, bags and their penalty, nils). Runs on the worker thread: it copies the game at
 * entry and reads only what its seat may know (its hand, the bids, the cards played). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The bid (rule-based, then checked by simulation): the hand's tricks counted (ace 1, a guarded king 0.7, a queen
 * with three 0.3 in the side suits; the spades' top honours and every spade over three; a void, singleton or doubleton
 * worth a ruff with spare spades), from .7 up rounded up, at least 1. Nil: a hand of few tricks (under 1.5), no ace
 * or king of spades, three spades or fewer, and simulated deals in which it takes no trick at least three times in
 * four (its partner covering); never when the partner has bid nil. In play a nil bidder ducks, its partner covers; a
 * side whose contract is safe stops taking tricks (bags) unless it can still set the other side.
 * The round's summary waits for a human tap (actor() -1); think() answers "n" there all the same (the tests).
 * Its moves are the game's notation ("b4", "b0", "Qs", "n"), played by spades_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/spades.h"
#include "games/trickbot.h"

#define SPADES_HARD_ITERS 14000                 /* about 0.3 s a card on a PC */

typedef struct { uint8_t broken, bid[SPADES_P]; int16_t bags[2]; } spx;

static int s_legal(const tb_state *st, const uint8_t *h, int n, uint8_t *out)
{
    if (st->ntrick) return tb_legal_follow(st, h, n, out);
    int k = 0, only = trick_only_suit(h, n, SUIT_SPADES);
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(h[i]) != SUIT_SPADES || TB_CXS(st, spx)->broken || only) out[k++] = h[i];
    return k;
}

static void s_trick_end(tb_state *st, int w)
{
    (void)w;
    for (int i = 0; i < st->ntrick; i++)
        if (CARD_SUIT(st->trick[i]) == SUIT_SPADES) TB_XS(st, spx)->broken = 1;
}

/* a side's points for the deal as the rules count them (bids not yet made count as nothing) */
static int side_points(const tb_state *st, int t)
{
    const spx *x = TB_CXS(st, spx);
    int contract = 0, made = 0, pts = 0, bags = 0;
    for (int pl = t; pl < SPADES_P; pl += 2) {
        if (x->bid[pl] == SPADES_NOBID) continue;
        if (x->bid[pl] == SPADES_NIL) { pts += st->won[pl] ? -100 : 100; bags += st->won[pl]; continue; }
        contract += x->bid[pl];
        made += st->won[pl];
    }
    if (contract > 0) {
        if (made >= contract) pts += 10 * contract, bags += made - contract;
        else pts -= 10 * contract;
    }
    pts += bags;
    if (x->bags[t] + bags >= 10) pts -= 100;
    return pts;
}

static void s_reward(const tb_state *st, float *rw)
{
    int d = side_points(st, 0) - side_points(st, 1);
    float r0 = 0.5f + d / 500.0f;
    for (int s = 0; s < st->players; s++) rw[s] = s & 1 ? 1 - r0 : r0;
}

static int s_want(const tb_state *st, int seat)
{
    const spx *x = TB_CXS(st, spx);
    if (x->bid[seat] == SPADES_NIL) return -1;
    int t = seat & 1, partner = (seat + 2) % SPADES_P, left = SPADES_HAND - st->tricks;
    if (x->bid[partner] == SPADES_NIL && !st->won[partner]) return 1;   /* cover the partner's nil */
    int need[2];
    for (int s = 0; s < 2; s++) {
        int c = 0, m = 0;
        for (int pl = s; pl < SPADES_P; pl += 2) {
            if (x->bid[pl] == SPADES_NOBID) c += SPADES_HAND;   /* not bid yet (a simulated bid): it wants tricks */
            else if (x->bid[pl] != SPADES_NIL) c += x->bid[pl], m += st->won[pl];
        }
        need[s] = c - m;
    }
    if (need[t] > 0) return 1;
    return need[!t] > 0 && need[!t] <= left ? 1 : -1;   /* set them while it can be done; else no bags */
}

static const tb_rules RULES = {
    .legal = s_legal, .trick_end = s_trick_end, .reward = s_reward, .want = s_want,
};

/* the view of seat me (bidding: the deal's start, the first player to lead) */
void spb_view(const sgame *g, int me, tb_state *v, sgame *t)
{
    const spos *p = &g->pos;
    tb_init(v, &RULES, SPADES_P, me);
    v->trump = SUIT_SPADES;
    for (int s = 0; s < SPADES_P; s++) v->team[s] = (uint8_t)(s & 1);
    v->turn = p->phase == SP_PLAY ? p->turn : p->first;
    v->leader = p->phase == SP_PLAY ? p->leader : p->first;
    v->tricks = p->tricks;
    spx *x = TB_XS(v, spx);
    x->broken = p->broken;
    for (int s = 0; s < SPADES_P; s++) x->bid[s] = p->bid[s];
    x->bags[0] = p->bags[0], x->bags[1] = p->bags[1];
    for (int s = 0; s < SPADES_P; s++) {
        v->won[s] = p->won[s];
        if (s != me) tb_hidden(v, s, p->n[s]);
    }
    tb_mine(v, p->hand[me], p->n[me]);
    int start = 0;
    for (int i = 0; i < g->n; i++)
        if (SA_KIND(g->act[i]) == SA_NEXT) start = i + 1;
    spades_new(t, g->seed);
    int plays = p->tricks * SPADES_P;
    for (int i = 0, k = 0; i < g->n; i++) {
        int a = g->act[i];
        if (i >= start && SA_KIND(a) == SA_PLAY) {
            const spos *q = &t->pos;
            uint8_t who[SPADES_P];
            for (int j = 0; j < q->ntrick; j++) who[j] = (uint8_t)((q->leader + j) % SPADES_P);
            tb_note(v, q->turn, SA_ARG(a), q->trick, who, q->ntrick);
            if (k++ < plays) tb_seen(v, SA_ARG(a));
        }
        spades_play(t, a);
    }
    for (int j = 0; j < p->ntrick; j++) tb_trick_add(v, (p->leader + j) % SPADES_P, p->trick[j]);
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_52, pack));
}

/* the hand's tricks, counted */
static float count_tricks(const uint8_t *h, int n)
{
    int len[4] = {0};
    for (int i = 0; i < n; i++) len[CARD_SUIT(h[i])]++;
    int ns = len[SUIT_SPADES], top = 0;
    float t = 0;
    for (int i = 0; i < n; i++) {
        int s = CARD_SUIT(h[i]), r = trick_rank(h[i]), l = len[s];
        if (s == SUIT_SPADES) {
            if (r == 14) t += 1, top++;
            else if (r == 13) t += ns >= 2 ? 1 : 0.5f, top++;
            else if (r == 12) t += ns >= 3 ? 0.8f : 0.2f, top++;
        } else {
            if (r == 14) t += l <= 6 ? 1 : 0.6f;
            else if (r == 13) t += l >= 2 && l <= 5 ? 0.7f : 0.2f;
            else if (r == 12) t += l >= 3 && l <= 4 ? 0.3f : 0;
        }
    }
    if (ns > 3) t += ns - 3;
    int spare = ns - top - (ns > 3 ? ns - 3 : 0);
    for (int s = 0; s < 4 && spare > 0; s++) {
        if (s == SUIT_SPADES) continue;
        float ruff = len[s] == 0 ? 1.0f : len[s] == 1 ? 0.6f : len[s] == 2 ? 0.2f : 0;
        if (ruff > 0) t += ruff, spare--;
    }
    return t;
}

static int spb_bid(const sgame *g, int me, ai_rng *r, sgame *scratch)
{
    const spos *p = &g->pos;
    const uint8_t *h = p->hand[me];
    int n = p->n[me];
    float t = count_tricks(h, n);
    int bid = (int)(t + 0.3f);
    if (bid < 1) bid = 1;
    if (bid > SPADES_HAND) bid = SPADES_HAND;
    int partner = (me + 2) % SPADES_P, high_spade = 0, ns = tb_count_suit(h, n, SUIT_SPADES);
    for (int i = 0; i < n; i++) high_spade |= CARD_SUIT(h[i]) == SUIT_SPADES && trick_rank(h[i]) >= 13;
    if (t < 1.5f && !high_spade && ns <= 3 && p->bid[partner] != SPADES_NIL) {
        tb_state *v = malloc(sizeof *v);        /* nil: simulated deals, it ducking, its partner covering */
        if (v) {
            spb_view(g, me, v, scratch);
            TB_XS(v, spx)->bid[me] = SPADES_NIL;
            int zero = 0, runs = 48;
            for (int k = 0; k < runs; k++) {
                float won[TB_MAXP];
                tb_simulate(v, 1, r, won, NULL, NULL);
                zero += won[me] < 0.5f;
            }
            if (zero * 4 >= runs * 3) bid = SPADES_NIL;
            free(v);
        }
    }
    return bid;
}

/* the action for seat at a level (0: nothing to do) */
int spb_choose(const sgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const spos *p = &g->pos;
    if (p->phase == SP_ROUND) return SA_NEXT;
    if (p->phase == SP_OVER || p->turn != seat) return 0;
    sgame *t = malloc(sizeof *t);
    tb_state *v = malloc(sizeof *v);
    int a = 0;
    if (t && v) {
        if (p->phase == SP_BID) {
            ai_rng r;
            ai_seed(&r, ai_mix(seed, (uint64_t)p->round * 8 + seat));
            a = SA_BID | spb_bid(g, seat, &r, t);
        } else {
            spb_view(g, seat, v, t);
            int c = tb_choose(v, level, seed, SPADES_HARD_ITERS, stats);
            if (c != TB_NONE && spades_can_play(p, c)) a = SA_PLAY | c;
        }
    }
    free(t);
    free(v);
    return a ? a : spades_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    sgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *spades_live();                        /* the copy, at entry: nothing else is read */
    int a = spb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    char c[3];
    switch (SA_KIND(a)) {
    case SA_BID: snprintf(move, (size_t)cap, "b%d", SA_ARG(a)); return 1;
    case SA_PLAY: trick_name(SA_ARG(a), c); snprintf(move, (size_t)cap, "%s", c); return 1;
    case SA_NEXT: snprintf(move, (size_t)cap, "n"); return 1;
    default: return 0;
    }
}

static int actor(void)
{
    const spos *p = &spades_live()->pos;
    return p->phase == SP_BID || p->phase == SP_PLAY ? p->turn : -1;
}

static const bot spades_bot = {.think = think, .actor = actor, .play = spades_show};
REGISTER_BOT(spades, &spades_bot)
