/*
 * French Tarot's bot (bots.h), for 3, 4 and 5 players: the card play searched by the trick bots' core (trickbot.h:
 * information-set Monte Carlo over deals of the unseen cards that keep to what each seat may know), the taker's side
 * against the defence, the reward the deal's score as the rules count it. Runs on the worker thread: it copies the
 * game at entry and reads only what its seat may know. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * What a seat knows (the view, tarotb_view): its own hand; the bids and the called card; the cards played, who played
 * them (the deal's actions replayed on a scratch game: a suit not followed is a void; a side card on a side suit led,
 * no trump either; a trump under the trick's highest trump, none over it); the poignées shown (those trumps held until
 * played; the lower trumps not shown are not held; the Excuse shown with them: no other trump). The dog turned up
 * (Petite, Garde) is everyone's: its kings, oudlers and trumps not shown in the discard are in the taker's hand, its
 * other cards in the taker's hand or the discard. The taker knows his discard; the others know only the trumps shown
 * in it, the rest of it holds no trump, king nor Excuse (dealt as a seat that never plays: the core's sitting seat).
 * Garde sans and Garde contre: the dog is nobody's, dealt at random out of play. Five players: the called card's
 * holder is the partner; until it is played only he knows (and the taker, when it lies in the dog he saw, or in his
 * own hand): each dealt world makes the partner whoever holds it there (dealt(); never the taker, out of the dog seen;
 * in an unseen dog the taker plays alone).
 * The core's rules: the Excuse is a trump of power 0 (it never wins; any seat may hold it, void in trumps or not);
 * led, the next card sets the suit; it stays with its owner, who gives half a point to the trick's winner of the other
 * side, but in the last trick it goes to the winner, unless its side took every trick before (then it wins that
 * trick). The reward: (25 + the difference) x the bid, petit au bout x the bid, the poignées, the chelem (200),
 * squeezed into 0..1 (0.5 + tanh(score / 50 x bid) / 2), the taker and his partner alike, the defence the opposite.
 *
 * The calls (rule-based, from the hand; every level alike):
 * - Bidding: the hand's points, counted as French players count them (scaled to the hand's size for 3 and 5
 *   players): a trump 2, a major trump (16..20) 1 more; the 21 10, the Excuse 8, the Petit 8 with 6 trumps or more
 *   (5 with 5, else 0); a king 6, a queen 3 (4 with her king), a knight 2, a jack 1; a long suit 5, 7 or 9; a void 6,
 *   a singleton 3 (with 5 trumps or more). Points over thresholds bid Petite / Garde / Garde sans / Garde contre
 *   (BID_T, per player count: 3 players need more, 5 fewer, for the partner); never under the bids made: pass then.
 * - The call (five players): the rank tarot_call_rank() names, in the suit where the hand holds the most points (its
 *   honours protected by the partner's king), then the longest; never a card of its own.
 * - The dog: taken. The discard: void the shortest suits without a king (all their cards, while they fit), then the
 *   most points of the suits without a king (queens, knights: safe in the discard), the shortest suits first; then
 *   the lowest cards of the suits with a king; trumps (the lowest, never an oudler) only when the rules force them.
 * - Poignée: the largest the hand holds, shown before its first card when its side looks like winning the deal:
 *   heuristic playouts of dealt worlds (tb_simulate) give the taker's side 0.5 or more (the defence 0.6: showing
 *   tells the taker where the trumps are).
 * - The deal's summary and a cancelled deal wait for a human tap (actor() -1); think() answers "n" there all the same.
 * Its moves are the game's notation ("p", "b2", "kh14", "t", "xs3", "f", "g1", "a21", "e", "n"), played by
 * tarot_show(). Every player count and contract is played; there are no other variants.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "bots.h"
#include "games/tarot.h"                        /* before trickbot.h: its TB_NONE enum, the core's macro after */
#include "games/trickbot.h"

#define TAROT_HARD_ITERS 3600                   /* about 0.3 s a card on a quiet PC (78 cards: slow playouts) */
#define TRUMP SUIT_TRUMP
#define ATT 1                                   /* team[]: the taker's side; 0 the defence; the discard's seat 2 */

