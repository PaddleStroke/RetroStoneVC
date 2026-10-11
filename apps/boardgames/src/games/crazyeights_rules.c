/*
 * Crazy Eights and Last Card: the rules (crazyeights.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/crazyeights.h"

static int last(const cpos *p) { return p->variant == CE_LAST; }
static int wild(int card) { return CARD_SUIT(card) == SUIT_JOKER; }
static int top(const cpos *p) { return p->pile[p->npile - 1]; }

int ce_goal(const cpos *p) { return last(p) ? CE_GOAL_LAST : CE_GOAL_EIGHTS; }

/* a card the starter cannot be: an eight; an action card or a wild */
static int special(const cpos *p, int card)
{
    if (!last(p)) return CARD_RANK(card) == 8;
    return wild(card) || CARD_RANK(card) >= ACTION_SKIP;
}

int ce_points(const cpos *p, int card)
{
    int r = CARD_RANK(card);
    if (last(p)) return wild(card) ? 50 : r >= ACTION_SKIP ? 20 : r;
    return r == 8 ? 50 : r >= RANK_JACK ? 10 : r;
}

int ce_hand_points(const cpos *p, int pl)
{
    int s = 0;
    for (int i = 0; i < p->n[pl]; i++) s += ce_points(p, p->hand[pl][i]);
    return s;
}

int ce_next(const cpos *p, int from, int steps)
{
    int n = p->players, d = p->dir ? -steps : steps;
    return ((from + d) % n + n) % n;
}

/* the hand's order: spades, hearts, clubs, diamonds (the colours alternate); the action deck's colours in order,
 * the wilds last; then the rank */
static int key(const cpos *p, int card)
{
    static const int french[4] = {0, 1, 3, 2};
    int s = CARD_SUIT(card);
    return (last(p) ? (s == SUIT_JOKER ? 4 : s) : french[s & 3]) * 32 + CARD_RANK(card);
}

static void insert(cpos *p, int pl, int card)
{
    int i = p->n[pl];
    while (i > 0 && key(p, p->hand[pl][i - 1]) > key(p, card)) {
        p->hand[pl][i] = p->hand[pl][i - 1];
        i--;
    }
    p->hand[pl][i] = (uint8_t)card;
    p->n[pl]++;
}

static int find(const cpos *p, int pl, int card)
{
    for (int i = 0; i < p->n[pl]; i++)
        if (p->hand[pl][i] == card) return i;
    return -1;
}

static void take_out(cpos *p, int pl, int i)
{
    memmove(p->hand[pl] + i, p->hand[pl] + i + 1, (size_t)(p->n[pl] - i - 1));
    p->n[pl]--;
}

static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

/* the stock is empty: the discard, but its top card, shuffled into it */
static void reshuffle(cpos *p)
{
    int n = p->npile - 1;
    if (n <= 0) return;
    uint8_t t = (uint8_t)top(p);
    memcpy(p->stock, p->pile, (size_t)n);
    p->reshuffles++;
    cards_shuffle(p->stock, n, mix(p->seed, 7u * p->round + 131u * p->reshuffles));
    p->nstock = (int16_t)n;
    p->pile[0] = t;
    p->npile = 1;
}

int ce_can_draw(const cpos *p) { return p->nstock > 0 || p->npile > 1; }

static int take(cpos *p)
{
    if (!p->nstock) reshuffle(p);
    return p->nstock ? p->stock[--p->nstock] : CE_NONE;
}

static void give(cpos *p, int pl, int k)
{
    for (int i = 0; i < k; i++) {
        int c = take(p);
        if (c == CE_NONE) break;
        insert(p, pl, c);
    }
    if (p->n[pl] != 1) p->called[pl] = 0;
}

