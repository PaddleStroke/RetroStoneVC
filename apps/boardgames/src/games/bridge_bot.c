/*
 * Contract bridge's bot (bots.h). The play is searched by the trick bots' core (trickbot.h: information-set Monte
 * Carlo over deals of the unseen cards that keep to the voids shown, the dummy's hand known once it is down); the
 * declarer plays his dummy's cards too, each side for itself; the reward is the deal's score for the declaring side
 * (the international laws, duplicate style: the game and part-score bonuses, slams, doubles, vulnerability). The
 * auction is a short natural system, by rule. Runs on the worker thread: it copies the game at entry and reads only
 * what its seat may know (its hand, the calls, the dummy once shown, the cards played). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * The bidding (high-card points: ace 4, king 3, queen 2, jack 1; "balanced": no singleton, at most one doubleton):
 * - Openings: 1NT 15-17 balanced, 2NT 20-21 balanced; one of a suit with 12 points or more counting a point for each
 *   card over four (five-card majors: the longer major, else the longer minor, diamonds with 4-4, clubs with 3-3);
 *   weak two in diamonds, hearts or spades with six cards and 6-10 points; three of a suit with seven.
 * - Responses to one of a suit: a major raised with three cards by support points (6-9 two, 10-12 three, 13+ game; a
 *   void 3, a singleton 2, a doubleton 1); a new suit of four cards at the one level with 6 points (hearts before
 *   spades), five at the two level with 10; a minor raised with four or five; 1NT 6-10, 2NT 13-15, 3NT 16-17.
 * - Over 1NT: 2 of a suit to play with five cards and 0-7; 3 of a major (game-going) with five and 10-15, 4 with six;
 *   2NT 8-9, 3NT 10-15, 4NT 16-17 (inviting six), 6NT 18-19, 7NT 20+. Over 2NT: 3NT 4-10 (4 of a six-card major), 6NT
 *   13+.
 * - The opener's rebid after a new suit (forcing for one round): four-card support raised by strength (12-15 one,
 *   16-18 two, 19+ game), 1NT or 2NT balanced, a six-card suit rebid (jumped with 16+), a new suit, else his own suit.
 * - Defending the auction: overcalls with five cards (8-16 at the one level, 10-16 at the two level, a jump weak with
 *   six cards and 6-10), 1NT 15-18 balanced with a stop in their suit, a takeout double with 12+ and two cards or
 *   fewer in their suit (or any 17+); the partner of a takeout double names his best suit (jumping with 10+).
 * - After that, each call reads what partner has shown (a range of points, the lengths of his suits) and places the
 *   contract: an eight-card major fit, else no trumps when balanced, else an eight-card minor fit, else a six-card
 *   suit; the points together (with shortness counted in a fit) bid game at 26 (29 in a minor), invite at 23, a small
 *   slam at 33, a grand slam at 37, else stop low. Doubles for penalty: the opponents above the contract our side's points
 *   should make, with 25 together at the three level or higher, or four trumps with an honour. No redoubles.
 * The deal's score sheet waits for a human tap (actor() -1); think() answers "N" there all the same (the tests).
 * Its moves are the game's notation ("b1n", "b4s", "p", "x", "Qs", "N"), played by bridge_show().
 */
#include <stdio.h>

#include "ai.h"
#include "bots.h"
#include "games/bridge.h"
#include "games/trickbot.h"

#define BRIDGE_HARD_ITERS 9000                  /* about 0.3 s a card on a PC */

/* ==== the play on the core ======================================================================================= */
typedef struct { uint8_t level, strain, dbl, vul, declarer; } brx;

/* the declaring side's score: positive made, negative down (duplicate scoring: the game bonus in) */
static int decl_score(const tb_state *st)
{
    const brx *x = TB_CXS(st, brx);
    int d = x->declarer, taken = st->won[d] + st->won[(d + 2) % BRG_P], below, ad, af;
    brg_score(x->level, x->strain, x->dbl, x->vul, taken, 1, &below, &ad, &af);
    return below + ad - af;
}

static void g_reward(const tb_state *st, float *rw)
{
    float s = decl_score(st) / 400.0f, t = s / (1 + (s < 0 ? -s : s));   /* a soft clamp, -1..1 */
    float r = 0.5f + 0.5f * t;
    int side = TB_CXS(st, brx)->declarer & 1;
    for (int q = 0; q < BRG_P; q++) rw[q] = (q & 1) == side ? r : 1 - r;
}

