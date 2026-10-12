/*
 * Bataille corse: the rules (slap.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/slap.h"

static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

void slap_new(slgame *g, int variant, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    slpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->variant = p->variant = (uint8_t)(variant == SLAP_SANDWICH ? SLAP_SANDWICH : SLAP_DOUBLE);
    g->players = p->players = (uint8_t)(players < 2 ? 2 : players > SLAP_MAXP ? SLAP_MAXP : players);
    uint8_t pack[CARDS_MAX];
    int n = cards_pack(PACK_52, pack);
    cards_shuffle(pack, n, mix(seed, 31u));
    for (int i = 0; i < n; i++) {
        int q = i % p->players;
        p->pile[q][p->n[q]++] = pack[i];
    }
    p->claimer = p->winner = p->lastp = SLAP_NONE;
}

int slap_tries(int c)
{
    int r = CARD_RANK(c);
    return r == RANK_ACE ? 4 : r == RANK_KING ? 3 : r == RANK_QUEEN ? 2 : r == RANK_JACK ? 1 : 0;
}

int slap_right(const slpos *p)
{
    int n = p->nc, up = p->nc - p->buried;    /* the cards face up */
    if (up >= 2 && CARD_RANK(p->centre[n - 1]) == CARD_RANK(p->centre[n - 2])) return 1;
    return p->variant == SLAP_SANDWICH && up >= 3 && CARD_RANK(p->centre[n - 1]) == CARD_RANK(p->centre[n - 3]);
}

static int alive(const slpos *p, int q) { return !(p->out >> q & 1); }

/* the next player after `from` with cards to turn (not `skip`); -1: none */
static int next_with_cards(const slpos *p, int from, int skip)
{
    for (int i = 1; i <= p->players; i++) {
        int q = (from + i) % p->players;
        if (q != skip && alive(p, q) && p->n[q] > 0) return q;
    }
    return -1;
}

int slap_legal(const slpos *p, int a)
{
    int k = SLA_KIND(a), q = SLA_ARG(a);
    switch (k) {
    case SLA_PLAY: return a == SLA_PLAY && p->phase == SLP_PLAY && p->n[p->turn] > 0;
    case SLA_SLAP: return p->phase != SLP_OVER && q < p->players && alive(p, q) && p->nc > 0;
    case SLA_CLAIM: return a == SLA_CLAIM && p->phase == SLP_CLAIM;
    }
    return 0;
}

static void check_over(slpos *p)
{
    int holders = 0, last = -1, left = 0, one = -1;
    for (int q = 0; q < p->players; q++) {
        if (p->n[q] > 0) holders++, last = q;
        if (alive(p, q)) left++, one = q;
    }
    if (left == 1) { p->phase = SLP_OVER; p->winner = (uint8_t)one; return; }
    if (!p->nc && holders == 1) { p->phase = SLP_OVER; p->winner = (uint8_t)last; }   /* all the cards */
}

/* the centre pile turned face down under q's pile; q plays next */
static void gather(slpos *p, int q)
{
    uint8_t old[52];
    int n = p->n[q];
    memcpy(old, p->pile[q], (size_t)n);
    for (int i = 0; i < p->nc; i++) p->pile[q][i] = p->centre[p->nc - 1 - i];
    memcpy(p->pile[q] + p->nc, old, (size_t)n);
    p->n[q] = (uint8_t)(n + p->nc);
    p->nc = p->buried = 0;
    p->owed = 0;
    p->claimer = SLAP_NONE;
    p->turn = (uint8_t)q;
    p->phase = SLP_PLAY;
    check_over(p);
}

/* after anything: the player to move must have a card to turn */
static void settle(slpos *p)
{
    if (p->phase != SLP_PLAY || p->n[p->turn] > 0) return;
    if (p->owed && p->claimer != SLAP_NONE) { p->phase = SLP_CLAIM; p->turn = p->claimer; return; }
    int q = next_with_cards(p, p->turn, -1);
    if (q >= 0) { p->turn = (uint8_t)q; return; }
    if (p->nc) {                                /* every card in the middle: the last to turn one takes them */
        p->claimer = p->turn = p->lastp != SLAP_NONE ? p->lastp : p->turn;
        p->phase = SLP_CLAIM;
    }
}

