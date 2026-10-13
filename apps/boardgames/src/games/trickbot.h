/*
 * The trick-taking bots' shared core: the card play of any trick game, searched by the AI kit's information-set Monte
 * Carlo tree search (ai_mcts with determinise()), with heuristic playouts. No SDL; static inline, so a game's bot
 * file takes it alone (a single game's build links). Used by Hearts, Spades, Rikiki, Barbu, Euchre, Bridge, Briscola
 * and Manille; written for any trick game (trumps or none, any ranking, partnerships or each for himself, card points
 * or tricks, a stock to draw from, a seat sitting out, cards out of play).
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * ==== How a game uses it ===========================================================================================
 * 1. Rules: one `static const tb_rules R = {...}`: the callbacks below that differ from the plain game (follow suit,
 *    the highest card of the suit led wins, a trump over any other, nothing counted but tricks). Each callback gets
 *    the state; the game's own counters live in st->x (TB_XS(st, mytype): a struct of the game's, TB_X bytes at most:
 *    bids, the contract, hearts broken...).
 * 2. The view: the bot's knowledge as a tb_state, built from a COPY of the rules' position (bots.h's thread rules):
 *        tb_state v;
 *        tb_init(&v, &R, players, me);               the seat thinking ("me": whose knowledge the deal respects)
 *        v.team[s] = ..., v.trump = ..., v.sitting = ..., v.turn, v.leader, v.tricks, v.pts[], v.won[], x fields;
 *        tb_mine(&v, hand, n);                       the observer's own hand (all known)
 *        tb_hidden(&v, seat, n);                     another seat holds n cards...
 *        tb_known(&v, seat, card);                   ...of which the observer knows these (the dummy, a turned card)
 *        tb_trick_add(&v, seat, card);               the trick on the table so far, in order
 *        tb_seen(&v, card);                          every card the observer has seen leave play (tricks played,
 *                                                    known discards)
 *        tb_note(&v, seat, card, trick, who, n);     replaying the deal's plays: what each says (a suit not followed:
 *                                                    void; the game's infer(): "did not beat, so holds nothing over")
 *        tb_stock_set(&v, n, known_bottom...);       a stock to draw from (briscola), its known cards at the bottom
 *        tb_finish(&v, pack, npack);                 the unseen cards: the pack minus all the observer has seen
 *    The unseen cards are then dealt at random to the unknown places (the others' hands, the stock, the cards out
 *    of play: st->rest) on each search iteration, consistently with the voids and caps. The view never holds another
 *    seat's real cards: a game builds it from public facts (the deal's actions replayed, the table) and its own hand.
 * 3. Search: tb_choose(&v, level, seed, hard_iters, NULL) gives the card to play (TB_NONE: none). The seat to move
 *    (v.turn) may differ from v.me (a bridge declarer plays the dummy): the search plays for v.turn with v.me's
 *    knowledge. Partners share a reward: they play for their side.
 * 4. Calls (bids, contracts, doubles): the game's own rules from hand strength; tb_simulate() estimates the tricks,
 *    points and reward each seat can expect from a view (heuristic playouts over determinised deals) when a call
 *    needs it.
 *
 * ==== The callbacks (tb_rules; NULL: the plain rule) ===============================================================
 *   suit(st, card)            the suit it follows as (euchre's left bower, schafkopf's obers: trumps; tarot's
 *                             trumps); NULL: CARD_SUIT. Suits are 0..TB_SUITS-1.
 *   power(st, card, led)      its strength in a trick whose led suit is `led` (suit() of the first card): 0 it can
 *                             not win (another suit, not a trump), else the higher wins, the first played among
 *                             equals. NULL: tb_power_std (trick_rank in the suit led, a trump 100 + trick_rank).
 *   order(st, card)           its rank inside its own suit, 1..254, for the caps (NULL: power(card, its suit)).
 *   legal(st, hand, n, out)   the cards the seat to move may play (out: TB_HAND); NULL: follow suit if you can.
 *   points(st, card)          its card points (scaled to ints: tarot's halves x2); NULL: none (tricks only).
 *   trick_end(st, winner)     after a trick (pts[] and won[] already counted, the trick still in st->trick):
 *                             the game's own counts (a trick's bonus, hearts broken, the last trick).
 *   over(st)                  1: the deal stops before the hands are out (barbu: the last queen taken).
 *   reward(st, rw)            the deal over: each seat's reward 0..1 (rw[players]); partners alike. Required.
 *   want(st, seat)            for the playouts: +1 the seat wants tricks now, -1 it avoids them, 0 indifferent.
 *                             NULL: +1.
 *   infer(st, seat, card, trick, who, n)   a play's meaning beyond a void (tb_note calls it before adding the card):
 *                             set st->cap[seat][suit] (the seat holds no card of that suit whose order() is above),
 *                             or tb_exclude(st, seat, card) (it does not hold that card). Caps are in order() units:
 *                             give order() (the rank in the suit) when power() lifts trumps over the rest.
 *   dealt(st)                 after each determinisation: what the game derives from the hands (hidden partners).
 *   draw                      1: after each trick each seat draws one card from the stock, the winner first.
 *   nofollow                  1: any card may be played, suit led or not (briscola): no voids to read, and any
 *                             opponent holding a trump may take a side card.
 *
 * ==== The state ====================================================================================================
 * Cards are cards.h's bytes; duplicates are allowed (a 48-card pack of pairs). hand[s][0..nknown[s]-1] are the
 * cards the observer knows seat s holds; n[s] - nknown[s] more are unknown (in a determinised state all are known).
 * team[s] the side of each seat (by default each its own); sitting a seat out of play (TB_NONE); pts[] card points
 * and won[] tricks taken this deal, per seat; trump the trump suit for tb_power_std (TB_NONE: none). After a
 * determinisation, rest[] holds the cards out of play as dealt (a skat or a dog the reward may count), and dealt()
 * may set team[] from the hands (partners known only by their cards). The state is copied whole on every iteration
 * (about 900 bytes): keep x small.
 *
 * ==== Other helpers ================================================================================================
 *   tb_legal(st, out)          the seat to move's legal cards (no repeats); tb_legal_follow() the plain rule
 *   tb_best(st)                the index in the trick of the card holding it; tb_led(st) the suit led
 *   tb_play(st, card)          plays a card for the seat to move (the trick gathered, the stock drawn)
 *   tb_over(st)                the deal is over
 *   tb_determinise(st, rng)    deals the unseen cards (a copy of a view)
 *   tb_heuristic(st, L, n, r)  the playouts' choice among the legal cards L (a determinised state)
 *   tb_level(level, hard)      the iterations, playout noise and slips for AI_EASY..AI_HARD
 *   tb_simulate(v, k, r, won, pts, rw)   averages over k dealt and played-out deals (calls, bids)
 *   tb_hcp(), tb_count_suit()  hand counting for the calls
 * Levels: hard runs the game's hard_iters (calibrated to about 0.3 s a move on a PC), medium a sixth with noisier
 * playouts, easy a fortieth (at least 60) with noisy playouts and a quarter of its moves the heuristic's, noisier
 * (slips); the AI kit's ms caps stay the safety net. A test reveals a view with tt_reveal() (tests/ai_tricks.h) to
 * check the game's rules against the core's, card by card, and measures the levels on duplicate deals (tt_run).
 */