static const tb_rules RULES = {.reward = g_reward};

void brb_view(const brg_game *g, int me, tb_state *v, brg_game *t)
{
    const brg_pos *p = &g->pos;
    tb_init(v, &RULES, BRG_P, me);
    v->trump = p->level ? (p->strain == BRG_NT ? TB_NONE : (uint8_t)brg_suit(p->strain)) : TB_NONE;
    for (int s = 0; s < BRG_P; s++) v->team[s] = (uint8_t)(s & 1);
    v->turn = p->turn;
    v->leader = p->leader;
    v->tricks = p->tricks;
    brx *x = TB_XS(v, brx);
    x->level = p->level, x->strain = p->strain, x->dbl = p->contract_dbl, x->declarer = p->declarer;
    x->vul = p->level ? (uint8_t)brg_vulnerable(p, brg_side(p->declarer)) : 0;
    int dummy = brg_dummy(p);
    for (int s = 0; s < BRG_P; s++) {
        v->won[s] = p->won[s];
        if (s != me) tb_hidden(v, s, p->n[s]);
    }
    tb_mine(v, p->hand[me], p->n[me]);
    if (dummy != BRG_NONE && dummy != me && brg_dummy_shown(p))
        for (int i = 0; i < p->n[dummy]; i++) tb_known(v, dummy, p->hand[dummy][i]);   /* face up */
    int start = 0;
    for (int i = 0; i < g->n; i++)
        if (BRG_KIND(g->act[i]) == BRG_ANEXT) start = i + 1;
    brg_new(t, p->variant, g->seed);
    int plays = p->tricks * BRG_P;
    for (int i = 0, k = 0; i < g->n; i++) {
        int a = g->act[i];
        if (i >= start && BRG_KIND(a) == BRG_APLAY) {
            const brg_pos *q = &t->pos;
            uint8_t who[BRG_P];
            for (int j = 0; j < q->ntrick; j++) who[j] = (uint8_t)((q->leader + j) % BRG_P);
            tb_note(v, q->turn, BRG_ARG(a), q->trick, who, q->ntrick);
            if (k++ < plays) tb_seen(v, BRG_ARG(a));
        }
        brg_play(t, a);
    }
    for (int j = 0; j < p->ntrick; j++) tb_trick_add(v, (p->leader + j) % BRG_P, p->trick[j]);
    uint8_t pack[CARDS_MAX];
    tb_finish(v, pack, cards_pack(PACK_52, pack));
}

/* ==== the auction ================================================================================================= */
typedef struct { int hcp, len[4], bal, tp; } handf;   /* len by strain: clubs, diamonds, hearts, spades */

static void features(const uint8_t *h, int n, handf *f)
{
    memset(f, 0, sizeof *f);
    f->hcp = tb_hcp(h, n);
    for (int st = 0; st < 4; st++) f->len[st] = tb_count_suit(h, n, brg_suit(st));
    int doubletons = 0, short1 = 0;
    for (int st = 0; st < 4; st++) doubletons += f->len[st] == 2, short1 |= f->len[st] < 2;
    f->bal = !short1 && doubletons <= 1;
    f->tp = f->hcp;
    for (int st = 0; st < 4; st++) f->tp += f->len[st] > 4 ? f->len[st] - 4 : 0;
}

static int shortness(const handf *f, int trump)  /* support points beyond the honours */
{
    int s = 0;
    for (int st = 0; st < 4; st++)
        if (st != trump) s += f->len[st] == 0 ? 3 : f->len[st] == 1 ? 2 : f->len[st] == 2 ? 1 : 0;
    return s;
}

/* a stop in a suit: the ace, a guarded king, a queen with two more */
static int stopper(const uint8_t *h, int n, int st)
{
    int suit = brg_suit(st), len = tb_count_suit(h, n, suit), ace = 0, king = 0, queen = 0;
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(h[i]) == suit) {
            int r = CARD_RANK(h[i]);
            ace |= r == RANK_ACE, king |= r == RANK_KING, queen |= r == RANK_QUEEN;
        }
    return ace || (king && len >= 2) || (queen && len >= 3);
}

/* what each seat has shown */
typedef struct { int lo, hi, len[4], bal, bids, called, takeout; } shown;
typedef struct {
    shown s[BRG_P];
    int opener, open_call;                      /* the first seat to bid (BRG_NONE) and its bid */
    int last[BRG_P];                            /* each seat's last call (-1: none) */
    int first_bid[BRG_P];                       /* each seat's first bid (0: none) */
} auction;

