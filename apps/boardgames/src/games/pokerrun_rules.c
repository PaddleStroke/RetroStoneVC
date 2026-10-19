/*
 * Poker Run: the rules (pokerrun.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games/pokerrun.h"

/* ---- the tables -------------------------------------------------------------------------------------------------- */
/* the hand types: chips, mult, and what each level adds */
const int16_t pkr_hand_base[HT_N][4] = {
    {5, 1, 10, 1},     /* high card */
    {10, 2, 15, 1},    /* pair */
    {20, 2, 20, 1},    /* two pair */
    {30, 3, 20, 2},    /* three of a kind */
    {35, 4, 30, 2},    /* straight */
    {40, 4, 20, 2},    /* flush */
    {45, 4, 25, 2},    /* full house */
    {65, 7, 30, 3},    /* four of a kind */
    {110, 8, 40, 4},   /* straight flush */
};

/* each charm: what it does (its rim), its rarity (0 common, 1 uncommon, 2 rare), its price */
const pkr_charm_info pkr_charms[CH_N] = {
    [CH_RUBY] = {CK_MULT, 0, 4},     [CH_JET] = {CK_CHIPS, 0, 4},    [CH_TOPAZ] = {CK_COINS, 0, 5},
    [CH_JADE] = {CK_XMULT, 1, 6},    [CH_PEBBLE] = {CK_MULT, 0, 4},  [CH_FEATHER] = {CK_MULT, 0, 5},
    [CH_LENS] = {CK_RULE, 1, 7},     [CH_COMPASS] = {CK_CHIPS, 0, 4}, [CH_SEAL] = {CK_XMULT, 1, 7},
    [CH_BELL] = {CK_COINS, 0, 5},    [CH_MIRROR] = {CK_MULT, 0, 4},  [CH_KNOT] = {CK_MULT, 0, 5},
    [CH_LADDER] = {CK_XMULT, 1, 6},  [CH_SHELL] = {CK_MULT, 0, 5},   [CH_PINES] = {CK_XMULT, 1, 6},
    [CH_CANDLE] = {CK_XMULT, 1, 6},  [CH_PALM] = {CK_CHIPS, 0, 4},   [CH_WORRY] = {CK_MULT, 0, 3},
    [CH_ANCHOR] = {CK_CHIPS, 0, 3},  [CH_LAST] = {CK_XMULT, 1, 6},   [CH_WATER] = {CK_CHIPS, 0, 4},
    [CH_SAND] = {CK_MULT, 0, 4},     [CH_MOTH] = {CK_XMULT, 1, 6},   [CH_PURSE] = {CK_MULT, 1, 6},
    [CH_BREATH] = {CK_MULT, 0, 4},   [CH_SNOW] = {CK_MULT, 1, 6},    [CH_SCRAP] = {CK_CHIPS, 0, 5},
    [CH_IVY] = {CK_XMULT, 2, 8},     [CH_TALLY] = {CK_MULT, 1, 6},   [CH_POCKET] = {CK_RULE, 1, 7},
    [CH_GLOVE] = {CK_RULE, 1, 7},    [CH_SHEARS] = {CK_RULE, 0, 5},  [CH_DOOR] = {CK_RULE, 1, 6},
    [CH_PIG] = {CK_COINS, 0, 5},     [CH_LEDGER] = {CK_COINS, 1, 6}, [CH_HAGGLE] = {CK_COINS, 1, 6},
    [CH_CLOVER] = {CK_RULE, 0, 4},   [CH_RAG] = {CK_RULE, 0, 4},     [CH_HAMMER] = {CK_RULE, 1, 6},
    [CH_GONG] = {CK_RULE, 2, 8},
};

/* the small blind's target in each ante (the big blind 1.5 times it, the boss twice) */
static const int64_t BASE[PKR_WIN_ANTE + 1] = {0, 250, 600, 1500, 3500, 8000, 18000, 33000, 55000};

/* ---- small helpers ----------------------------------------------------------------------------------------------- */
static int rnd(pkr_state *s, int n) { return n > 0 ? (int)(cards_rand(&s->rng) % (uint32_t)n) : 0; }
static int popcount(unsigned m) { int n = 0; for (; m; m &= m - 1) n++; return n; }
static int boss_on(const pkr_state *s, int b) { return s->phase != PH_BLIND && s->blind == 2 && s->boss == b; }
static int enh_of(const pkr_state *s, int di) { return boss_on(s, BO_RUST) ? EN_NONE : s->deck[di].enh; }
static int high_rank(int r) { return r == RANK_ACE ? 14 : r; }

int pkr_rank_chips(int rank) { return rank == RANK_ACE ? 11 : rank >= 10 ? 10 : rank; }
int pkr_is_face(int rank) { return rank >= RANK_JACK && rank <= RANK_KING; }

int pkr_has(const pkr_state *s, int charm)
{
    int n = 0;
    for (int i = 0; i < s->ncharms; i++) n += s->charm[i].id == charm;
    return n;
}

int64_t pkr_target(int ante, int blind, int boss)
{
    int64_t t;
    if (ante <= PKR_WIN_ANTE) t = BASE[ante < 1 ? 1 : ante];
    else {                                      /* endless: 1.7 times each ante, two figures kept */
        t = BASE[PKR_WIN_ANTE];
        for (int a = PKR_WIN_ANTE; a < ante && t < (int64_t)1 << 52; a++) t = t * 17 / 10;
        int64_t p = 1;
        while (t / p >= 100) p *= 10;
        t = t / p * p;
    }
    t = blind == 0 ? t : blind == 1 ? t * 3 / 2 : t * 2;
    if (blind == 2 && boss == BO_WALL) t = t * 3 / 2;
    return t;
}

int pkr_hand_chips(const pkr_state *s, int type)
{
    return pkr_hand_base[type][0] + (s->level[type] - 1) * pkr_hand_base[type][2];
}

int pkr_hand_mult(const pkr_state *s, int type)
{
    return pkr_hand_base[type][1] + (s->level[type] - 1) * pkr_hand_base[type][3];
}

int pkr_price(const pkr_state *s, const pkr_item *it)
{
    int p = it->price - pkr_has(s, CH_HAGGLE);
    return p < 1 ? 1 : p;
}

int pkr_reroll_cost(const pkr_state *s)
{
    int c = 4 + s->rerolls - pkr_has(s, CH_HAGGLE);
    return c < 1 ? 1 : c;
}

