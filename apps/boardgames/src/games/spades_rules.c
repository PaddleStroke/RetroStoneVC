/*
 * Spades: the rules (spades.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/spades.h"
#include "games/tricks.h"

static void deal(spos *p)
{
    uint8_t deck[CARDS_MAX];
    int k = cards_pack(PACK_52, deck);
    cards_shuffle(deck, k, trick_mix(p->seed, 1000u + p->round));
    p->first = (uint8_t)(p->round % SPADES_P);
    for (int pl = 0; pl < SPADES_P; pl++) p->n[pl] = 0, p->won[pl] = 0, p->bid[pl] = SPADES_NOBID;
    for (int i = 0; i < k; i++) {
        int pl = (p->first + i) % SPADES_P;
        p->hand[pl][p->n[pl]++] = deck[i];
    }
    for (int pl = 0; pl < SPADES_P; pl++) trick_sort(p->hand[pl], p->n[pl]);
    memset(p->last, CARD_NONE, sizeof p->last);
    p->ntrick = p->tricks = p->broken = 0;
    p->last_winner = p->last_leader = TRICK_NONE;
    p->phase = SP_BID;
    p->turn = p->leader = p->first;
}

void spades_new(sgame *g, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    g->seed = g->pos.seed = seed;
    deal(&g->pos);
}

int spades_can_play(const spos *p, int card)
{
    if (p->phase != SP_PLAY) return 0;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    if (trick_find(h, n, card) < 0) return 0;
    if (p->ntrick == 0)
        return CARD_SUIT(card) != SUIT_SPADES || p->broken || trick_only_suit(h, n, SUIT_SPADES);
    return trick_follows(h, n, CARD_SUIT(p->trick[0]), card);
}

int spades_legal(const spos *p, int act)
{
    switch (SA_KIND(act)) {
    case SA_BID: return p->phase == SP_BID && SA_ARG(act) <= SPADES_HAND;
    case SA_PLAY: return spades_can_play(p, SA_ARG(act));
    case SA_NEXT: return p->phase == SP_ROUND && act == SA_NEXT;
    default: return 0;
    }
}

int spades_contract(const spos *p, int side)
{
    int c = 0;
    for (int pl = side; pl < SPADES_P; pl += 2)
        if (p->bid[pl] != SPADES_NOBID) c += p->bid[pl];
    return c;
}

int spades_side_won(const spos *p, int side)
{
    int w = 0;
    for (int pl = side; pl < SPADES_P; pl += 2)
        if (p->bid[pl] != SPADES_NIL) w += p->won[pl];
    return w;
}

static void round_end(spos *p)
{
    for (int t = 0; t < 2; t++) {
        int contract = spades_contract(p, t), made = spades_side_won(p, t), pts = 0, bags = 0;
        if (contract > 0) {
            if (made >= contract) pts = 10 * contract, bags = made - contract;
            else pts = -10 * contract;
        }
        for (int pl = t; pl < SPADES_P; pl += 2) {
            if (p->bid[pl] != SPADES_NIL) { p->nil[pl] = 0; continue; }
            p->nil[pl] = (int16_t)(p->won[pl] ? -100 : 100);
            pts += p->nil[pl];
            bags += p->won[pl];             /* a nil bidder's tricks: bags */
        }
        pts += bags;
        p->bags[t] = (int16_t)(p->bags[t] + bags);
        p->penalty[t] = 0;
        while (p->bags[t] >= 10) {
            p->bags[t] = (int16_t)(p->bags[t] - 10);
            pts -= 100;
            p->penalty[t]++;
        }
        p->pts[t] = (int16_t)pts;
        p->over[t] = (int16_t)bags;
        p->score[t] = (int16_t)(p->score[t] + pts);
    }
    p->round++;
    p->turn = (uint8_t)(p->round % SPADES_P);
    int top = p->score[0] > p->score[1] ? p->score[0] : p->score[1];
    p->phase = top >= SPADES_LIMIT && p->score[0] != p->score[1] ? SP_OVER : SP_ROUND;
}

