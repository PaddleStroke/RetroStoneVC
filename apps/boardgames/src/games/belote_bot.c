/*
 * Belote's bot (bots.h), both variants (classic and coinche): the card play searched by the trick bots' core
 * (trickbot.h: information-set Monte Carlo over deals of the unseen cards), partners playing for their side, the
 * reward the deal's score as the rules count it (made or dedans, capot, belote, the contract and its coinche). Runs
 * on the worker thread: it copies the game at entry and reads only what its seat may know: its hand, the bidding,
 * the cards played, the turned card (gone to the taker), the belote once announced. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * What the play reads (beyond the suits not followed): a player who could not follow and did not trump while an
 * opponent held the trick has no trump; a trump played under the trick's best one (trumps led, or ruffing an
 * opponent's trick) shows no trump above it; the taker holds the turned card until he plays it; the belote announced
 * (the trump king or queen played by its holder) shows the other one in that hand.
 *
 * The classic bidding (rule-based, from the five cards and the turned one): the hand's worth in a trump suit, the
 * cards it would hold: trump jack 6, nine 4.5 (3.5 without the jack), ace 3, ten 2.5, king and queen 1.5 (both: the
 * belote, 2 more), eight and seven 1; a side ace 2.5, a side ten with its ace 1.5 (guarded, without it, 0.5); every
 * trump past three 1. Round one: take the turned card's suit at 12 or more. Round two: name the best other suit
 * (the turned card counts as a side card in it) at 12.5 or more. Easy reads its hand with a little noise (+-1.5).
 * The coinche auction (rule-based, checked by simulation): each suit's worth to the team is simulated (deals of the
 * unseen cards played out by the core's heuristic, the team's points with that suit as trumps: cards, dix de der,
 * capot, its own belote). With no bid yet, open in the best suit at the highest level the estimate reaches (80 at
 * least 78 points estimated); over the opponents, bid a suit whose estimate reaches the next level and 5 more;
 * partner's bid: raise it 10 for each side ace and for the trump jack (or nine with three trumps) held, once, when
 * the raise is worth 10 or more, unless the estimate in another suit is clearly better; capot when the simulated
 * team takes every trick nearly always (7.85 of 8). Coinche the opponents' contract (90 or more) holding two of its
 * three top trumps with three trumps or more, or the jack with two side aces; surcoinche when the estimate clears
 * the contract by 30. Easy bids from estimates with noise (+-12).
 * The deal's summary waits for a human tap (actor() -1); think() answers "N" there all the same (the tests).
 * Its moves are the game's notation ("t", "p", "nh", "b90s", "x", "xx", "Jh", "N"), played by belote_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/belote.h"
#include "games/trickbot.h"

#define BELOTE_HARD_ITERS 40000                 /* about 0.15 s a card on a quiet PC (0.3 s at the first tricks) */
#define BELB_UNKNOWN 254                        /* the belote: not known yet (found in the dealt hands) */

typedef struct {
    uint8_t variant, taker, level, coinche;     /* the contract: the taker (a seat), coinche's level and doubling */
    uint8_t bel;                                /* the belote's holder: a seat, TB_NONE none, BELB_UNKNOWN */
} belx;

static int b_power(const tb_state *st, int c, int led)
{
    int s = CARD_SUIT(c);
    if (s == st->trump) return 100 + bel_rank(c, st->trump);
    return s == led ? bel_rank(c, st->trump) : 0;
}

static int b_order(const tb_state *st, int c) { return bel_rank(c, st->trump); }

static int b_points(const tb_state *st, int c) { return bel_points(c, st->trump); }

/* the trick's best trump (its rank), 0: none */
static int b_high_trump(const tb_state *st)
{
    int h = 0;
    for (int i = 0; i < st->ntrick; i++)
        if (CARD_SUIT(st->trick[i]) == st->trump && bel_rank(st->trick[i], st->trump) > h)
            h = bel_rank(st->trick[i], st->trump);
    return h;
}

