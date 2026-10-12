/*
 * Rikiki: the rules (rikiki.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/rikiki.h"
#include "games/tricks.h"

int rikiki_max(int players)
{
    int m = 52 / players;
    return m > RIKIKI_HAND ? RIKIKI_HAND : m;
}

int rikiki_size(int players, int deal)
{
    int m = rikiki_max(players);
    return deal < m ? deal + 1 : 2 * m - 1 - deal;
}

static void deal(rikiki_pos *p)
{
    uint8_t deck[CARDS_MAX];
    int k = cards_pack(PACK_52, deck);
    cards_shuffle(deck, k, trick_mix(p->seed, 2000u + p->deal));
    p->size = (uint8_t)rikiki_size(p->players, p->deal);
    p->dealer = (uint8_t)(p->deal % p->players);
    p->first = (uint8_t)((p->dealer + 1) % p->players);
    for (int pl = 0; pl < p->players; pl++) p->n[pl] = 0, p->won[pl] = 0, p->bid[pl] = RIKIKI_NOBID;
    int dealt = p->size * p->players;
    for (int i = 0; i < dealt; i++) {
        int pl = (p->first + i) % p->players;
        p->hand[pl][p->n[pl]++] = deck[i];
    }
    for (int pl = 0; pl < p->players; pl++) trick_sort(p->hand[pl], p->n[pl]);
    p->turned = dealt < k ? deck[dealt] : CARD_NONE;
    p->trump = (int8_t)(p->turned == CARD_NONE ? -1 : CARD_SUIT(p->turned));
    memset(p->last, CARD_NONE, sizeof p->last);
    p->ntrick = p->tricks = 0;
    p->last_winner = p->last_leader = TRICK_NONE;
    p->phase = RK_BID;
    p->turn = p->leader = p->first;
}

void rikiki_new(rikiki_game *g, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    rikiki_pos *p = &g->pos;
    g->seed = p->seed = seed;
    g->players = p->players = (uint8_t)(players < 3 ? 3 : players > RIKIKI_MAXP ? RIKIKI_MAXP : players);
    p->max = (uint8_t)rikiki_max(p->players);
    p->deals = (uint8_t)(2 * p->max - 1);
    deal(p);
}

int rikiki_bids(const rikiki_pos *p)
{
    int s = 0;
    for (int pl = 0; pl < p->players; pl++)
        if (p->bid[pl] != RIKIKI_NOBID) s += p->bid[pl];
    return s;
}

int rikiki_forbidden(const rikiki_pos *p)
{
    if (p->phase != RK_BID || p->turn != p->dealer) return -1;
    int f = p->size - rikiki_bids(p);
    return f >= 0 ? f : -1;
}

int rikiki_can_bid(const rikiki_pos *p, int bid)
{
    return p->phase == RK_BID && bid >= 0 && bid <= p->size && bid != rikiki_forbidden(p);
}

int rikiki_can_play(const rikiki_pos *p, int card)
{
    if (p->phase != RK_PLAY) return 0;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    if (trick_find(h, n, card) < 0) return 0;
    return trick_follows(h, n, p->ntrick ? CARD_SUIT(p->trick[0]) : -1, card);
}

int rikiki_legal(const rikiki_pos *p, int act)
{
    switch (RKA_KIND(act)) {
    case RKA_BID: return rikiki_can_bid(p, RKA_ARG(act));
    case RKA_PLAY: return rikiki_can_play(p, RKA_ARG(act));
    case RKA_NEXT: return p->phase == RK_ROUND && act == RKA_NEXT;
    default: return 0;
    }
}

int rikiki_points(int bid, int won) { return bid == won ? 10 + won : -(bid > won ? bid - won : won - bid); }

static void deal_end(rikiki_pos *p)
{
    for (int pl = 0; pl < p->players; pl++) {
        p->score[pl] = (int16_t)rikiki_points(p->bid[pl], p->won[pl]);
        p->total[pl] = (int16_t)(p->total[pl] + p->score[pl]);
    }
    p->deal++;
    p->turn = (uint8_t)((p->deal + 1) % p->players);   /* the next deal's first player */
    p->phase = p->deal >= p->deals ? RK_OVER : RK_ROUND;
}

