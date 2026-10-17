/*
 * Hearts: the rules (hearts.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <stdio.h>
#include <string.h>

#include "games/hearts.h"
#include "games/tricks.h"

int hearts_points(int card)
{
    return CARD_SUIT(card) == SUIT_HEARTS ? 1 : card == HEARTS_QUEEN ? 13 : 0;
}

/* the cards taken out for three, five or six players (low diamonds and spades: every point and the 2 of clubs stay) */
static int removed(int players, int card)
{
    int s = CARD_SUIT(card), r = CARD_RANK(card);
    if (players == 3) return card == CARD(SUIT_DIAMONDS, 2);
    if (players == 5) return r == 2 && (s == SUIT_DIAMONDS || s == SUIT_SPADES);
    if (players == 6) return (r == 2 || r == 3) && (s == SUIT_DIAMONDS || s == SUIT_SPADES);
    return 0;
}

int hearts_target(const hpos *p, int pl)
{
    int n = p->players;
    switch (p->dir) {
    case PASS_LEFT: return (pl + 1) % n;
    case PASS_RIGHT: return (pl + n - 1) % n;
    case PASS_ACROSS: return (pl + n / 2) % n;
    default: return pl;
    }
}

static int holder(const hpos *p, int card)
{
    for (int pl = 0; pl < p->players; pl++)
        if (trick_find(p->hand[pl], p->n[pl], card) >= 0) return pl;
    return 0;
}

static void start_play(hpos *p)
{
    p->phase = HP_PLAY;
    p->turn = p->leader = (uint8_t)holder(p, HEARTS_LEAD);
}

static void deal(hpos *p)
{
    uint8_t pack[CARDS_MAX], deck[CARDS_MAX];
    int n = cards_pack(PACK_52, pack), k = 0;
    for (int i = 0; i < n; i++)
        if (!removed(p->players, pack[i])) deck[k++] = pack[i];
    cards_shuffle(deck, k, trick_mix(p->seed, 1000u + p->round));
    p->size = (uint8_t)(k / p->players);
    p->first = (uint8_t)(p->round % p->players);
    for (int pl = 0; pl < p->players; pl++) p->n[pl] = 0, p->taken[pl] = 0;
    for (int i = 0; i < k; i++) {
        int pl = (p->first + i) % p->players;
        p->hand[pl][p->n[pl]++] = deck[i];
    }
    for (int pl = 0; pl < p->players; pl++) trick_sort(p->hand[pl], p->n[pl]);
    memset(p->got, CARD_NONE, sizeof p->got);
    memset(p->pass, CARD_NONE, sizeof p->pass);
    memset(p->last, CARD_NONE, sizeof p->last);
    p->passed = p->ntrick = p->tricks = p->broken = 0;
    p->last_winner = p->last_leader = TRICK_NONE;
    int cycle = p->players == 4 || p->players == 6 ? 4 : 3;
    p->dir = (uint8_t)(p->round % cycle);
    if (cycle == 3 && p->dir == 2) p->dir = PASS_HOLD;   /* three or five: left, right, no pass */
    if (p->dir == PASS_HOLD) start_play(p);
    else p->phase = HP_PASS, p->turn = p->first;
}

void hearts_new(hgame *g, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    hpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->players = p->players = (uint8_t)(players < 3 ? 3 : players > HEARTS_MAXP ? HEARTS_MAXP : players);
    p->moon = TRICK_NONE;
    deal(p);
}

/* a point card on the first trick: only with nothing else in hand */
static int all_points(const uint8_t *hand, int n)
{
    for (int i = 0; i < n; i++)
        if (!hearts_points(hand[i])) return 0;
    return 1;
}

int hearts_can_play(const hpos *p, int card)
{
    if (p->phase != HP_PLAY) return 0;
    const uint8_t *h = p->hand[p->turn];
    int n = p->n[p->turn];
    if (trick_find(h, n, card) < 0) return 0;
    if (p->ntrick == 0) {                       /* leading */
        if (p->tricks == 0) return card == HEARTS_LEAD;
        if (CARD_SUIT(card) == SUIT_HEARTS && !p->broken) return trick_only_suit(h, n, SUIT_HEARTS);
        return 1;
    }
    int led = CARD_SUIT(p->trick[0]);
    if (!trick_follows(h, n, led, card)) return 0;
    if (p->tricks == 0 && CARD_SUIT(card) != led && hearts_points(card)) return all_points(h, n);
    return 1;
}

int hearts_legal(const hpos *p, uint32_t act)
{
    switch (HA_KIND(act)) {
    case HA_PASS: {
        if (p->phase != HP_PASS) return 0;
        const uint8_t *h = p->hand[p->turn];
        int n = p->n[p->turn];
        for (int i = 0; i < 3; i++) {
            int c = HA_CARD(act, i);
            if (trick_find(h, n, c) < 0) return 0;
            for (int j = 0; j < i; j++)
                if (HA_CARD(act, j) == c) return 0;
        }
        return 1;
    }
    case HA_PLAY: return (act & 0xffff00u) == 0 && hearts_can_play(p, HA_CARD(act, 2));
    case HA_NEXT: return p->phase == HP_ROUND && act == HA_NEXT;
    default: return 0;
    }
}