static void apply(slpos *p, int a)
{
    int k = SLA_KIND(a), q = SLA_ARG(a);
    switch (k) {
    case SLA_PLAY: {
        int me = p->turn, c = p->pile[me][--p->n[me]];
        p->centre[p->nc++] = (uint8_t)c;
        p->lastp = (uint8_t)me;
        p->event = SLE_PLAY, p->who = (uint8_t)me;
        int t = slap_tries(c);
        if (t) {                                /* tribute from the next player */
            p->owed = (uint8_t)t;
            p->claimer = (uint8_t)me;
            int nx = next_with_cards(p, me, me);
            if (nx < 0) { p->phase = SLP_CLAIM; p->turn = (uint8_t)me; }
            else p->turn = (uint8_t)nx;
        } else if (p->owed) {                   /* paying tribute */
            if (--p->owed == 0) { p->phase = SLP_CLAIM; p->turn = p->claimer; }
        } else {
            int nx = next_with_cards(p, me, -1);
            if (nx >= 0) p->turn = (uint8_t)nx;
        }
        break;
    }
    case SLA_SLAP:
        if (slap_right(p)) {
            p->event = SLE_TAKE, p->who = (uint8_t)q, p->how = 1;
            gather(p, q);
            return;
        }
        p->who = (uint8_t)q;
        if (p->n[q] > 0) {                      /* the penalty: q's top card under the centre pile */
            p->event = SLE_PENALTY;
            memmove(p->centre + 1, p->centre, p->nc);
            p->centre[0] = p->pile[q][--p->n[q]];
            p->nc++;
            p->buried++;
        } else {
            p->event = SLE_OUT;
            p->out |= (uint8_t)(1u << q);
            check_over(p);
            if (p->phase == SLP_OVER) return;
            if (p->turn == q) p->turn = (uint8_t)((q + 1) % p->players);
        }
        break;
    case SLA_CLAIM:
        p->event = SLE_TAKE, p->who = p->turn, p->how = 2;
        gather(p, p->turn);
        return;
    }
    settle(p);
}

int slap_play(slgame *g, int a)
{
    if (g->n >= SLAP_MAX_ACTS || !slap_legal(&g->pos, a)) return 0;
    apply(&g->pos, a);
    g->act[g->n++] = (uint16_t)a;
    return 1;
}

int slap_undo(slgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    static uint16_t acts[SLAP_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    slap_new(g, g->variant, g->players, g->seed);
    for (int i = 0; i < n; i++) slap_play(g, acts[i]);
    return 1;
}

int slap_auto(const slpos *p)
{
    if (p->phase == SLP_OVER) return 0;
    if (slap_right(p))                          /* the next player not out slaps */
        for (int i = 1; i <= p->players; i++) {
            int q = (p->turn + i) % p->players;
            if (alive(p, q)) return SLA_SLAP | q;
        }
    if (p->phase == SLP_CLAIM) return SLA_CLAIM;
    return SLA_PLAY;
}

int slap_script(slgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int a;
        switch (*s++) {
        case 'p': a = SLA_PLAY; break;
        case 'c': a = SLA_CLAIM; break;
        case 's':
            if (*s < '0' || *s > '9') return 0;
            a = SLA_SLAP | (*s++ - '0');
            break;
        case '.': a = slap_auto(&g->pos); break;
        case '#':
            for (int k = 0; k < 20000 && g->pos.phase != SLP_OVER; k++)
                if (!slap_play(g, slap_auto(&g->pos))) return 0;
            continue;
        default: return 0;
        }
        if (!slap_play(g, a)) return 0;
    }
    return 1;
}