static int cheapest(int above, int strain)      /* the lowest bid in a strain over a bid (0: none left) */
{
    for (int lv = 1; lv <= 7; lv++)
        if (BRG_BID(lv, strain) > above) return BRG_BID(lv, strain);
    return 0;
}

static void read_auction(const brg_pos *p, auction *a)
{
    memset(a, 0, sizeof *a);
    a->opener = BRG_NONE;
    for (int s = 0; s < BRG_P; s++) a->s[s].hi = 37, a->last[s] = -1;
    int cur = 0, bidder = BRG_NONE;
    for (int i = 0; i < p->ncalls; i++) {
        int s = (p->dealer + i) % BRG_P, c = p->call[i], partner = (s + 2) % BRG_P;
        shown *h = &a->s[s];
        if (c == BRG_PASS) {
            if (!h->called) {
                if (a->opener == BRG_NONE) h->hi = h->hi < 11 ? h->hi : 11;
                else if (a->opener == partner && !h->bids) h->hi = h->hi < 5 ? h->hi : 5;
            }
        } else if (c == BRG_DOUBLE) {
            int theirs = bidder != BRG_NONE && brg_side(bidder) != brg_side(s);
            if (theirs && !a->first_bid[s] && !a->first_bid[partner] && BRG_LEVEL(cur) <= 3) {
                h->takeout = BRG_STRAIN(cur) + 1;
                if (h->lo < 12) h->lo = 12;
            } else if (h->lo < 10) h->lo = 10;
        } else if (c >= 1 && c <= 35) {
            int L = BRG_LEVEL(c), st = BRG_STRAIN(c), jump = L - BRG_LEVEL(cheapest(cur, st));
#define SET(l, u) (h->lo = (l) > h->lo ? (l) : h->lo, h->hi = (u) < h->hi ? (u) : h->hi)
#define LEN(x, n) (h->len[(x)] = (n) > h->len[(x)] ? (n) : h->len[(x)])
            if (a->opener == BRG_NONE) {
                a->opener = s, a->open_call = c;
                if (st == BRG_NT) { if (L == 1) SET(15, 17); else SET(20, 21); h->bal = 1; }
                else if (L == 1) { SET(12, 21); LEN(st, st >= BRG_HEARTS ? 5 : 3); }
                else if (L == 2) { SET(6, 10); LEN(st, 6); }
                else { SET(6, 10); LEN(st, 7); }
            } else if (a->opener == partner && !h->bids && a->first_bid[partner] == a->open_call) {
                int ost = BRG_STRAIN(a->open_call), oL = BRG_LEVEL(a->open_call);
                if (ost == BRG_NT && oL == 1) {
                    if (st == BRG_NT) {
                        if (L == 2) SET(8, 9); else if (L == 3) SET(10, 15); else if (L == 4) SET(16, 17);
                        else if (L == 6) SET(18, 19); else SET(20, 37);
                    } else if (L == 2) { SET(0, 7); LEN(st, 5); }
                    else if (L == 3) { SET(10, 15); LEN(st, 5); }
                    else { SET(10, 15); LEN(st, 6); }
                } else if (ost == BRG_NT) {
                    if (st == BRG_NT) { if (L == 3) SET(4, 10); else if (L == 4) SET(11, 12); else SET(13, 37); }
                    else { SET(4, 10); LEN(st, L >= 4 ? 6 : 5); }
                } else if (st == ost) {
                    LEN(st, ost >= BRG_HEARTS ? 3 : 4);
                    if (L - oL <= 1) SET(6, 9); else if (L - oL == 2) SET(10, 12); else SET(13, 17);
                } else if (st == BRG_NT) {
                    if (L == 1) SET(6, 10);
                    else if (L == 2) { SET(13, 15); h->bal = 1; }
                    else { SET(16, 17); h->bal = 1; }
                } else if (jump >= 1) { SET(16, 37); LEN(st, 5); }
                else if (L == 1) { SET(6, 37); LEN(st, 4); }
                else { SET(10, 37); LEN(st, st >= BRG_HEARTS ? 5 : 4); }
            } else if (a->opener != BRG_NONE && brg_side(a->opener) != brg_side(s) && !h->bids &&
                       !a->first_bid[partner]) {
                if (a->s[partner].takeout) {    /* the answer to partner's takeout double */
                    LEN(st, 4);
                    if (jump >= 1) SET(10, 37); else SET(0, 9);
                } else if (st == BRG_NT) { SET(15, 18); h->bal = 1; }
                else if (jump >= 1) { SET(6, 10); LEN(st, 6); }
                else if (L == 1) { SET(8, 16); LEN(st, 5); }
                else { SET(10, 16); LEN(st, 5); }
            } else {                            /* later bids */
                if (st == BRG_NT) { if (L <= 3) h->bal = 1; }
                else if (h->len[st]) LEN(st, h->len[st] + 1);
                else LEN(st, a->s[partner].len[st] >= 4 ? 3 : 4);
                if (jump >= 1 && h->lo + 3 <= h->hi) h->lo += 3;
            }
#undef SET
#undef LEN
            if (!a->first_bid[s]) a->first_bid[s] = c;
            h->bids++;
            cur = c, bidder = s;
        }
        h->called = 1;
        a->last[s] = c;
    }
}