static void deal(cpos *p)
{
    int n = cards_pack(last(p) ? PACK_ACTION : PACK_52, p->stock);
    cards_shuffle(p->stock, n, mix(p->seed, 1000u + p->round));
    p->nstock = (int16_t)n;
    p->npile = 0;
    p->reshuffles = 0;
    int each = last(p) || p->players == 2 ? 7 : 5;
    for (int pl = 0; pl < p->players; pl++) p->n[pl] = 0, p->called[pl] = 0;
    for (int k = 0; k < each; k++)              /* one at a time, from the dealer's left */
        for (int i = 1; i <= p->players; i++) insert(p, (p->dealer + i) % p->players, p->stock[--p->nstock]);
    while (special(p, p->stock[p->nstock - 1])) {   /* the starter: a plain card (the others go under the stock) */
        uint8_t c = p->stock[p->nstock - 1];
        memmove(p->stock + 1, p->stock, (size_t)(p->nstock - 1));
        p->stock[0] = c;
    }
    p->pile[p->npile++] = p->stock[--p->nstock];
    p->suit = (uint8_t)CARD_SUIT(top(p));
    p->dir = 0;
    p->turn = (uint8_t)((p->dealer + 1) % p->players);
    p->phase = CP_PLAY;
    p->drawn = CE_NONE;
    p->drew = p->acted = p->passes = p->blocked = 0;
    p->exposed = CE_NONE;
    p->winner = CE_NONE;
    p->gain = 0;
}

void ce_new(cgame *g, int variant, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    cpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->variant = p->variant = (uint8_t)(variant == CE_LAST ? CE_LAST : CE_EIGHTS);
    g->players = p->players = (uint8_t)(players < 2 ? 2 : players > CE_MAXP ? CE_MAXP : players);
    p->deck = last(p) ? DECK_ACTION : DECK_FRENCH;
    p->dealer = (uint8_t)(p->players - 1);      /* player 0 starts the first round */
    deal(p);
}

/* a card that matches the discard (the turn's limits aside) */
static int matches(const cpos *p, int card)
{
    int t = top(p);
    if (!last(p)) return CARD_RANK(card) == 8 || CARD_SUIT(card) == p->suit || CARD_RANK(card) == CARD_RANK(t);
    if (card == ACTION_WILD) return 1;
    if (card == ACTION_WILD4) {                 /* only with no card of the colour to follow */
        for (int i = 0; i < p->n[p->turn]; i++) {
            int c = p->hand[p->turn][i];
            if (!wild(c) && CARD_SUIT(c) == p->suit) return 0;
        }
        return 1;
    }
    return CARD_SUIT(card) == p->suit || (!wild(t) && CARD_RANK(card) == CARD_RANK(t));
}

int ce_playable(const cpos *p, int card)
{
    if (p->phase != CP_PLAY || find(p, p->turn, card) < 0) return 0;
    if (last(p) && p->drawn != CE_NONE && card != p->drawn) return 0;   /* after drawing: that card only */
    return matches(p, card);
}

static int has_play(const cpos *p)
{
    for (int i = 0; i < p->n[p->turn]; i++)
        if (ce_playable(p, p->hand[p->turn][i])) return 1;
    return 0;
}

int ce_legal(const cpos *p, int act)
{
    int k = CA_KIND(act), a = CA_ARG(act), pl = p->turn;
    switch (k) {
    case CA_PLAY: return ce_playable(p, a);
    case CA_CHOOSE: return p->phase == CP_CHOOSE && a < 4;
    case CA_DRAW:
        if (p->phase != CP_PLAY || !ce_can_draw(p)) return 0;
        return last(p) ? !p->drew : !has_play(p);
    case CA_PASS:
        if (p->phase != CP_PLAY) return 0;
        if (last(p) && p->drawn != CE_NONE) return 1;  /* keep the card drawn */
        return !ce_can_draw(p) && !has_play(p);
    case CA_CALL: return last(p) && p->phase == CP_PLAY && p->n[pl] == 2 && !p->called[pl] && has_play(p);
    case CA_CATCH:
        return last(p) && p->phase == CP_PLAY && p->exposed != CE_NONE && p->exposed != pl && !p->acted &&
               p->n[p->exposed] == 1;
    case CA_NEXT: return p->phase == CP_ROUND;
    default: return 0;
    }
}

