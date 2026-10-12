/*
 * Euchre: the rules (euchre.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/euchre.h"
#include "games/tricks.h"

/* ---- the cards --------------------------------------------------------------------------------------------------- */
static int same_colour(int suit) { return 3 - suit; }   /* spades and clubs, hearts and diamonds */

static int left_bower(int card, int trump)
{
    return trump < 4 && CARD_RANK(card) == RANK_JACK && CARD_SUIT(card) == same_colour(trump);
}

int euchre_suit(int card, int trump) { return left_bower(card, trump) ? trump : CARD_SUIT(card); }

int euchre_power(int card, int trump)
{
    int r = trick_rank(card);                   /* 9..14 */
    if (trump < 4 && CARD_RANK(card) == RANK_JACK) {
        if (CARD_SUIT(card) == trump) return 40;
        if (CARD_SUIT(card) == same_colour(trump)) return 39;
    }
    return euchre_suit(card, trump) == trump ? 20 + r : r;
}

int euchre_trick_best(const uint8_t *cards, int n, int trump)
{
    int best = 0;
    for (int i = 1; i < n; i++) {
        int s = euchre_suit(cards[i], trump), bs = euchre_suit(cards[best], trump);
        if (s == bs ? euchre_power(cards[i], trump) > euchre_power(cards[best], trump) : s == trump) best = i;
    }
    return best;
}

/* shown left to right: the trumps (the bowers first), then the other suits, their colours alternating; the
 * strongest first */
void euchre_sort(uint8_t *cards, int n, int trump)
{
    static const int alt[4] = {SUIT_SPADES, SUIT_HEARTS, SUIT_CLUBS, SUIT_DIAMONDS};
    int order[4], k = 0;
    if (trump < 4) {
        int red = trump == SUIT_HEARTS || trump == SUIT_DIAMONDS, other[2], same = same_colour(trump), no = 0;
        for (int i = 0; i < 4; i++)
            if ((alt[i] == SUIT_HEARTS || alt[i] == SUIT_DIAMONDS) != red) other[no++] = alt[i];
        order[trump] = k++;
        order[other[0]] = k++;
        order[same] = k++;
        order[other[1]] = k++;
    } else
        for (int i = 0; i < 4; i++) order[alt[i]] = k++;
    for (int i = 1; i < n; i++) {
        uint8_t c = cards[i];
        int j = i;
#define KEY(x) (order[euchre_suit(x, trump)] * 64 + 63 - euchre_power(x, trump))
        while (j > 0 && KEY(cards[j - 1]) > KEY(c)) { cards[j] = cards[j - 1]; j--; }
#undef KEY
        cards[j] = c;
    }
}

/* ---- the deal ---------------------------------------------------------------------------------------------------- */
int euchre_sitting(const epos *p, int player) { return p->out == player; }

static int next_player(const epos *p, int pl)  /* the next one in play (one may sit out) */
{
    pl = (pl + 1) % EU_P;
    return pl == p->out ? (pl + 1) % EU_P : pl;
}

static void deal(epos *p)
{
    uint8_t pack[CARDS_MAX], deck[24];
    int k = cards_pack(PACK_32, pack), m = 0, i = 0;
    for (int j = 0; j < k; j++)
        if (CARD_RANK(pack[j]) == RANK_ACE || CARD_RANK(pack[j]) >= 9) deck[m++] = pack[j];
    cards_shuffle(deck, m, trick_mix(p->seed, 3000u + p->deal));
    p->dealer = (uint8_t)((3 + p->deal) % EU_P);
    for (int pl = 0; pl < EU_P; pl++) p->n[pl] = 0;
    static const int packets[2][4] = {{3, 2, 3, 2}, {2, 3, 2, 3}};
    for (int r = 0; r < 2; r++)
        for (int q = 0; q < EU_P; q++) {
            int pl = (p->dealer + 1 + q) % EU_P;
            for (int c = 0; c < packets[r][q]; c++) p->hand[pl][p->n[pl]++] = deck[i++];
        }
    memcpy(p->kitty, deck + i, 4);
    p->up = p->kitty[0];
    p->turned_down = 0;
    for (int pl = 0; pl < EU_P; pl++) euchre_sort(p->hand[pl], p->n[pl], EU_NONE);
    memset(p->word, EW_NONE, sizeof p->word);
    p->maker = p->trump = p->out = p->laid = EU_NONE;
    p->alone = 0;
    p->ntrick = p->tricks = p->nlast = 0;
    p->last_winner = EU_NONE;
    p->won[0] = p->won[1] = 0;
    memset(p->took, 0, sizeof p->took);
    p->phase = EP_ROUND1;
    p->turn = (uint8_t)((p->dealer + 1) % EU_P);
}

