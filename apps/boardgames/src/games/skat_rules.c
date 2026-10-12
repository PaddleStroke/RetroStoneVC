/*
 * Skat: the rules (skat.h), by the International Skat Order. No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdlib.h>
#include <string.h>

#include "games/skat.h"
#include "games/tricks.h"

/* ---- the bid values ---------------------------------------------------------------------------------------------- */
int skat_bids(int16_t *out)
{
    static const int bases[5] = {9, 10, 11, 12, 24}, top[5] = {18, 18, 18, 18, 11}, nulls[4] = {23, 35, 46, 59};
    int n = 0;
    for (int v = 18; v <= 24 * 11; v++) {
        int ok = 0;
        for (int b = 0; b < 5 && !ok; b++) ok = v % bases[b] == 0 && v / bases[b] >= 2 && v / bases[b] <= top[b];
        for (int k = 0; k < 4 && !ok; k++) ok = v == nulls[k];
        if (ok) {
            if (out) out[n] = (int16_t)v;
            n++;
        }
    }
    return n;
}

int skat_next_bid(int value)
{
    static int16_t bids[80];
    static int n;
    if (!n) n = skat_bids(bids);
    for (int i = 0; i < n; i++)
        if (bids[i] > value) return bids[i];
    return 0;
}

static int is_bid(int v) { return v >= 18 && skat_next_bid(v - 1) == v; }

int skat_deals(int variant) { return variant == SKAT_LONG ? 36 : 12; }

/* ---- the cards --------------------------------------------------------------------------------------------------- */
int skat_points(int card)
{
    switch (CARD_RANK(card)) {
    case RANK_ACE: return 11;
    case 10: return 10;
    case RANK_KING: return 4;
    case RANK_QUEEN: return 3;
    case RANK_JACK: return 2;
    default: return 0;
    }
}

int skat_base(int type, int flags, int hand)
{
    static const int suit[4] = {11, 10, 9, 12};  /* spades, hearts, diamonds, clubs */
    if (type == SG_NULL) return (flags & SF_OUVERT) ? (hand ? 59 : 46) : (hand ? 35 : 23);
    return type == SG_GRAND ? 24 : type >= 0 && type < 4 ? suit[type] : 0;
}

int skat_trump(int card, int type)
{
    if (type == SG_NULL) return 0;
    return CARD_RANK(card) == RANK_JACK || (type < 4 && CARD_SUIT(card) == type);
}

int skat_suit(int card, int type) { return skat_trump(card, type) ? SUIT_TRUMP : CARD_SUIT(card); }

static int jack_order(int suit)                 /* clubs 4, spades 3, hearts 2, diamonds 1 */
{
    static const int8_t o[4] = {3, 2, 1, 4};
    return o[suit & 3];
}

int skat_power(int card, int type)
{
    int r = CARD_RANK(card);
    if (type == SG_NULL) {                      /* A K Q J 10 9 8 7 */
        static const int8_t p[14] = {0, 8, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7};
        return p[r];
    }
    if (r == RANK_JACK) return 20 + jack_order(CARD_SUIT(card));
    static const int8_t p[14] = {0, 8, 0, 0, 0, 0, 0, 2, 3, 4, 7, 0, 5, 6};   /* A 10 K Q 9 8 7 */
    return p[r];
}

int skat_trick_best(const uint8_t *cards, int n, int type)
{
    int best = 0;
    for (int i = 1; i < n; i++) {
        int s = skat_suit(cards[i], type), bs = skat_suit(cards[best], type);
        if (s == bs ? skat_power(cards[i], type) > skat_power(cards[best], type) : s == SUIT_TRUMP) best = i;
    }
    return best;
}