int pkr_sell_value(const pkr_charm *c) { return c->price / 2 < 1 ? 1 : c->price / 2; }

int pkr_interest(const pkr_state *s)
{
    int cap = 5 + 5 * pkr_has(s, CH_LEDGER), i = s->coins > 0 ? s->coins / 5 : 0;
    return i < cap ? i : cap;
}

void pkr_card_suits(const pkr_state *s, int di, uint8_t *rank, uint8_t *suits)
{
    int c = s->deck[di].card;
    *rank = (uint8_t)CARD_RANK(c);
    *suits = enh_of(s, di) == EN_PRISM ? 0xf : (uint8_t)(1 << CARD_SUIT(c));
}

/* ---- hand evaluation --------------------------------------------------------------------------------------------- */
int pkr_eval(const uint8_t *rank, const uint8_t *suits, int n, uint8_t *scoring)
{
    int cnt[15] = {0};
    unsigned all = 0xf;
    for (int i = 0; i < n; i++) cnt[rank[i]]++, all &= suits[i];
    int top = 0, second = 0, top_rank = 0;
    for (int r = 1; r <= 13; r++) {
        int c = cnt[r];
        if (c > top || (c == top && high_rank(r) > high_rank(top_rank))) {
            if (top > second) second = top;
            top = c, top_rank = r;
        } else if (c > second) second = c;
    }
    int flush = n == 5 && all != 0, straight = 0;
    if (n == 5 && top == 1) {
        int lo = 14, hi = 0;
        for (int i = 0; i < n; i++) {
            if (rank[i] < lo) lo = rank[i];
            if (rank[i] > hi) hi = rank[i];
        }
        if (hi - lo == 4) straight = 1;         /* the ace low: A 2 3 4 5 */
        else if (cnt[RANK_ACE] && cnt[10] && cnt[11] && cnt[12] && cnt[13]) straight = 1;   /* 10 J Q K A */
    }
    uint8_t m = 0;
    int type;
    if (straight && flush) type = HT_SFLUSH, m = (uint8_t)((1 << n) - 1);
    else if (top >= 4) {
        type = HT_QUADS;
        for (int i = 0; i < n; i++) m |= (uint8_t)((rank[i] == top_rank) << i);
    } else if (top == 3 && second >= 2) type = HT_FULL, m = (uint8_t)((1 << n) - 1);
    else if (flush) type = HT_FLUSH, m = (uint8_t)((1 << n) - 1);
    else if (straight) type = HT_STRAIGHT, m = (uint8_t)((1 << n) - 1);
    else if (top == 3) {
        type = HT_TRIPS;
        for (int i = 0; i < n; i++) m |= (uint8_t)((rank[i] == top_rank) << i);
    } else if (top == 2) {
        type = second == 2 ? HT_TWO : HT_PAIR;
        for (int i = 0; i < n; i++) m |= (uint8_t)((cnt[rank[i]] >= 2) << i);
    } else {
        type = HT_HIGH;
        int best = 0;
        for (int i = 1; i < n; i++)
            if (high_rank(rank[i]) > high_rank(rank[best])) best = i;
        m = (uint8_t)(1 << best);
    }
    if (scoring) *scoring = m;
    return type;
}

/* ---- the deck, the hand ------------------------------------------------------------------------------------------ */
static int sort_key(const pkr_state *s, int di)
{
    int c = s->deck[di].card, r = high_rank(CARD_RANK(c)), su = CARD_SUIT(c);
    int k = s->sort ? su * 16 + (15 - r) : (15 - r) * 8 + su;
    return k + (s->hid[di] ? 1000 : 0);         /* the cards dealt face down stay at the right end */
}

static void sort_hand(pkr_state *s)
{
    for (int i = 1; i < s->nhand; i++) {        /* insertion: stable */
        uint8_t v = s->hand[i];
        int k = sort_key(s, v), j = i - 1;
        while (j >= 0 && sort_key(s, s->hand[j]) > k) s->hand[j + 1] = s->hand[j], j--;
        s->hand[j + 1] = v;
    }
}

static void draw_up(pkr_state *s)
{
    while (s->nhand < s->hand_size && s->nhand < PKR_HAND_MAX && s->ndraw > 0) {
        uint8_t di = s->draw[--s->ndraw];
        s->hid[di] = boss_on(s, BO_FOG) && rnd(s, 4) == 0;
        s->hand[s->nhand++] = di;
    }
    sort_hand(s);
}

/* the broken cards taken out of the deck (between blinds: no hand, no draw pile then) */
static void compact(pkr_state *s)
{
    int k = 0;
    for (int i = 0; i < s->ndeck; i++)
        if (s->deck[i].card != CARD_NONE) s->deck[k++] = s->deck[i];
    s->ndeck = (uint8_t)k;
    s->nhand = s->ndraw = 0;
    memset(s->hid, 0, sizeof s->hid);
}

static void choose_boss(pkr_state *s)
{
    if (s->boss_next >= BO_N) {                 /* all met: a new order */
        for (int i = BO_N - 1; i > 0; i--) {
            int j = rnd(s, i + 1);
            uint8_t t = s->boss_order[i];
            s->boss_order[i] = s->boss_order[j], s->boss_order[j] = t;
        }
        s->boss_next = 0;
    }
    s->boss = s->boss_order[s->boss_next++];
    s->boss_suit = (uint8_t)rnd(s, 4);
}

static void begin_blind(pkr_state *s)
{
    s->phase = PH_PLAY;
    int hs = 8 + pkr_has(s, CH_POCKET) - boss_on(s, BO_FIST);
    s->hand_size = (uint8_t)(hs > PKR_HAND_MAX ? PKR_HAND_MAX : hs);
    s->hands_left = (uint8_t)((boss_on(s, BO_ROPE) ? 3 : 4) + pkr_has(s, CH_GLOVE));
    s->discards_left = (uint8_t)(boss_on(s, BO_WELL) ? 0 : 3 + pkr_has(s, CH_SHEARS));
    s->hands_played = s->discards_used = 0;
    s->score = 0;
    s->target = pkr_target(s->ante, s->blind, s->boss);
    memset(s->hid, 0, sizeof s->hid);
    s->nhand = 0;
    s->ndraw = s->ndeck;
    for (int i = 0; i < s->ndeck; i++) s->draw[i] = (uint8_t)i;
    for (int i = s->ndraw - 1; i > 0; i--) {
        int j = rnd(s, i + 1);
        uint8_t t = s->draw[i];
        s->draw[i] = s->draw[j], s->draw[j] = t;
    }
    draw_up(s);
}

