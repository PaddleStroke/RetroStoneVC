/*
 * Pinochle's bot (bots.h): the card play searched by the trick bots' core (trickbot.h: information-set Monte Carlo over
 * deals of the unseen cards), the bids, trumps and melds by rule. Runs on the worker thread: it copies the game at
 * entry and reads only what its seat may know (its hand, the cards on the table and those once shown, the turned
 * card under the stock, the cards played, the melds shown). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * Two players: while the stock lasts any card may be played (the core's open rules, no voids read); the trick that
 * empties it switches the search's state to the strict rules (follow and beat, trump and overtrump), whose plays also
 * tell what a seat does not hold. The reward: the counters taken, and a trick won while the stock lasts worth a little
 * more (the meld it allows). The meld: the best one at hand (the easy level now and then skips it or takes another).
 * Four players: the bid from the hand's melds for its best trump suit, its aces and the trumps' length, a partner's
 * melds guessed; the side wants its bid made (or to set the bidders). The melds' table waits for a tap (actor() -1).
 * Its moves are the game's notation ("As", "MKsQs", "x", "b24", "p", "th", "g", "n"), played by pinochle_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/pinochle.h"
#include "games/trickbot.h"

#define PN_HARD_ITERS 6000                      /* about 0.25 s a card on a PC */

typedef struct { uint8_t stockwins[2], bs, bid; int16_t melds[2]; } pnx;

static int p_power(const tb_state *st, int c, int led) { return pinochle_power(c, st->trump, led); }
static int p_order(const tb_state *st, int c) { (void)st; return pinochle_order(c); }
static int p_points(const tb_state *st, int c) { (void)st; return pinochle_counter(c); }

static int p_legal(const tb_state *st, const uint8_t *h, int n, uint8_t *out)
{
    if (st->nstock) {
        for (int i = 0; i < n; i++) out[i] = h[i];
        return n;
    }
    return pinochle_strict(h, n, st->trick, st->ntrick, st->trump, out);
}

static const tb_rules R_STRICT;

static void p_trick_end(tb_state *st, int w)
{
    pnx *x = TB_XS(st, pnx);
    if (st->nstock) {
        x->stockwins[st->team[w]]++;
        if (st->nstock <= st->players) st->R = &R_STRICT;   /* the stock's last cards: strict from now on */
        return;
    }
    int left = 0;
    for (int s = 0; s < st->players; s++) left += st->n[s];
    if (!left) st->pts[w]++;                    /* the last trick */
}

/* a strict play: followed without beating (holds nothing over), void without trumping (no trumps), trumped under */
static void p_infer(tb_state *st, int seat, int card, const uint8_t *trick, const uint8_t *who, int n)
{
    (void)who;
    if (!n || st->nstock) return;
    int led = CARD_SUIT(trick[0]), b = 0, s = CARD_SUIT(card), tr = st->trump;
    for (int i = 1; i < n; i++)
        if (pinochle_power(trick[i], tr, led) > pinochle_power(trick[b], tr, led)) b = i;
    int bs = CARD_SUIT(trick[b]), bo = pinochle_order(trick[b]);
    if (s != led && s != tr) st->cap[seat][tr] = 0;
    if (s == bs && pinochle_power(card, tr, led) <= pinochle_power(trick[b], tr, led) && st->cap[seat][s] > bo)
        st->cap[seat][s] = (uint8_t)bo;
}

static void p_reward(const tb_state *st, float *rw)
{
    const pnx *x = TB_CXS(st, pnx);
    float r0;
    if (st->players == 2) r0 = 0.5f + (st->pts[0] - st->pts[1] + 0.6f * (x->stockwins[0] - x->stockwins[1])) / 25.0f;
    else {
        float d[2];
        for (int s = 0; s < 2; s++) {
            int took = st->won[s] + st->won[s + 2] > 0, pts = st->pts[s] + st->pts[s + 2];
            d[s] = (float)(pts + (took ? x->melds[s] : 0));
            if (s == x->bs && d[s] < x->bid) d[s] = -(float)x->bid;
        }
        r0 = 0.5f + (d[0] - d[1]) / 60.0f;
    }
    for (int s = 0; s < st->players; s++) rw[s] = st->team[s] == 0 ? r0 : 1 - r0;
}