/* the opening (nobody has bid) */
static int opening(const handf *f)
{
    if (f->bal && f->hcp >= 15 && f->hcp <= 17) return BRG_BID(1, BRG_NT);
    if (f->bal && f->hcp >= 20 && f->hcp <= 21) return BRG_BID(2, BRG_NT);
    if (f->tp >= 12 && f->hcp >= 10) {
        int H = f->len[BRG_HEARTS], S = f->len[BRG_SPADES], C = f->len[BRG_CLUBS], D = f->len[BRG_DIAMONDS];
        if (S >= 5 && S >= H) return BRG_BID(1, BRG_SPADES);
        if (H >= 5) return BRG_BID(1, BRG_HEARTS);
        if (D > C || (D == C && D >= 4)) return BRG_BID(1, BRG_DIAMONDS);
        return BRG_BID(1, BRG_CLUBS);
    }
    for (int st = BRG_SPADES; st >= BRG_CLUBS; st--) {
        if (f->hcp < 6 || f->hcp > 10) break;
        if (f->len[st] == 6 && st != BRG_CLUBS) return BRG_BID(2, st);
        if (f->len[st] >= 7) return BRG_BID(3, st);
    }
    return BRG_PASS;
}

/* the longest suit (the higher of equal ones), not `not` */
static int longest(const handf *f, int not)
{
    int b = -1;
    for (int st = BRG_SPADES; st >= BRG_CLUBS; st--)
        if (st != not && (b < 0 || f->len[st] > f->len[b])) b = st;
    return b;
}