/* ---- the scoring ------------------------------------------------------------------------------------------------- */
#define CAP ((int64_t)1 << 50)
static int64_t cap(int64_t v) { return v > CAP ? CAP : v; }

static void step(pkr_log *L, int kind, int what, int idx, int src, int32_t value, int64_t chips, int64_t mult)
{
    if (L->nsteps >= PKR_STEPS) return;
    pkr_step *t = &L->step[L->nsteps++];
    t->kind = (uint8_t)kind, t->what = (uint8_t)what, t->idx = (int8_t)idx, t->src = (uint8_t)src;
    t->value = value, t->chips = chips, t->mult = mult;
}

/* a times-mult in hundredths, rounded half up, never below 1 */
static int64_t times(int64_t mult, int h)
{
    int64_t m = (mult * h + 50) / 100;
    return cap(m < 1 ? 1 : m);
}

static int has_suit(const pkr_state *s, int di, int suit)
{
    uint8_t r, m;
    pkr_card_suits(s, di, &r, &m);
    return m >> suit & 1;
}

/* the hand's cards in `mask` scored on s (its random state drawn, the coins and the growing charms changed): the log */
static void score_hand(pkr_state *s, unsigned mask, pkr_log *L)
{
    memset(L, 0, sizeof *L);
    uint8_t rank[PKR_PLAY_MAX], suits[PKR_PLAY_MAX];
    for (int i = 0; i < s->nhand && L->n < PKR_PLAY_MAX; i++)
        if (mask >> i & 1) {
            L->played[L->n] = s->hand[i];
            pkr_card_suits(s, s->hand[i], &rank[L->n], &suits[L->n]);
            L->n++;
        }
    int n = L->n, type = pkr_eval(rank, suits, n, &L->scoring);
    if (pkr_has(s, CH_DOOR)) L->scoring = (uint8_t)((1 << n) - 1);
    L->type = (uint8_t)type;
    L->level = s->level[type];
    int64_t chips = pkr_hand_chips(s, type), mult = pkr_hand_mult(s, type);
    if (boss_on(s, BO_TIDE)) chips = (chips + 1) / 2, mult = (mult + 1) / 2;
    step(L, SS_BASE, SV_NONE, -1, 0, 0, chips, mult);
    /* the growing charms grow first */
    for (int c = 0; c < s->ncharms; c++) {
        pkr_charm *ch = &s->charm[c];
        if (ch->id == CH_SNOW && ch->val < 30000) ch->val++;
        if (ch->id == CH_TALLY && type == HT_PAIR && ch->val < 30000) ch->val += 3;
    }
    int lens = pkr_has(s, CH_LENS), gong = s->hands_played == 0 ? pkr_has(s, CH_GONG) : 0;
    int luck = 1 + pkr_has(s, CH_CLOVER);
    /* the scoring cards, left to right */
    for (int k = 0; k < n; k++) {
        if (!(L->scoring >> k & 1)) continue;
        int di = L->played[k], c = s->deck[di].card, r = CARD_RANK(c), en = enh_of(s, di);
        if (boss_on(s, BO_FADE) && CARD_SUIT(c) == s->boss_suit && en != EN_PRISM) {
            step(L, SS_DEBUFF, SV_NONE, k, 0, 0, chips, mult);
            continue;
        }
        int triggers = 1 + (pkr_is_face(r) ? lens : 0) + gong;
        for (int t = 0; t < triggers; t++) {
            if (t) step(L, SS_AGAIN, SV_NONE, k, 0, 0, chips, mult);
            chips = cap(chips + pkr_rank_chips(r));
            step(L, SS_CARD, SV_CHIPS, k, 0, pkr_rank_chips(r), chips, mult);
            switch (en) {
            case EN_ETCHED: chips = cap(chips + 30), step(L, SS_CARD, SV_CHIPS, k, 1, 30, chips, mult); break;
            case EN_EMBER: mult = cap(mult + 4), step(L, SS_CARD, SV_MULT, k, 1, 4, chips, mult); break;
            case EN_CRYSTAL: mult = times(mult, 200), step(L, SS_CARD, SV_XMULT, k, 1, 200, chips, mult); break;
            case EN_CLOVER:
                if (rnd(s, 5) < luck) mult = cap(mult + 20), step(L, SS_CARD, SV_MULT, k, 1, 20, chips, mult);
                if (rnd(s, 12) < luck) {
                    s->coins += 15, L->coins += 15;
                    step(L, SS_CARD, SV_COINS, k, 1, 15, chips, mult);
                }
                break;
            }
            for (int j = 0; j < s->ncharms; j++) {
                int id = s->charm[j].id, src = 2 + j;
                switch (id) {
                case CH_RUBY:
                    if (has_suit(s, di, SUIT_HEARTS))
                        mult = cap(mult + 4), step(L, SS_CARD, SV_MULT, k, src, 4, chips, mult);
                    break;
                case CH_JET:
                    if (has_suit(s, di, SUIT_SPADES))
                        chips = cap(chips + 30), step(L, SS_CARD, SV_CHIPS, k, src, 30, chips, mult);
                    break;
                case CH_TOPAZ:
                    if (has_suit(s, di, SUIT_DIAMONDS))
                        s->coins++, L->coins++, step(L, SS_CARD, SV_COINS, k, src, 1, chips, mult);
                    break;
                case CH_JADE:
                    if (has_suit(s, di, SUIT_CLUBS))
                        mult = times(mult, 125), step(L, SS_CARD, SV_XMULT, k, src, 125, chips, mult);
                    break;
                case CH_PEBBLE:
                    if (r >= 2 && r <= 5) mult = cap(mult + 3), step(L, SS_CARD, SV_MULT, k, src, 3, chips, mult);
                    break;
                case CH_FEATHER:
                    if (r == RANK_ACE) {
                        chips = cap(chips + 20), step(L, SS_CARD, SV_CHIPS, k, src, 20, chips, mult);
                        mult = cap(mult + 4), step(L, SS_CARD, SV_MULT, k, src, 4, chips, mult);
                    }
                    break;
                case CH_COMPASS:
                    if (r == RANK_ACE || (r <= 9 && r % 2))
                        chips = cap(chips + 25), step(L, SS_CARD, SV_CHIPS, k, src, 25, chips, mult);
                    break;
                case CH_SEAL:
                    if (r == RANK_KING) mult = times(mult, 150), step(L, SS_CARD, SV_XMULT, k, src, 150, chips, mult);
                    break;
                case CH_BELL:
                    if (pkr_is_face(r)) s->coins++, L->coins++, step(L, SS_CARD, SV_COINS, k, src, 1, chips, mult);
                    break;
                }
            }
        }
    }
    /* the cards held in hand: iron */
    int held = 0, iron = pkr_has(s, CH_HAMMER) ? 200 : 150;
    for (int i = 0; i < s->nhand; i++) {
        if (mask >> i & 1) continue;
        if (enh_of(s, s->hand[i]) == EN_IRON)
            mult = times(mult, iron), step(L, SS_HELD, SV_XMULT, held, 0, iron, chips, mult);
        held++;
    }
    /* the charms, left to right */
    int pair = type == HT_PAIR || type == HT_TWO || type == HT_TRIPS || type == HT_FULL || type == HT_QUADS;
    int trips = type == HT_TRIPS || type == HT_FULL || type == HT_QUADS;
    for (int j = 0; j < s->ncharms; j++) {
        const pkr_charm *ch = &s->charm[j];
        int64_t c0 = chips, m0 = mult;
        int what = SV_NONE, v = 0;
        switch (ch->id) {
        case CH_MIRROR: if (pair) mult += 8, what = SV_MULT, v = 8; break;
        case CH_KNOT: if (trips) mult += 14, what = SV_MULT, v = 14; break;
        case CH_LADDER: if (type == HT_STRAIGHT || type == HT_SFLUSH) what = SV_XMULT, v = 250; break;
        case CH_SHELL: if (type == HT_FLUSH || type == HT_SFLUSH) mult += 16, what = SV_MULT, v = 16; break;
        case CH_PINES: if (n == 3) what = SV_XMULT, v = 175; break;
        case CH_CANDLE: if (n == 1) what = SV_XMULT, v = 300; break;
        case CH_PALM: if (n == 5) chips += 60, what = SV_CHIPS, v = 60; break;
        case CH_WORRY: mult += 5, what = SV_MULT, v = 5; break;
        case CH_ANCHOR: chips += 45, what = SV_CHIPS, v = 45; break;
        case CH_LAST: if (s->hands_left == 1) what = SV_XMULT, v = 300; break;
        case CH_WATER:
            if (s->discards_left) v = 15 * s->discards_left, chips += v, what = SV_CHIPS;
            break;
        case CH_SAND:
            if (s->hands_left > 1) v = 3 * (s->hands_left - 1), mult += v, what = SV_MULT;
            break;
        case CH_MOTH: if (!s->discards_used) what = SV_XMULT, v = 200; break;
        case CH_PURSE:
            if (s->coins >= 3) v = s->coins / 3 > 30000 ? 30000 : s->coins / 3, mult += v, what = SV_MULT;
            break;
        case CH_BREATH: if (held) v = 2 * held, mult += v, what = SV_MULT; break;
        case CH_SNOW: case CH_TALLY: if (ch->val > 0) v = ch->val, mult += v, what = SV_MULT; break;
        case CH_SCRAP: if (ch->val > 0) v = ch->val, chips += v, what = SV_CHIPS; break;
        case CH_IVY: if (ch->val > 100) what = SV_XMULT, v = ch->val; break;
        }
        if (what == SV_XMULT) mult = times(mult, v);
        if (what == SV_NONE) continue;
        chips = cap(chips), mult = cap(mult);
        (void)c0, (void)m0;
        step(L, SS_CHARM, what, j, 0, v, chips, mult);
    }
    L->chips = chips, L->mult = mult;
    L->total = chips > 0 && mult > CAP / chips ? CAP : chips * mult;
    /* after the score: the toll, the crystals that break */
    if (boss_on(s, BO_TOLL)) {
        int lose = s->coins > 0 ? 1 : 0;
        s->coins -= lose, L->coins -= lose;
        if (lose) step(L, SS_COIN, SV_COINS, -1, 0, -lose, chips, mult);
    }
    if (!pkr_has(s, CH_RAG))
        for (int k = 0; k < n; k++)
            if (L->scoring >> k & 1 && enh_of(s, L->played[k]) == EN_CRYSTAL && rnd(s, 4) == 0) {
                L->broke |= (uint8_t)(1 << k);
                step(L, SS_BREAK, SV_NONE, k, 0, 0, chips, mult);
            }
}