/* as the rules' card_ok: follow; void, trump (over the best trump if you can) unless the partner holds the trick;
 * trumps led, over the best trump if you can */
static int b_legal(const tb_state *st, const uint8_t *h, int n, uint8_t *out)
{
    if (!st->ntrick) { memcpy(out, h, (size_t)n); return n; }
    int led = CARD_SUIT(st->trick[0]), tr = st->trump, hi = b_high_trump(st), k = 0;
    int has_led = trick_has_suit(h, n, led), has_tr = trick_has_suit(h, n, tr), over = 0;
    for (int i = 0; i < n; i++) over |= CARD_SUIT(h[i]) == tr && bel_rank(h[i], tr) > hi;
    if (led == tr) {
        if (!has_led) { memcpy(out, h, (size_t)n); return n; }
        for (int i = 0; i < n; i++)
            if (CARD_SUIT(h[i]) == tr && (!over || bel_rank(h[i], tr) > hi)) out[k++] = h[i];
        return k;
    }
    if (has_led) {
        for (int i = 0; i < n; i++)
            if (CARD_SUIT(h[i]) == led) out[k++] = h[i];
        return k;
    }
    int master = st->who[tb_best(st)];
    if (!tb_opponent(st, master, st->turn) || !has_tr) { memcpy(out, h, (size_t)n); return n; }
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(h[i]) == tr && (!over || bel_rank(h[i], tr) > hi)) out[k++] = h[i];
    return k;
}

static void b_trick_end(tb_state *st, int w)
{
    if (st->tricks == BEL_HAND - 1) st->pts[w] = (int16_t)(st->pts[w] + 10);   /* dix de der */
}

static void b_cap(tb_state *st, int seat, int suit, int cap)
{
    if (st->cap[seat][suit] == TB_NOCAP || st->cap[seat][suit] > cap) st->cap[seat][suit] = (uint8_t)cap;
}

static void b_infer(tb_state *st, int seat, int card, const uint8_t *trick, const uint8_t *who, int n)
{
    if (!n) return;
    int led = CARD_SUIT(trick[0]), tr = st->trump, s = CARD_SUIT(card), hi = 0, best = 0;
    for (int i = 0; i < n; i++) {
        if (CARD_SUIT(trick[i]) == tr && bel_rank(trick[i], tr) > hi) hi = bel_rank(trick[i], tr);
        if (i && b_power(st, trick[i], led) > b_power(st, trick[best], led)) best = i;
    }
    int opp_master = tb_opponent(st, who[best], seat);
    if (led == tr) {
        if (s == tr && bel_rank(card, tr) < hi) b_cap(st, seat, tr, hi);   /* could not go over */
        return;
    }
    if (s == led || !opp_master) return;
    if (s != tr) b_cap(st, seat, tr, 0);        /* void, the opponents' trick, no trump: none */
    else if (bel_rank(card, tr) < hi) b_cap(st, seat, tr, hi);
}

/* the belote's holder in a dealt world: the seat holding both the trump king and queen */
static void b_dealt(tb_state *st)
{
    belx *x = TB_XS(st, belx);
    if (x->bel != BELB_UNKNOWN) return;
    x->bel = TB_NONE;
    for (int s = 0; s < st->players; s++)
        if (trick_find(st->hand[s], st->n[s], CARD(st->trump, RANK_KING)) >= 0 &&
            trick_find(st->hand[s], st->n[s], CARD(st->trump, RANK_QUEEN)) >= 0)
            x->bel = (uint8_t)s;
}