static void round_end(hpos *p)
{
    p->moon = TRICK_NONE;
    for (int pl = 0; pl < p->players; pl++)
        if (p->taken[pl] == 26) p->moon = (uint8_t)pl;
    int top = -1000;
    for (int pl = 0; pl < p->players; pl++) {
        int s = p->moon == TRICK_NONE ? p->taken[pl] : pl == p->moon ? 0 : 26;
        p->score[pl] = (int16_t)s;
        p->total[pl] = (int16_t)(p->total[pl] + s);
        if (p->total[pl] > top) top = p->total[pl];
    }
    p->round++;
    p->turn = (uint8_t)(p->round % p->players);   /* the next round's first player */
    p->phase = top >= HEARTS_LIMIT ? HP_OVER : HP_ROUND;
}

static void apply(hpos *p, uint32_t act)
{
    int pl = p->turn;
    switch (HA_KIND(act)) {
    case HA_PASS:
        for (int i = 0; i < 3; i++) {
            p->pass[pl][i] = (uint8_t)HA_CARD(act, i);
            trick_remove(p->hand[pl], &p->n[pl], HA_CARD(act, i));
        }
        p->turn = (uint8_t)((pl + 1) % p->players);
        if (++p->passed < p->players) break;
        for (int q = 0; q < p->players; q++) {  /* everyone has chosen: the cards change hands */
            int to = hearts_target(p, q);
            for (int i = 0; i < 3; i++) {
                p->got[to][i] = p->pass[q][i];
                p->hand[to][p->n[to]++] = p->pass[q][i];
            }
        }
        for (int q = 0; q < p->players; q++) trick_sort(p->hand[q], p->n[q]);
        start_play(p);
        break;
    case HA_PLAY: {
        int c = HA_CARD(act, 2);
        trick_remove(p->hand[pl], &p->n[pl], c);
        if (CARD_SUIT(c) == SUIT_HEARTS) p->broken = 1;
        p->trick[p->ntrick++] = (uint8_t)c;
        if (p->ntrick < p->players) { p->turn = (uint8_t)((pl + 1) % p->players); break; }
        int w = (p->leader + trick_winner(p->trick, p->ntrick, -1)) % p->players;
        for (int i = 0; i < p->ntrick; i++) {
            p->taken[w] = (uint8_t)(p->taken[w] + hearts_points(p->trick[i]));
            p->last[i] = p->trick[i];
        }
        p->last_leader = p->leader;
        p->last_winner = (uint8_t)w;
        p->ntrick = 0;
        p->tricks++;
        p->turn = p->leader = (uint8_t)w;
        if (p->n[w] == 0) round_end(p);
        break;
    }
    case HA_NEXT:
        deal(p);
        break;
    }
}

int hearts_play(hgame *g, uint32_t act)
{
    if (g->n >= HEARTS_MAX_ACTS || !hearts_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = act;
    return 1;
}

int hearts_undo(hgame *g)
{
    if (!g->n) return 0;
    static uint32_t acts[HEARTS_MAX_ACTS];
    int n = g->n - 1;
    memcpy(acts, g->act, (size_t)n * sizeof acts[0]);
    hearts_new(g, g->players, g->seed);
    for (int i = 0; i < n; i++) hearts_play(g, acts[i]);
    return 1;
}

int hearts_winner(const hpos *p)
{
    if (p->phase != HP_OVER) return -1;
    int best = 0, tie = 0;
    for (int pl = 1; pl < p->players; pl++) {
        if (p->total[pl] < p->total[best]) best = pl, tie = 0;
        else if (p->total[pl] == p->total[best]) tie = 1;
    }
    return tie ? -1 : best;
}

uint32_t hearts_auto(const hpos *p)
{
    const uint8_t *h = p->hand[p->turn];
    switch (p->phase) {
    case HP_PASS: return HA_PASS3(h[0], h[1], h[2]);
    case HP_PLAY:
        for (int i = 0; i < p->n[p->turn]; i++)
            if (hearts_can_play(p, h[i])) return HA_PLAY | h[i];
        return 0;
    case HP_ROUND: return HA_NEXT;
    default: return 0;
    }
}

int hearts_script(hgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        uint32_t act;
        if (*s == '#' || *s == '!') {          /* plain actions to the end of the round (!: of the game) */
            int all = *s++ == '!';
            while (g->pos.phase != HP_OVER && (all || g->pos.phase == HP_PASS || g->pos.phase == HP_PLAY))
                if (!hearts_play(g, hearts_auto(&g->pos))) return 0;
            continue;
        }
        if (*s == '.') { s++; act = hearts_auto(&g->pos); }
        else if (*s == 'n') { s++; act = HA_NEXT; }
        else if (*s == 'p') {
            s++;
            int c[3];
            for (int i = 0; i < 3; i++)
                if ((c[i] = trick_parse(&s)) == CARD_NONE) return 0;
            act = HA_PASS3(c[0], c[1], c[2]);
        } else {
            int c = trick_parse(&s);
            if (c == CARD_NONE) return 0;
            act = HA_PLAY | (uint32_t)c;
        }
        if (!hearts_play(g, act)) return 0;
    }
    return 1;
}

int hearts_act_text(uint32_t act, char *out, int cap)
{
    char c[3][3];
    if (!out || cap < 8) return 0;
    switch (HA_KIND(act)) {
    case HA_PASS:
        for (int i = 0; i < 3; i++) trick_name(HA_CARD(act, i), c[i]);
        snprintf(out, (size_t)cap, "p%s%s%s", c[0], c[1], c[2]);
        return 1;
    case HA_PLAY:
        trick_name(HA_CARD(act, 2), c[0]);
        snprintf(out, (size_t)cap, "%s", c[0]);
        return 1;
    case HA_NEXT: snprintf(out, (size_t)cap, "n"); return 1;
    default: return 0;
    }
}
