/*
 * Happy Families (le jeu des 7 familles): the rules (families.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/families.h"

static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

int fam_holds(const fpos *p, int pl, int card)
{
    for (int i = 0; i < p->n[pl]; i++)
        if (p->hand[pl][i] == card) return 1;
    return 0;
}

int fam_has_family(const fpos *p, int pl, int family)
{
    for (int i = 0; i < p->n[pl]; i++)
        if (FAM_FAMILY(p->hand[pl][i]) == family) return 1;
    return 0;
}

static int key(int card) { return FAM_FAMILY(card) * 8 + FAM_MEMBER(card); }

static void insert(fpos *p, int pl, int card)
{
    int i = p->n[pl];
    while (i > 0 && key(p->hand[pl][i - 1]) > key(card)) {
        p->hand[pl][i] = p->hand[pl][i - 1];
        i--;
    }
    p->hand[pl][i] = (uint8_t)card;
    p->n[pl]++;
}

static void take(fpos *p, int pl, int card)
{
    int k = 0;
    for (int i = 0; i < p->n[pl]; i++)
        if (p->hand[pl][i] != card) p->hand[pl][k++] = p->hand[pl][i];
    p->n[pl] = (uint8_t)k;
}

/* a family complete in the hand goes down */
static void lay(fpos *p, int pl)
{
    for (int f = 0; f < FAM_FAMILIES; f++) {
        int k = 0;
        for (int i = 0; i < p->n[pl]; i++) k += FAM_FAMILY(p->hand[pl][i]) == f;
        if (k < FAM_MEMBERS) continue;
        for (int m = 0; m < FAM_MEMBERS; m++) take(p, pl, FAM_CARD(f, m));
        p->owner[f] = (uint8_t)pl;
        p->laid[pl][p->nlaid[pl]++] = (uint8_t)f;
    }
}

static int done(const fpos *p)
{
    for (int f = 0; f < FAM_FAMILIES; f++)
        if (p->owner[f] == FAM_NONE) return 0;
    return 1;
}

/* the turn goes to pl, or on: an empty hand draws a card (the stock empty: the turn passes on) */
static void turn_to(fpos *p, int pl)
{
    if (done(p)) { p->phase = FP_OVER; return; }
    for (int i = 0; i < p->players; i++, pl = (pl + 1) % p->players) {
        if (!p->n[pl] && p->nstock) {
            insert(p, pl, p->stock[--p->nstock]);
            p->refills++;
        }
        if (p->n[pl]) { p->turn = (uint8_t)pl; return; }
    }
    p->phase = FP_OVER;                         /* (nobody holds a card: every family is down) */
}

void fam_new(fgame *g, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    fpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->players = p->players = (uint8_t)(players < 2 ? 2 : players > FAM_MAXP ? FAM_MAXP : players);
    int n = 0;
    for (int f = 0; f < FAM_FAMILIES; f++)
        for (int m = 0; m < FAM_MEMBERS; m++) p->stock[n++] = FAM_CARD(f, m);
    cards_shuffle(p->stock, n, mix(seed, 1000u));
    p->nstock = (int16_t)n;
    memset(p->owner, FAM_NONE, sizeof p->owner);
    int each = p->players <= 3 ? 7 : p->players == 4 ? 6 : 5;
    for (int k = 0; k < each; k++)
        for (int q = 0; q < p->players; q++) insert(p, q, p->stock[--p->nstock]);
    for (int q = 0; q < p->players; q++) lay(p, q);
    p->ask_from = p->drawn = FAM_NONE;
    p->phase = FP_ASK;
    turn_to(p, 0);
}

/* a player who may be asked: one holding cards (or anyone, when nobody else holds any: the stock answers) */
int fam_askable(const fpos *p, int to)
{
    if (to < 0 || to >= p->players || to == p->turn) return 0;
    if (p->n[to]) return 1;
    for (int q = 0; q < p->players; q++)
        if (q != p->turn && p->n[q]) return 0;
    return 1;
}