/* the deal's score for each team, as the rules' score() (the tests compare them) */
void belb_score(const tb_state *st, int *sc)
{
    const belx *x = TB_CXS(st, belx);
    int won[2] = {0, 0}, pts[2] = {0, 0}, bel[2] = {0, 0};
    for (int s = 0; s < st->players; s++) won[s & 1] += st->won[s], pts[s & 1] += st->pts[s];
    if (x->bel < BEL_P) bel[x->bel & 1] = 20;
    for (int e = 0; e < 2; e++) pts[e] = won[e] == BEL_HAND ? 250 : won[1 - e] == BEL_HAND ? 0 : pts[e];
    int t = x->taker & 1, d = 1 - t, pt = pts[t] + bel[t], pd = pts[d] + bel[d];
    if (x->variant == BEL_CLASSIC) {
        int made = pt > pd;
        sc[t] = made ? pt : bel[t];
        sc[d] = made ? pd : (won[d] == BEL_HAND ? 250 : 162) + bel[d];
        return;
    }
    int v = bel_value(x->level), mult = 1 << x->coinche;
    int made = x->level == BEL_CAPOT ? won[t] == BEL_HAND : pt >= v && pt > 81;
    if (!x->coinche) {
        sc[t] = made ? v + pt : bel[t];
        sc[d] = made ? pd : 160 + v + bel[d];
    } else {
        int w = made ? t : d;
        sc[w] = (160 + v) * mult + bel[w];
        sc[1 - w] = bel[1 - w];
    }
}

static void b_reward(const tb_state *st, float *rw)
{
    const belx *x = TB_CXS(st, belx);
    int sc[2];
    belb_score(st, sc);
    float span = x->variant == BEL_CLASSIC ? 540.0f : 2.0f * (330 + bel_value(x->level)) * (float)(1 << x->coinche);
    float r0 = 0.5f + (sc[0] - sc[1]) / span;
    for (int s = 0; s < st->players; s++) rw[s] = s & 1 ? 1 - r0 : r0;
}

static const tb_rules RULES = {
    .power = b_power, .order = b_order, .legal = b_legal, .points = b_points, .trick_end = b_trick_end,
    .reward = b_reward, .infer = b_infer, .dealt = b_dealt,
};

/* the view of seat me in the play (the deal's actions replayed on t: who played what, the turned card's taker) */
void belb_view(const bgame *g, int me, tb_state *v, bgame *t)
{
    const bpos *p = &g->pos;
    tb_init(v, &RULES, BEL_P, me);
    for (int s = 0; s < BEL_P; s++) v->team[s] = (uint8_t)(s & 1);
    v->trump = p->trump == BEL_NONE ? TB_NONE : p->trump;
    v->turn = p->turn;
    v->leader = p->leader;
    v->tricks = p->tricks;
    belx *x = TB_XS(v, belx);
    x->variant = p->variant;
    x->taker = p->taker == BEL_NONE ? 0 : p->taker;
    x->level = p->level;
    x->coinche = p->coinche;
    for (int e = 0; e < 2; e++) v->won[e] = p->won[e], v->pts[e] = p->points[e];   /* a team's, on its first seat */
    for (int s = 0; s < BEL_P; s++)
        if (s != me) tb_hidden(v, s, p->n[s]);
    tb_mine(v, p->hand[me], p->n[me]);
    int turned = CARD_NONE, taker = -1, k = 0, plays = p->tricks * BEL_P;
    bel_start(t, g->variant, g->seed, g->start);
    for (int i = 0; i < g->n; i++) {
        int a = g->act[i];
        const bpos *q = &t->pos;
        if (q->dealt == p->dealt) {
            if ((BA_KIND(a) == BA_TAKE || BA_KIND(a) == BA_NAME) && q->variant == BEL_CLASSIC)
                turned = q->turned, taker = q->turn;
            if (BA_KIND(a) == BA_PLAY && q->phase == BP_PLAY) {
                uint8_t who[BEL_P];
                for (int j = 0; j < q->ntrick; j++) who[j] = (uint8_t)((q->leader + j) % BEL_P);
                if (v->trump != TB_NONE) tb_note(v, q->turn, BA_ARG(a), q->trick, who, q->ntrick);
                if (k++ < plays) tb_seen(v, BA_ARG(a));
                if (BA_ARG(a) == turned) turned = CARD_NONE;   /* the turned card is out */
            }
        }
        bel_play(t, a);
    }
    for (int j = 0; j < p->ntrick; j++) tb_trick_add(v, (p->leader + j) % BEL_P, p->trick[j]);
    if (turned != CARD_NONE && taker >= 0 && taker != me) tb_known(v, taker, turned);
    /* the belote: announced (public), or what the seat can tell */
    int K = CARD(p->trump, RANK_KING), Q = CARD(p->trump, RANK_QUEEN), out = 0, mine = 0;
    for (int j = 0; j < v->ngone; j++) out += v->gone[j] == K || v->gone[j] == Q;
    for (int j = 0; j < v->ntrick; j++) out += v->trick[j] == K || v->trick[j] == Q;
    for (int j = 0; j < p->n[me]; j++) mine += p->hand[me][j] == K || p->hand[me][j] == Q;
    if (p->belote_said && p->belote != BEL_NONE) {
        x->bel = p->belote;
        if (p->belote != me && out < 2) {
            int left = K;
            for (int j = 0; j < v->ngone; j++) left = v->gone[j] == K ? Q : left;
            for (int j = 0; j < v->ntrick; j++) left = v->trick[j] == K ? Q : left;
            if (!(left == turned && taker == p->belote)) tb_known(v, p->belote, left);
        }
    } else if (mine == 2) x->bel = (uint8_t)me;
    else if (mine == 1 || out > 0) x->bel = TB_NONE;   /* split, or one played unannounced */
    else x->bel = BELB_UNKNOWN;
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_32, pack));
}