int skat_matadors(const uint8_t *cards, int n, int type)
{
    if (type == SG_NULL) return 0;
    uint8_t seq[11];
    int k = 0;
    static const int jacks[4] = {SUIT_CLUBS, SUIT_SPADES, SUIT_HEARTS, SUIT_DIAMONDS};
    for (int j = 0; j < 4; j++) seq[k++] = CARD(jacks[j], RANK_JACK);
    if (type < 4) {
        static const int ranks[7] = {RANK_ACE, 10, RANK_KING, RANK_QUEEN, 9, 8, 7};
        for (int j = 0; j < 7; j++) seq[k++] = CARD(type, ranks[j]);
    }
    int with = trick_find(cards, n, seq[0]) >= 0, m = 0;
    while (m < k && (trick_find(cards, n, seq[m]) >= 0) == with) m++;
    return with ? m : -m;
}

/* the order a hand is shown in: jacks (but in null), the trumps' suit, then the other suits, their colours
 * alternating (clubs, hearts, spades, diamonds), the strongest first */
void skat_sort(uint8_t *cards, int n, int type)
{
    int order[4], k = 0;                        /* the place of each suit */
    static const int alt[4] = {SUIT_CLUBS, SUIT_HEARTS, SUIT_SPADES, SUIT_DIAMONDS};
    if (type >= 0 && type < 4) {
        int red = type == SUIT_HEARTS || type == SUIT_DIAMONDS, other[2], same = -1, no = 0;
        for (int i = 0; i < 4; i++) {
            int s = alt[i];
            if (s == type) continue;
            if ((s == SUIT_HEARTS || s == SUIT_DIAMONDS) == red) same = s;
            else other[no++] = s;
        }
        order[type] = k++;
        order[other[0]] = k++;
        order[same] = k++;
        order[other[1]] = k++;
    } else
        for (int i = 0; i < 4; i++) order[alt[i]] = k++;
    int t = type < 0 ? SG_GRAND : type;
    for (int i = 1; i < n; i++) {
        uint8_t c = cards[i];
        int j = i;
#define KEY(x) ((skat_suit(x, t) == SUIT_TRUMP && CARD_RANK(x) == RANK_JACK ? 0 : 1 + order[CARD_SUIT(x)]) * 32 + \
                (31 - skat_power(x, t)))
        while (j > 0 && KEY(cards[j - 1]) > KEY(c)) { cards[j] = cards[j - 1]; j--; }
#undef KEY
        cards[j] = c;
    }
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
static int forehand(const skpos *p) { return (p->dealer + 1) % SKAT_P; }

static void deal(skpos *p)
{
    uint8_t deck[CARDS_MAX];
    int k = cards_pack(PACK_32, deck), i = 0;
    cards_shuffle(deck, k, trick_mix(p->seed, 2000u + p->deal));
    p->dealer = (uint8_t)((2 + p->deal) % SKAT_P);
    for (int pl = 0; pl < SKAT_P; pl++) p->n[pl] = 0, p->won[pl] = p->points[pl] = 0;
    static const int packets[4] = {3, 0, 4, 3};   /* 3 each, the skat, 4 each, 3 each */
    for (int r = 0; r < 4; r++) {
        if (!packets[r]) { p->skat[0] = deck[i++], p->skat[1] = deck[i++]; continue; }
        for (int q = 1; q <= SKAT_P; q++) {
            int pl = (p->dealer + q) % SKAT_P;
            for (int c = 0; c < packets[r]; c++) p->hand[pl][p->n[pl]++] = deck[i++];
        }
    }
    p->nskat = 2;
    memcpy(p->dealt, p->skat, 2);
    for (int pl = 0; pl < SKAT_P; pl++) skat_sort(p->hand[pl], p->n[pl], -1);
    p->stage = 1;
    p->sayer = (uint8_t)((p->dealer + 2) % SKAT_P);   /* middlehand says to forehand */
    p->hearer = (uint8_t)forehand(p);
    p->turn = p->sayer;
    p->bid = 0;
    memset(p->word, SW_NONE, sizeof p->word);
    memset(p->said, 0, sizeof p->said);
    p->declarer = SKAT_NONE;
    p->type = p->flags = p->hand_game = 0;
    p->ntrick = p->tricks = 0;
    memset(p->last, CARD_NONE, sizeof p->last);
    p->last_leader = p->last_winner = SKAT_NONE;
    p->phase = SK_BID;
}