void euchre_new(egame *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->seed = g->pos.seed = seed;
    deal(&g->pos);
}

/* ---- legality ---------------------------------------------------------------------------------------------------- */
int euchre_can_play(const epos *p, int card)
{
    if (p->phase != EP_PLAY) return 0;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    if (trick_find(h, n, card) < 0) return 0;
    if (p->ntrick == 0) return 1;
    int led = euchre_suit(p->trick[0], p->trump);
    if (euchre_suit(card, p->trump) == led) return 1;
    for (int i = 0; i < n; i++)
        if (euchre_suit(h[i], p->trump) == led) return 0;
    return 1;
}

int euchre_legal(const epos *p, int act)
{
    int a = EA_ARG(act);
    switch (EA_KIND(act)) {
    case EA_ORDER: return p->phase == EP_ROUND1 && (a & ~EA_ALONE) == 0;
    case EA_PASS: return (p->phase == EP_ROUND1 || (p->phase == EP_ROUND2 && p->turn != p->dealer)) && a == 0;
    case EA_NAME: return p->phase == EP_ROUND2 && (a & ~(EA_ALONE | 3)) == 0 && (a & 3) != CARD_SUIT(p->up);
    case EA_DISCARD: return p->phase == EP_DISCARD && trick_find(p->hand[p->turn], p->n[p->turn], a) >= 0;
    case EA_PLAY: return euchre_can_play(p, a);
    case EA_NEXT: return p->phase == EP_HAND && a == 0;
    default: return 0;
    }
}

/* ---- the actions ------------------------------------------------------------------------------------------------- */
static void play_starts(epos *p)
{
    p->phase = EP_PLAY;
    for (int pl = 0; pl < EU_P; pl++) euchre_sort(p->hand[pl], p->n[pl], p->trump);
    p->turn = p->leader = (uint8_t)next_player(p, p->dealer);
}

static void make(epos *p, int suit, int alone)
{
    p->maker = p->turn;
    p->trump = (uint8_t)suit;
    p->alone = (uint8_t)(alone != 0);
    if (alone) p->out = (uint8_t)((p->maker + 2) % EU_P);
}

static void hand_end(epos *p)
{
    int m = p->maker & 1, t = p->won[m];
    p->pts[0] = p->pts[1] = 0;
    if (t >= 3) p->pts[m] = (int8_t)(t == 5 ? (p->alone ? 4 : 2) : 1);
    else p->pts[!m] = 2;
    for (int s = 0; s < 2; s++) p->score[s] = (int16_t)(p->score[s] + p->pts[s]);
    p->phase = p->score[0] >= EU_GOAL || p->score[1] >= EU_GOAL ? EP_OVER : EP_HAND;
    p->turn = (uint8_t)((p->dealer + 2) % EU_P);   /* the next hand's first to speak */
}

static void apply(epos *p, int act)
{
    int pl = p->turn, a = EA_ARG(act);
    switch (EA_KIND(act)) {
    case EA_ORDER:
        p->word[pl] = EW_ORDER;
        make(p, CARD_SUIT(p->up), a & EA_ALONE);
        if (p->out == p->dealer) { play_starts(p); break; }   /* the dealer sits out: the card stays */
        p->hand[p->dealer][p->n[p->dealer]++] = p->up;
        euchre_sort(p->hand[p->dealer], p->n[p->dealer], p->trump);
        p->phase = EP_DISCARD;
        p->turn = p->dealer;
        break;
    case EA_PASS:
        p->word[pl] = EW_PASS;
        p->turn = (uint8_t)((pl + 1) % EU_P);
        if (pl == p->dealer) {                  /* everyone passed: round two */
            p->phase = EP_ROUND2;
            p->turned_down = 1;
            memset(p->word, EW_NONE, sizeof p->word);
        }
        break;
    case EA_NAME:
        p->word[pl] = EW_NAME;
        make(p, a & 3, a & EA_ALONE);
        play_starts(p);
        break;
    case EA_DISCARD:
        trick_remove(p->hand[pl], &p->n[pl], a);
        p->laid = (uint8_t)a;
        play_starts(p);
        break;
    case EA_PLAY: {
        trick_remove(p->hand[pl], &p->n[pl], a);
        p->who[p->ntrick] = (uint8_t)pl;
        p->trick[p->ntrick++] = (uint8_t)a;
        int in = p->out == EU_NONE ? EU_P : EU_P - 1;
        if (p->ntrick < in) { p->turn = (uint8_t)next_player(p, pl); break; }
        int w = p->who[euchre_trick_best(p->trick, p->ntrick, p->trump)];
        memcpy(p->last, p->trick, EU_P);
        memcpy(p->last_who, p->who, EU_P);
        p->nlast = p->ntrick;
        p->last_winner = (uint8_t)w;
        p->won[w & 1]++;
        p->took[w]++;
        p->ntrick = 0;
        p->tricks++;
        p->turn = p->leader = (uint8_t)w;
        if (p->tricks == 5) hand_end(p);
        break;
    }
    case EA_NEXT:
        p->deal++;
        deal(p);
        break;
    }
}

