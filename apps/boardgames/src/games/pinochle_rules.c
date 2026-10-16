/*
 * Pinochle: the rules (pinochle.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/pinochle.h"
#include "games/tricks.h"

#define TABLE_BIT 0x80                          /* a play's card from the table (two players) */

static const uint8_t RANKS[6] = {9, 10, RANK_JACK, RANK_QUEEN, RANK_KING, RANK_ACE};

int pinochle_side(const pino_pos *p, int player) { return p->players == 4 ? player & 1 : player; }

int pinochle_order(int card)
{
    switch (CARD_RANK(card)) {
    case RANK_ACE: return 6;
    case 10: return 5;
    case RANK_KING: return 4;
    case RANK_QUEEN: return 3;
    case RANK_JACK: return 2;
    default: return 1;
    }
}

int pinochle_counter(int card)
{
    int r = CARD_RANK(card);
    return r == RANK_ACE || r == 10 || r == RANK_KING;
}

int pinochle_power(int card, int trump, int led)
{
    int s = CARD_SUIT(card);
    if (s == trump) return 100 + pinochle_order(card);
    return s == led ? pinochle_order(card) : 0;
}

static int sort_key(int card) { return trick_suit_order(CARD_SUIT(card)) * 16 + pinochle_order(card); }

/* a hand sorted with its flags */
static void sort_hand(uint8_t *h, uint8_t *f, int n)
{
    for (int i = 1; i < n; i++) {
        uint8_t c = h[i], g = f[i];
        int j = i;
        while (j > 0 && sort_key(h[j - 1]) > sort_key(c)) { h[j] = h[j - 1]; f[j] = f[j - 1]; j--; }
        h[j] = c;
        f[j] = g;
    }
}

static void take_slot(pino_pos *p, int pl, int i)
{
    for (int k = i; k < p->n[pl] - 1; k++) p->hand[pl][k] = p->hand[pl][k + 1], p->flag[pl][k] = p->flag[pl][k + 1];
    p->n[pl]--;
}

static void give(pino_pos *p, int pl, int card, int flags)
{
    if (p->n[pl] >= PN_HAND) return;
    p->hand[pl][p->n[pl]] = (uint8_t)card;
    p->flag[pl][p->n[pl]++] = (uint8_t)flags;
    sort_hand(p->hand[pl], p->flag[pl], p->n[pl]);
}

/* ---- melds ------------------------------------------------------------------------------------------------------- */
int pinochle_meld_value(int kind, int players)
{
    static const int8_t v[PM_KINDS] = {15, 4, 2, 1, 4, 30, 10, 8, 6, 4};
    (void)players;
    return kind >= 0 && kind < PM_KINDS ? v[kind] : 0;
}

static int kind_bit(int kind)
{
    if (kind <= PM_MARRIAGE) return PK_MARRIAGE;
    if (kind == PM_DIX) return PK_DIX;
    if (kind <= PM_DOUBLE) return PK_PINOCHLE;
    return PK_AROUND;
}

int pinochle_meld_cards(int kind, int suit, int trump, uint8_t *out)
{
    int n = 0;
    switch (kind) {
    case PM_RUN:
        for (int r = 1; r < 6; r++) out[n++] = CARD(trump, RANKS[r]);
        break;
    case PM_ROYAL: out[n++] = CARD(trump, RANK_KING); out[n++] = CARD(trump, RANK_QUEEN); break;
    case PM_MARRIAGE:
        if (suit == trump) return 0;
        out[n++] = CARD(suit, RANK_KING);
        out[n++] = CARD(suit, RANK_QUEEN);
        break;
    case PM_DIX: out[n++] = CARD(trump, 9); break;
    case PM_DOUBLE: out[n++] = CARD(SUIT_SPADES, RANK_QUEEN); out[n++] = CARD(SUIT_DIAMONDS, RANK_JACK); /* fall through */
    case PM_PINOCHLE: out[n++] = CARD(SUIT_SPADES, RANK_QUEEN); out[n++] = CARD(SUIT_DIAMONDS, RANK_JACK); break;
    default: {
        static const uint8_t rank[4] = {RANK_ACE, RANK_KING, RANK_QUEEN, RANK_JACK};
        if (kind < PM_ACES || kind >= PM_KINDS) return 0;
        for (int s = 0; s < 4; s++) out[n++] = CARD(s, rank[kind - PM_ACES]);
    }
    }
    return n;
}