/* the first response to partner's opening (cur: the bid to beat) */
static int response(const handf *f, const uint8_t *h, int n, int open, int cur)
{
    int ost = BRG_STRAIN(open), oL = BRG_LEVEL(open);
    if (ost == BRG_NT) {
        int M = f->len[BRG_SPADES] >= f->len[BRG_HEARTS] ? BRG_SPADES : BRG_HEARTS, ML = f->len[M];
        if (oL == 1) {
            if (ML >= 6 && f->hcp >= 10 && f->hcp <= 15) return BRG_BID(4, M);
            if (ML >= 5 && f->hcp >= 10 && f->hcp <= 15) return BRG_BID(3, M);
            if (f->hcp <= 7) {
                int st = longest(f, BRG_CLUBS);
                return f->len[st] >= 5 && cheapest(cur, st) == BRG_BID(2, st) ? BRG_BID(2, st) : BRG_PASS;
            }
            return f->hcp <= 9 ? BRG_BID(2, BRG_NT) : f->hcp <= 15 ? BRG_BID(3, BRG_NT) : f->hcp <= 17 ?
                   BRG_BID(4, BRG_NT) : f->hcp <= 19 ? BRG_BID(6, BRG_NT) : BRG_BID(7, BRG_NT);
        }
        if (f->hcp < 4) return BRG_PASS;
        if (ML >= 6 && f->hcp <= 10) return BRG_BID(4, M);
        return f->hcp <= 10 ? BRG_BID(3, BRG_NT) : f->hcp <= 12 ? BRG_BID(4, BRG_NT) : BRG_BID(6, BRG_NT);
    }
    if (oL >= 2) {                              /* a weak opening: game with a fit and 16+, else pass */
        if (f->len[ost] >= 2 && f->hcp >= 16) return BRG_BID(ost >= BRG_HEARTS ? 4 : 5, ost);
        return BRG_PASS;
    }
    if (f->hcp < 6) return BRG_PASS;
    int sp = f->hcp + shortness(f, ost);
    if (ost >= BRG_HEARTS && f->len[ost] >= 3) {
        int lv = sp <= 9 ? 2 : sp <= 12 ? 3 : 4, c = BRG_BID(lv, ost);
        return c > cur ? c : BRG_PASS;
    }
    int order[4] = {BRG_HEARTS, BRG_SPADES, BRG_DIAMONDS, BRG_CLUBS};   /* hearts before spades */
    if (f->len[BRG_SPADES] >= 5 && f->len[BRG_SPADES] > f->len[BRG_HEARTS])
        order[0] = BRG_SPADES, order[1] = BRG_HEARTS;
    for (int k = 0; k < 4; k++) {               /* a new suit of four at the one level */
        int st = order[k], c = cheapest(cur, st);
        if (st != ost && f->len[st] >= 4 && c && BRG_LEVEL(c) == 1) return c;
    }
    if (ost <= BRG_DIAMONDS && f->len[ost] >= (ost == BRG_CLUBS ? 5 : 4)) {
        int lv = f->hcp <= 9 ? oL + 1 : oL + 2, c = BRG_BID(lv, ost);
        if (c > cur) return c;
    }
    if (f->hcp >= 10) {                         /* five cards at the two level */
        int st = longest(f, ost), c = cheapest(cur, st);
        if (f->len[st] >= 5 && c && BRG_LEVEL(c) == 2) return c;
    }
    if (f->hcp <= 10) { int c = cheapest(cur, BRG_NT); return c && BRG_LEVEL(c) == 1 ? c : BRG_PASS; }
    (void)h, (void)n;
    if (f->bal && f->hcp <= 15) { int c = BRG_BID(2, BRG_NT); return c > cur ? c : BRG_PASS; }
    if (f->bal && f->hcp <= 17) { int c = BRG_BID(3, BRG_NT); return c > cur ? c : BRG_PASS; }
    int st = longest(f, ost), c = cheapest(cur, st);
    return c && BRG_LEVEL(c) <= 2 ? c : cheapest(cur, BRG_NT);
}

/* the opener's rebid after partner's new suit (forcing) */
static int rebid(const handf *f, int open, int resp, int cur)
{
    int ost = BRG_STRAIN(open), rst = BRG_STRAIN(resp), pts = f->tp;
    if (rst != BRG_NT && f->len[rst] >= 4) {
        int game = rst >= BRG_HEARTS ? 4 : 5, base = BRG_LEVEL(cheapest(cur, rst));
        int lv = pts >= 19 ? game : pts >= 16 ? base + 1 : base;
        if (lv > game) lv = game;
        int c = BRG_BID(lv, rst);
        if (c > cur) return c;
    }
    if (f->bal) {
        int c = cheapest(cur, BRG_NT);
        if (f->hcp >= 18 && c) c = BRG_BID(BRG_LEVEL(c) + 1 <= 3 ? BRG_LEVEL(c) + 1 : 3, BRG_NT);
        if (c > cur) return c;
    }
    if (f->len[ost] >= 6) {
        int c = cheapest(cur, ost);
        if (pts >= 16 && c && BRG_LEVEL(c) < 4) c = BRG_BID(BRG_LEVEL(c) + 1, ost);
        if (c) return c;
    }
    for (int st = BRG_SPADES; st >= BRG_CLUBS; st--) {   /* a new suit of four, cheaply */
        int c = cheapest(cur, st);
        if (st != ost && st != rst && f->len[st] >= 4 && c && BRG_LEVEL(c) <= 2) return c;
    }
    int c = cheapest(cur, ost);
    return c && BRG_LEVEL(c) <= 3 ? c : BRG_PASS;
}