typedef struct {
    uint8_t taker, called, partner_known, handsize;
    uint8_t mult, aside_att, aside_oud, pab;    /* aside_att: the aside counts for the taker's side */
    int16_t aside_half, poignee;                /* the aside's known half points; the poignées' bonus */
    uint8_t rw;                                 /* EXPERIMENT */
    uint8_t oud[TB_MAXP];                       /* the oudlers each seat took (bit 0 Petit, 1 the 21, 2 Excuse) */
} tx;

int tarotb_exp_rw[2] = {-1, -1};   /* EXPERIMENT */
#ifndef TAROT_RW
#define TAROT_RW 0
#endif

static int oud_bit(int c) { return c == TAROT_PETIT ? 1 : c == TAROT_21 ? 2 : c == TAROT_EXCUSE ? 4 : 0; }
static int is_trump(int c) { return CARD_SUIT(c) == TRUMP && c != TAROT_EXCUSE; }
static int nbits(int m) { return (m & 1) + (m >> 1 & 1) + (m >> 2 & 1); }

/* ---- the core's rules ---------------------------------------------------------------------------------------- */
static int t_power(const tb_state *st, int c, int led)
{
    if (c == TAROT_EXCUSE) return 0;
    int s = CARD_SUIT(c), r = CARD_RANK(c);
    if (s == TRUMP) return 100 + r;
    if (led == TRUMP && st->ntrick && st->trick[0] == TAROT_EXCUSE)   /* the Excuse led: the next card sets it */
        led = st->ntrick > 1 ? CARD_SUIT(st->trick[1]) : s;
    return s == led ? r : 0;
}

static int t_order(const tb_state *st, int c) { (void)st; return c == TAROT_EXCUSE ? 0 : CARD_RANK(c); }

static int t_points(const tb_state *st, int c) { (void)st; return tarot_value2(c); }

/* the suit to follow and the highest trump of a trick (the Excuse aside) */
static int led_of(const uint8_t *t, int n, int *top)
{
    int led = -1;
    *top = 0;
    for (int i = 0; i < n; i++) {
        if (t[i] == TAROT_EXCUSE) continue;
        if (led < 0) led = CARD_SUIT(t[i]);
        if (is_trump(t[i]) && CARD_RANK(t[i]) > *top) *top = CARD_RANK(t[i]);
    }
    return led;
}

/* follow; void, trump; over the highest trump if you can; the Excuse at any time */
static int t_legal(const tb_state *st, const uint8_t *h, int n, uint8_t *out)
{
    int top, led = led_of(st->trick, st->ntrick, &top), k = 0;
    if (led < 0) {
        memcpy(out, h, (size_t)n);
        return n;
    }
    int has_suit = 0, has_trump = 0, has_over = 0;
    for (int i = 0; i < n; i++) {
        if (h[i] == TAROT_EXCUSE) continue;
        if (led != TRUMP && CARD_SUIT(h[i]) == led) has_suit = 1;
        if (is_trump(h[i])) {
            has_trump = 1;
            if (CARD_RANK(h[i]) > top) has_over = 1;
        }
    }
    for (int i = 0; i < n; i++) {
        int c = h[i], ok;
        if (c == TAROT_EXCUSE) ok = 1;
        else if (has_suit) ok = CARD_SUIT(c) == led;
        else if (has_trump) ok = is_trump(c) && (!has_over || CARD_RANK(c) > top);
        else ok = 1;
        if (ok) out[k++] = (uint8_t)c;
    }
    return k;
}

/* after a trick: the Excuse's way, the oudlers taken, the Petit in the last trick (pts[] and won[] counted) */
static void t_trick_end(tb_state *st, int w)
{
    tx *x = TB_XS(st, tx);
    int last = st->tricks == x->handsize - 1, xi = -1, win = w;
    for (int i = 0; i < st->ntrick; i++)
        if (st->trick[i] == TAROT_EXCUSE) xi = i;
    int owner = xi < 0 ? -1 : st->who[xi];
    if (owner >= 0 && last) {                   /* the last trick: to the winner, unless the owner's side swept */
        int all = 0;
        for (int q = 0; q < st->players; q++)
            if (q != st->sitting && st->team[q] == st->team[owner]) all += st->won[q];
        all -= st->team[w] == st->team[owner];  /* this trick is counted already */
        if (st->tricks > 0 && all == st->tricks && owner != w) {
            int tp = 0;
            for (int i = 0; i < st->ntrick; i++) tp += tarot_value2(st->trick[i]);
            st->pts[w] = (int16_t)(st->pts[w] - tp), st->pts[owner] = (int16_t)(st->pts[owner] + tp);
            st->won[w]--, st->won[owner]++;
            st->last_winner = (uint8_t)owner;
            win = owner;
        }
        owner = -1;
    }
    for (int i = 0; i < st->ntrick; i++) {
        int c = st->trick[i];
        if (i == xi && owner >= 0) {            /* kept by its owner; half a point to the other side's winner */
            x->oud[owner] |= 4;
            st->pts[win] = (int16_t)(st->pts[win] - 9), st->pts[owner] = (int16_t)(st->pts[owner] + 9);
            if (st->team[owner] != st->team[win])
                st->pts[win] = (int16_t)(st->pts[win] + 1), st->pts[owner] = (int16_t)(st->pts[owner] - 1);
            continue;
        }
        x->oud[win] |= (uint8_t)oud_bit(c);
        if (c == TAROT_PETIT && last) x->pab = (uint8_t)win;
    }
}