void skat_new(skgame *g, int variant, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->seed = g->pos.seed = seed;
    g->variant = g->pos.variant = (uint8_t)(variant == SKAT_LONG ? SKAT_LONG : SKAT_SHORT);
    deal(&g->pos);
}

/* ---- legality ---------------------------------------------------------------------------------------------------- */
int skat_saying(const skpos *p) { return p->phase == SK_BID && (p->stage == 3 || p->turn == p->sayer); }

int skat_can_play(const skpos *p, int card)
{
    if (p->phase != SK_PLAY) return 0;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    if (trick_find(h, n, card) < 0) return 0;
    if (p->ntrick == 0) return 1;
    int led = skat_suit(p->trick[0], p->type);
    if (skat_suit(card, p->type) == led) return 1;
    for (int i = 0; i < n; i++)
        if (skat_suit(h[i], p->type) == led) return 0;
    return 1;
}

int skat_game_ok(const skpos *p, int type, int flags)
{
    if (p->phase != SK_SKAT && !(p->phase == SK_DISCARD && p->nskat == 2)) return 0;
    if (type < 0 || type >= SG_TYPES || (flags & ~7)) return 0;
    int hand = p->phase == SK_SKAT;
    if (type == SG_NULL) return !(flags & ~SF_OUVERT) && skat_base(type, flags, hand) >= p->bid;
    if (flags && !hand) return 0;               /* announcements: hand games only */
    if ((flags & SF_SCHWARZ) && !(flags & SF_SCHNEIDER)) return 0;
    if ((flags & SF_OUVERT) && !(flags & SF_SCHWARZ)) return 0;
    return 1;
}

int skat_legal(const skpos *p, uint32_t act)
{
    int a = SKA_ARG(act), pl = p->turn;
    switch (SKA_KIND(act)) {
    case SA_SAY: return skat_saying(p) && is_bid(a) && a > p->bid;
    case SA_HOLD: return p->phase == SK_BID && !skat_saying(p) && a == 0;
    case SA_PASS: return p->phase == SK_BID && a == 0;
    case SA_PICKUP: return p->phase == SK_SKAT && a == 0;
    case SA_PUT: return p->phase == SK_DISCARD && p->nskat < 2 && trick_find(p->hand[pl], p->n[pl], a) >= 0;
    case SA_TAKE: return p->phase == SK_DISCARD && trick_find(p->skat, p->nskat, a) >= 0;
    case SA_GAME: return skat_game_ok(p, a & 15, a >> 4);
    case SA_PLAY: return skat_can_play(p, a);
    case SA_NEXT: return p->phase == SK_DEAL && a == 0;
    default: return 0;
    }
}

/* ---- the deal's end ---------------------------------------------------------------------------------------------- */
int skat_total(const skpos *p, int player)
{
    int others = 0;
    for (int q = 0; q < SKAT_P; q++)
        if (q != player) others += p->losses[q];
    return p->score[player] + 50 * (p->wins[player] - p->losses[player]) + 40 * others;
}