/* the round is over: won (winner) or blocked (the fewest points in hand win, the difference with each hand) */
static void round_end(cpos *p, int winner)
{
    for (int q = 0; q < p->players; q++) p->left[q] = (int16_t)ce_hand_points(p, q);
    if (winner < 0) {
        p->blocked = 1;
        winner = p->turn;
        for (int i = 0; i < p->players; i++) {
            int q = (p->turn + i) % p->players;
            if (p->left[q] < p->left[winner]) winner = q;
        }
    }
    int gain = 0;
    for (int q = 0; q < p->players; q++)
        if (q != winner) gain += p->left[q] - p->left[winner];
    p->winner = (uint8_t)winner;
    p->gain = (int16_t)gain;
    p->score[winner] = (int16_t)(p->score[winner] + gain);
    p->phase = p->score[winner] >= ce_goal(p) ? CP_OVER : CP_ROUND;
    p->exposed = CE_NONE;
}

/* the turn goes to `next`; the player who moved may be caught (one card, not called) */
static void end_turn(cpos *p, int pl, int next)
{
    if (last(p)) {
        if (p->n[pl] == 1 && !p->called[pl]) p->exposed = (uint8_t)pl;
        if (p->n[pl] != 1) p->called[pl] = 0;
    }
    p->turn = (uint8_t)next;
    p->drew = p->acted = 0;
    p->drawn = CE_NONE;
}

/* the card on the discard takes effect (its suit already set) */
static void effect(cpos *p, int card)
{
    int pl = p->turn, r = CARD_RANK(card), victim = ce_next(p, pl, 1);
    if (last(p)) {
        int draw = card == ACTION_WILD4 ? 4 : !wild(card) && r == ACTION_DRAW2 ? 2 : 0;
        if (draw) give(p, victim, draw);
        if (!p->n[pl]) { round_end(p, pl); return; }
        if (!wild(card) && r == ACTION_REVERSE) {
            p->dir ^= 1;
            end_turn(p, pl, p->players == 2 ? pl : ce_next(p, pl, 1));
            return;
        }
        int skip = draw || (!wild(card) && r == ACTION_SKIP);
        end_turn(p, pl, ce_next(p, pl, skip ? 2 : 1));
        return;
    }
    if (!p->n[pl]) { round_end(p, pl); return; }
    end_turn(p, pl, victim);
}

static void apply(cpos *p, int act)
{
    int k = CA_KIND(act), a = CA_ARG(act), pl = p->turn;
    if (k == CA_PLAY || k == CA_DRAW || k == CA_PASS) p->exposed = CE_NONE, p->acted = 1;
    switch (k) {
    case CA_PLAY:
        take_out(p, pl, find(p, pl, a));
        p->pile[p->npile++] = (uint8_t)a;
        p->passes = 0;
        if ((last(p) ? wild(a) : CARD_RANK(a) == 8) && p->n[pl]) { p->phase = CP_CHOOSE; break; }
        if (!wild(a)) p->suit = (uint8_t)CARD_SUIT(a);
        effect(p, a);
        break;
    case CA_CHOOSE:
        p->suit = (uint8_t)a;
        p->phase = CP_PLAY;
        effect(p, top(p));
        break;
    case CA_DRAW: {
        int c = take(p);
        insert(p, pl, c);
        p->drew++;
        if (p->n[pl] != 1) p->called[pl] = 0;
        if (!last(p)) break;
        p->drawn = (uint8_t)c;
        if (!matches(p, c)) end_turn(p, pl, ce_next(p, pl, 1));   /* it cannot be played: the turn ends */
        break;
    }
    case CA_PASS:
        if (!ce_can_draw(p) && ++p->passes >= p->players) { round_end(p, -1); break; }
        end_turn(p, pl, ce_next(p, pl, 1));
        break;
    case CA_CALL: p->called[pl] = 1; break;
    case CA_CATCH: {
        int q = p->exposed;
        give(p, q, 2);
        p->exposed = CE_NONE;
        break;
    }
    case CA_NEXT:
        p->round++;
        p->dealer = (uint8_t)((p->dealer + 1) % p->players);
        deal(p);
        break;
    }
}