#ifndef TRICKBOT_H
#define TRICKBOT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ai.h"
#include "cards.h"
#include "games/tricks.h"

#define TB_MAXP 6                               /* seats at a table */
#define TB_HAND 24                              /* cards in a hand at most (tarot for three: 24) */
#define TB_PACK 112                             /* cards in a pack at most (tarot 78, doppelkopf 48) */
#define TB_STOCK 48                             /* a stock to draw from; cards out of play (rest) */
#define TB_SUITS 6
#define TB_X 64                                 /* the game's own bytes */
#define TB_NONE 255
#define TB_NOCAP 255

typedef struct tb_state tb_state;

typedef struct tb_rules {
    int  (*suit)(const tb_state *st, int card);
    int  (*power)(const tb_state *st, int card, int led);
    int  (*order)(const tb_state *st, int card);
    int  (*legal)(const tb_state *st, const uint8_t *hand, int n, uint8_t *out);
    int  (*points)(const tb_state *st, int card);
    void (*trick_end)(tb_state *st, int winner);
    int  (*over)(const tb_state *st);
    void (*reward)(const tb_state *st, float *rw);
    int  (*want)(const tb_state *st, int seat);
    void (*infer)(tb_state *st, int seat, int card, const uint8_t *trick, const uint8_t *who, int n);
    void (*dealt)(tb_state *st);
    int  draw;
    int  nofollow;
} tb_rules;

struct tb_state {
    const tb_rules *R;                          /* the game's rules: a static const table (no live data) */
    uint8_t players, me;                        /* the seats; the observer */
    uint8_t turn, leader, ntrick, tricks;       /* the seat to move; the trick's leader; cards in it; tricks done */
    uint8_t sitting, trump, full, eps;          /* full: determinised; eps: the playouts' random moves, percent */
    uint8_t depth, maxdepth;                    /* the search: plies from its root; branching stops at maxdepth */
    uint8_t team[TB_MAXP];
    uint8_t trick[TB_MAXP], who[TB_MAXP];       /* the trick on the table and who played each */
    uint8_t hand[TB_MAXP][TB_HAND], n[TB_MAXP], nknown[TB_MAXP];
    uint8_t cap[TB_MAXP][TB_SUITS];             /* seat s holds no card of suit u with order() over cap (0: void) */
    uint8_t ex[TB_MAXP][32];                    /* seat s holds no card c with bit c set (tb_exclude) */
    uint8_t stock[TB_STOCK], nstock, nstock_known;   /* drawn from the end; stock[0..nstock_known-1] known */
    uint8_t rest[TB_STOCK], nrest;              /* cards out of play (a kitty, the skat): unknown slots, then dealt */
    uint8_t unseen[TB_PACK], nunseen;           /* the view: the cards the observer has not seen (tb_finish) */
    uint8_t gone[TB_PACK], ngone;               /* the view: cards seen leaving play (tb_seen) */
    int16_t pts[TB_MAXP];                       /* card points taken this deal */
    uint8_t won[TB_MAXP];                       /* tricks taken this deal */
    uint8_t last_winner;
    _Alignas(8) uint8_t x[TB_X];                /* the game's own state */
};