/* ---- the shop ---------------------------------------------------------------------------------------------------- */
static int charm_taken(const pkr_state *s, int id, const pkr_item *also, int nalso)
{
    if (pkr_has(s, id)) return 1;
    for (int i = 0; i < nalso; i++)
        if (also[i].kind == IT_CHARM && also[i].id == id && !also[i].sold) return 1;
    return 0;
}

/* a charm at random (by rarity) not equipped nor among `also` */
static int random_charm(pkr_state *s, const pkr_item *also, int nalso)
{
    int roll = rnd(s, 100), rarity = roll < 68 ? 0 : roll < 93 ? 1 : 2;
    int pool[CH_N], n = 0;
    for (int id = 0; id < CH_N; id++)
        if (pkr_charms[id].rarity == rarity && !charm_taken(s, id, also, nalso)) pool[n++] = id;
    if (!n)
        for (int id = 0; id < CH_N; id++)
            if (!charm_taken(s, id, also, nalso)) pool[n++] = id;
    return n ? pool[rnd(s, n)] : CH_WORRY;
}

static void set_charm_item(pkr_state *s, pkr_item *it, int id)
{
    memset(it, 0, sizeof *it);
    it->kind = IT_CHARM, it->id = (uint8_t)id, it->price = pkr_charms[id].price;
    s->seen |= (uint64_t)1 << id;
}

static void fill_item(pkr_state *s, int i)
{
    pkr_item *it = &s->shop[i];
    int roll = rnd(s, 100);
    if (roll < 62) {
        pkr_item tmp[PKR_SHOP];
        memcpy(tmp, s->shop, sizeof tmp);
        tmp[i].kind = IT_NONE;
        set_charm_item(s, it, random_charm(s, tmp, PKR_SHOP));
    } else if (roll < 85) {
        memset(it, 0, sizeof *it);
        it->kind = IT_TOME, it->id = (uint8_t)rnd(s, HT_N), it->price = 3;
    } else {
        memset(it, 0, sizeof *it);
        it->kind = IT_ENH, it->id = (uint8_t)(1 + rnd(s, EN_N - 1)), it->price = 3;
    }
}