int ce_play(cgame *g, int act)
{
    if (g->n >= CE_MAX_ACTS || !ce_legal(&g->pos, act)) return 0;
    int turn = g->pos.turn, phase = g->pos.phase;
    apply(&g->pos, act);
    g->act[g->n++] = (uint16_t)act;
    int k = CA_KIND(act);
    if (k == CA_DRAW || k == CA_CATCH || g->pos.turn != turn || g->pos.phase != phase || k == CA_NEXT)
        if (!(k == CA_PLAY && g->pos.phase == CP_CHOOSE)) g->floor = g->n;   /* no undoing a turn or a drawn card */
    return 1;
}

int ce_undo(cgame *g)
{
    if (g->n <= g->floor) return 0;
    int n = g->n - 1;
    static uint16_t acts[CE_MAX_ACTS];
    memcpy(acts, g->act, sizeof acts[0] * (size_t)n);
    ce_new(g, g->variant, g->players, g->seed);
    for (int i = 0; i < n; i++) ce_play(g, acts[i]);
    return 1;
}

int ce_winner(const cpos *p) { return p->phase == CP_OVER ? p->winner : -1; }

int ce_auto(const cpos *p)
{
    int pl = p->turn;
    switch (p->phase) {
    case CP_ROUND: return CA_NEXT;
    case CP_CHOOSE: {                           /* the suit held most */
        int count[4] = {0, 0, 0, 0}, best = 0;
        for (int i = 0; i < p->n[pl]; i++)
            if (!wild(p->hand[pl][i]) && (last(p) || CARD_RANK(p->hand[pl][i]) != 8))
                count[CARD_SUIT(p->hand[pl][i]) & 3]++;
        for (int s = 1; s < 4; s++)
            if (count[s] > count[best]) best = s;
        return CA_CHOOSE | best;
    }
    case CP_PLAY: {
        if (ce_legal(p, CA_CATCH)) return CA_CATCH;
        if (ce_legal(p, CA_CALL)) return CA_CALL;
        int any = -1;
        for (int i = 0; i < p->n[pl]; i++) {    /* a plain card first, the eights and wilds kept */
            int c = p->hand[pl][i];
            if (!ce_playable(p, c)) continue;
            if (!(last(p) ? wild(c) : CARD_RANK(c) == 8)) return CA_PLAY | c;
            if (any < 0) any = c;
        }
        if (any >= 0) return CA_PLAY | any;
        if (ce_legal(p, CA_DRAW)) return CA_DRAW;
        return CA_PASS;
    }
    default: return 0;
    }
}

int ce_script(cgame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        char c = *s++;
        int arg = 0, digits = 0, act;
        while (*s >= '0' && *s <= '9') arg = arg * 10 + (*s++ - '0'), digits++;
        const cpos *p = &g->pos;
        if (c == '#' || c == '*') {             /* plain actions to the end of the round (of the game) */
            while (p->phase != CP_OVER && (c == '*' || p->phase != CP_ROUND))
                if (!ce_play(g, ce_auto(p))) return 0;
            continue;
        }
        switch (c) {
        case 'p':
            if (!digits || arg >= p->n[p->turn]) return 0;
            act = CA_PLAY | p->hand[p->turn][arg];
            break;
        case 'c': act = CA_CHOOSE | (arg & 0xff); break;
        case 'd': act = CA_DRAW; break;
        case 'x': act = CA_PASS; break;
        case 'l': act = CA_CALL; break;
        case 'k': act = CA_CATCH; break;
        case 'n': act = CA_NEXT; break;
        case 'a': act = ce_auto(p); break;
        default: return 0;
        }
        if (!ce_play(g, act)) return 0;
    }
    return 1;
}
