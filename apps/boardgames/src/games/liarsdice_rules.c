/*
 * Liar's Dice: the rules (liarsdice.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "cards.h"
#include "games/liarsdice.h"

static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

/* the round's throw: each player's dice from the seed, the round and the player */
static void roll(lpos *p)
{
    for (int q = 0; q < p->players; q++) {
        uint32_t s = mix(p->seed, 1000u * p->round + 17u * q + 1);
        for (int i = 0; i < LD_DICE; i++) p->dice[q][i] = (uint8_t)(i < p->ndice[q] ? 1 + cards_rand(&s) % 6 : 0);
    }
}

void ld_new(lgame *g, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    lpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->players = p->players = (uint8_t)(players < 2 ? 2 : players > LD_MAXP ? LD_MAXP : players);
    for (int q = 0; q < p->players; q++) p->ndice[q] = LD_DICE;
    p->phase = LP_BID;
    p->winner = 255;
    roll(p);
}

int ld_total(const lpos *p)
{
    int n = 0;
    for (int q = 0; q < p->players; q++) n += p->ndice[q];
    return n;
}

int ld_counts(const lpos *p, int face)
{
    int n = 0;
    for (int q = 0; q < p->players; q++)
        for (int i = 0; i < p->ndice[q]; i++) n += p->dice[q][i] == face || p->dice[q][i] == 1;
    return n;
}

int ld_raises(const lpos *p, int count, int face)
{
    if (face < 2 || face > 6 || count < 1 || count > ld_total(p)) return 0;
    return !p->count || count > p->count || (count == p->count && face > p->face);
}

int ld_alive(const lpos *p)
{
    int n = 0;
    for (int q = 0; q < p->players; q++) n += p->ndice[q] > 0;
    return n;
}

int ld_next(const lpos *p, int pl)
{
    for (int k = 1; k <= p->players; k++) {
        int q = (pl + k) % p->players;
        if (p->ndice[q]) return q;
    }
    return pl;
}

int ld_legal(const lpos *p, int a)
{
    switch (LD_KIND(a)) {
    case LA_BID: return p->phase == LP_BID && ld_raises(p, LD_COUNT(a), LD_FACE(a));
    case LA_LIAR: return p->phase == LP_BID && p->count > 0;
    case LA_NEXT: return p->phase == LP_REVEAL;
    default: return 0;
    }
}

static void apply(lpos *p, int a)
{
    switch (LD_KIND(a)) {
    case LA_BID:
        p->count = (uint8_t)LD_COUNT(a);
        p->face = (uint8_t)LD_FACE(a);
        p->bidder = p->turn;
        if (p->nbids < LD_BIDS) {
            p->bids[p->nbids].player = p->turn;
            p->bids[p->nbids].count = p->count;
            p->bids[p->nbids].face = p->face;
            p->nbids++;
        }
        p->turn = (uint8_t)ld_next(p, p->turn);
        break;
    case LA_LIAR:                               /* every cup lifted: the bid holds, or the bidder lied */
        p->caller = p->turn;
        p->found = (uint8_t)ld_counts(p, p->face);
        p->held = p->found >= p->count;
        p->loser = p->held ? p->caller : p->bidder;
        p->ndice[p->loser]--;
        p->phase = LP_REVEAL;
        if (ld_alive(p) == 1) {
            p->winner = (uint8_t)ld_next(p, p->loser);
            p->phase = LP_OVER;
        }
        break;
    case LA_NEXT:                               /* a new throw; the loser starts (if out, the next one still in) */
        p->round++;
        p->starter = (uint8_t)(p->ndice[p->loser] ? p->loser : ld_next(p, p->loser));
        p->turn = p->starter;
        p->count = p->face = 0;
        p->nbids = 0;
        p->phase = LP_BID;
        roll(p);
        break;
    }
}

int ld_play(lgame *g, int a)
{
    if (g->n >= LD_MAX_ACTS || !ld_legal(&g->pos, a)) return 0;
    apply(&g->pos, a);
    g->act[g->n++] = (uint16_t)a;
    return 1;
}

int ld_undo(lgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    static uint16_t acts[LD_MAX_ACTS];
    memcpy(acts, g->act, sizeof acts[0] * (size_t)n);
    ld_new(g, g->players, g->seed);
    for (int i = 0; i < n; i++) ld_play(g, acts[i]);
    return 1;
}

/* a plain player: the dice it has of each face (the ones counting for all), the others' dice a third each likely */
int ld_auto(const lpos *p)
{
    if (p->phase == LP_REVEAL) return LD_ACT(LA_NEXT, 0, 0);
    if (p->phase != LP_BID) return 0;
    int me = p->turn, mine[7] = {0}, others = ld_total(p) - p->ndice[me];
    for (int i = 0; i < p->ndice[me]; i++) mine[p->dice[me][i]]++;
    if (p->count) {
        float likely = mine[p->face] + mine[1] + others / 3.0f;
        if (p->count > likely + 1.0f) return LD_ACT(LA_LIAR, 0, 0);
    }
    int best = 2;
    for (int f = 3; f <= 6; f++)
        if (mine[f] >= mine[best]) best = f;
    int count = !p->count ? 1 + (mine[best] + mine[1]) / 2 : best > p->face ? p->count : p->count + 1;
    if (count > ld_total(p)) return p->count ? LD_ACT(LA_LIAR, 0, 0) : 0;
    return LD_ACT(LA_BID, count, best);
}

int ld_script(lgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        int a;
        if (*s == '#') { a = ld_auto(&g->pos); s++; if (g->pos.phase == LP_OVER) continue; }
        else if (*s == 'l') { a = LD_ACT(LA_LIAR, 0, 0); s++; }
        else if (*s == 'n') { a = LD_ACT(LA_NEXT, 0, 0); s++; }
        else if (*s >= '0' && *s <= '9') {
            int c = 0;
            while (*s >= '0' && *s <= '9') c = c * 10 + (*s++ - '0');
            if (*s++ != 'x' || *s < '1' || *s > '6' || c > 255) return 0;
            a = LD_ACT(LA_BID, c, *s++ - '0');
        } else return 0;
        if (!ld_play(g, a)) return 0;
    }
    return 1;
}