static const uint8_t PACK_PRICE[PK_N] = {5, 4, 4};

static void enter_shop(pkr_state *s)
{
    s->phase = PH_SHOP;
    s->rerolls = 0;
    memset(s->shop, 0, sizeof s->shop);
    for (int i = 0; i < PKR_SHOP_ITEMS; i++) fill_item(s, i);
    for (int i = PKR_SHOP_ITEMS; i < PKR_SHOP; i++) {
        pkr_item *it = &s->shop[i];
        it->kind = IT_PACK, it->id = (uint8_t)rnd(s, PK_N), it->price = PACK_PRICE[it->id];
    }
}

static pkr_card random_card(pkr_state *s)
{
    pkr_card c;
    c.card = CARD(rnd(s, 4), 1 + rnd(s, 13));
    c.enh = rnd(s, 2) ? (uint8_t)(1 + rnd(s, EN_N - 1)) : EN_NONE;
    return c;
}

static void open_pack(pkr_state *s, int kind)
{
    s->phase = PH_PACK;
    s->pack_kind = (uint8_t)kind;
    memset(s->pack, 0, sizeof s->pack);
    s->npack = kind == PK_CARD ? 4 : 3;
    for (int i = 0; i < s->npack; i++) {
        pkr_item *it = &s->pack[i];
        if (kind == PK_CHARM) set_charm_item(s, it, random_charm(s, s->pack, i));
        else if (kind == PK_TOME) {
            int t, again;
            do {                                /* three different hand types */
                t = rnd(s, HT_N), again = 0;
                for (int j = 0; j < i; j++) again |= s->pack[j].id == t;
            } while (again);
            it->kind = IT_TOME, it->id = (uint8_t)t;
        } else {
            it->kind = IT_PACK;
            it->c = random_card(s);
        }
    }
}

static void add_charm(pkr_state *s, int id, int price)
{
    if (s->ncharms >= PKR_CHARMS) return;
    pkr_charm *c = &s->charm[s->ncharms++];
    c->id = (uint8_t)id, c->price = (uint8_t)price, c->val = id == CH_IVY ? 100 : 0;
    s->seen |= (uint64_t)1 << id;
}

/* an enhancement bought: the cards offered, drawn from the deck */
static void offer_cards(pkr_state *s, int enh)
{
    s->phase = PH_ENH;
    s->pick_enh = (uint8_t)enh;
    s->pick_max = 2;
    uint8_t idx[PKR_DECK_MAX];
    for (int i = 0; i < s->ndeck; i++) idx[i] = (uint8_t)i;
    int n = s->ndeck < PKR_PICK ? s->ndeck : PKR_PICK;
    for (int i = 0; i < n; i++) {
        int j = i + rnd(s, s->ndeck - i);
        uint8_t t = idx[i];
        idx[i] = idx[j], idx[j] = t;
    }
    /* shown by rank, as the hand */
    for (int i = 1; i < n; i++) {
        uint8_t v = idx[i];
        int j = i - 1;
        while (j >= 0 && high_rank(CARD_RANK(s->deck[idx[j]].card)) < high_rank(CARD_RANK(s->deck[v].card)))
            idx[j + 1] = idx[j], j--;
        idx[j + 1] = v;
    }
    memcpy(s->pick, idx, (size_t)n);
    s->npick = (uint8_t)n;
}

/* ---- the run ----------------------------------------------------------------------------------------------------- */
void pkr_new(pkr_game *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->seed = seed;
    pkr_state *s = &g->s;
    s->rng = seed * 2654435761u ^ 0x5bd1e995u;
    if (!s->rng) s->rng = 1;
    for (int i = 0; i < 4; i++) cards_rand(&s->rng);
    s->ndeck = 52;
    for (int su = 0; su < 4; su++)
        for (int r = 1; r <= 13; r++) s->deck[su * 13 + r - 1].card = CARD(su, r);
    for (int i = 0; i < HT_N; i++) s->level[i] = 1;
    s->coins = 4;
    s->ante = 1;
    for (int i = 0; i < BO_N; i++) s->boss_order[i] = (uint8_t)i;
    s->boss_next = BO_N;
    choose_boss(s);
    s->phase = PH_BLIND;
    s->target = pkr_target(1, 0, s->boss);
}

int pkr_over(const pkr_state *s) { return s->phase == PH_LOST || s->phase == PH_DONE; }

static int gate_ok(const pkr_state *s, unsigned mask)
{
    if (!boss_on(s, BO_GATE)) return 1;
    int need = s->nhand < 5 ? s->nhand : 5;
    return popcount(mask) == need;
}

int pkr_legal(const pkr_state *s, int act)
{
    int k = PKR_KIND(act), a = PKR_ARG(act);
    switch (k) {
    case A_BEGIN: return s->phase == PH_BLIND;
    case A_PLAY:
    case A_DISCARD: {
        if (s->phase != PH_PLAY || a == 0 || a >> s->nhand) return 0;
        int c = popcount((unsigned)a);
        if (c > PKR_PLAY_MAX) return 0;
        if (k == A_DISCARD) return s->discards_left > 0;
        return s->hands_left > 0 && gate_ok(s, (unsigned)a);
    }
    case A_SORT: return (s->phase == PH_PLAY || s->phase == PH_BLIND) && a <= 1;
    case A_CASH: return s->phase == PH_CASH;
    case A_BUY: {
        if (s->phase != PH_SHOP || a >= PKR_SHOP) return 0;
        const pkr_item *it = &s->shop[a];
        if (it->kind == IT_NONE || it->sold || s->coins < pkr_price(s, it)) return 0;
        return it->kind != IT_CHARM || s->ncharms < PKR_CHARMS;
    }
    case A_SELL: return (s->phase == PH_SHOP || s->phase == PH_PACK) && a < s->ncharms;
    case A_REROLL: return s->phase == PH_SHOP && s->coins >= pkr_reroll_cost(s);
    case A_LEAVE: return s->phase == PH_SHOP;
    case A_PICK:
        if (s->phase != PH_PACK || a >= s->npack) return 0;
        return s->pack_kind != PK_CHARM || s->ncharms < PKR_CHARMS;
    case A_SKIP: return s->phase == PH_PACK;
    case A_ENH: return s->phase == PH_ENH && !(a >> s->npick) && popcount((unsigned)a) <= s->pick_max;
    case A_ENDLESS:
    case A_FINISH: return s->phase == PH_WON;
    case A_MOVE:
        return s->phase != PH_LOST && s->phase != PH_DONE && a / 8 < s->ncharms && a % 8 < s->ncharms && a / 8 != a % 8;
    }
    return 0;
}