static int same_cards(const uint8_t *a, int na, const uint8_t *b, int nb)
{
    if (na != nb) return 0;
    int cnt[256] = {0};
    for (int i = 0; i < na; i++) cnt[a[i]]++, cnt[b[i]]--;
    for (int i = 0; i < na; i++)
        if (cnt[a[i]]) return 0;
    return 1;
}

int pinochle_meld_of(const uint8_t *cards, int n, int trump)
{
    uint8_t want[8];
    for (int k = 0; k < PM_KINDS; k++)
        for (int s = 0; s < (k == PM_MARRIAGE ? 4 : 1); s++) {
            int m = pinochle_meld_cards(k, s, trump, want);
            if (m && same_cards(cards, n, want, m)) return PA_MELD_OF(k, s);
        }
    return 0;
}

/* the slots of the player's cards a meld takes: on the table first, at least one from the hand, none already in a
 * meld of its kind; the count (-1: it can not be made) */
static int assign(const pino_pos *p, int pl, int kind, int suit, uint8_t *slot)
{
    uint8_t want[8];
    int m = pinochle_meld_cards(kind, suit, p->trump, want), bit = kind_bit(kind), hidden = 0;
    if (!m) return -1;
    unsigned used = 0;
    for (int k = 0; k < m; k++) {
        int best = -1;
        for (int i = 0; i < p->n[pl]; i++) {
            if (used >> i & 1 || p->hand[pl][i] != want[k] || p->flag[pl][i] & bit) continue;
            if (best < 0 || (p->flag[pl][i] & PK_TABLE && !(p->flag[pl][best] & PK_TABLE))) best = i;
        }
        if (best < 0) return -1;
        used |= 1u << best;
        slot[k] = (uint8_t)best;
        hidden += !(p->flag[pl][best] & PK_TABLE);
    }
    if (hidden) return m;
    for (int k = 0; k < m; k++)                 /* all on the table: one from the hand instead */
        for (int i = 0; i < p->n[pl]; i++)
            if (!(used >> i & 1) && p->hand[pl][i] == want[k] && !(p->flag[pl][i] & (bit | PK_TABLE))) {
                slot[k] = (uint8_t)i;
                return m;
            }
    return -1;
}

static int meld_ok(const pino_pos *p, int act)
{
    if (p->players != 2 || p->phase != PP_MELD || PA_KIND(act) != PA_MELD) return 0;
    uint8_t slot[8];
    int kind = PA_ARG(act) >> 2, suit = PA_ARG(act) & 3;
    if (kind >= PM_KINDS || (kind != PM_MARRIAGE && suit)) return 0;
    return assign(p, p->turn, kind, suit, slot) > 0;
}

int pinochle_can_meld(const pino_pos *p)
{
    if (p->players != 2) return 0;
    pino_pos q = *p;
    q.phase = PP_MELD;
    for (int k = 0; k < PM_KINDS; k++)
        for (int s = 0; s < (k == PM_MARRIAGE ? 4 : 1); s++)
            if (meld_ok(&q, PA_MELD_OF(k, s))) return 1;
    return 0;
}