#define TB_XS(st, type) ((type *)(void *)(st)->x)
#define TB_CXS(st, type) ((const type *)(const void *)(st)->x)

/* ---- the plain rules ------------------------------------------------------------------------------------------ */
static inline int tb_suit(const tb_state *st, int c) { return st->R->suit ? st->R->suit(st, c) : CARD_SUIT(c); }

static inline int tb_power_std(const tb_state *st, int c, int led)
{
    int s = tb_suit(st, c);
    if (st->trump != TB_NONE && s == st->trump) return 100 + trick_rank(c);
    return s == led ? trick_rank(c) : 0;
}

static inline int tb_power(const tb_state *st, int c, int led)
{
    return st->R->power ? st->R->power(st, c, led) : tb_power_std(st, c, led);
}

static inline int tb_order(const tb_state *st, int c)
{
    return st->R->order ? st->R->order(st, c) : tb_power(st, c, tb_suit(st, c));
}

static inline int tb_points(const tb_state *st, int c) { return st->R->points ? st->R->points(st, c) : 0; }

static inline int tb_want(const tb_state *st, int seat) { return st->R->want ? st->R->want(st, seat) : 1; }

static inline int tb_is_trump(const tb_state *st, int c) { return st->trump != TB_NONE && tb_suit(st, c) == st->trump; }

/* the led suit (-1: no card yet) */
static inline int tb_led(const tb_state *st) { return st->ntrick ? tb_suit(st, st->trick[0]) : -1; }

/* the index in the trick of the card that holds it (-1: empty) */
static inline int tb_best(const tb_state *st)
{
    if (!st->ntrick) return -1;
    int led = tb_led(st), b = 0, bp = tb_power(st, st->trick[0], led);
    for (int i = 1; i < st->ntrick; i++) {
        int p = tb_power(st, st->trick[i], led);
        if (p > bp) b = i, bp = p;
    }
    return b;
}

/* the seats in play: all but the one sitting out */
static inline int tb_active(const tb_state *st) { return st->players - (st->sitting != TB_NONE); }

static inline int tb_next(const tb_state *st, int seat)
{
    seat = (seat + 1) % st->players;
    return seat == st->sitting ? (seat + 1) % st->players : seat;
}

/* follow suit if you can (the plain rule) */
static inline int tb_legal_follow(const tb_state *st, const uint8_t *h, int n, uint8_t *out)
{
    int k = 0, led = st->R->nofollow ? -1 : tb_led(st);
    if (led >= 0)
        for (int i = 0; i < n; i++)
            if (tb_suit(st, h[i]) == led) out[k++] = h[i];
    if (!k)
        for (int i = 0; i < n; i++) out[k++] = h[i];
    return k;
}

/* the legal cards of the seat to move, without repeats (a pack of pairs) */
static inline int tb_legal(const tb_state *st, uint8_t *out)
{
    const uint8_t *h = st->hand[st->turn];
    int n = st->n[st->turn];
    uint8_t tmp[TB_HAND];
    int k = st->R->legal ? st->R->legal(st, h, n, tmp) : tb_legal_follow(st, h, n, tmp), m = 0;
    for (int i = 0; i < k; i++) {
        int dup = 0;
        for (int j = 0; j < m && !dup; j++) dup = out[j] == tmp[i];
        if (!dup) out[m++] = tmp[i];
    }
    return m;
}

/* the deal is over: the hands out (and the stock), or the game's early stop */
static inline int tb_over(const tb_state *st)
{
    if (st->R->over && st->R->over(st)) return 1;
    if (st->ntrick) return 0;
    for (int s = 0; s < st->players; s++)
        if (s != st->sitting && st->n[s]) return 0;
    return 1;
}

/* ---- playing a card (the search's make) ------------------------------------------------------------------------- */
static inline void tb_take(tb_state *st, int seat, int card)   /* out of the seat's hand (an unknown slot if need be) */
{
    uint8_t *h = st->hand[seat];
    for (int i = 0; i < st->n[seat]; i++)
        if (h[i] == card) {
            memmove(h + i, h + i + 1, (size_t)(st->n[seat] - i - 1));
            st->n[seat]--;
            if (i < st->nknown[seat]) st->nknown[seat]--;
            return;
        }
    if (st->n[seat] > st->nknown[seat]) st->n[seat]--;
}