static void blind_won(pkr_state *s)
{
    s->cash_blind = (int16_t)(3 + s->blind);
    s->cash_hands = s->hands_left;
    int gold = 0;
    for (int i = 0; i < s->nhand; i++) gold += enh_of(s, s->hand[i]) == EN_GILDED;
    s->cash_gilded = (int16_t)(3 * gold);
    s->cash_charms = (int16_t)(4 * pkr_has(s, CH_PIG));
    s->cash_interest = (int16_t)pkr_interest(s);
    for (int j = 0; j < s->ncharms; j++)
        if (s->charm[j].id == CH_IVY && s->charm[j].val < 30000) s->charm[j].val += 20;
    s->blinds_beaten++;
    s->phase = PH_CASH;
    compact(s);
}

int pkr_apply(pkr_state *s, int act)
{
    if (!pkr_legal(s, act)) return 0;
    int k = PKR_KIND(act), a = PKR_ARG(act);
    switch (k) {
    case A_BEGIN: begin_blind(s); break;
    case A_PLAY: {
        score_hand(s, (unsigned)a, &s->log);
        pkr_log *L = &s->log;
        s->score = cap(s->score + L->total);
        if (L->total > s->best_hand) s->best_hand = L->total;
        if (s->played_n[L->type] < 60000) s->played_n[L->type]++;
        s->hands_left--;
        s->hands_played++;
        int m = 0;
        for (int i = 0; i < s->nhand; i++)
            if (!(a >> i & 1)) s->hand[m++] = s->hand[i];
        s->nhand = (uint8_t)m;
        for (int i = 0; i < L->n; i++) {
            s->hid[L->played[i]] = 0;
            if (L->broke >> i & 1) s->deck[L->played[i]].card = CARD_NONE;   /* taken out after the blind */
        }
        if (s->score >= s->target) blind_won(s);
        else if (s->hands_left == 0) s->phase = PH_LOST;
        else draw_up(s);
        break;
    }
    case A_DISCARD: {
        int m = 0, gone = 0;
        for (int i = 0; i < s->nhand; i++)
            if (!(a >> i & 1)) s->hand[m++] = s->hand[i];
            else gone++, s->hid[s->hand[i]] = 0;
        s->nhand = (uint8_t)m;
        s->discards_left--;
        s->discards_used++;
        for (int j = 0; j < s->ncharms; j++)
            if (s->charm[j].id == CH_SCRAP && s->charm[j].val < 30000) s->charm[j].val += (int16_t)(4 * gone);
        draw_up(s);
        break;
    }
    case A_SORT:
        s->sort = (uint8_t)a;
        sort_hand(s);
        break;
    case A_CASH:
        s->coins += s->cash_blind + s->cash_hands + s->cash_interest + s->cash_gilded + s->cash_charms;
        if (s->ante == PKR_WIN_ANTE && s->blind == 2 && !s->endless) s->phase = PH_WON, s->won = 1;
        else enter_shop(s);
        break;
    case A_BUY: {
        pkr_item *it = &s->shop[a];
        s->coins -= pkr_price(s, it);
        it->sold = 1;
        switch (it->kind) {
        case IT_CHARM: add_charm(s, it->id, it->price); break;
        case IT_TOME: if (s->level[it->id] < 200) s->level[it->id]++; break;
        case IT_ENH: offer_cards(s, it->id); break;
        case IT_PACK: open_pack(s, it->id); break;
        }
        break;
    }
    case A_SELL:
        s->coins += pkr_sell_value(&s->charm[a]);
        memmove(&s->charm[a], &s->charm[a + 1], sizeof s->charm[0] * (size_t)(s->ncharms - a - 1));
        s->ncharms--;
        break;
    case A_REROLL:
        s->coins -= pkr_reroll_cost(s);
        if (s->rerolls < 200) s->rerolls++;
        for (int i = 0; i < PKR_SHOP_ITEMS; i++) s->shop[i].kind = IT_NONE;
        for (int i = 0; i < PKR_SHOP_ITEMS; i++) fill_item(s, i);
        break;
    case A_LEAVE:
        if (s->blind == 2) s->ante++, s->blind = 0, choose_boss(s);
        else s->blind++;
        s->phase = PH_BLIND;
        s->target = pkr_target(s->ante, s->blind, s->boss);
        break;
    case A_PICK: {
        pkr_item *it = &s->pack[a];
        if (s->pack_kind == PK_CHARM) add_charm(s, it->id, it->price);
        else if (s->pack_kind == PK_TOME) { if (s->level[it->id] < 200) s->level[it->id]++; }
        else if (s->ndeck < PKR_DECK_MAX) s->deck[s->ndeck++] = it->c;
        s->phase = PH_SHOP;
        break;
    }
    case A_SKIP: s->phase = PH_SHOP; break;
    case A_ENH:
        for (int i = 0; i < s->npick; i++)
            if (a >> i & 1) s->deck[s->pick[i]].enh = s->pick_enh;
        s->phase = PH_SHOP;
        break;
    case A_ENDLESS: s->endless = 1, enter_shop(s); break;
    case A_FINISH: s->phase = PH_DONE; break;
    case A_MOVE: {
        int from = a / 8, to = a % 8;
        pkr_charm c = s->charm[from];
        if (from < to) memmove(&s->charm[from], &s->charm[from + 1], sizeof c * (size_t)(to - from));
        else memmove(&s->charm[to + 1], &s->charm[to], sizeof c * (size_t)(from - to));
        s->charm[to] = c;
        break;
    }
    }
    return 1;
}

int pkr_play(pkr_game *g, int act)
{
    if (!pkr_apply(&g->s, act)) return 0;
    if (g->n < PKR_MAX_ACTS) g->act[g->n++] = (uint16_t)act;
    return 1;
}