int pinochle_hand_melds(const uint8_t *h, int n, int trump, uint8_t *cards, int *ncards)
{
    int cnt[256] = {0}, need[256] = {0}, pts = 0;
    for (int i = 0; i < n; i++) cnt[h[i]]++;
#define NEED(c, k) do { if (need[c] < (k)) need[c] = (k); } while (0)
    int runs = 2;
    for (int r = 1; r < 6; r++) runs = cnt[CARD(trump, RANKS[r])] < runs ? cnt[CARD(trump, RANKS[r])] : runs;
    pts += runs == 2 ? 150 : runs * 15;
    for (int r = 1; r < 6; r++) NEED(CARD(trump, RANKS[r]), runs);
    for (int s = 0; s < 4; s++) {
        int k = cnt[CARD(s, RANK_KING)], q = cnt[CARD(s, RANK_QUEEN)], m = k < q ? k : q;
        if (s == trump) pts += (m - runs) * 4;
        else pts += m * 2;
        NEED(CARD(s, RANK_KING), m);
        NEED(CARD(s, RANK_QUEEN), m);
    }
    pts += cnt[CARD(trump, 9)];
    NEED(CARD(trump, 9), cnt[CARD(trump, 9)]);
    int qs = cnt[CARD(SUIT_SPADES, RANK_QUEEN)], jd = cnt[CARD(SUIT_DIAMONDS, RANK_JACK)], pin = qs < jd ? qs : jd;
    pts += pin == 2 ? 30 : pin * 4;
    NEED(CARD(SUIT_SPADES, RANK_QUEEN), pin);
    NEED(CARD(SUIT_DIAMONDS, RANK_JACK), pin);
    static const uint8_t rank[4] = {RANK_ACE, RANK_KING, RANK_QUEEN, RANK_JACK};
    static const int8_t once[4] = {10, 8, 6, 4};
    for (int k = 0; k < 4; k++) {
        int m = 2;
        for (int s = 0; s < 4; s++) m = cnt[CARD(s, rank[k])] < m ? cnt[CARD(s, rank[k])] : m;
        pts += m == 2 ? once[k] * 10 : m * once[k];
        for (int s = 0; s < 4; s++) NEED(CARD(s, rank[k]), m);
    }
#undef NEED
    if (cards) {
        int k = 0;
        for (int i = 0; i < n; i++)             /* in the hand's order, each card as often as a meld needs it */
            if (need[h[i]] > 0) cards[k++] = h[i], need[h[i]]--;
        *ncards = k;
    }
    return pts;
}

/* ---- the deal ------------------------------------------------------------------------------------------------------ */
static void deal(pino_pos *p)
{
    uint8_t deck[PN_PACK];
    int k = 0, P = p->players;
    for (int c = 0; c < 2; c++)
        for (int s = 0; s < 4; s++)
            for (int r = 0; r < 6; r++) deck[k++] = CARD(s, RANKS[r]);
    cards_shuffle(deck, k, trick_mix(p->seed, 3000u + p->round));
    p->dealer = (uint8_t)(p->round % P);
    memset(p->n, 0, sizeof p->n);
    memset(p->flag, 0, sizeof p->flag);
    memset(p->won, 0, sizeof p->won);
    memset(p->bid, 0, sizeof p->bid);
    memset(p->nm, 0, sizeof p->nm);
    memset(p->meldpts, 0, sizeof p->meldpts);
    for (int s = 0; s < 2; s++) p->counters[s] = p->melds[s] = p->pts[s] = 0;
    int i = 0;
    for (; i < P * PN_HAND; i++) {
        int pl = (p->dealer + 1 + (P == 4 ? i / 3 : i)) % P;
        p->hand[pl][p->n[pl]++] = deck[i];
    }
    for (int pl = 0; pl < P; pl++) sort_hand(p->hand[pl], p->flag[pl], p->n[pl]);
    p->nstock = 0;
    p->dix = p->set = 0;
    p->up = CARD_NONE;
    p->trump = PN_NONE;
    if (P == 2) {
        p->up = deck[i++];
        p->stock[p->nstock++] = p->up;
        while (i < k) p->stock[p->nstock++] = deck[i++];
        p->trump = (uint8_t)CARD_SUIT(p->up);
        if (CARD_RANK(p->up) == 9) {            /* the dix turned: 1 for the dealer */
            p->dix = 1;
            p->meldpts[p->dealer] = 1;
            p->melds[p->dealer] = 1;
            p->score[p->dealer]++;
        }
    }
    memset(p->last, CARD_NONE, sizeof p->last);
    p->drew[0] = p->drew[1] = CARD_NONE;
    p->ntrick = p->tricks = 0;
    p->last_winner = p->last_leader = PN_NONE;
    p->last_meld = p->last_melder = PN_NONE;
    p->passed = p->high = 0;
    p->bidder = PN_NONE;
    p->turn = p->leader = (uint8_t)((p->dealer + 1) % P);
    p->phase = P == 2 ? PP_PLAY : PP_BID;
}