/* the deal's count from a finished (or dealt) state: the taker's side's half points and oudlers, and what each
 * defender pays (+) or is paid (-), as the rules' score() */
int tarotb_count(const tb_state *st, int *half_out, int *oud_out)
{
    const tx *x = TB_CXS(st, tx);
    int half = 0, oud = 0, aw = 0;
    for (int s = 0; s < st->players; s++) {
        if (s == st->sitting || st->team[s] != ATT) continue;
        half += st->pts[s], oud |= x->oud[s], aw += st->won[s];
    }
    if (x->aside_att) {
        half += x->aside_half, oud |= x->aside_oud;
        for (int i = 0; i < st->nrest; i++) half += tarot_value2(st->rest[i]), oud |= oud_bit(st->rest[i]);
        if (st->sitting != TB_NONE)
            for (int i = 0; i < st->n[st->sitting]; i++) {
                int c = st->hand[st->sitting][i];
                half += tarot_value2(c), oud |= oud_bit(c);
            }
    }
    int d2 = half - 2 * tarot_need(nbits(oud)), sign = d2 >= 0 ? 1 : -1, diff = (abs(d2) + 1) / 2;
    int pab = x->pab == TB_NONE ? 0 : st->team[x->pab] == ATT ? 10 : -10;
    int chelem = aw == x->handsize ? 200 : aw == 0 ? -200 : 0;
    if (half_out) *half_out = half;
    if (oud_out) *oud_out = oud;
    return sign * (25 + diff) * x->mult + pab * x->mult + sign * x->poignee + chelem;
}

static void t_reward(const tb_state *st, float *rw)
{
    const tx *x = TB_CXS(st, tx);
#ifndef TAROT_RW
#define TAROT_RW 0
#endif
    float u = (float)tarotb_count(st, NULL, NULL) / (float)(x->mult ? x->mult : 1), a;
    int rwv = x->rw;
    if (rwv == 1) a = 0.5f + u / 300.0f;
    else if (rwv == 2) a = (u > 0 ? 0.7f : 0.3f) + u / 1000.0f;
    else if (rwv == 3) a = 0.5f + 0.5f * tanhf(u / 25.0f);
    else a = 0.5f + 0.5f * tanhf(u / 50.0f);
    for (int s = 0; s < st->players; s++) rw[s] = st->team[s] == ATT ? a : 1 - a;
}

/* what a play shows beyond the suit led not followed (the Excuse is never noted: it may go at any time) */
static void t_infer(tb_state *st, int seat, int card, const uint8_t *trick, const uint8_t *who, int n)
{
    (void)who;
    if (!n) return;
    int top, led = led_of(trick, n, &top), s = CARD_SUIT(card);
    uint8_t *cap = st->cap[seat];
    if (led < 0) return;
    if (s != led && s != TRUMP) cap[TRUMP] = 0;  /* could not trump: no trump (the Excuse, order 0, still may be) */
    else if (s == TRUMP && top && CARD_RANK(card) < top && (cap[TRUMP] == TB_NOCAP || cap[TRUMP] > top))
        cap[TRUMP] = (uint8_t)top;              /* under the highest trump: none over it */
}