int64_t pkr_preview(const pkr_state *s, unsigned mask, pkr_log *log)
{
    static pkr_state t;                         /* (big: not on the stack) */
    pkr_log L0, *L = log ? log : &L0;
    if (s->phase != PH_PLAY || !mask || mask >> s->nhand || popcount(mask) > PKR_PLAY_MAX) {
        memset(L, 0, sizeof *L);
        return 0;
    }
    t = *s;
    score_hand(&t, mask, L);
    return L->total;
}

int pkr_preview_type(const pkr_state *s, unsigned mask)
{
    uint8_t rank[PKR_PLAY_MAX], suits[PKR_PLAY_MAX];
    int n = 0;
    for (int i = 0; i < s->nhand; i++)
        if (mask >> i & 1) {
            if (n >= PKR_PLAY_MAX) return -1;
            pkr_card_suits(s, s->hand[i], &rank[n], &suits[n]);
            n++;
        }
    return n ? pkr_eval(rank, suits, n, NULL) : -1;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int pkr_notation(int act, char *buf, int cap)
{
    int k = PKR_KIND(act), a = PKR_ARG(act);
    switch (k) {
    case A_PLAY: return snprintf(buf, (size_t)cap, "p%x", a);
    case A_DISCARD: return snprintf(buf, (size_t)cap, "d%x", a);
    case A_ENH: return snprintf(buf, (size_t)cap, "e%x", a);
    case A_SORT: return snprintf(buf, (size_t)cap, "o%d", a);
    case A_BUY: return snprintf(buf, (size_t)cap, "k%d", a);
    case A_SELL: return snprintf(buf, (size_t)cap, "x%d", a);
    case A_PICK: return snprintf(buf, (size_t)cap, "q%d", a);
    case A_BEGIN: return snprintf(buf, (size_t)cap, "b");
    case A_CASH: return snprintf(buf, (size_t)cap, "c");
    case A_REROLL: return snprintf(buf, (size_t)cap, "r");
    case A_LEAVE: return snprintf(buf, (size_t)cap, "n");
    case A_SKIP: return snprintf(buf, (size_t)cap, "s");
    case A_ENDLESS: return snprintf(buf, (size_t)cap, "w");
    case A_FINISH: return snprintf(buf, (size_t)cap, "f");
    case A_MOVE: return snprintf(buf, (size_t)cap, "m%d%d", a / 8, a % 8);
    }
    return snprintf(buf, (size_t)cap, "?");
}

static int parse_act(const char *t, int len)
{
    char c = t[0];
    long v = 0;
    if (len > 1) {
        char tmp[16];
        int l = len - 1 < 15 ? len - 1 : 15;
        memcpy(tmp, t + 1, (size_t)l);
        tmp[l] = 0;
        char *end;
        v = strtol(tmp, &end, c == 'p' || c == 'd' || c == 'e' ? 16 : 10);
        if (*end || v < 0 || v > 0xfff) return 0;
    }
    switch (c) {
    case 'b': return PKR_ACT(A_BEGIN, 0);
    case 'p': return PKR_ACT(A_PLAY, v);
    case 'd': return PKR_ACT(A_DISCARD, v);
    case 'o': return PKR_ACT(A_SORT, v);
    case 'c': return PKR_ACT(A_CASH, 0);
    case 'k': return PKR_ACT(A_BUY, v);
    case 'x': return PKR_ACT(A_SELL, v);
    case 'r': return PKR_ACT(A_REROLL, 0);
    case 'n': return PKR_ACT(A_LEAVE, 0);
    case 'q': return PKR_ACT(A_PICK, v);
    case 's': return PKR_ACT(A_SKIP, 0);
    case 'e': return PKR_ACT(A_ENH, v);
    case 'w': return PKR_ACT(A_ENDLESS, 0);
    case 'f': return PKR_ACT(A_FINISH, 0);
    case 'm': return len == 3 && t[1] >= '0' && t[2] >= '0' ? PKR_ACT(A_MOVE, (t[1] - '0') * 8 + t[2] - '0') : 0;
    }
    return 0;
}

int pkr_script(pkr_game *g, const char *m)
{
    while (*m) {
        while (*m == ' ' || *m == ',' || *m == '\n') m++;
        if (!*m) break;
        const char *t = m;
        while (*m && *m != ' ' && *m != ',' && *m != '\n') m++;
        int len = (int)(m - t);
        if (t[0] == '#') {
            if (len == 2 && t[1] >= 'A') {      /* to a phase: b a blind, B a boss blind, s the shop, c the coins
                                                 * counted; any: the win (or the end) */
                char k = t[1];
                for (int i = 0; i < 4000 && !pkr_over(&g->s); i++) {
                    int a = pkr_bot(&g->s);
                    if (!a || !pkr_play(g, a)) return 0;
                    const pkr_state *s = &g->s;
                    if (s->phase == PH_WON || (k == 'b' && s->phase == PH_BLIND) ||
                        (k == 's' && s->phase == PH_SHOP) || (k == 'c' && s->phase == PH_CASH) ||
                        (k == 'B' && s->phase == PH_BLIND && s->blind == 2))
                        break;
                }
                continue;
            }
            int n = len > 1 ? atoi(t + 1) : 1;
            for (int i = 0; i < n && !pkr_over(&g->s); i++) {
                int a = pkr_bot(&g->s);
                if (!a || !pkr_play(g, a)) return 0;
            }
            continue;
        }
        int a = parse_act(t, len);
        if (!a || !pkr_play(g, a)) return 0;
    }
    return 1;
}

/* ---- the bot ----------------------------------------------------------------------------------------------------- */
/* what a charm is worth to the bot (plain play: pairs, a few flushes) */
static const uint8_t WORTH[CH_N] = {
    [CH_RUBY] = 5, [CH_JET] = 4, [CH_TOPAZ] = 3, [CH_JADE] = 6, [CH_PEBBLE] = 4, [CH_FEATHER] = 4, [CH_LENS] = 5,
    [CH_COMPASS] = 4, [CH_SEAL] = 5, [CH_BELL] = 3, [CH_MIRROR] = 6, [CH_KNOT] = 5, [CH_LADDER] = 3, [CH_SHELL] = 4,
    [CH_PINES] = 5, [CH_CANDLE] = 2, [CH_PALM] = 4, [CH_WORRY] = 5, [CH_ANCHOR] = 4, [CH_LAST] = 6, [CH_WATER] = 4,
    [CH_SAND] = 5, [CH_MOTH] = 4, [CH_PURSE] = 6, [CH_BREATH] = 5, [CH_SNOW] = 7, [CH_SCRAP] = 6, [CH_IVY] = 9,
    [CH_TALLY] = 7, [CH_POCKET] = 7, [CH_GLOVE] = 7, [CH_SHEARS] = 4, [CH_DOOR] = 3, [CH_PIG] = 3, [CH_LEDGER] = 2,
    [CH_HAGGLE] = 2, [CH_CLOVER] = 1, [CH_RAG] = 1, [CH_HAMMER] = 1, [CH_GONG] = 7,
};

static int most_played(const pkr_state *s)
{
    int best = HT_PAIR;
    for (int t = 0; t < HT_N; t++)
        if (s->played_n[t] > s->played_n[best]) best = t;
    return best;
}

/* the best play of the hand: its mask and score */
static unsigned best_play(const pkr_state *s, int64_t *score)
{
    unsigned best = 0;
    int64_t top = -1;
    int n = s->nhand;
    for (unsigned m = 1; m < (1u << n); m++) {
        int c = popcount(m);
        if (c > PKR_PLAY_MAX || !gate_ok(s, m)) continue;
        int64_t v = pkr_preview(s, m, NULL);
        /* among equals, fewer cards (the others stay for later) */
        if (v > top || (v == top && c < popcount(best))) top = v, best = m;
    }
    *score = top;
    return best;
}

/* the cards worth keeping for a discard: pairs and more, or four or more of a suit */
static unsigned discard_mask(const pkr_state *s)
{
    int n = s->nhand, rc[15] = {0}, sc[4] = {0};
    uint8_t rank[PKR_HAND_MAX], suits[PKR_HAND_MAX];
    for (int i = 0; i < n; i++) {
        pkr_card_suits(s, s->hand[i], &rank[i], &suits[i]);
        rc[rank[i]]++;
        for (int su = 0; su < 4; su++) sc[su] += suits[i] >> su & 1;
    }
    int fs = 0;
    for (int su = 1; su < 4; su++)
        if (sc[su] > sc[fs]) fs = su;
    unsigned keep = 0;
    for (int i = 0; i < n; i++) {
        if (sc[fs] >= 4) keep |= (unsigned)(suits[i] >> fs & 1) << i;
        else keep |= (unsigned)(rc[rank[i]] >= 2) << i;
    }
    /* the rest, the lowest first, five at most */
    unsigned out = 0;
    for (int k = 0; k < PKR_PLAY_MAX; k++) {
        int pick = -1;
        for (int i = 0; i < n; i++) {
            if (keep >> i & 1 || out >> i & 1) continue;
            if (pick < 0 || pkr_rank_chips(rank[i]) < pkr_rank_chips(rank[pick])) pick = i;
        }
        if (pick < 0) break;
        out |= 1u << pick;
    }
    return out;
}

static int weakest_charm(const pkr_state *s)
{
    int w = 0;
    for (int i = 1; i < s->ncharms; i++)
        if (WORTH[s->charm[i].id] < WORTH[s->charm[w].id]) w = i;
    return w;
}

int pkr_bot(const pkr_state *s)
{
    switch (s->phase) {
    case PH_BLIND: return PKR_ACT(A_BEGIN, 0);
    case PH_PLAY: {
        int64_t v;
        unsigned m = best_play(s, &v);
        int64_t need = s->target - s->score;
        if (v >= need || s->discards_left == 0 || v * s->hands_left >= need) return PKR_ACT(A_PLAY, m);
        unsigned d = discard_mask(s);
        return d ? PKR_ACT(A_DISCARD, d) : PKR_ACT(A_PLAY, m);
    }
    case PH_CASH: return PKR_ACT(A_CASH, 0);
    case PH_WON: return PKR_ACT(A_FINISH, 0);
    case PH_SHOP: {
        int best = -1, bw = 0;
        for (int i = 0; i < PKR_SHOP; i++) {
            const pkr_item *it = &s->shop[i];
            if (!pkr_legal(s, PKR_ACT(A_BUY, i))) continue;
            int w = 0;
            if (it->kind == IT_CHARM) w = 10 + WORTH[it->id];
            else if (it->kind == IT_PACK && it->id == PK_CHARM && s->ncharms < PKR_CHARMS) w = 12;
            else if (it->kind == IT_TOME && it->id == most_played(s)) w = 9;
            else if (it->kind == IT_PACK && it->id == PK_TOME) w = 6;
            else if (it->kind == IT_ENH && s->coins >= pkr_price(s, it) + 6) w = 5;
            if (w > bw) bw = w, best = i;
        }
        if (best >= 0) return PKR_ACT(A_BUY, best);
        if (s->ncharms == PKR_CHARMS) {         /* a better charm than the weakest: sell it */
            int wk = weakest_charm(s);
            for (int i = 0; i < PKR_SHOP_ITEMS; i++) {
                const pkr_item *it = &s->shop[i];
                if (it->kind == IT_CHARM && !it->sold && WORTH[it->id] >= WORTH[s->charm[wk].id] + 3 &&
                    s->coins + pkr_sell_value(&s->charm[wk]) >= pkr_price(s, it))
                    return PKR_ACT(A_SELL, wk);
            }
        }
        if (s->ncharms < PKR_CHARMS && s->coins >= pkr_reroll_cost(s) + 8) return PKR_ACT(A_REROLL, 0);
        return PKR_ACT(A_LEAVE, 0);
    }
    case PH_PACK: {
        int best = -1, bw = -1;
        for (int i = 0; i < s->npack; i++) {
            if (!pkr_legal(s, PKR_ACT(A_PICK, i))) continue;
            const pkr_item *it = &s->pack[i];
            int w = s->pack_kind == PK_CHARM ? WORTH[it->id]
                  : s->pack_kind == PK_TOME ? (it->id == most_played(s) ? 20 : it->id)
                  : (it->c.enh ? 10 : 0) + high_rank(CARD_RANK(it->c.card)) / 2;
            if (w > bw) bw = w, best = i;
        }
        return best >= 0 ? PKR_ACT(A_PICK, best) : PKR_ACT(A_SKIP, 0);
    }
    case PH_ENH: {
        unsigned m = 0;
        for (int i = 0; i < s->npick && i < s->pick_max; i++) m |= 1u << i;   /* the highest (shown first) */
        return PKR_ACT(A_ENH, m);
    }
    }
    return 0;
}