void pinochle_new(pino_game *g, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->players = (uint8_t)(players == 4 ? 4 : 2);
    g->seed = g->pos.seed = seed;
    g->pos.players = g->players;
    deal(&g->pos);
}

/* ---- playing ----------------------------------------------------------------------------------------------------- */
int pinochle_strict(const uint8_t *h, int n, const uint8_t *trick, int ntrick, int trump, uint8_t *out)
{
    int k = 0;
    if (!ntrick) {
        for (int i = 0; i < n; i++) out[k++] = h[i];
        return k;
    }
    int led = CARD_SUIT(trick[0]), best = 0;
    for (int i = 1; i < ntrick; i++)
        if (pinochle_power(trick[i], trump, led) > pinochle_power(trick[best], trump, led)) best = i;
    int bp = pinochle_power(trick[best], trump, led), bs = CARD_SUIT(trick[best]);
    int suit = trick_has_suit(h, n, led) ? led : trick_has_suit(h, n, trump) ? trump : -1;
    if (suit < 0) {
        for (int i = 0; i < n; i++) out[k++] = h[i];
        return k;
    }
    if (bs == suit)                             /* beat the card holding the trick if you can */
        for (int i = 0; i < n; i++)
            if (CARD_SUIT(h[i]) == suit && pinochle_power(h[i], trump, led) > bp) out[k++] = h[i];
    if (k) return k;
    for (int i = 0; i < n; i++)
        if (CARD_SUIT(h[i]) == suit) out[k++] = h[i];
    return k;
}

static int strict_now(const pino_pos *p) { return p->players == 4 || p->nstock == 0; }

int pinochle_can_play(const pino_pos *p, int card)
{
    if (p->phase != PP_PLAY) return 0;
    int pl = p->turn, c = card & ~TABLE_BIT, table = card & TABLE_BIT, found = 0;
    for (int i = 0; i < p->n[pl] && !found; i++)
        found = p->hand[pl][i] == c && (!table || p->flag[pl][i] & PK_TABLE);
    if (!found) return 0;
    if (!strict_now(p)) return 1;
    uint8_t L[PN_HAND];
    int k = pinochle_strict(p->hand[pl], p->n[pl], p->trick, p->ntrick, p->trump, L);
    return trick_find(L, k, c) >= 0;
}

int pinochle_legal(const pino_pos *p, int act)
{
    int a = PA_ARG(act), P = p->players;
    switch (PA_KIND(act)) {
    case PA_PLAY: return pinochle_can_play(p, a);
    case PA_MELD: return meld_ok(p, act);
    case PA_SKIP: return p->phase == PP_MELD && a == 0;
    case PA_BID: return P == 4 && p->phase == PP_BID && a >= PN_MIN_BID && a > p->high;
    case PA_PASS: {
        if (P != 4 || p->phase != PP_BID || a) return 0;
        int out = 0;
        for (int q = 0; q < 4; q++) out += p->passed >> q & 1;
        return !(out == 3 && p->high == 0);     /* the last one, nobody having bid: must bid */
    }
    case PA_TRUMP: return P == 4 && p->phase == PP_TRUMP && a < 4;
    case PA_GO: return p->phase == PP_MELDS && !a;
    case PA_NEXT: return p->phase == PP_ROUND && !a;
    default: return 0;
    }
}