static inline void tb_give(tb_state *st, int seat, int card, int known)   /* into the hand (known: in the view) */
{
    if (st->n[seat] >= TB_HAND) return;
    if (known || st->full) {
        uint8_t *h = st->hand[seat];
        int k = st->nknown[seat];
        memmove(h + k + 1, h + k, (size_t)(st->n[seat] - k));
        h[k] = (uint8_t)card;
        st->nknown[seat]++;
    }
    st->n[seat]++;
}

static inline void tb_play(tb_state *st, int card)
{
    int seat = st->turn;
    if (st->depth < 255) st->depth++;
    tb_take(st, seat, card);
    st->trick[st->ntrick] = (uint8_t)card;
    st->who[st->ntrick++] = (uint8_t)seat;
    if (st->ntrick < tb_active(st)) { st->turn = (uint8_t)tb_next(st, seat); return; }
    int b = tb_best(st), w = st->who[b], p = 0;
    for (int i = 0; i < st->ntrick; i++) p += tb_points(st, st->trick[i]);
    st->pts[w] = (int16_t)(st->pts[w] + p);
    st->won[w]++;
    st->last_winner = (uint8_t)w;
    if (st->R->trick_end) st->R->trick_end(st, w);
    st->ntrick = 0;
    st->tricks++;
    if (st->R->draw)
        for (int k = 0, q = w; k < st->players && st->nstock; k++, q = (q + 1) % st->players) {
            if (q == st->sitting) continue;
            int known = st->nstock <= st->nstock_known;
            uint8_t c = st->stock[--st->nstock];
            if (st->nstock_known > st->nstock) st->nstock_known = st->nstock;
            tb_give(st, q, c, known);
        }
    st->turn = st->leader = (uint8_t)w;
}

/* ---- building the view ---------------------------------------------------------------------------------------- */
static inline void tb_init(tb_state *st, const tb_rules *R, int players, int me)
{
    memset(st, 0, sizeof *st);
    st->R = R;
    st->players = (uint8_t)(players > TB_MAXP ? TB_MAXP : players);
    st->me = (uint8_t)me;
    st->sitting = st->trump = st->last_winner = TB_NONE;
    st->eps = 8;
    for (int s = 0; s < TB_MAXP; s++) st->team[s] = (uint8_t)s;
    memset(st->cap, TB_NOCAP, sizeof st->cap);
}

static inline void tb_mine(tb_state *st, const uint8_t *hand, int n)
{
    int s = st->me;
    n = n > TB_HAND ? TB_HAND : n;
    memcpy(st->hand[s], hand, (size_t)n);
    st->n[s] = st->nknown[s] = (uint8_t)n;
}

static inline void tb_hidden(tb_state *st, int seat, int n)
{
    st->n[seat] = (uint8_t)(n > TB_HAND ? TB_HAND : n);
    st->nknown[seat] = 0;
}

static inline void tb_known(tb_state *st, int seat, int card)
{
    if (st->nknown[seat] < st->n[seat]) st->hand[seat][st->nknown[seat]++] = (uint8_t)card;
}

static inline void tb_trick_add(tb_state *st, int seat, int card)
{
    if (st->ntrick >= TB_MAXP) return;
    st->trick[st->ntrick] = (uint8_t)card;
    st->who[st->ntrick++] = (uint8_t)seat;
}

static inline void tb_seen(tb_state *st, int card)
{
    if (st->ngone < TB_PACK) st->gone[st->ngone++] = (uint8_t)card;
}

/* what a play showed: the seat does not hold this card (infer()) */
static inline void tb_exclude(tb_state *st, int seat, int card)
{
    st->ex[seat][(card & 255) >> 3] |= (uint8_t)(1u << (card & 7));
}

/* the deal's plays replayed, one by one, with the trick as it stood before the card (who: the seats): a suit not
 * followed shows a void; the game's infer() reads the rest */
static inline void tb_note(tb_state *st, int seat, int card, const uint8_t *trick, const uint8_t *who, int n)
{
    if (seat < 0 || seat >= st->players) return;
    if (n > 0 && !st->R->nofollow) {
        int led = tb_suit(st, trick[0]);
        if (tb_suit(st, card) != led && led < TB_SUITS) st->cap[seat][led] = 0;
    }
    if (st->R->infer) st->R->infer(st, seat, card, trick, who, n);
}

/* a stock of n cards, drawn from its end; the first k (the bottom: briscola's turned card) known */
static inline void tb_stock_set(tb_state *st, int n, const uint8_t *known_bottom, int k)
{
    st->nstock = (uint8_t)(n > TB_STOCK ? TB_STOCK : n);
    st->nstock_known = (uint8_t)(k > st->nstock ? st->nstock : k);
    for (int i = 0; i < st->nstock_known; i++) st->stock[i] = known_bottom[i];
}