static void apply(rikiki_pos *p, int act)
{
    int pl = p->turn;
    switch (RKA_KIND(act)) {
    case RKA_BID:
        p->bid[pl] = (uint8_t)RKA_ARG(act);
        p->turn = (uint8_t)((pl + 1) % p->players);
        if (pl == p->dealer) p->phase = RK_PLAY, p->turn = p->leader = p->first;
        break;
    case RKA_PLAY: {
        int c = RKA_ARG(act);
        trick_remove(p->hand[pl], &p->n[pl], c);
        p->trick[p->ntrick++] = (uint8_t)c;
        if (p->ntrick < p->players) { p->turn = (uint8_t)((pl + 1) % p->players); break; }
        int w = (p->leader + trick_winner(p->trick, p->players, p->trump)) % p->players;
        memcpy(p->last, p->trick, p->players);
        p->won[w]++;
        p->last_leader = p->leader;
        p->last_winner = (uint8_t)w;
        p->ntrick = 0;
        p->tricks++;
        p->turn = p->leader = (uint8_t)w;
        if (p->n[w] == 0) deal_end(p);
        break;
    }
    case RKA_NEXT:
        deal(p);
        break;
    }
}

int rikiki_play(rikiki_game *g, int act)
{
    if (g->n >= RIKIKI_MAX_ACTS || !rikiki_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int rikiki_undo(rikiki_game *g)
{
    if (!g->n) return 0;
    static uint16_t acts[RIKIKI_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    rikiki_new(g, g->players, g->seed);
    for (int i = 0; i < n; i++) rikiki_play(g, acts[i]);
    return 1;
}

unsigned rikiki_winners(const rikiki_pos *p)
{
    if (p->phase != RK_OVER) return 0;
    int top = -30000;
    unsigned m = 0;
    for (int pl = 0; pl < p->players; pl++) top = p->total[pl] > top ? p->total[pl] : top;
    for (int pl = 0; pl < p->players; pl++) m |= (unsigned)(p->total[pl] == top) << pl;
    return m;
}

/* a plain bid: the aces, the kings and the trumps from the jack up, kept to what the dealer may say */
static int plain_bid(const rikiki_pos *p)
{
    const uint8_t *h = p->hand[p->turn];
    int t = 0;
    for (int i = 0; i < p->n[p->turn]; i++) {
        int r = trick_rank(h[i]);
        t += r == 14 || (r == 13 && p->size > 3) || (CARD_SUIT(h[i]) == p->trump && r >= 11 && r < 13);
    }
    if (t > p->size) t = p->size;
    if (!rikiki_can_bid(p, t)) t = t > 0 ? t - 1 : t + 1;
    return t;
}

int rikiki_auto(const rikiki_pos *p)
{
    const uint8_t *h = p->hand[p->turn];
    switch (p->phase) {
    case RK_BID: return RKA_BID | plain_bid(p);
    case RK_PLAY:
        for (int i = 0; i < p->n[p->turn]; i++)
            if (rikiki_can_play(p, h[i])) return RKA_PLAY | h[i];
        return 0;
    case RK_ROUND: return RKA_NEXT;
    default: return 0;
    }
}

int rikiki_script(rikiki_game *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int act;
        if (*s == '#' || *s == '!') {          /* plain actions to the end of the deal (!: of the game) */
            int all = *s++ == '!';
            while (g->pos.phase != RK_OVER && (all || g->pos.phase == RK_BID || g->pos.phase == RK_PLAY))
                if (!rikiki_play(g, rikiki_auto(&g->pos))) return 0;
            continue;
        }
        if (*s == '.') { s++; act = rikiki_auto(&g->pos); }
        else if (*s == 'n') { s++; act = RKA_NEXT; }
        else if (*s == 'b') {
            s++;
            int v = 0, digits = 0;
            while (*s >= '0' && *s <= '9' && digits < 3) v = v * 10 + (*s++ - '0'), digits++;
            if (!digits) return 0;
            act = RKA_BID | (v > 255 ? 255 : v);
        } else {
            int c = trick_parse(&s);
            if (c == CARD_NONE) return 0;
            act = RKA_PLAY | c;
        }
        if (!rikiki_play(g, act)) return 0;
    }
    return 1;
}