static void deal_end(skpos *p)
{
    skdeal *r = &p->list[p->ndeals < SKAT_DEALS_MAX ? p->ndeals : SKAT_DEALS_MAX - 1];
    memset(r, 0, sizeof *r);
    r->declarer = p->declarer;
    if (p->declarer != SKAT_NONE) {
        int d = p->declarer, pts = p->points[d] + skat_points(p->skat[0]) + skat_points(p->skat[1]);
        r->type = p->type;
        r->flags = p->flags;
        r->hand = p->hand_game;
        r->bid = p->bid;
        r->points = (uint8_t)pts;
        r->tricks = p->won[d];
        if (p->type == SG_NULL) {
            r->won = p->won[d] == 0;
            r->base = (uint8_t)skat_base(SG_NULL, p->flags, p->hand_game);
            r->value = r->base;
        } else {
            int ann_s = p->flags & SF_SCHNEIDER ? 1 : 0, ann_z = p->flags & SF_SCHWARZ ? 1 : 0;
            int ouvert = p->flags & SF_OUVERT ? 1 : 0;
            int schneider = pts >= 90 || pts <= 30, schwarz = p->won[d] == 10 || p->won[d] == 0;
            r->won = pts >= 61 && (!ann_s || pts >= 90) && (!ann_z || p->won[d] == 10);
            r->schneider = (uint8_t)(schneider || ann_s);
            r->schwarz = (uint8_t)(schwarz || ann_z);
            r->matadors = (int8_t)skat_matadors(p->cards12, SKAT_HAND, p->type);
            r->levels = (uint8_t)(abs(r->matadors) + 1 + p->hand_game + r->schneider + ann_s + r->schwarz + ann_z +
                                  ouvert);
            r->base = (uint8_t)skat_base(p->type, p->flags, p->hand_game);
            r->value = (int16_t)(r->base * r->levels);
            if (r->value < p->bid) {            /* overbid: lost, at the lowest multiple of the base over the bid */
                r->overbid = 1;
                r->won = 0;
                r->value = (int16_t)((p->bid + r->base - 1) / r->base * r->base);
            }
        }
        r->score = (int16_t)(r->won ? r->value : -2 * r->value);
        p->score[d] = (int16_t)(p->score[d] + r->score);
        if (r->won) p->wins[d]++;
        else p->losses[d]++;
    }
    if (p->ndeals < SKAT_DEALS_MAX) p->ndeals++;
    p->phase = p->ndeals >= skat_deals(p->variant) ? SK_OVER : SK_DEAL;
    p->turn = p->dealer;                        /* the next deal's middlehand: the first to speak */
}

/* ---- the actions ------------------------------------------------------------------------------------------------- */
static void end_duel(skpos *p, int survivor)
{
    if (p->stage == 1) {
        p->stage = 2;
        p->sayer = p->dealer;                   /* rearhand says to the survivor */
        p->hearer = (uint8_t)survivor;
        p->turn = p->sayer;
        return;
    }
    if (p->bid > 0) {                           /* the declarer */
        p->declarer = (uint8_t)survivor;
        p->turn = (uint8_t)survivor;
        p->phase = SK_SKAT;
        return;
    }
    p->stage = 3;                               /* nobody said anything: forehand may still bid */
    p->sayer = p->hearer = (uint8_t)forehand(p);
    p->turn = p->sayer;
}