static void draw_cards(pino_pos *p)
{
    int w = p->last_winner;
    p->drew[0] = p->drew[1] = CARD_NONE;
    for (int k = 0; k < 2 && p->nstock; k++) {
        int q = (w + k) % 2, c = p->stock[--p->nstock];
        p->drew[k] = (uint8_t)c;
        give(p, q, c, p->nstock == 0 ? PK_PUBLIC : 0);   /* the turned card: everyone saw it */
    }
    if (!p->nstock)                             /* the melds back to the hands */
        for (int q = 0; q < 2; q++)
            for (int i = 0; i < p->n[q]; i++) p->flag[q][i] &= (uint8_t)~(PK_TABLE | PK_MARRIAGE | PK_DIX |
                                                                          PK_PINOCHLE | PK_AROUND);
    p->phase = PP_PLAY;
    p->turn = p->leader = (uint8_t)w;
}

static void deal_end(pino_pos *p)
{
    int P = p->players;
    if (P == 2) {
        for (int s = 0; s < 2; s++) {
            p->pts[s] = (int16_t)(p->counters[s] + p->melds[s]);
            p->score[s] = (int16_t)(p->score[s] + p->counters[s]);
        }
        p->round++;
        int top = p->score[0] > p->score[1] ? p->score[0] : p->score[1];
        p->phase = top >= PN_TARGET2 && p->score[0] != p->score[1] ? PP_OVER : PP_ROUND;
    } else {
        int bs = p->bidder & 1;
        for (int s = 0; s < 2; s++) {
            int took = p->won[s] + p->won[s + 2] > 0, total = (took ? p->melds[s] : 0) + p->counters[s];
            if (s == bs && total < p->high) p->pts[s] = (int16_t)-p->high, p->set = 1;
            else p->pts[s] = (int16_t)total;
            p->score[s] = (int16_t)(p->score[s] + p->pts[s]);
        }
        p->round++;
        p->phase = p->score[0] >= PN_TARGET4 || p->score[1] >= PN_TARGET4 ? PP_OVER : PP_ROUND;
    }
    p->turn = (uint8_t)((p->round + 1) % P);
}

static void play_card(pino_pos *p, int a)
{
    int pl = p->turn, c = a & ~TABLE_BIT, P = p->players, at = -1;
    for (int i = 0; i < p->n[pl]; i++) {         /* the copy named: from the hand first, unless from the table */
        if (p->hand[pl][i] != c) continue;
        int table = p->flag[pl][i] & PK_TABLE;
        if (a & TABLE_BIT ? table : !table) { at = i; break; }
        if (at < 0) at = i;
    }
    take_slot(p, pl, at);
    p->trick[p->ntrick++] = (uint8_t)c;
    p->last_meld = PN_NONE;
    if (p->ntrick < P) { p->turn = (uint8_t)((pl + 1) % P); return; }
    int led = CARD_SUIT(p->trick[0]), b = 0, pts = 0;
    for (int i = 0; i < P; i++) {
        if (pinochle_power(p->trick[i], p->trump, led) > pinochle_power(p->trick[b], p->trump, led)) b = i;
        pts += pinochle_counter(p->trick[i]);
    }
    int w = (p->leader + b) % P, side = pinochle_side(p, w), left = p->nstock;
    for (int q = 0; q < P; q++) left += p->n[q];
    memcpy(p->last, p->trick, (size_t)P);
    p->won[w]++;
    p->counters[side] = (int16_t)(p->counters[side] + pts + (left == 0));
    p->last_leader = p->leader;
    p->last_winner = (uint8_t)w;
    p->ntrick = 0;
    p->tricks++;
    p->drew[0] = p->drew[1] = CARD_NONE;
    p->turn = p->leader = (uint8_t)w;
    if (!left) { deal_end(p); return; }
    if (P == 2 && p->nstock) {
        if (pinochle_can_meld(p)) p->phase = PP_MELD;
        else draw_cards(p);
    }
}