/* ---- the calls ----------------------------------------------------------------------------------------------- */
static char suit_char(int s) { return "shdc"[s & 3]; }

/* the classic hand's worth with suit tr as trumps (the cards it would hold) */
static float worth(const uint8_t *h, int n, int tr)
{
    int len[4] = {0}, ace[4] = {0}, jack = 0, kq = 0;
    for (int i = 0; i < n; i++) {
        len[CARD_SUIT(h[i])]++;
        if (CARD_RANK(h[i]) == RANK_ACE) ace[CARD_SUIT(h[i])] = 1;
        if (CARD_SUIT(h[i]) == tr && CARD_RANK(h[i]) == RANK_JACK) jack = 1;
    }
    float w = 0;
    for (int i = 0; i < n; i++) {
        int s = CARD_SUIT(h[i]), r = CARD_RANK(h[i]);
        if (s == tr) {
            switch (r) {
            case RANK_JACK: w += 6; break;
            case 9: w += jack ? 4.5f : 3.5f; break;
            case RANK_ACE: w += 3; break;
            case 10: w += 2.5f; break;
            case RANK_KING: case RANK_QUEEN: w += 1.5f; kq++; break;
            default: w += 1; break;
            }
        } else if (r == RANK_ACE) w += 2.5f;
        else if (r == 10) w += ace[s] ? 1.5f : len[s] >= 2 ? 0.5f : 0;
    }
    if (kq == 2) w += 2;
    if (len[tr] > 3) w += len[tr] - 3;
    return w;
}

/* the coinche: the team's points with trumps tr, simulated from the seat's hand (won: the team's tricks) */
static float estimate(const bgame *g, int me, int tr, ai_rng *r, float *won)
{
    const bpos *p = &g->pos;
    tb_state *v = malloc(sizeof *v);
    if (!v) return 0;
    tb_init(v, &RULES, BEL_P, me);
    for (int s = 0; s < BEL_P; s++) v->team[s] = (uint8_t)(s & 1);
    v->trump = (uint8_t)tr;
    v->turn = v->leader = (uint8_t)((p->dealer + 1) % BEL_P);
    belx *x = TB_XS(v, belx);
    x->variant = BEL_CLASSIC;                   /* the points only */
    x->taker = (uint8_t)me;
    x->bel = BELB_UNKNOWN;
    for (int s = 0; s < BEL_P; s++)
        if (s != me) tb_hidden(v, s, BEL_HAND);
    tb_mine(v, p->hand[me], p->n[me]);
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_32, pack));
    float W[TB_MAXP], P[TB_MAXP];
    tb_simulate(v, 40, r, W, P, NULL);
    free(v);
    int t = me & 1;
    float tw = W[t] + W[t + 2], pts = P[t] + P[t + 2];
    if (won) *won = tw;
    if (tw > 7.9f) pts = 250;
    int K = 0;
    for (int i = 0; i < p->n[me]; i++)
        K += p->hand[me][i] == CARD(tr, RANK_KING) || p->hand[me][i] == CARD(tr, RANK_QUEEN);
    return pts + (K == 2 ? 20 : 0);
}