int euchre_play(egame *g, int act)
{
    if (g->n >= EU_MAX_ACTS || !euchre_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int euchre_undo(egame *g)
{
    if (!g->n) return 0;
    static uint16_t acts[EU_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    euchre_new(g, g->seed);
    for (int i = 0; i < n; i++) euchre_play(g, acts[i]);
    return 1;
}

int euchre_winner(const epos *p)
{
    if (p->phase != EP_OVER) return -1;
    return p->score[1] > p->score[0];
}

/* ---- plain actions ----------------------------------------------------------------------------------------------- */
static int strength(const uint8_t *h, int n, int trump)   /* the trumps held, the bowers counting more */
{
    int s = 0;
    for (int i = 0; i < n; i++)
        if (euchre_suit(h[i], trump) == trump) s += euchre_power(h[i], trump) >= 39 ? 2 : 1;
    return s;
}

int euchre_auto(const epos *p)
{
    int pl = p->turn;
    const uint8_t *h = p->hand[pl];
    int n = p->n[pl];
    switch (p->phase) {
    case EP_ROUND1: {
        int s = strength(h, n, CARD_SUIT(p->up));
        return s >= 3 ? EA_ORDER | (s >= 6 ? EA_ALONE : 0) : EA_PASS;
    }
    case EP_ROUND2: {
        int best = -1, bs = -1;
        for (int t = 0; t < 4; t++)
            if (t != CARD_SUIT(p->up) && strength(h, n, t) > bs) best = t, bs = strength(h, n, t);
        return bs >= 3 || pl == p->dealer ? EA_NAME | best : EA_PASS;
    }
    case EP_DISCARD: {
        int k = n - 1;                          /* the weakest card that is not a trump (the hand is sorted) */
        for (int i = n - 1; i >= 0; i--)
            if (euchre_suit(h[i], p->trump) != p->trump &&
                (euchre_suit(h[k], p->trump) == p->trump || euchre_power(h[i], p->trump) < euchre_power(h[k], p->trump)))
                k = i;
        return EA_DISCARD | h[k];
    }
    case EP_PLAY:
        for (int i = 0; i < n; i++)
            if (euchre_can_play(p, h[i])) return EA_PLAY | h[i];
        return 0;
    case EP_HAND: return EA_NEXT;
    default: return 0;
    }
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
int euchre_script(egame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int act;
        const epos *p = &g->pos;
        if (*s == '#' || *s == '!') {
            int all = *s++ == '!';
            while (p->phase != EP_OVER && (all || p->phase != EP_HAND))
                if (!euchre_play(g, euchre_auto(p))) return 0;
            continue;
        }
        char c = *s++;
        if (c == '.') act = euchre_auto(p);
        else if (c == 'p') act = EA_PASS;
        else if (c == 'N') act = EA_NEXT;
        else if (c == 'o') {
            act = EA_ORDER;
            if (*s == 'a') s++, act |= EA_ALONE;
        } else if (c == 'n') {
            static const char suits[] = "shdc";
            const char *t = *s ? strchr(suits, *s) : NULL;
            if (!t) return 0;
            s++;
            act = EA_NAME | (int)(t - suits);
            if (*s == 'a') s++, act |= EA_ALONE;
        } else if (c == 'd') {
            int card = trick_parse(&s);
            if (card == CARD_NONE) return 0;
            act = EA_DISCARD | card;
        } else {
            s--;
            int card = trick_parse(&s);
            if (card == CARD_NONE) return 0;
            act = EA_PLAY | card;
        }
        if (!euchre_play(g, act)) return 0;
    }
    return 1;
}