static void meld(pino_pos *p, int act)
{
    uint8_t slot[8];
    int pl = p->turn, kind = PA_ARG(act) >> 2, suit = PA_ARG(act) & 3, m = assign(p, pl, kind, suit, slot);
    int v = pinochle_meld_value(kind, 2), bit = kind_bit(kind);
    for (int k = 0; k < m; k++) p->flag[pl][slot[k]] |= (uint8_t)(bit | PK_TABLE | PK_PUBLIC);
    if (kind == PM_DIX && p->nstock && CARD_RANK(p->stock[0]) != 9) {   /* the dix for the turned card */
        int i = slot[0], c = p->stock[0];
        p->stock[0] = p->hand[pl][i];
        p->up = p->stock[0];
        take_slot(p, pl, i);
        give(p, pl, c, PK_PUBLIC);
    }
    p->meldpts[pl] = (int16_t)(p->meldpts[pl] + v);
    p->melds[pl] = (int16_t)(p->melds[pl] + v);
    p->score[pl] = (int16_t)(p->score[pl] + v);
    p->last_meld = (uint8_t)kind;
    p->last_melder = (uint8_t)pl;
    draw_cards(p);
}

static void apply(pino_pos *p, int act)
{
    int a = PA_ARG(act);
    switch (PA_KIND(act)) {
    case PA_PLAY: play_card(p, a); break;
    case PA_MELD: meld(p, act); break;
    case PA_SKIP: p->last_meld = PN_NONE; draw_cards(p); break;
    case PA_BID:
    case PA_PASS: {
        int pl = p->turn, out = 0;
        if (PA_KIND(act) == PA_BID) p->bid[pl] = (uint8_t)a, p->high = (uint8_t)a, p->bidder = (uint8_t)pl;
        else p->passed |= (uint8_t)(1u << pl);
        for (int q = 0; q < 4; q++) out += p->passed >> q & 1;
        if (out == 3 && p->high) {
            for (int q = 0; q < 4; q++)
                if (!(p->passed >> q & 1)) p->bidder = (uint8_t)q;
            p->phase = PP_TRUMP;
            p->turn = p->bidder;
            break;
        }
        do pl = (pl + 1) % 4; while (p->passed >> pl & 1);
        p->turn = (uint8_t)pl;
        break;
    }
    case PA_TRUMP:
        p->trump = (uint8_t)a;
        for (int q = 0; q < 4; q++) {
            int k;
            p->meldpts[q] = (int16_t)pinochle_hand_melds(p->hand[q], p->n[q], a, p->mcards[q], &k);
            p->nm[q] = (uint8_t)k;
            p->melds[q & 1] = (int16_t)(p->melds[q & 1] + p->meldpts[q]);
        }
        p->phase = PP_MELDS;
        break;
    case PA_GO: p->phase = PP_PLAY; p->turn = p->leader = p->bidder; break;
    case PA_NEXT: deal(p); break;
    }
}

