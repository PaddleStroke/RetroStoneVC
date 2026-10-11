/*
 * War (la bataille): the rules (war.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/war.h"

int war_value(int card) { return CARD_RANK(card) == RANK_ACE ? 14 : CARD_RANK(card); }

static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

/* the players holding cards (bit q) */
static unsigned alive(const wpos *p)
{
    unsigned m = 0;
    for (int q = 0; q < p->players; q++) m |= (unsigned)(p->n[q] > 0) << q;
    return m;
}

void war_new(wgame *g, int variant, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    wpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->variant = p->variant = (uint8_t)(variant == WAR_SHORT ? WAR_SHORT : WAR_FULL);
    g->players = p->players = (uint8_t)(players < 2 ? 2 : players > WAR_MAXP ? WAR_MAXP : players);
    uint8_t pack[CARDS_MAX];
    int n = cards_pack(PACK_52, pack);
    cards_shuffle(pack, n, mix(seed, 1000u));
    for (int i = 0; i < n; i++) {
        int q = i % p->players;
        p->pile[q][p->n[q]++] = pack[i];
    }
    p->in = (uint8_t)alive(p);
    p->winner = WAR_NONE;
}

int war_cards(const wpos *p, int q) { return p->n[q] + p->nt[q]; }

int war_legal(const wpos *p, int q)
{
    return p->phase == WP_TURN && q >= 0 && q < p->players && (p->in >> q & 1) && !(p->done >> q & 1) && p->n[q];
}

int war_turn(const wpos *p)
{
    for (int q = 0; q < p->players; q++)
        if (war_legal(p, q)) return q;
    return -1;
}

/* the top of a pile onto the table */
static void turn_card(wpos *p, int q, int up)
{
    p->table[q][p->nt[q]] = p->pile[q][--p->n[q]];
    if (up) p->up[q] |= 1ull << p->nt[q];
    p->nt[q]++;
}

static int top(const wpos *p, int q) { return war_value(p->table[q][p->nt[q] - 1]); }

/* everything on the table goes under the winner's pile, in no set order */
static void collect(wpos *p, int w)
{
    uint8_t pot[52];
    int k = 0;
    for (int q = 0; q < p->players; q++) {
        for (int i = 0; i < p->nt[q]; i++) pot[k++] = p->table[q][i];
        p->nt[q] = 0;
        p->up[q] = 0;
    }
    cards_shuffle(pot, k, mix(p->seed, 31u * p->battles + 7u));
    memmove(p->pile[w] + k, p->pile[w], p->n[w]);
    memcpy(p->pile[w], pot, (size_t)k);
    p->n[w] = (uint8_t)(p->n[w] + k);
    p->winner = (uint8_t)w;
    p->battles++;
    p->war = 0;
    p->done = 0;
    p->in = (uint8_t)alive(p);
    int left = 0;
    for (int q = 0; q < p->players; q++) left += p->in >> q & 1;
    if (left <= 1 || (p->variant == WAR_SHORT && p->battles >= WAR_SHORT_BATTLES)) p->phase = WP_OVER;
}

/* everyone in has turned: the highest takes all, or a war among the tied */
static void resolve(wpos *p)
{
    int best = 0;
    unsigned tied = 0;
    for (int q = 0; q < p->players; q++) {
        if (!(p->in >> q & 1)) continue;
        int v = top(p, q);
        if (v > best) best = v, tied = 0;
        if (v == best) tied |= 1u << q;
    }
    int first = -1, count = 0;
    unsigned go = 0;
    for (int q = 0; q < p->players; q++) {
        if (!(tied >> q & 1)) continue;
        if (first < 0) first = q;
        if (p->n[q]) go |= 1u << q, count++;
    }
    if (count == 1 || (tied & (tied - 1)) == 0 || count == 0) {   /* one winner (alone, or the last one able) */
        int w = first;
        if (count == 1)
            for (int q = 0; q < p->players; q++)
                if (go >> q & 1) w = q;
        collect(p, w);
        return;
    }
    p->war = 1;
    p->in = (uint8_t)go;
    p->done = 0;
}

int war_play(wgame *g, int q)
{
    wpos *p = &g->pos;
    if (g->n >= WAR_MAX_ACTS || !war_legal(p, q)) return 0;
    if (p->war && p->n[q] >= 2) turn_card(p, q, 0);
    turn_card(p, q, 1);
    p->done |= (uint8_t)(1u << q);
    g->act[g->n++] = (uint8_t)q;
    if (p->done == p->in) resolve(p);
    return 1;
}

int war_undo(wgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    static uint8_t acts[WAR_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n);
    war_new(g, g->variant, g->players, g->seed);
    for (int i = 0; i < n; i++) war_play(g, acts[i]);
    return 1;
}

unsigned war_winners(const wpos *p)
{
    if (p->phase != WP_OVER) return 0;
    int most = 0;
    unsigned m = 0;
    for (int q = 0; q < p->players; q++) most = war_cards(p, q) > most ? war_cards(p, q) : most;
    for (int q = 0; q < p->players; q++) m |= (unsigned)(war_cards(p, q) == most) << q;
    return m;
}

int war_script(wgame *g, const char *s)
{
    while (*s) {
        char c = *s++;
        if (c == ' ') continue;
        if (c >= '0' && c <= '9') {
            if (!war_play(g, c - '0')) return 0;
        } else if (c == 'b' || c == '#') {      /* to the end of the battle (of the game) */
            uint16_t b = g->pos.battles;
            while (g->pos.phase == WP_TURN && (c == '#' || g->pos.battles == b))
                if (!war_play(g, war_turn(&g->pos))) return 0;
        } else {
            return 0;
        }
    }
    return 1;
}