/* five players: the partner is whoever holds the called card in this dealt world (none: the taker alone) */
static void t_dealt(tb_state *st)
{
    tx *x = TB_XS(st, tx);
    if (x->partner_known || x->called == TB_NONE) return;
    int holder = x->taker;
    for (int q = 0; q < st->players; q++)
        if (q != st->sitting)
            for (int i = 0; i < st->n[q]; i++)
                if (st->hand[q][i] == x->called) holder = q;
    for (int q = 0; q < st->players; q++)
        if (q != st->sitting) st->team[q] = (uint8_t)(q == x->taker || q == holder ? ATT : 0);
}

static const tb_rules RULES = {
    .power = t_power, .order = t_order, .legal = t_legal, .points = t_points, .trick_end = t_trick_end,
    .reward = t_reward, .infer = t_infer, .dealt = t_dealt,
};

/* ---- the view -------------------------------------------------------------------------------------------------- */
static int in(const uint8_t *c, int n, int card)
{
    for (int i = 0; i < n; i++)
        if (c[i] == card) return 1;
    return 0;
}

/* the view of seat me in the play (t: a scratch game for the replay) */
void tarotb_view(const tgame *g, int me, tb_state *v, tgame *t)
{
    const tpos *p = &g->pos;
    int P = p->players, taker = p->taker, shown_dog = p->contract <= TB_GARDE;
    int ecart_seat = shown_dog && me != taker ? P : TB_NONE;   /* the discard unseen: a seat that never plays */
    tb_init(v, &RULES, ecart_seat == TB_NONE ? P : P + 1, me);
    v->trump = TRUMP;
    v->sitting = (uint8_t)ecart_seat;
    v->turn = p->turn, v->leader = p->leader, v->tricks = p->tricks;
    tx *x = TB_XS(v, tx);
    x->taker = taker;
    x->called = P == 5 ? p->called : TB_NONE;
    x->handsize = p->handsize;
    x->mult = (uint8_t)tarot_mult(p->contract);
    x->aside_att = p->aside_attack;
    x->pab = TB_NONE;
    x->rw = (uint8_t)(tarotb_exp_rw[me & 1] < 0 ? TAROT_RW : tarotb_exp_rw[me & 1]);   /* EXPERIMENT */
    for (int q = 0; q < P; q++) x->poignee = (int16_t)(x->poignee + tarot_poignee_bonus(p->poignee[q]));
    /* the sides: known, or dealt() decides (the called card's holder) */
    int mine = x->called != TB_NONE && in(p->hand[me], p->nhand[me], x->called);
    int known = P < 5 || p->revealed || mine || (shown_dog && in(p->dog, p->dogsize, x->called));
    x->partner_known = (uint8_t)known;
    for (int q = 0; q < P; q++) v->team[q] = (uint8_t)(q == taker || (known && q == p->partner) ? ATT : 0);
    if (ecart_seat != TB_NONE) v->team[ecart_seat] = 2;
    /* the hands, the tricks taken */
    for (int q = 0; q < P; q++) {
        if (q != me) tb_hidden(v, q, p->nhand[q]);
        v->won[q] = p->won[q];
        for (int i = 0; i < p->npile[q]; i++) {
            v->pts[q] = (int16_t)(v->pts[q] + tarot_value2(p->pile[q][i]));
            x->oud[q] |= (uint8_t)oud_bit(p->pile[q][i]);
        }
    }
    if (p->xowner != TAROT_NONE) v->pts[p->xowner]--, v->pts[p->xto]++;
    tb_mine(v, p->hand[me], p->nhand[me]);
    /* the deal's plays replayed: who played what, on which trick */
    int start = 0;
    for (int i = 0; i < g->n; i++)
        if (TA_KIND(g->act[i]) == TA_NEXT) start = i + 1;
    tarot_start(t, g->players, g->seed, g->start);
    int plays = p->tricks * P;
    for (int i = 0, k = 0; i < g->n; i++) {
        int a = g->act[i];
        if (i >= start && TA_KIND(a) == TA_PLAY) {
            const tpos *q = &t->pos;
            int c = TA_ARG(a);
            if (c != TAROT_EXCUSE) {
                uint8_t tr[TAROT_MAXP], who[TAROT_MAXP];
                int m = 0;
                for (int j = 0; j < q->ntrick; j++)
                    if (q->trick[j] != TAROT_EXCUSE) tr[m] = q->trick[j], who[m++] = (uint8_t)((q->leader + j) % P);
                tb_note(v, q->turn, c, tr, who, m);
            }
            if (k++ < plays) tb_seen(v, c);
        }
        tarot_play(t, a);
    }
    for (int j = 0; j < p->ntrick; j++) tb_trick_add(v, (p->leader + j) % P, p->trick[j]);
    /* the aside: the taker's own; the trumps shown in it; the dog turned up, where its cards went */
    if (shown_dog) {
        for (int i = 0; i < p->necart; i++) {
            int c = p->ecart[i];
            if (me != taker && CARD_SUIT(c) != TRUMP) continue;
            tb_seen(v, c);
            x->aside_half = (int16_t)(x->aside_half + tarot_value2(c));
            x->aside_oud |= (uint8_t)oud_bit(c);
        }
        if (me != taker) {
            int shown = 0;
            for (int i = 0; i < p->necart; i++) shown += CARD_SUIT(p->ecart[i]) == TRUMP;
            tb_hidden(v, ecart_seat, p->necart - shown);
            v->cap[ecart_seat][TRUMP] = 0;      /* no trump, king nor Excuse put aside unseen */
            tb_exclude(v, ecart_seat, TAROT_EXCUSE);
            for (int s = 0; s < 4; s++) tb_exclude(v, ecart_seat, CARD(s, TAROT_KING));
            for (int i = 0; i < p->dogsize; i++) {
                int c = p->dog[i], gone = in(v->gone, v->ngone, c) || in(v->trick, v->ntrick, c);
                if (gone || (CARD_SUIT(c) == TRUMP && in(p->ecart, p->necart, c))) continue;
                if (CARD_SUIT(c) == TRUMP || CARD_RANK(c) == TAROT_KING) tb_known(v, taker, c);
                else
                    for (int q = 0; q < P; q++)
                        if (q != taker) tb_exclude(v, q, c);
            }
        }
    }
    /* five players, the partner unknown: the taker did not call his own card; it was not in the dog seen */
    if (!known && me != taker) tb_exclude(v, taker, x->called);
    if (!known && ecart_seat != TB_NONE) tb_exclude(v, ecart_seat, x->called);
    /* the poignées shown: those trumps held until played; the lower ones not shown are not held */
    for (int q = 0; q < P; q++) {
        if (q == me || !p->nshown[q]) continue;
        int top = 0, excuse = 0;
        for (int i = 0; i < p->nshown[q]; i++) {
            int c = p->shown[q][i];
            if (c == TAROT_EXCUSE) excuse = 1;
            else if (CARD_RANK(c) > top) top = CARD_RANK(c);
            if (!in(v->gone, v->ngone, c) && !in(v->trick, v->ntrick, c) && !in(v->hand[q], v->nknown[q], c))
                tb_known(v, q, c);
        }
        for (int r = 1; r < top; r++)
            if (!in(p->shown[q], p->nshown[q], CARD(TRUMP, r))) tb_exclude(v, q, CARD(TRUMP, r));
        if (excuse) v->cap[q][TRUMP] = 0;       /* every trump it held was shown */
    }
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_TAROT, pack));
}