/* the opponents' contract in suit tr: the seat's defence (two of the three top trumps, three trumps; the jack and
 * two side aces) */
static int defence(const uint8_t *h, int n, int tr)
{
    int top = 0, trumps = 0, aces = 0, jack = 0;
    for (int i = 0; i < n; i++) {
        int s = CARD_SUIT(h[i]), r = CARD_RANK(h[i]);
        if (s == tr) {
            trumps++;
            top += r == RANK_JACK || r == 9 || r == RANK_ACE;
            jack |= r == RANK_JACK;
        } else aces += r == RANK_ACE;
    }
    return (top >= 2 && trumps >= 3) || (jack && aces >= 2);
}

static int coinche_bid(const bgame *g, int me, int level, uint64_t seed)
{
    const bpos *p = &g->pos;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)p->dealt * 16 + (uint64_t)me + 4096));
    float noise = level <= AI_EASY ? 12 : 0, E[4], W[4], margin = 15;
    int best = 0;
    for (int s = 0; s < 4; s++) {
        E[s] = estimate(g, me, s, &r, &W[s]) + (noise ? (float)(ai_rand_f(&r) * 2 - 1) * noise : 0);
        if (E[s] > E[best]) best = s;
    }
    if (ai_cancelled()) return BA_PASS;
    int lvl_of = -1;                            /* the highest level the estimate reaches (cap at 160) */
    for (int l = 0; l < BEL_CAPOT; l++)
        if (E[best] >= bel_value(l) + margin) lvl_of = l;
    if (W[best] >= 7.85f) lvl_of = BEL_CAPOT;
    if (p->coinche) {                           /* we are coinched: surcoinche, or pass */
        if (p->bidder == me && E[p->trump] >= bel_value(p->level) + 30) return BA_SURCOINCHE;
        return BA_PASS;
    }
    if (p->bidder == BEL_NONE) return lvl_of >= 0 ? BA_BIDOF(lvl_of, best) : BA_PASS;
    if ((p->bidder & 1) == (me & 1)) {          /* the partner's: raise once, or a clearly better suit of ours */
        int tr = p->trump, aces = 0, raise = 0, said = p->said[me];
        for (int i = 0; i < p->n[me]; i++) {
            int c = p->hand[me][i];
            if (CARD_SUIT(c) != tr && CARD_RANK(c) == RANK_ACE) aces++;
        }
        raise = aces + (trick_find(p->hand[me], p->n[me], CARD(tr, RANK_JACK)) >= 0 ||
                        (trick_find(p->hand[me], p->n[me], CARD(tr, 9)) >= 0 && tb_count_suit(p->hand[me], p->n[me], tr) >= 3));
        raise--;                                /* an average partner holds one */
        if (p->bidder != me && BA_KIND(said) != BA_BID && raise > 0) {
            int l = p->level + raise;
            if (l >= BEL_CAPOT) l = BEL_CAPOT - 1;
            if (l > p->level) return BA_BIDOF(l, tr);
        }
        if (best != tr && lvl_of > p->level && E[best] >= E[tr] + 30 && p->bidder != me) return BA_BIDOF(lvl_of, best);
        return BA_PASS;
    }
    /* the opponents': overbid, coinche, or pass */
    if (lvl_of > p->level && E[best] >= bel_value(p->level + 1) + margin) {
        int l = p->level + 1;
        while (l < lvl_of && E[best] >= bel_value(l + 1) + margin) l++;
        if (W[best] >= 7.85f) l = BEL_CAPOT;
        return BA_BIDOF(l, best);
    }
    if (p->level >= 1 && defence(p->hand[me], p->n[me], p->trump)) return BA_COINCHE;
    return BA_PASS;
}