int fam_legal(const fpos *p, int a)
{
    int to = FAM_ASK_PLAYER(a), card = FAM_ASK_CARD(a), pl = p->turn;
    return p->phase == FP_ASK && fam_askable(p, to) && FAM_FAMILY(card) < FAM_FAMILIES &&
           FAM_MEMBER(card) < FAM_MEMBERS && fam_has_family(p, pl, FAM_FAMILY(card)) && !fam_holds(p, pl, card);
}

int fam_play(fgame *g, int a)
{
    fpos *p = &g->pos;
    if (g->n >= FAM_MAX_ACTS || !fam_legal(p, a)) return 0;
    int to = FAM_ASK_PLAYER(a), card = FAM_ASK_CARD(a), pl = p->turn, again = 0;
    p->ask_from = (uint8_t)pl;
    p->ask_to = (uint8_t)to;
    p->ask_card = (uint8_t)card;
    p->drawn = FAM_NONE;
    if (fam_holds(p, to, card)) {               /* given */
        take(p, to, card);
        insert(p, pl, card);
        p->answer = FAM_GIVEN;
        again = 1;
    } else {                                    /* pioche */
        p->answer = FAM_MISSED;
        if (p->nstock) {
            int c = p->stock[--p->nstock];
            insert(p, pl, c);
            p->drawn = (uint8_t)c;
            if (c == card) p->answer = FAM_LUCKY, again = 1;
        }
    }
    lay(p, pl);
    g->act[g->n++] = (uint16_t)a;
    turn_to(p, again ? pl : (pl + 1) % p->players);
    return 1;
}

int fam_undo(fgame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    uint16_t acts[FAM_MAX_ACTS];
    memcpy(acts, g->act, sizeof acts[0] * (size_t)n);
    fam_new(g, g->players, g->seed);
    for (int i = 0; i < n; i++) fam_play(g, acts[i]);
    return 1;
}

unsigned fam_winners(const fpos *p)
{
    if (p->phase != FP_OVER) return 0;
    int most = 0;
    unsigned m = 0;
    for (int q = 0; q < p->players; q++) most = p->nlaid[q] > most ? p->nlaid[q] : most;
    for (int q = 0; q < p->players; q++) m |= (unsigned)(p->nlaid[q] == most) << q;
    return m;
}


/* a plain question, for the family held most: a card someone holds when there is one, else the first card missing,
 * asked of the next player who may be asked */
int fam_auto(const fpos *p)
{
    int pl = p->turn, count[FAM_FAMILIES] = {0}, best = -1, fallback = 0;
    for (int i = 0; i < p->n[pl]; i++) count[FAM_FAMILY(p->hand[pl][i])]++;
    for (int f = 0; f < FAM_FAMILIES; f++)
        if (count[f] && (best < 0 || count[f] > count[best])) best = f;
    for (int r = 0; r < FAM_FAMILIES && best >= 0; r++) {
        int f = (best + r) % FAM_FAMILIES;
        if (!count[f]) continue;
        for (int m = 0; m < FAM_MEMBERS; m++) {
            int c = FAM_CARD(f, m);
            if (fam_holds(p, pl, c)) continue;
            for (int k = 1; k < p->players; k++) {
                int q = (pl + k) % p->players;
                if (fam_holds(p, q, c)) return FAM_ASK(q, c);
                if (!fallback && fam_legal(p, FAM_ASK(q, c))) fallback = FAM_ASK(q, c);
            }
        }
    }
    return fallback;
}

int fam_script(fgame *g, const char *s)
{
    while (*s) {
        char c = *s++;
        if (c == ' ') continue;
        if (c == '#') {                         /* to the end */
            while (g->pos.phase == FP_ASK)
                if (!fam_play(g, fam_auto(&g->pos))) return 0;
            continue;
        }
        if (c == 'x') {
            if (!fam_play(g, fam_auto(&g->pos))) return 0;
            continue;
        }
        if (c != 'a' || s[0] < '0' || s[0] > '5' || s[1] < '0' || s[1] > '6' || s[2] < '0' || s[2] > '5') return 0;
        int a = FAM_ASK(s[0] - '0', FAM_CARD(s[1] - '0', s[2] - '0'));
        s += 3;
        if (!fam_play(g, a)) return 0;
    }
    return 1;
}