/* ---- the calls ----------------------------------------------------------------------------------------------- */
/* the hand's points, for four players' 18 cards (3 and 5 players: scaled) */
int tarotb_points(const uint8_t *h, int n, int handsize)
{
    int len[4] = {0}, king[4] = {0}, queen[4] = {0}, trumps = 0, pts = 0, petit = 0;
    for (int i = 0; i < n; i++) {
        int c = h[i], s = CARD_SUIT(c), r = CARD_RANK(c);
        if (c == TAROT_EXCUSE) { pts += 8; continue; }
        if (s == TRUMP) {
            trumps++;
            pts += 2 + (r >= 16 && r <= 20);
            if (r == 21) pts += 10;
            if (r == 1) petit = 1;
            continue;
        }
        len[s]++;
        if (r == TAROT_KING) pts += 6, king[s] = 1;
        else if (r == TAROT_QUEEN) pts += 3, queen[s] = 1;
        else if (r == TAROT_KNIGHT) pts += 2;
        else if (r == TAROT_JACK) pts += 1;
    }
    float sc = 18.0f / (float)handsize;         /* the counts below for 18 cards */
    float tr = (float)trumps * sc;
    if (petit) pts += tr >= 6 ? 8 : tr >= 5 ? 5 : 0;
    for (int s = 0; s < 4; s++) {
        float l = (float)len[s] * sc;
        pts += king[s] && queen[s];
        pts += l >= 7 ? 9 : l >= 6 ? 7 : l >= 5 ? 5 : 0;
        if (tr >= 5) pts += len[s] == 0 ? 6 : len[s] == 1 ? 3 : 0;
    }
    return (int)lroundf((float)pts * (0.5f + 0.5f * sc));   /* half the scale: kings and oudlers are kings and
                                                             * oudlers in any hand */
}