static const tb_rules R_OPEN = {
    .power = p_power, .order = p_order, .points = p_points, .legal = p_legal, .trick_end = p_trick_end,
    .reward = p_reward, .draw = 1, .nofollow = 1,
};
static const tb_rules R_STRICT = {
    .power = p_power, .order = p_order, .points = p_points, .legal = p_legal, .trick_end = p_trick_end,
    .reward = p_reward, .infer = p_infer, .draw = 1,
};

void pnb_view(const pino_game *g, int me, tb_state *v, pino_game *t)
{
    const pino_pos *p = &g->pos;
    int P = p->players;
    tb_init(v, P == 2 && p->nstock ? &R_OPEN : &R_STRICT, P, me);
    v->trump = p->trump;
    for (int s = 0; s < P; s++) v->team[s] = (uint8_t)pinochle_side(p, s);
    v->turn = p->turn;
    v->leader = p->leader;
    v->tricks = p->tricks;
    pnx *x = TB_XS(v, pnx);
    x->bs = (uint8_t)(p->bidder == PN_NONE ? 0 : p->bidder & 1);
    x->bid = p->high;
    x->melds[0] = p->melds[0], x->melds[1] = p->melds[1];
    for (int s = 0; s < P; s++) {
        v->won[s] = p->won[s];
        if (s != me) tb_hidden(v, s, p->n[s]);
    }
    if (P == 2) v->pts[0] = p->counters[0], v->pts[1] = p->counters[1];
    else v->pts[0] = p->counters[0], v->pts[1] = p->counters[1];
    tb_mine(v, p->hand[me], p->n[me]);
    int start = 0;
    for (int i = 0; i < g->n; i++)
        if (PA_KIND(g->act[i]) == PA_NEXT) start = i + 1;
    pinochle_new(t, g->players, g->seed);
    uint8_t played[PN_PACK], by[PN_PACK];
    int nplayed = 0;
    for (int i = 0; i < g->n; i++) {
        int a = g->act[i];
        const pino_pos *q = &t->pos;
        if (i >= start && PA_KIND(a) == PA_PLAY) {
            uint8_t who[PN_MAXP];
            for (int j = 0; j < q->ntrick; j++) who[j] = (uint8_t)((q->leader + j) % P);
            const tb_rules *keep = v->R;
            v->R = P == 2 && q->nstock ? &R_OPEN : &R_STRICT;   /* the rules of that moment */
            v->nstock = q->nstock;
            tb_note(v, q->turn, PA_ARG(a) & 0x7f, q->trick, who, q->ntrick);
            v->R = keep;
            played[nplayed] = (uint8_t)(PA_ARG(a) & 0x7f);
            by[nplayed++] = q->turn;
        }
        pinochle_play(t, a);
    }
    v->nstock = 0;
    for (int k = 0; k < nplayed - p->ntrick; k++) tb_seen(v, played[k]);
    for (int s = 0; s < P; s++) {               /* what the others have shown and still hold */
        if (s == me) continue;
        if (P == 2) {
            for (int i = 0; i < p->n[s]; i++)
                if (p->flag[s][i] & (PK_PUBLIC | PK_TABLE)) tb_known(v, s, p->hand[s][i]);
        } else {
            uint8_t used[PN_PACK] = {0};
            for (int i = 0; i < p->nm[s]; i++) {
                int c = p->mcards[s][i], gone = 0;
                for (int k = 0; k < nplayed && !gone; k++)
                    if (by[k] == s && played[k] == c && !used[k]) used[k] = 1, gone = 1;
                if (!gone) tb_known(v, s, c);
            }
        }
    }
    for (int j = 0; j < p->ntrick; j++) tb_trick_add(v, (p->leader + j) % P, p->trick[j]);
    if (P == 2 && p->nstock) tb_stock_set(v, p->nstock, p->stock, 1);
    uint8_t pack[PN_PACK];
    int k = 0;
    for (int c = 0; c < 2; c++)
        for (int s = 0; s < 4; s++)
            for (int r = 0; r < 6; r++) {
                static const uint8_t R6[6] = {9, 10, RANK_JACK, RANK_QUEEN, RANK_KING, RANK_ACE};
                pack[k++] = CARD(s, R6[r]);
            }
    tb_finish(v, pack, k);
}