/* the unseen cards: the pack (the cards in play this deal) minus the observer's hand, the cards it knows elsewhere,
 * the trick and the cards gone. What is left, beyond the unknown hand and stock slots, is out of play (rest). */
static inline void tb_finish(tb_state *st, const uint8_t *pack, int npack)
{
    uint8_t cnt[256];
    memset(cnt, 0, sizeof cnt);
    for (int i = 0; i < npack; i++) cnt[pack[i]]++;
#define TB_DROP(c) do { if (cnt[(c)]) cnt[(c)]--; } while (0)
    for (int s = 0; s < st->players; s++)
        for (int i = 0; i < st->nknown[s]; i++) TB_DROP(st->hand[s][i]);
    for (int i = 0; i < st->ntrick; i++) TB_DROP(st->trick[i]);
    for (int i = 0; i < st->ngone; i++) TB_DROP(st->gone[i]);
    for (int i = 0; i < st->nstock_known; i++) TB_DROP(st->stock[i]);
#undef TB_DROP
    st->nunseen = 0;
    for (int c = 0; c < 256; c++)
        for (int k = 0; k < cnt[c] && st->nunseen < TB_PACK; k++) st->unseen[st->nunseen++] = (uint8_t)c;
    int slots = st->nstock - st->nstock_known;
    for (int s = 0; s < st->players; s++) slots += st->n[s] - st->nknown[s];
    st->nrest = (uint8_t)(st->nunseen > slots ? st->nunseen - slots : 0);
    if (st->nrest > TB_STOCK) st->nrest = TB_STOCK;
}

/* ---- the determinisation ---------------------------------------------------------------------------------------- */
static inline int tb_fits(const tb_state *st, int seat, int c)
{
    int u = tb_suit(st, c);
    if (st->ex[seat][(c & 255) >> 3] >> (c & 7) & 1) return 0;
    return u >= TB_SUITS || st->cap[seat][u] == TB_NOCAP || tb_order(st, c) <= st->cap[seat][u];
}

/* deals the unseen cards to the unknown places: the others' hands (within their voids and caps), the stock, the cards
 * out of play; the hardest cards first, each to a place at random weighted by its room. Retried a few times, then
 * the caps are let go (an impossible view: never stall). */
static inline void tb_determinise(tb_state *st, ai_rng *r)
{
    if (st->full) return;
    int nu = st->nunseen, need[TB_MAXP + 2], P = st->players;   /* places: the seats, the stock (P), the rest */
    uint8_t pool[TB_PACK], ord[TB_PACK], elig[TB_PACK], to[TB_PACK];
    memcpy(pool, st->unseen, (size_t)nu);
    for (int i = nu - 1; i > 0; i--) {
        int j = ai_rand_n(r, i + 1);
        uint8_t t = pool[i]; pool[i] = pool[j]; pool[j] = t;
    }
    for (int attempt = 0; attempt < 12; attempt++) {
        int strict = attempt < 10, ok = 1;
        for (int s = 0; s < P; s++) need[s] = s == st->me ? 0 : st->n[s] - st->nknown[s];
        need[P] = st->nstock - st->nstock_known;
        need[P + 1] = st->nrest;
        for (int i = 0; i < nu; i++) {          /* how many seats may hold each card */
            int e = 0;
            for (int s = 0; s < P; s++) e += need[s] > 0 && (!strict || tb_fits(st, s, pool[i]));
            elig[i] = (uint8_t)e;
            ord[i] = (uint8_t)i;
        }
        for (int i = 1; i < nu; i++)            /* the fewest places first (stable: the shuffle breaks ties) */
            for (int j = i; j > 0 && elig[ord[j]] < elig[ord[j - 1]]; j--) {
                uint8_t t = ord[j]; ord[j] = ord[j - 1]; ord[j - 1] = t;
            }
        for (int k = 0; k < nu && ok; k++) {
            int i = ord[k], c = pool[i], room = 0;
            for (int s = 0; s <= P + 1; s++)
                if (need[s] > 0 && (s >= P || !strict || tb_fits(st, s, c))) room += need[s];
            if (!room) { ok = 0; break; }
            int x = ai_rand_n(r, room), s = 0;
            for (; s <= P + 1; s++) {
                if (need[s] <= 0 || (s < P && strict && !tb_fits(st, s, c))) continue;
                if (x < need[s]) break;
                x -= need[s];
            }
            to[i] = (uint8_t)s;
            need[s]--;
        }
        if (!ok) continue;
        int ns = st->nstock_known, nr = 0;
        for (int i = 0; i < nu; i++) {
            int s = to[i];
            if (s < P) st->hand[s][st->nknown[s]++] = pool[i];
            else if (s == P) st->stock[ns++] = pool[i];
            else st->rest[nr++] = pool[i];
        }
        st->nstock = (uint8_t)ns;               /* more slots than unseen cards (a faulty view): the places shrink */
        st->nrest = (uint8_t)nr;
        break;
    }
    for (int s = 0; s < P; s++) st->n[s] = st->nknown[s];
    st->nstock_known = st->nstock;
    st->full = 1;
    if (st->R->dealt) st->R->dealt(st);
}

