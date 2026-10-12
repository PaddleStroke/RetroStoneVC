/*
 * Le Nain jaune: the rules (nainjaune.h). No SDL: tested by tests/nainjaune_test.c.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/nainjaune.h"

const uint8_t nj_box_card[NJ_BOXES] = {
    CARD(SUIT_DIAMONDS, 10), CARD(SUIT_CLUBS, RANK_JACK), CARD(SUIT_SPADES, RANK_QUEEN), CARD(SUIT_HEARTS, RANK_KING),
    CARD(SUIT_DIAMONDS, 7),
};

int nj_box_of(int card)
{
    for (int k = 0; k < NJ_BOXES; k++)
        if (nj_box_card[k] == card) return k;
    return -1;
}

int nj_cards_each(int players)
{
    static const int each[NJ_MAXP + 1] = {0, 0, 0, 15, 12, 9, 8};
    return players >= 3 && players <= NJ_MAXP ? each[players] : 15;
}

int nj_deals(int players) { return 2 * players; }

static int pay(njpos *p, int q, int want)       /* what q can pay of it */
{
    int v = want < p->chips[q] ? want : p->chips[q];
    p->chips[q] -= v;
    return v;
}

/* a new deal: the stakes, the shuffle, the cards dealt one at a time from the dealer's left, the talon */
static void deal(njpos *p)
{
    for (int q = 0; q < p->players; q++)
        for (int k = 0; k < NJ_BOXES; k++) p->box[k] += pay(p, q, k + 1);
    uint8_t c[CARDS_MAX];
    int n = cards_pack(PACK_52, c);
    cards_shuffle(c, n, p->seed * 977u + p->deals * 7919u + 1);
    int each = nj_cards_each(p->players), k = 0;
    memset(p->n, 0, sizeof p->n);
    for (int i = 0; i < each; i++)
        for (int j = 1; j <= p->players; j++) {
            int q = (p->dealer + j) % p->players;
            p->hand[q][p->n[q]++] = c[k++];
        }
    p->ntalon = 0;
    while (k < n) p->talon[p->ntalon++] = c[k++];
    p->nplayed = p->seq = 0;
    p->played_by = 0;
    p->need = 0;
    p->turn = p->lead = p->last = (uint8_t)((p->dealer + 1) % p->players);
    p->phase = NP_PLAY;
    p->deals++;
}

void nj_new(njgame *g, int variant, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->variant = (uint8_t)(variant == NJ_NO_OPERA ? NJ_NO_OPERA : NJ_CLASSIC);
    g->players = (uint8_t)(players < 3 ? 3 : players > NJ_MAXP ? NJ_MAXP : players);
    g->seed = seed;
    njpos *p = &g->pos;
    p->players = g->players;
    p->variant = g->variant;
    p->seed = seed;
    p->dealer = (uint8_t)(p->players - 1);      /* player 0 leads the first deal */
    p->out = -1;
    for (int q = 0; q < p->players; q++) p->chips[q] = NJ_CHIPS;
    deal(p);
}

static int holds(const njpos *p, int q, int rank)
{
    for (int i = 0; i < p->n[q]; i++)
        if (CARD_RANK(p->hand[q][i]) == rank) return 1;
    return 0;
}

static int index_in(const njpos *p, int q, int card)
{
    for (int i = 0; i < p->n[q]; i++)
        if (p->hand[q][i] == card) return i;
    return -1;
}

int nj_legal(const njpos *p, int act)
{
    if (act == NJ_NEXT) return p->phase == NP_DEALT;
    if (p->phase != NP_PLAY || act < 0 || act > 255 || index_in(p, p->turn, act) < 0) return 0;
    return !p->need || CARD_RANK(act) == p->need;
}

/* the deal is over: q went out */
static void settle(njpos *p, int q)
{
    p->out = (int8_t)q;
    memset(p->got_cards, 0, sizeof p->got_cards);
    memset(p->paid_again, 0, sizeof p->paid_again);
    for (int o = 0; o < p->players; o++) {
        if (o == q) continue;
        int v = pay(p, o, p->n[o]);             /* a chip a card */
        p->got_cards[o] -= v;
        p->got_cards[q] += v;
        p->chips[q] += v;
        for (int i = 0; i < p->n[o]; i++) {     /* the five cards still held: their stakes again */
            int k = nj_box_of(p->hand[o][i]);
            if (k < 0) continue;
            int w = pay(p, o, k + 1);
            p->box[k] += w;
            p->paid_again[o] -= w;
        }
    }
    p->opera = p->variant == NJ_CLASSIC && p->played_by == 1u << q;
    if (p->opera)
        for (int k = 0; k < NJ_BOXES; k++) {
            p->chips[q] += p->box[k];
            p->got_boxes[q] += p->box[k];
            p->box[k] = 0;
        }
    p->phase = NP_DEALT;
}