/* Petite, Garde, Garde sans, Garde contre: the points from, per player count */
static const int BID_T[3][4] = {{42, 56, 72, 82}, {38, 52, 68, 78}, {34, 48, 64, 74}};

static int choose_bid(const tpos *p, int me)
{
    int pts = tarotb_points(p->hand[me], p->nhand[me], p->handsize), bid = TB_PASS;
    const int *t = BID_T[p->players - 3];
    for (int b = TB_CONTRE; b >= TB_PETITE && bid == TB_PASS; b--)
        if (pts >= t[b - 1]) bid = b;
    return bid > tarot_highest(p) ? bid : TB_PASS;
}

static int choose_call(const tpos *p, int me)
{
    int r = tarot_call_rank(p), best = -1, bs = -1;
    for (int s = 0; s < 4; s++) {
        if (in(p->hand[me], p->nhand[me], CARD(s, r))) continue;
        int v = 0, l = 0;
        for (int i = 0; i < p->nhand[me]; i++)
            if (CARD_SUIT(p->hand[me][i]) == s) v += tarot_value2(p->hand[me][i]), l++;
        if (v * 16 + l > bs) bs = v * 16 + l, best = s;
    }
    return best < 0 ? 0 : TACT(TA_CALL, CARD(best, r));
}

/* the discard planned from the hand and the dog together (the same plan at each card) */
int tarotb_discard_plan(const tpos *p, uint8_t *plan)
{
    int me = p->taker, ds = p->dogsize, n = 0, k = 0;
    uint8_t H[TAROT_HAND], used[TAROT_HAND] = {0};
    for (int i = 0; i < p->nhand[me]; i++) H[n++] = p->hand[me][i];
    for (int i = 0; i < p->necart; i++) H[n++] = p->ecart[i];
    int len[4] = {0}, king[4] = {0}, val[4] = {0}, voided[4] = {0};
    for (int i = 0; i < n; i++) {
        int s = CARD_SUIT(H[i]);
        if (s == TRUMP) continue;
        len[s]++, val[s] += tarot_value2(H[i]);
        if (CARD_RANK(H[i]) == TAROT_KING) king[s] = 1;
    }
    for (;;) {                                  /* the voids: the shortest suits without a king */
        int b = -1;
        for (int s = 0; s < 4; s++)
            if (!king[s] && !voided[s] && len[s] && len[s] <= ds - k &&
                (b < 0 || len[s] < len[b] || (len[s] == len[b] && val[s] > val[b])))
                b = s;
        if (b < 0) break;
        voided[b] = 1;
        for (int i = 0; i < n; i++)
            if (CARD_SUIT(H[i]) == b) plan[k++] = H[i], used[i] = 1;
    }
    while (k < ds) {                            /* then points out of danger, or the lowest under a king */
        int b = -1, bs = 0;
        for (int i = 0; i < n; i++) {
            int c = H[i], s = CARD_SUIT(c);
            if (used[i] || s == TRUMP || CARD_RANK(c) == TAROT_KING) continue;
            int sc = king[s] ? 100 - CARD_RANK(c) - 4 * len[s] : 1000 + tarot_value2(c) * 20 - 4 * len[s] - CARD_RANK(c);
            if (b < 0 || sc > bs) b = i, bs = sc;
        }
        if (b < 0) break;
        plan[k++] = H[b], used[b] = 1;
    }
    for (int r = 2; r <= 20 && k < ds; r++)     /* trumps only when nothing else may go */
        for (int i = 0; i < n && k < ds; i++)
            if (!used[i] && H[i] == CARD(TRUMP, r)) plan[k++] = H[i], used[i] = 1;
    return k;
}

static int choose_discard(const tpos *p)
{
    if (p->necart >= p->dogsize) return TACT(TA_DONE, 0);
    uint8_t plan[TAROT_HAND];
    int k = tarotb_discard_plan(p, plan);
    for (int i = 0; i < k; i++)
        if (!in(p->ecart, p->necart, plan[i]) && tarot_legal(p, TACT(TA_DISCARD, plan[i])))
            return TACT(TA_DISCARD, plan[i]);
    return 0;
}