/* ---- the playouts' heuristic ---------------------------------------------------------------------------------------
 * Leading, wanting tricks: cash a sure winner of a side suit; draw trumps while holding the most of them and the
 * best; else lead low from the longest side suit. Following: partner holds the trick (and nobody after can take it):
 * give it points or play low; else win with the cheapest card nobody after can beat (not a trump for a trick worth
 * nothing in a game of points); else play low (the fewest points, a side suit's lowest). Avoiding tricks: lead the
 * card most others can beat; follow with the highest card that stays under; void: shed the most points, the highest.
 * In a determinised state every hand is known: the "sure" winners are those of the sampled world. */
static inline int tb_rank_of(const tb_state *st, int c) { return tb_power(st, c, tb_suit(st, c)); }

static inline int tb_opponent(const tb_state *st, int a, int b) { return st->team[a] != st->team[b]; }

/* no other seat in play holds a card of its suit over it (nor, for a side suit, could an opponent void in it trump) */
static inline int tb_master(const tb_state *st, int c)
{
    int me = st->turn, u = tb_suit(st, c), p = tb_power(st, c, u), trump = tb_is_trump(st, c);
    for (int q = 0; q < st->players; q++) {
        if (q == me || q == st->sitting) continue;
        int has = 0, trumps = 0;
        for (int i = 0; i < st->n[q]; i++) {
            int d = st->hand[q][i], du = tb_suit(st, d);
            if (du == u) {
                has = 1;
                if (tb_power(st, d, u) > p) return 0;
            } else if (tb_is_trump(st, d)) trumps = 1;
        }
        if (!trump && (!has || st->R->nofollow) && trumps && tb_opponent(st, me, q)) return 0;
    }
    return 1;
}

/* a seat still to play in this trick could beat power p (led suit led) */
static inline int tb_beatable(const tb_state *st, int p, int led, int opponents_only)
{
    int me = st->turn, left = tb_active(st) - st->ntrick - 1;
    for (int q = tb_next(st, me), k = 0; k < left; k++, q = tb_next(st, q)) {
        if (opponents_only && !tb_opponent(st, me, q)) continue;
        int has_led = 0, best_led = 0, best_other = 0;
        for (int i = 0; i < st->n[q]; i++) {
            int d = st->hand[q][i], dp = tb_power(st, d, led);
            if (tb_suit(st, d) == led) { has_led = 1; if (dp > best_led) best_led = dp; }
            else if (dp > best_other) best_other = dp;
        }
        if ((has_led && !st->R->nofollow ? best_led : best_led > best_other ? best_led : best_other) > p) return 1;
    }
    return 0;
}