/* the overcall, or the answer to partner's takeout double */
static int defend(const handf *f, const uint8_t *h, int n, const auction *a, int me, int cur)
{
    int partner = (me + 2) % BRG_P, ts = BRG_STRAIN(cur);
    if (a->s[partner].takeout && !a->first_bid[me]) {
        int best = -1;
        for (int st = BRG_SPADES; st >= BRG_CLUBS; st--) {
            if (st == ts) continue;
            int score = f->len[st] * 2 + (st >= BRG_HEARTS);
            if (best < 0 || score > f->len[best] * 2 + (best >= BRG_HEARTS)) best = st;
        }
        if (ts < 4 && f->len[ts] >= 5 && f->hcp >= 8) return BRG_PASS;   /* their suit: leave it doubled */
        int c = cheapest(cur, best);
        if (f->hcp >= 10 && c && BRG_LEVEL(c) < 4) c = BRG_BID(BRG_LEVEL(c) + 1, best);
        return c ? c : BRG_PASS;
    }
    if (ts == BRG_NT) return f->hcp >= 15 ? BRG_DOUBLE : BRG_PASS;
    if (f->bal && f->hcp >= 15 && f->hcp <= 18 && BRG_LEVEL(cur) == 1 && stopper(h, n, ts))
        return BRG_BID(1, BRG_NT);
    int st = longest(f, ts), c = cheapest(cur, st), L = c ? BRG_LEVEL(c) : 8;
    if (f->len[st] >= 6 && f->hcp >= 6 && f->hcp <= 10 && L <= 2) return BRG_BID(L + 1, st);   /* weak jump */
    if (f->len[st] >= 5 && ((L == 1 && f->hcp >= 8 && f->hcp <= 16) || (L == 2 && f->hcp >= 10 && f->hcp <= 16) ||
                            (L == 3 && f->len[st] >= 6 && f->hcp >= 12)))
        return c;
    if (BRG_LEVEL(cur) <= 3 && ((f->hcp >= 12 && f->len[ts] <= 2) || f->hcp >= 17)) return BRG_DOUBLE;
    return BRG_PASS;
}

/* later calls: place the contract from what partner has shown */
static int place(const handf *f, const uint8_t *h, int n, const brg_pos *p, const auction *a, int me)
{
    int partner = (me + 2) % BRG_P, cur = p->bid, ours = p->bid && brg_side(p->bidder) == brg_side(me);
    const shown *ps = &a->s[partner];
    int span = ps->hi - ps->lo, pexp = ps->lo + (span / 2 < 3 ? span / 2 : 3);
    if (!ps->bids && !ps->takeout) pexp = ps->hi <= 11 ? 5 : 7;
    int strain = -1, fit = 0;
    for (int st = BRG_SPADES; st >= BRG_HEARTS; st--)
        if (f->len[st] + ps->len[st] >= 8 && f->len[st] + ps->len[st] > fit)
            fit = f->len[st] + ps->len[st], strain = st;
    if (strain < 0 && (f->bal || ps->bal)) strain = BRG_NT;
    if (strain < 0)
        for (int st = BRG_DIAMONDS; st >= BRG_CLUBS; st--)
            if (f->len[st] + ps->len[st] >= 8 && f->len[st] + ps->len[st] > fit)
                fit = f->len[st] + ps->len[st], strain = st;
    if (strain < 0) {
        int st = longest(f, -1);
        if (f->len[st] >= 6) strain = st;
    }
    if (strain < 0)
        for (int st = BRG_SPADES; st >= BRG_CLUBS; st--)
            if (ps->len[st] >= 4 && f->len[st] >= 3) { strain = st; break; }
    if (strain < 0) strain = BRG_NT;
    int pts = strain != BRG_NT && fit >= 8 ? f->hcp + shortness(f, strain) : f->tp;
    int comb = pts + pexp, game = strain == BRG_NT ? 3 : strain >= BRG_HEARTS ? 4 : 5;
    int target = comb >= 37 ? 7 : comb >= 33 ? 6 : comb >= (strain <= BRG_DIAMONDS ? 29 : 26) ? game
                 : comb >= 23 && pts + ps->hi >= 26 ? game - 1 : comb >= 20 ? 2 : 1;
    if (target > game && target < 6) target = game;
    int want = BRG_BID(target, strain);
    if (want > cur) {
        int c = cheapest(cur, strain);
        if (!c) return BRG_PASS;
        if (ours && BRG_STRAIN(cur) == strain && BRG_LEVEL(cur) >= target) return BRG_PASS;
        return target >= game || BRG_LEVEL(c) >= target ? want : c;   /* game and slams at once; else the least */
    }
    if (ours) return BRG_PASS;
    /* the opponents are above what we would bid: compete one level, or double them */
    int c = cheapest(cur, strain);
    if (c && BRG_LEVEL(c) <= target) return c;
    int ts = BRG_STRAIN(cur), trumps = ts < 4 ? f->len[ts] : 0, honour = 0;
    for (int i = 0; i < n; i++)
        honour |= ts < 4 && CARD_SUIT(h[i]) == brg_suit(ts) && CARD_RANK(h[i]) >= RANK_QUEEN;
    if (brg_can_call(p, BRG_DOUBLE) && ((comb >= 25 && BRG_LEVEL(cur) >= 3) || (trumps >= 4 && honour && comb >= 20)))
        return BRG_DOUBLE;
    return BRG_PASS;
}