static int classic_bid(const bgame *g, int me, int level, uint64_t seed)
{
    const bpos *p = &g->pos;
    uint8_t h[BEL_HAND + 1];
    int n = p->n[me];
    memcpy(h, p->hand[me], (size_t)n);
    h[n++] = p->turned;
    ai_rng r;
    ai_seed(&r, ai_mix(seed, (uint64_t)p->dealt * 16 + (uint64_t)me));
    float noise = level <= AI_EASY ? (float)(ai_rand_f(&r) * 3 - 1.5) : 0;
    if (p->phase == BP_BID1) return worth(h, n, CARD_SUIT(p->turned)) + noise >= 11 ? BA_TAKE : BA_PASS;
    int best = -1;
    float bw = 11.5f;
    for (int s = 0; s < 4; s++) {
        if (s == CARD_SUIT(p->turned)) continue;
        float w = worth(h, n, s) + noise;
        if (w >= bw) bw = w, best = s;
    }
    return best >= 0 ? BA_NAME | best : BA_PASS;
}

/* the action for seat at a level (0: nothing to do) */
int belb_choose(const bgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const bpos *p = &g->pos;
    if (p->phase == BP_DEAL) return BA_NEXT;
    if (p->phase == BP_OVER || p->turn != seat) return 0;
    int a = 0;
    if (p->phase == BP_BID1 || p->phase == BP_BID2) a = classic_bid(g, seat, level, seed);
    else if (p->phase == BP_AUCTION) a = coinche_bid(g, seat, level, seed);
    else {
        bgame *t = malloc(sizeof *t);
        tb_state *v = malloc(sizeof *v);
        if (t && v) {
            belb_view(g, seat, v, t);
            int c = tb_choose(v, level, seed, BELOTE_HARD_ITERS, stats);
            if (c != TB_NONE) a = BA_PLAY | c;
        }
        free(t);
        free(v);
    }
    return a && bel_legal(p, a) ? a : bel_auto(p);
}

/* an action in the notation */
void belb_name(int a, char *move, int cap)
{
    char c[3];
    switch (BA_KIND(a)) {
    case BA_PASS: snprintf(move, (size_t)cap, "p"); break;
    case BA_TAKE: snprintf(move, (size_t)cap, "t"); break;
    case BA_NAME: snprintf(move, (size_t)cap, "n%c", suit_char(BA_ARG(a))); break;
    case BA_BID: snprintf(move, (size_t)cap, "b%d%c", bel_value(BA_ARG(a) >> 2), suit_char(BA_ARG(a) & 3)); break;
    case BA_COINCHE: snprintf(move, (size_t)cap, "x"); break;
    case BA_SURCOINCHE: snprintf(move, (size_t)cap, "xx"); break;
    case BA_PLAY: trick_name(BA_ARG(a), c); snprintf(move, (size_t)cap, "%s", c); break;
    case BA_NEXT: snprintf(move, (size_t)cap, "N"); break;
    default: if (cap) move[0] = 0; break;
    }
}

static int think(int seat, int level, char *move, int cap)
{
    bgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *belote_live();                        /* the copy, at entry: nothing else is read */
    int a = belb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    belb_name(a, move, cap);
    return move[0] != 0;
}

static int actor(void)
{
    const bpos *p = &belote_live()->pos;
    return p->phase <= BP_PLAY ? p->turn : -1;
}

static const bot belote_bot = {.think = think, .actor = actor, .play = belote_show};
REGISTER_BOT(belote, &belote_bot)