static inline int tb_heuristic(const tb_state *st, const uint8_t *L, int n, ai_rng *r)
{
    if (n == 1) return L[0];
    if (ai_rand_n(r, 100) < st->eps) return L[ai_rand_n(r, n)];
    int me = st->turn, want = tb_want(st, me), pointy = st->R->points != NULL;
    int best = -1;
    long bs = -(1L << 40);
#define TB_PICK(score) do { long s_ = (score); if (s_ > bs) { bs = s_; best = i; } } while (0)
    if (!st->ntrick) {                          /* leading */
        int len[TB_SUITS] = {0}, mytr = 0, optr = 0;
        const uint8_t *h = st->hand[me];
        for (int i = 0; i < st->n[me]; i++) {
            int u = tb_suit(st, h[i]);
            if (u < TB_SUITS) len[u]++;
            mytr += tb_is_trump(st, h[i]);
        }
        if (want > 0) {
            for (int q = 0; q < st->players; q++)
                if (q != st->sitting && tb_opponent(st, me, q))
                    for (int i = 0; i < st->n[q]; i++) optr += tb_is_trump(st, st->hand[q][i]);
            for (int i = 0; i < n; i++) {
                int c = L[i], u = tb_suit(st, c), tr = tb_is_trump(st, c), m = tb_master(st, c), rk = tb_rank_of(st, c);
                long s;
                if (m && !tr) s = 3000 + tb_points(st, c) * 10 + rk;                /* cash a side winner */
                else if (tr && m && optr > 0 && mytr >= optr) s = 2500 + rk;         /* draw trumps */
                else if (tr) s = 500 - rk + (optr == 0 ? -400 : 0);                  /* keep the trumps */
                else s = 1000 + (u < TB_SUITS ? len[u] : 0) * 40 - rk - tb_points(st, c) * 8;   /* low from length */
                TB_PICK(s);
            }
        } else if (want < 0) {
            for (int i = 0; i < n; i++) {
                int c = L[i], u = tb_suit(st, c), p = tb_power(st, c, u), higher = 0;
                for (int q = 0; q < st->players; q++) {
                    if (q == me || q == st->sitting) continue;
                    for (int k = 0; k < st->n[q]; k++)
                        higher += tb_suit(st, st->hand[q][k]) == u && tb_power(st, st->hand[q][k], u) > p;
                }
                TB_PICK((higher > 0 ? 1000 : 0) + higher * 20 - p + tb_points(st, c) * (higher > 0 ? 5 : -50));
            }
        } else
            for (int i = 0; i < n; i++) TB_PICK(-tb_rank_of(st, L[i]));
        return L[best];
    }
    int led = tb_led(st), b = tb_best(st), holder = st->who[b], bp = tb_power(st, st->trick[b], led);
    int tp = 0, last = st->ntrick == tb_active(st) - 1;
    for (int i = 0; i < st->ntrick; i++) tp += tb_points(st, st->trick[i]);
    if (want > 0) {
        if (!tb_opponent(st, me, holder) && (last || !tb_beatable(st, bp, led, 1))) {
            for (int i = 0; i < n; i++) {       /* the partner holds it: points to it, or low */
                int c = L[i];
                TB_PICK(tb_points(st, c) * 100 - tb_power(st, c, led) * 2 - (tb_is_trump(st, c) ? 400 : 0)
                        - (tb_master(st, c) && !pointy ? 50 : 0));
            }
            return L[best];
        }
        for (int i = 0; i < n; i++) {           /* the cheapest sure winner */
            int c = L[i], p = tb_power(st, c, led);
            if (p <= bp || tb_beatable(st, p, led, 1)) continue;
            if (pointy && tp + tb_points(st, c) == 0 && tb_is_trump(st, c) && led != st->trump) continue;
            TB_PICK(10000 - p - tb_points(st, c) * 3);
        }
        if (best >= 0) return L[best];
        for (int i = 0; i < n; i++) {           /* low: the fewest points, a side suit's lowest */
            int c = L[i];
            TB_PICK(-tb_points(st, c) * 100 - tb_rank_of(st, c) - (tb_is_trump(st, c) ? 300 : 0));
        }
        return L[best];
    }
    if (want < 0) {
        for (int i = 0; i < n; i++) {           /* under the trick: the highest such; void: shed points, then height */
            int c = L[i], p = tb_power(st, c, led);
            if (p > bp) continue;
            int follows = tb_suit(st, c) == led;
            TB_PICK(follows ? 1000 + p : 2000 + tb_points(st, c) * 50 + tb_rank_of(st, c));
        }
        if (best >= 0) return L[best];
        for (int i = 0; i < n; i++) {           /* it must win: the highest if last or safe, else the lowest */
            int c = L[i], p = tb_power(st, c, led);
            TB_PICK(last ? p * 4 - tb_points(st, c) * 40 : -p - tb_points(st, c) * 40);
        }
        return L[best];
    }
    for (int i = 0; i < n; i++) TB_PICK(-tb_power(st, L[i], led));
#undef TB_PICK
    return L[best];
}

/* ---- the adapter (ai.h) ------------------------------------------------------------------------------------------ */
static inline int tb_a_moves(const void *s, int *out)
{
    const tb_state *st = s;
    if (tb_over(st)) return 0;
    uint8_t L[TB_HAND];
    int n = tb_legal(st, L);
    if (n > 1 && st->maxdepth && st->depth >= st->maxdepth) {   /* past the tree's depth: the heuristic's move */
        ai_rng r;
        uint64_t h = (uint64_t)st->hand[st->turn][0] << 8 | st->n[st->turn];
        ai_seed(&r, ai_mix((uint64_t)st->depth * 977 + st->turn, h));
        out[0] = tb_heuristic(st, L, n, &r);
        return 1;
    }
    for (int i = 0; i < n; i++) out[i] = L[i];
    return n;
}

static inline void tb_a_make(void *s, int m) { tb_play(s, m); }
static inline int tb_a_side(const void *s) { return ((const tb_state *)s)->turn; }

static inline int tb_a_result(const void *s, float *rw)
{
    const tb_state *st = s;
    if (!tb_over(st)) return 0;
    for (int i = 0; i < st->players; i++) rw[i] = 0.5f;
    st->R->reward(st, rw);
    for (int i = 0; i < st->players; i++) rw[i] = rw[i] < 0 ? 0 : rw[i] > 1 ? 1 : rw[i];
    return 1;
}

static inline int tb_a_playout(const void *s, ai_rng *r)
{
    const tb_state *st = s;
    uint8_t L[TB_HAND];
    int n = tb_legal(st, L);
    return n ? tb_heuristic(st, L, n, r) : AI_NONE;
}

static inline void tb_a_determinise(void *s, int seat, ai_rng *r)
{
    (void)seat;                                 /* the observer is st->me (a declarer plays his dummy) */
    tb_determinise(s, r);
}

/* ---- the levels ----------------------------------------------------------------------------------------------------
 * The search's iterations: hard_iters (the game's calibration: about 0.3 s on a PC), medium a sixth of it, easy a
 * small search with noisy playouts and, a quarter of the time, the playouts' heuristic move with much noise
 * (human-like slips). depth: the tree branches that many plies, then follows the heuristic (0: no limit; a flat or
 * shallow tree played no better than the full one in euchre, so every level uses 0). */