/* ---- the calls ---------------------------------------------------------------------------------------------------- */
static float suit_worth(const uint8_t *h, int n, int t)
{
    int aces = 0, len = 0, tens = 0;
    for (int i = 0; i < n; i++) {
        aces += CARD_RANK(h[i]) == RANK_ACE;
        tens += CARD_RANK(h[i]) == 10;
        len += CARD_SUIT(h[i]) == t;
    }
    return (float)pinochle_hand_melds(h, n, t, NULL, NULL) + 1.0f * (aces - 2) + 0.3f * tens + 1.3f * (len - 3);
}

static int best_suit(const uint8_t *h, int n, float *worth)
{
    int best = 0;
    float bw = -1e9f;
    for (int t = 0; t < 4; t++) {
        float w = suit_worth(h, n, t);
        if (w > bw) bw = w, best = t;
    }
    if (worth) *worth = bw;
    return best;
}

static int bid_action(const pino_pos *p, int me, int level, ai_rng *r)
{
    float w;
    best_suit(p->hand[me], p->n[me], &w);
    float top = w + 4 + 11;                      /* a partner's melds, the side's share of the counters */
    if (level == AI_EASY) top += (float)(ai_rand_n(r, 7) - 3);
    int next = p->high < PN_MIN_BID ? PN_MIN_BID : p->high + 1, partner = (me + 2) % 4;
    if (p->high && p->bidder == partner) top -= 4;   /* leave the partner's bid alone unless much stronger */
    if (!pinochle_legal(p, PA_PASS)) return PA_BID | next;
    return top >= next ? PA_BID | next : PA_PASS;
}

static int meld_action(const pino_pos *p, int level, ai_rng *r)
{
    int best = PA_SKIP, bv = -1, any[PM_KINDS * 4], na = 0;
    for (int k = 0; k < PM_KINDS; k++)
        for (int s = 0; s < (k == PM_MARRIAGE ? 4 : 1); s++) {
            int a = PA_MELD_OF(k, s);
            if (!pinochle_legal(p, a)) continue;
            any[na++] = a;
            int v = pinochle_meld_value(k, 2) * 4;
            if (k == PM_DIX && p->nstock && pinochle_counter(p->stock[0])) v += 3;   /* the dix for a counter */
            if (v > bv) bv = v, best = a;
        }
    if (level == AI_EASY && na && ai_rand_n(r, 100) < 25) return ai_rand_n(r, 3) ? any[ai_rand_n(r, na)] : PA_SKIP;
    return best;
}

int pnb_choose(const pino_game *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const pino_pos *p = &g->pos;
    if (p->phase == PP_ROUND) return PA_NEXT;
    if (p->phase == PP_MELDS) return PA_GO;
    if (p->phase == PP_OVER || p->turn != seat) return 0;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)p->round * 64 + (uint64_t)p->tricks * 4 + seat));
    switch (p->phase) {
    case PP_BID: return bid_action(p, seat, level, &r);
    case PP_TRUMP: return PA_TRUMP | best_suit(p->hand[seat], p->n[seat], NULL);
    case PP_MELD: return meld_action(p, level, &r);
    default: break;
    }
    pino_game *t = malloc(sizeof *t);
    tb_state *v = malloc(sizeof *v);
    int a = 0;
    if (t && v) {
        pnb_view(g, seat, v, t);
        int c = tb_choose(v, level, seed, PN_HARD_ITERS, stats);
        if (c != TB_NONE && pinochle_can_play(p, c)) a = PA_PLAY | c;
    }
    free(t);
    free(v);
    return a ? a : pinochle_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    pino_game *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *pinochle_live();                      /* the copy, at entry: nothing else is read */
    int a = pnb_choose(g, seat, level, ai_session_seed(), NULL);
    int ok = a && !ai_cancelled() && pinochle_name(&g->pos, a, move, cap);
    free(g);
    return ok;
}

static int actor(void)
{
    const pino_pos *p = &pinochle_live()->pos;
    return p->phase == PP_ROUND || p->phase == PP_MELDS || p->phase == PP_OVER ? -1 : p->turn;
}

static const bot pinochle_bot = {.think = think, .actor = actor, .play = pinochle_show};
REGISTER_BOT(pinochle, &pinochle_bot)