int pinochle_play(pino_game *g, int act)
{
    if (g->n >= PN_MAX_ACTS || !pinochle_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int pinochle_undo(pino_game *g)
{
    if (!g->n) return 0;
    static uint16_t acts[PN_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    pinochle_new(g, g->players, g->seed);
    for (int i = 0; i < n; i++) pinochle_play(g, acts[i]);
    return 1;
}

int pinochle_winner(const pino_pos *p)
{
    if (p->phase != PP_OVER) return -1;
    if (p->players == 2) return p->score[1] > p->score[0];
    int bs = p->bidder & 1;
    return p->score[bs] >= PN_TARGET4 ? bs : !bs;
}

int pinochle_auto(const pino_pos *p)
{
    int pl = p->turn;
    switch (p->phase) {
    case PP_BID: return pinochle_legal(p, PA_PASS) ? PA_PASS : PA_BID | (p->high < PN_MIN_BID ? PN_MIN_BID : p->high + 1);
    case PP_TRUMP: {
        int len[4] = {0}, best = 0;
        for (int i = 0; i < p->n[pl]; i++) len[CARD_SUIT(p->hand[pl][i])]++;
        for (int s = 1; s < 4; s++) best = len[s] > len[best] ? s : best;
        return PA_TRUMP | best;
    }
    case PP_MELDS: return PA_GO;
    case PP_MELD: return PA_SKIP;
    case PP_PLAY:
        for (int i = 0; i < p->n[pl]; i++)
            if (pinochle_can_play(p, p->hand[pl][i])) return PA_PLAY | p->hand[pl][i];
        return 0;
    case PP_ROUND: return PA_NEXT;
    default: return 0;
    }
}

int pinochle_name(const pino_pos *p, int act, char *out, int cap)
{
    static const char suits[] = "shdc";
    int a = PA_ARG(act);
    char c[3];
    switch (PA_KIND(act)) {
    case PA_PLAY:
        trick_name(a & ~TABLE_BIT, c);
        return snprintf(out, (size_t)cap, "%s%s", c, a & TABLE_BIT ? "*" : "") > 0;
    case PA_MELD: {
        uint8_t m[8];
        int n = pinochle_meld_cards(a >> 2, a & 3, p->trump, m), k = 0;
        if (cap < 2 * n + 2) return 0;
        out[k++] = 'M';
        for (int i = 0; i < n; i++, k += 2) trick_name(m[i], out + k);
        out[k] = 0;
        return n > 0;
    }
    case PA_SKIP: return snprintf(out, (size_t)cap, "x") > 0;
    case PA_BID: return snprintf(out, (size_t)cap, "b%d", a) > 0;
    case PA_PASS: return snprintf(out, (size_t)cap, "p") > 0;
    case PA_TRUMP: return snprintf(out, (size_t)cap, "t%c", suits[a & 3]) > 0;
    case PA_GO: return snprintf(out, (size_t)cap, "g") > 0;
    case PA_NEXT: return snprintf(out, (size_t)cap, "n") > 0;
    default: return 0;
    }
}

int pinochle_script(pino_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int act;
        pino_pos *p = &g->pos;
        if (*s == '#' || *s == '!') {
            int all = *s++ == '!';
            while (p->phase != PP_OVER && (all || p->phase != PP_ROUND))
                if (!pinochle_play(g, pinochle_auto(p))) return 0;
            continue;
        }
        switch (*s) {
        case '.': s++; act = pinochle_auto(p); break;
        case 'n': s++; act = PA_NEXT; break;
        case 'g': s++; act = PA_GO; break;
        case 'x': s++; act = PA_SKIP; break;
        case 'p': s++; act = PA_PASS; break;
        case 't': {
            static const char suits[] = "shdc";
            const char *q = s[1] ? strchr(suits, s[1]) : NULL;
            if (!q) return 0;
            act = PA_TRUMP | (int)(q - suits);
            s += 2;
            break;
        }
        case 'b': {
            s++;
            int v = 0, digits = 0;
            while (*s >= '0' && *s <= '9' && digits < 3) v = v * 10 + (*s++ - '0'), digits++;
            if (!digits || v > 255) return 0;
            act = PA_BID | v;
            break;
        }
        case 'M': {
            s++;
            uint8_t m[8];
            int n = 0;
            while (*s && *s != ' ' && n < 8) {
                int c = trick_parse(&s);
                if (c == CARD_NONE) return 0;
                m[n++] = (uint8_t)c;
            }
            act = p->trump == PN_NONE ? 0 : pinochle_meld_of(m, n, p->trump);
            if (!act) return 0;
            break;
        }
        default: {
            int c = trick_parse(&s);
            if (c == CARD_NONE) return 0;
            if (*s == '*') s++, c |= TABLE_BIT;
            act = PA_PLAY | c;
        }
        }
        if (!pinochle_play(g, act)) return 0;
    }
    return 1;
}