static void apply(skpos *p, uint32_t act)
{
    int pl = p->turn, a = SKA_ARG(act);
    switch (SKA_KIND(act)) {
    case SA_SAY:
        p->bid = (int16_t)a;
        p->said[pl] = (int16_t)a;
        p->word[pl] = SW_SAID;
        if (p->stage == 3) {
            p->declarer = (uint8_t)pl;
            p->phase = SK_SKAT;
        } else
            p->turn = p->hearer;
        break;
    case SA_HOLD:
        p->said[pl] = p->bid;
        p->word[pl] = SW_HELD;
        p->turn = p->sayer;
        break;
    case SA_PASS:
        p->word[pl] = SW_PASSED;
        if (p->stage == 3) { deal_end(p); break; }   /* all passed: thrown in */
        end_duel(p, pl == p->sayer ? p->hearer : p->sayer);
        break;
    case SA_PICKUP:
        p->hand[pl][p->n[pl]++] = p->skat[0];
        p->hand[pl][p->n[pl]++] = p->skat[1];
        p->nskat = 0;
        skat_sort(p->hand[pl], p->n[pl], -1);
        p->phase = SK_DISCARD;
        break;
    case SA_PUT:
        trick_remove(p->hand[pl], &p->n[pl], a);
        p->skat[p->nskat++] = (uint8_t)a;
        break;
    case SA_TAKE: {
        int i = trick_find(p->skat, p->nskat, a);
        if (i == 0) p->skat[0] = p->skat[1];
        p->nskat--;
        p->hand[pl][p->n[pl]++] = (uint8_t)a;
        skat_sort(p->hand[pl], p->n[pl], -1);
        break;
    }
    case SA_GAME:
        p->type = (uint8_t)(a & 15);
        p->flags = (uint8_t)(a >> 4);
        if (p->type == SG_NULL) p->flags &= SF_OUVERT;
        else if (p->flags & SF_OUVERT) p->flags |= SF_SCHWARZ | SF_SCHNEIDER;
        else if (p->flags & SF_SCHWARZ) p->flags |= SF_SCHNEIDER;
        p->hand_game = p->phase == SK_SKAT;
        memcpy(p->cards12, p->hand[pl], 10);
        p->cards12[10] = p->skat[0];
        p->cards12[11] = p->skat[1];
        for (int q = 0; q < SKAT_P; q++) skat_sort(p->hand[q], p->n[q], p->type);
        p->phase = SK_PLAY;
        p->turn = p->leader = (uint8_t)forehand(p);
        break;
    case SA_PLAY: {
        trick_remove(p->hand[pl], &p->n[pl], a);
        p->trick[p->ntrick++] = (uint8_t)a;
        if (p->ntrick < SKAT_P) { p->turn = (uint8_t)((pl + 1) % SKAT_P); break; }
        int w = (p->leader + skat_trick_best(p->trick, SKAT_P, p->type)) % SKAT_P, pts = 0;
        for (int i = 0; i < SKAT_P; i++) pts += skat_points(p->trick[i]);
        memcpy(p->last, p->trick, SKAT_P);
        p->last_leader = p->leader;
        p->last_winner = (uint8_t)w;
        p->won[w]++;
        p->points[w] = (uint8_t)(p->points[w] + pts);
        p->ntrick = 0;
        p->tricks++;
        p->turn = p->leader = (uint8_t)w;
        if (p->tricks == 10 || (p->type == SG_NULL && w == p->declarer)) deal_end(p);
        break;
    }
    case SA_NEXT:
        p->deal++;
        deal(p);
        break;
    }
}