typedef struct { long iters; int eps, slip, depth; } tb_level_t;

static inline tb_level_t tb_level(int level, long hard_iters)
{
    if (level <= AI_EASY) return (tb_level_t){hard_iters / 40 > 60 ? hard_iters / 40 : 60, 30, 25, 0};
    if (level == AI_MEDIUM) return (tb_level_t){hard_iters / 6 > 300 ? hard_iters / 6 : 300, 12, 0, 0};
    return (tb_level_t){hard_iters, 6, 0, 0};
}

/* the card the seat to move plays (v: the observer's view; the seat to move is v->turn); TB_NONE: none */
static inline int tb_choose(const tb_state *v, int level, uint64_t seed, long hard_iters, ai_stats *stats)
{
    tb_state *root = malloc(sizeof *root);
    if (!root) return TB_NONE;
    *root = *v;
    tb_level_t lv = tb_level(level, hard_iters);
    root->eps = (uint8_t)lv.eps;
    root->depth = 0;
    root->maxdepth = (uint8_t)lv.depth;
    uint8_t L[TB_HAND];
    int n = tb_over(root) ? 0 : tb_legal(root, L), card = TB_NONE;
    ai_rng rng;
    ai_seed(&rng, ai_mix(seed, (uint64_t)root->tricks * 131 + root->ntrick * 7 + root->turn + (uint64_t)n * 977));
    if (stats) memset(stats, 0, sizeof *stats);
    if (n == 1) card = L[0];
    else if (n > 1 && lv.slip && ai_rand_n(&rng, 100) < lv.slip) {
        tb_state w = *root;                     /* a slip: the heuristic's move in one dealt world, noisy */
        tb_determinise(&w, &rng);
        w.eps = 40;
        card = tb_heuristic(&w, L, n, &rng);
    } else if (n > 1) {
        ai_game g = {
            .size = sizeof(tb_state), .moves = tb_a_moves, .make = tb_a_make, .side = tb_a_side,
            .players = root->players, .result = tb_a_result, .playout = tb_a_playout,
            .determinise = tb_a_determinise,
        };
        ai_opts o = {.budget = ai_level_budget(level), .rng = &rng, .uct_c = 0.7f};
        o.budget.nodes = lv.iters;
        int m = ai_mcts(&g, root, &o, stats);
        card = m == AI_NONE ? L[0] : m;
    }
    free(root);
    return card;
}

/* ---- estimates for calls -------------------------------------------------------------------------------------------
 * From a view, `samples` deals of the unseen cards played out by the heuristic (everyone by its want()): each seat's
 * average tricks, card points and reward (any of the outputs may be NULL). Calls and bids read their hand's worth
 * here when a count of honours is not enough. Stops early if cancelled. */
static inline void tb_simulate(const tb_state *v, int samples, ai_rng *r, float *won, float *pts, float *reward)
{
    float W[TB_MAXP] = {0}, Pt[TB_MAXP] = {0}, Rw[TB_MAXP] = {0};
    tb_state *w = malloc(sizeof *w);
    int done = 0;
    if (w)
        for (; done < samples && !ai_cancelled(); done++) {
            *w = *v;
            tb_determinise(w, r);
            uint8_t L[TB_HAND];
            for (int guard = 0; guard < 400 && !tb_over(w); guard++) {
                int n = tb_legal(w, L);
                if (!n) break;
                tb_play(w, tb_heuristic(w, L, n, r));
            }
            float rw[TB_MAXP];
            for (int s = 0; s < TB_MAXP; s++) rw[s] = 0.5f;
            if (tb_over(w)) w->R->reward(w, rw);
            for (int s = 0; s < w->players; s++) W[s] += w->won[s], Pt[s] += w->pts[s], Rw[s] += rw[s];
        }
    free(w);
    for (int s = 0; s < v->players; s++) {
        float d = done ? (float)done : 1;
        if (won) won[s] = W[s] / d;
        if (pts) pts[s] = Pt[s] / d;
        if (reward) reward[s] = Rw[s] / d;
    }
}

/* ---- small helpers for the games -------------------------------------------------------------------------------- */
static inline int tb_count_suit(const uint8_t *h, int n, int suit)
{
    int k = 0;
    for (int i = 0; i < n; i++) k += CARD_SUIT(h[i]) == suit;
    return k;
}

/* high-card points: ace 4, king 3, queen 2, jack 1 */
static inline int tb_hcp(const uint8_t *h, int n)
{
    int t = 0;
    for (int i = 0; i < n; i++) {
        int r = CARD_RANK(h[i]);
        t += r == RANK_ACE ? 4 : r >= RANK_JACK && r <= RANK_KING ? r - 10 : 0;
    }
    return t;
}

#endif