static int nj_play_pos(njpos *p, int act)
{
    if (!nj_legal(p, act)) return 0;
    if (act == NJ_NEXT) {
        int over = p->deals >= nj_deals(p->players);
        for (int q = 0; q < p->players; q++) over |= p->chips[q] < NJ_STAKES;
        if (over) { p->phase = NP_OVER; return 1; }
        p->dealer = (uint8_t)((p->dealer + 1) % p->players);
        p->out = -1;
        memset(p->got_boxes, 0, sizeof p->got_boxes);
        deal(p);
        return 1;
    }
    int q = p->turn, i = index_in(p, q, act);
    if (!p->nplayed || !p->need) p->seq = p->nplayed;   /* a new sequence starts */
    for (int j = i; j + 1 < p->n[q]; j++) p->hand[q][j] = p->hand[q][j + 1];
    p->n[q]--;
    p->played[p->nplayed++] = (uint8_t)act;
    p->played_by |= (uint16_t)(1u << q);
    p->last = (uint8_t)q;
    int k = nj_box_of(act);
    if (k >= 0) {                               /* one of the five: its compartment */
        p->chips[q] += p->box[k];
        p->got_boxes[q] += p->box[k];
        p->box[k] = 0;
    }
    if (!p->n[q]) { settle(p, q); return 1; }
    int r = CARD_RANK(act);
    if (r == RANK_KING) {                       /* the sequence ends: the same player leads */
        p->need = 0;
        p->lead = (uint8_t)q;
        return 1;
    }
    p->need = (uint8_t)(r + 1);
    for (int j = 0; j < p->players; j++) {      /* the player themself first, then round the table */
        int o = (q + j) % p->players;
        if (holds(p, o, p->need)) { p->turn = (uint8_t)o; return 1; }
    }
    p->need = 0;                                /* nobody: the last to play leads again */
    p->lead = (uint8_t)q;
    return 1;
}

int nj_play(njgame *g, int act)
{
    if (g->n >= NJ_MAX_ACTS) return 0;
    int before = g->pos.turn, phase = g->pos.phase;
    if (!nj_play_pos(&g->pos, act)) return 0;
    g->act[g->n++] = (uint8_t)act;
    if (g->pos.turn != before || g->pos.phase != phase || act == NJ_NEXT) g->floor = g->n;   /* the device passes */
    return 1;
}

int nj_undo(njgame *g)
{
    if (g->n <= g->floor) return 0;
    int n = g->n - 1, floor = g->floor;
    uint8_t acts[NJ_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n);
    nj_new(g, g->variant, g->players, g->seed);
    for (int i = 0; i < n; i++) nj_play(g, acts[i]);
    g->floor = floor;
    return 1;
}

int nj_auto(const njpos *p)
{
    if (p->phase == NP_DEALT) return NJ_NEXT;
    if (p->phase != NP_PLAY) return -1;
    int q = p->turn, best = -1;
    for (int i = 0; i < p->n[q]; i++) {
        int c = p->hand[q][i];
        if (!nj_legal(p, c)) continue;
        if (best < 0 || CARD_RANK(c) < CARD_RANK(best) || (CARD_RANK(c) == CARD_RANK(best) && c < best)) best = c;
    }
    return best;
}

unsigned nj_winners(const njpos *p)
{
    if (p->phase != NP_OVER) return 0;
    int best = -1;
    unsigned m = 0;
    for (int q = 0; q < p->players; q++) best = p->chips[q] > best ? p->chips[q] : best;
    for (int q = 0; q < p->players; q++) m |= (unsigned)(p->chips[q] == best) << q;
    return m;
}

static int parse_card(const char *s)
{
    static const char ranks[] = "a23456789tjqk", suits[] = "shdc";
    const char *r = strchr(ranks, s[0]), *u = s[0] ? strchr(suits, s[1]) : NULL;
    if (!s[0] || !s[1] || !r || !u) return -1;
    return CARD(u - suits, r - ranks + 1);
}

int nj_script(njgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == '#') {
            s++;
            while (g->pos.phase == NP_PLAY) nj_play(g, nj_auto(&g->pos));
            continue;
        }
        if (*s == '.') {
            if (!nj_play(g, nj_auto(&g->pos))) return 0;
            s++;
            continue;
        }
        if (*s == 'n') {
            if (!nj_play(g, NJ_NEXT)) return 0;
            s++;
            continue;
        }
        int c = parse_card(s);
        if (c < 0 || !nj_play(g, c)) return 0;
        s += 2;
    }
    return 1;
}
