/*
 * Old Maid (le pouilleux): the rules (oldmaid.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/oldmaid.h"

static int black(int card) { return CARD_SUIT(card) == SUIT_SPADES || CARD_SUIT(card) == SUIT_CLUBS; }

int om_pairs(int a, int b) { return a != b && CARD_RANK(a) == CARD_RANK(b) && black(a) == black(b); }

static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

static int active(const opos *p) { int k = 0; for (int q = 0; q < p->players; q++) k += p->n[q] > 0; return k; }

/* the next (step 1) or previous (step -1) player still holding cards */
static int seat(const opos *p, int from, int step)
{
    for (int i = 1; i <= p->players; i++) {
        int q = ((from + step * i) % p->players + p->players) % p->players;
        if (p->n[q]) return q;
    }
    return -1;
}

int om_offerer(const opos *p) { return p->phase == OP_DRAW ? seat(p, p->turn, -1) : -1; }

/* the hand offered is shuffled first: nobody knows where its cards are */
static void offer(opos *p)
{
    int o = om_offerer(p);
    if (o >= 0) cards_shuffle(p->hand[o], p->n[o], mix(p->seed, 77u + p->draws));
}

/* the card goes into the hand, or down with its partner */
static void receive(opos *p, int pl, int card)
{
    for (int i = 0; i < p->n[pl]; i++)
        if (om_pairs(p->hand[pl][i], card)) {
            p->pile[p->npile++] = p->hand[pl][i];
            p->pile[p->npile++] = (uint8_t)card;
            memmove(p->hand[pl] + i, p->hand[pl] + i + 1, (size_t)(p->n[pl] - i - 1));
            p->n[pl]--;
            return;
        }
    p->hand[pl][p->n[pl]++] = (uint8_t)card;
}

/* the game's end: one player holding cards (the jack of spades) */
static void settle(opos *p)
{
    if (active(p) > 1) return;
    p->phase = OP_OVER;
    p->loser = OM_NONE;
    for (int q = 0; q < p->players; q++)
        if (p->n[q]) p->loser = (uint8_t)q;
}

void om_new(ogame *g, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    opos *p = &g->pos;
    g->seed = p->seed = seed;
    g->players = p->players = (uint8_t)(players < 2 ? 2 : players > OM_MAXP ? OM_MAXP : players);
    uint8_t pack[CARDS_MAX];
    int n = cards_pack(PACK_52, pack), m = 0;
    for (int i = 0; i < n; i++)
        if (pack[i] != CARD(SUIT_CLUBS, RANK_JACK)) pack[m++] = pack[i];
    cards_shuffle(pack, m, mix(seed, 1000u));
    for (int i = 0; i < m; i++) receive(p, i % p->players, pack[i]);   /* the pairs go down as they come */
    p->turn = 0;
    p->phase = OP_DRAW;
    p->loser = OM_NONE;
    if (!p->n[0]) p->turn = (uint8_t)seat(p, 0, 1);
    settle(p);
    offer(p);
}

int om_play(ogame *g, int index)
{
    opos *p = &g->pos;
    int o = om_offerer(p);
    if (o < 0 || index < 0 || index >= p->n[o] || g->n >= OM_MAX_ACTS) return 0;
    int card = p->hand[o][index];
    memmove(p->hand[o] + index, p->hand[o] + index + 1, (size_t)(p->n[o] - index - 1));
    p->n[o]--;
    receive(p, p->turn, card);
    p->draws++;
    g->act[g->n++] = (uint8_t)index;
    settle(p);
    if (p->phase == OP_OVER) return 1;
    int next = seat(p, p->turn, 1);
    p->turn = (uint8_t)next;
    offer(p);
    return 1;
}

int om_undo(ogame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    uint8_t acts[OM_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n);
    om_new(g, g->players, g->seed);
    for (int i = 0; i < n; i++) om_play(g, acts[i]);
    return 1;
}

unsigned om_winners(const opos *p)
{
    if (p->phase != OP_OVER) return 0;
    unsigned m = (1u << p->players) - 1;
    return p->loser == OM_NONE ? m : m & ~(1u << p->loser);
}

int om_script(ogame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        char c = *s++;
        int arg = 0;
        while (*s >= '0' && *s <= '9') arg = arg * 10 + (*s++ - '0');
        if (c == '#') {
            while (g->pos.phase == OP_DRAW)
                if (!om_play(g, 0)) return 0;
            continue;
        }
        if (c != 'd' || !om_play(g, arg)) return 0;
    }
    return 1;
}