int skat_play(skgame *g, uint32_t act)
{
    if (g->n >= SKAT_MAX_ACTS || !skat_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = act;
    return 1;
}

int skat_undo(skgame *g)
{
    if (!g->n) return 0;
    static uint32_t acts[SKAT_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    skat_new(g, g->variant, g->seed);
    for (int i = 0; i < n; i++) skat_play(g, acts[i]);
    return 1;
}

int skat_winners(const skpos *p)
{
    if (p->phase != SK_OVER) return 0;
    int best = -100000, m = 0;
    for (int q = 0; q < SKAT_P; q++) best = skat_total(p, q) > best ? skat_total(p, q) : best;
    for (int q = 0; q < SKAT_P; q++) m |= (skat_total(p, q) == best) << q;
    return m;
}

/* ---- plain actions ----------------------------------------------------------------------------------------------- */
static int count_trumps(const uint8_t *h, int n, int type)
{
    int t = 0;
    for (int i = 0; i < n; i++) t += skat_trump(h[i], type);
    return t;
}

/* the value the hand hopes for in this game (0: not worth it) */
static int hope(const uint8_t *h, int n, int type)
{
    int t = count_trumps(h, n, type), m = abs(skat_matadors(h, n, type)), aces = 0;
    for (int i = 0; i < n; i++) aces += CARD_RANK(h[i]) == RANK_ACE;
    if (m > 3) m = 3;
    if (type == SG_GRAND) return t >= 3 && aces >= 2 ? 24 * (m + 1) : 0;
    return t * 10 >= n * 5 ? skat_base(type, 0, 0) * (m + 1) : 0;
}

static int plan(const uint8_t *h, int n, int bid)  /* the game to name: the best one that reaches the bid */
{
    int best = -1, bt = -1;
    for (int t = 0; t <= SG_GRAND; t++) {
        int v = hope(h, n, t), k = count_trumps(h, n, t) * 2 + (v >= bid) * 100 + (t == SG_GRAND) * 3;
        if (v > 0 && k > bt) best = t, bt = k;
    }
    if (best >= 0) return best;
    for (int t = 0; t < 4; t++)                 /* nothing good: the longest suit */
        if (best < 0 || count_trumps(h, n, t) > count_trumps(h, n, best)) best = t;
    return best;
}

uint32_t skat_auto(const skpos *p)
{
    int pl = p->turn;
    const uint8_t *h = p->hand[pl];
    int n = p->n[pl];
    switch (p->phase) {
    case SK_BID: {
        int limit = 0;
        for (int t = 0; t <= SG_GRAND; t++) limit = hope(h, n, t) > limit ? hope(h, n, t) : limit;
        if (skat_saying(p)) {
            int next = skat_next_bid(p->bid);
            return next && next <= limit ? SA_SAY | (uint32_t)next : SA_PASS;
        }
        return p->bid <= limit ? SA_HOLD : SA_PASS;
    }
    case SK_SKAT: return SA_PICKUP;
    case SK_DISCARD: {
        int t = plan(h, n, p->bid);
        if (p->nskat == 2) return SKA_GAME(t, 0);
        int k = -1;                             /* lay away the weakest card that is not a trump */
        for (int i = 0; i < n; i++) {
            if (skat_trump(h[i], t)) continue;
            if (k < 0 || skat_power(h[i], t) < skat_power(h[k], t)) k = i;
        }
        return SA_PUT | h[k < 0 ? n - 1 : k];
    }
    case SK_PLAY:
        for (int i = 0; i < n; i++)
            if (skat_can_play(p, h[i])) return SA_PLAY | h[i];
        return 0;
    case SK_DEAL: return SA_NEXT;
    default: return 0;
    }
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int skat_script(skgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        uint32_t act;
        const skpos *p = &g->pos;
        if (*s == '#' || *s == '!') {           /* plain actions to the end of the deal (!: of the game) */
            int all = *s++ == '!';
            while (p->phase != SK_OVER && (all || p->phase != SK_DEAL))
                if (!skat_play(g, skat_auto(p))) return 0;
            continue;
        }
        char c = *s++;
        if (c == '.') act = skat_auto(p);
        else if (c == 'h') act = SA_HOLD;
        else if (c == 'p') act = SA_PASS;
        else if (c == 's') act = SA_PICKUP;
        else if (c == 'N') act = SA_NEXT;
        else if (c == 'b') {
            int v = 0, digits = 0;
            while (*s >= '0' && *s <= '9' && digits < 3) v = v * 10 + (*s++ - '0'), digits++;
            if (!digits) return 0;
            act = SA_SAY | (uint32_t)v;
        } else if (c == 'd' || c == 'u') {
            int card = trick_parse(&s);
            if (card == CARD_NONE) return 0;
            act = (c == 'd' ? SA_PUT : SA_TAKE) | (uint32_t)card;
        } else if (c == 'g') {
            static const char types[] = "shdcgn";
            const char *t = *s ? strchr(types, *s) : NULL;
            if (!t) return 0;
            s++;
            int flags = 0;
            for (;; s++) {
                if (*s == 'S') flags |= SF_SCHNEIDER;
                else if (*s == 'Z') flags |= SF_SCHWARZ;
                else if (*s == 'O') flags |= SF_OUVERT;
                else break;
            }
            act = SKA_GAME(t - types, flags);
        } else {
            s--;
            int card = trick_parse(&s);
            if (card == CARD_NONE) return 0;
            act = SA_PLAY | (uint32_t)card;
        }
        if (!skat_play(g, act)) return 0;
    }
    return 1;
}