static int call(const brg_pos *p, int me)
{
    handf f;
    const uint8_t *h = p->hand[me];
    int n = p->n[me];
    features(h, n, &f);
    auction a;
    read_auction(p, &a);
    int partner = (me + 2) % BRG_P, cur = p->bid, c;
    if (a.opener == BRG_NONE) c = opening(&f);
    else if (a.opener == partner && !a.first_bid[me] && !a.s[me].called) c = response(&f, h, n, a.open_call, cur);
    else if (a.opener == partner && !a.first_bid[me] && a.last[partner] == a.open_call && a.s[me].called &&
             cur == a.open_call)
        c = BRG_PASS;                           /* passed before: still too weak */
    else if (a.opener == me && a.s[me].bids == 1 && a.first_bid[partner] && a.last[partner] == a.first_bid[partner] &&
             BRG_STRAIN(a.first_bid[partner]) != BRG_STRAIN(a.open_call) &&
             BRG_STRAIN(a.first_bid[partner]) != BRG_NT && BRG_STRAIN(a.open_call) != BRG_NT &&
             BRG_LEVEL(a.open_call) == 1 && a.s[partner].hi > 12)
        c = rebid(&f, a.open_call, a.first_bid[partner], cur);
    else if (brg_side(a.opener) != brg_side(me) && !a.first_bid[me] && !a.first_bid[partner])
        c = defend(&f, h, n, &a, me, cur);
    else c = place(&f, h, n, p, &a, me);
    return brg_can_call(p, c) ? c : BRG_PASS;
}

/* the action for the controller `seat` at a level (0: nothing to do) */
int brb_choose(const brg_game *g, int seat, int level, uint64_t seed, ai_stats *stats)
{
    const brg_pos *p = &g->pos;
    if (p->phase == BRG_SCORE) return BRG_ANEXT;
    if (p->phase == BRG_OVER || brg_controller(p) != seat) return 0;
    if (p->phase == BRG_AUCTION) return BRG_ACALL | call(p, seat);
    brg_game *t = malloc(sizeof *t);
    tb_state *v = malloc(sizeof *v);
    int a = 0;
    if (t && v) {
        brb_view(g, seat, v, t);                /* the declarer's knowledge, the dummy to play or his own hand */
        int c = tb_choose(v, level, seed, BRIDGE_HARD_ITERS, stats);
        if (c != TB_NONE && brg_can_play(p, c)) a = BRG_APLAY | c;
    }
    free(t);
    free(v);
    return a ? a : brg_auto(p);
}

static int think(int seat, int level, char *move, int cap)
{
    brg_game *g = malloc(sizeof *g);
    if (!g) return 0;
    *g = *bridge_live();                        /* the copy, at entry: nothing else is read */
    int a = brb_choose(g, seat, level, ai_session_seed(), NULL);
    free(g);
    if (!a || ai_cancelled() || cap < 8) return 0;
    static const char strains[] = "cdhsn";
    char c[3];
    int x = BRG_ARG(a);
    switch (BRG_KIND(a)) {
    case BRG_ACALL:
        if (x == BRG_PASS) snprintf(move, (size_t)cap, "p");
        else if (x == BRG_DOUBLE) snprintf(move, (size_t)cap, "x");
        else if (x == BRG_REDOUBLE) snprintf(move, (size_t)cap, "xx");
        else snprintf(move, (size_t)cap, "b%d%c", BRG_LEVEL(x), strains[BRG_STRAIN(x)]);
        return 1;
    case BRG_APLAY: trick_name(x, c); snprintf(move, (size_t)cap, "%s", c); return 1;
    case BRG_ANEXT: snprintf(move, (size_t)cap, "N"); return 1;
    default: return 0;
    }
}

static int actor(void)
{
    const brg_pos *p = &bridge_live()->pos;
    return p->phase == BRG_AUCTION || p->phase == BRG_PLAY ? brg_controller(p) : -1;
}

static const bot bridge_bot = {.think = think, .actor = actor, .play = bridge_show};
REGISTER_BOT(bridge, &bridge_bot)