/* a poignée before the seat's first card: the largest it holds, if its side looks like winning (0: none) */
static int choose_poignee(const tgame *g, int me, tb_state *v, tgame *t, uint64_t seed)
{
    const tpos *p = &g->pos;
    int level = 0;
    for (int l = 3; l >= 1 && !level; l--)
        if (tarot_legal(p, TACT(TA_POIGNEE, l))) level = l;
    if (!level) return 0;
    tarotb_view(g, me, v, t);
    ai_rng r;
    ai_seed(&r, ai_mix(seed, 0x9019ee + (uint64_t)me));
    float rw[TB_MAXP];
    tb_simulate(v, 40, &r, NULL, NULL, rw);
    return rw[me] >= (v->team[me] == ATT ? 0.5f : 0.6f) ? level : 0;
}

/* the action for seat at a level (0: nothing to do) */
int tarotb_choose(const tgame *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const tpos *p = &g->pos;
    if (stats) memset(stats, 0, sizeof *stats);
    if (p->phase == TP_SCORE || p->phase == TP_VOID) return TACT(TA_NEXT, 0);
    if (p->phase == TP_OVER || p->turn != seat) return 0;
    int a = 0;
    switch (p->phase) {
    case TP_BID: a = TACT(TA_BID, choose_bid(p, seat)); break;
    case TP_CALL: a = choose_call(p, seat); break;
    case TP_DOG: a = TACT(TA_TAKE, 0); break;
    case TP_DISCARD: a = choose_discard(p); break;
    case TP_PLAY: {
        tb_state *v = malloc(sizeof *v);
        tgame *t = malloc(sizeof *t);
        if (v && t) {
            int l = choose_poignee(g, seat, v, t, seed);
            if (l) a = TACT(TA_POIGNEE, l);
            else if (!ai_cancelled()) {
                tarotb_view(g, seat, v, t);
                int c = tb_choose(v, level, seed, TAROT_HARD_ITERS, stats);
                if (c != TB_NONE) a = TACT(TA_PLAY, c);
            }
        }
        free(v);
        free(t);
        break;
    }
    }
    return a && tarot_legal(p, a) ? a : tarot_auto(p);
}

/* a card in the notation: s, h, d, c and the rank; a and the trump's number; e the Excuse */
void tarotb_name(int c, char *out)
{
    if (c == TAROT_EXCUSE) snprintf(out, 8, "e");
    else snprintf(out, 8, "%c%d", "shdcaeee"[CARD_SUIT(c) & 7], CARD_RANK(c));
}

/* an action in the notation (0: none) */
int tarotb_format(int a, char *move, int cap)
{
    char c[8];
    tarotb_name(TA_ARG(a), c);
    switch (TA_KIND(a)) {
    case TA_BID: return TA_ARG(a) ? snprintf(move, (size_t)cap, "b%d", TA_ARG(a)) : snprintf(move, (size_t)cap, "p");
    case TA_CALL: return snprintf(move, (size_t)cap, "k%s", c);
    case TA_TAKE: return snprintf(move, (size_t)cap, "t");
    case TA_DISCARD: return snprintf(move, (size_t)cap, "x%s", c);
    case TA_RETURN: return snprintf(move, (size_t)cap, "r%s", c);
    case TA_DONE: return snprintf(move, (size_t)cap, "f");
    case TA_POIGNEE: return snprintf(move, (size_t)cap, "g%d", TA_ARG(a));
    case TA_PLAY: return snprintf(move, (size_t)cap, "%s", c);
    case TA_NEXT: return snprintf(move, (size_t)cap, "n");
    default: return 0;
    }
}

static int think(int seat, int level, char *move, int cap)
{
    tgame *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *tarot_live();                         /* the copy, at entry: nothing else is read */
    int a = tarotb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    return tarotb_format(a, move, cap) > 0;
}

static int actor(void)
{
    const tpos *p = &tarot_live()->pos;
    return p->phase == TP_BID || p->phase == TP_CALL || p->phase == TP_DOG || p->phase == TP_DISCARD ||
           p->phase == TP_PLAY ? p->turn : -1;
}

static const bot tarot_bot = {.think = think, .actor = actor, .play = tarot_show};
REGISTER_BOT(tarot, &tarot_bot)