static void apply(spos *p, int act)
{
    int pl = p->turn;
    switch (SA_KIND(act)) {
    case SA_BID:
        p->bid[pl] = (uint8_t)SA_ARG(act);
        p->turn = (uint8_t)((pl + 1) % SPADES_P);
        if (p->turn == p->first) p->phase = SP_PLAY, p->leader = p->first;
        break;
    case SA_PLAY: {
        int c = SA_ARG(act);
        trick_remove(p->hand[pl], &p->n[pl], c);
        if (CARD_SUIT(c) == SUIT_SPADES) p->broken = 1;
        p->trick[p->ntrick++] = (uint8_t)c;
        if (p->ntrick < SPADES_P) { p->turn = (uint8_t)((pl + 1) % SPADES_P); break; }
        int w = (p->leader + trick_winner(p->trick, SPADES_P, SUIT_SPADES)) % SPADES_P;
        memcpy(p->last, p->trick, SPADES_P);
        p->won[w]++;
        p->last_leader = p->leader;
        p->last_winner = (uint8_t)w;
        p->ntrick = 0;
        p->tricks++;
        p->turn = p->leader = (uint8_t)w;
        if (p->n[w] == 0) round_end(p);
        break;
    }
    case SA_NEXT:
        deal(p);
        break;
    }
}

int spades_play(sgame *g, int act)
{
    if (g->n >= SPADES_MAX_ACTS || !spades_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    return 1;
}

int spades_undo(sgame *g)
{
    if (!g->n) return 0;
    static uint16_t acts[SPADES_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    spades_new(g, g->seed);
    for (int i = 0; i < n; i++) spades_play(g, acts[i]);
    return 1;
}

int spades_winner(const spos *p)
{
    if (p->phase != SP_OVER) return -1;
    return p->score[1] > p->score[0];
}

/* a plain bid: the hand's likely tricks (aces, kings, the spades over the 10), at least 1 */
static int plain_bid(const uint8_t *h, int n)
{
    int t = 0;
    for (int i = 0; i < n; i++) {
        int r = trick_rank(h[i]);
        t += r == 14 || r == 13 || (CARD_SUIT(h[i]) == SUIT_SPADES && r >= 11 && r < 13);
    }
    return t < 1 ? 1 : t;
}

int spades_auto(const spos *p)
{
    const uint8_t *h = p->hand[p->turn];
    switch (p->phase) {
    case SP_BID: return SA_BID | plain_bid(h, p->n[p->turn]);
    case SP_PLAY:
        for (int i = 0; i < p->n[p->turn]; i++)
            if (spades_can_play(p, h[i])) return SA_PLAY | h[i];
        return 0;
    case SP_ROUND: return SA_NEXT;
    default: return 0;
    }
}

int spades_script(sgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int act;
        if (*s == '#' || *s == '!') {          /* plain actions to the end of the round (!: of the game) */
            int all = *s++ == '!';
            while (g->pos.phase != SP_OVER && (all || g->pos.phase == SP_BID || g->pos.phase == SP_PLAY))
                if (!spades_play(g, spades_auto(&g->pos))) return 0;
            continue;
        }
        if (*s == '.') { s++; act = spades_auto(&g->pos); }
        else if (*s == 'n') { s++; act = SA_NEXT; }
        else if (*s == 'b') {
            s++;
            int v = 0, digits = 0;
            while (*s >= '0' && *s <= '9' && digits < 3) v = v * 10 + (*s++ - '0'), digits++;
            if (!digits) return 0;
            act = SA_BID | (v > 255 ? 255 : v);
        } else {
            int c = trick_parse(&s);
            if (c == CARD_NONE) return 0;
            act = SA_PLAY | c;
        }
        if (!spades_play(g, act)) return 0;
    }
    return 1;
}
