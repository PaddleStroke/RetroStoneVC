/*
 * Golf, the card game: the rules (golf.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/golf.h"

int golf_value(const gpos *p, int card)
{
    if (p->variant == GOLF_NUMBER) return NUMBER_VALUE(card);
    int r = CARD_RANK(card);
    return r == RANK_KING ? 0 : r >= RANK_JACK ? 10 : r;
}

/* the same rank: classic columns of two; the same value: number golf's columns of three */
static int same(const gpos *p, int a, int b)
{
    return p->variant == GOLF_NUMBER ? a == b : CARD_RANK(a) == CARD_RANK(b);
}

static int alive(const gpos *p, int pl, int s) { return !(p->gone[pl] >> s & 1); }
static int is_up(const gpos *p, int pl, int s) { return p->up[pl] >> s & 1; }

int golf_down(const gpos *p, int pl)
{
    int n = 0;
    for (int s = 0; s < p->slots; s++) n += alive(p, pl, s) && !is_up(p, pl, s);
    return n;
}

int golf_score(const gpos *p, int pl)
{
    int sum = 0;
    for (int c = 0; c < p->cols; c++) {
        int pair = p->variant == GOLF_CLASSIC;  /* classic: a column of two of the same rank scores 0 */
        for (int r = 0; r < p->rows; r++) {
            int s = r * p->cols + c;
            if (!alive(p, pl, s) || !is_up(p, pl, s)) { pair = 0; continue; }
            if (pair && r && !same(p, p->card[pl][s], p->card[pl][c])) pair = 0;
        }
        if (pair) continue;
        for (int r = 0; r < p->rows; r++) {
            int s = r * p->cols + c;
            if (alive(p, pl, s) && is_up(p, pl, s)) sum += golf_value(p, p->card[pl][s]);
        }
    }
    return sum;
}

/* number golf: a column of three visible cards of one value goes to the discard (the top one last) */
static void columns(gpos *p, int pl)
{
    if (p->variant != GOLF_NUMBER) return;
    for (int c = 0; c < p->cols; c++) {
        int ok = 1;
        for (int r = 0; r < p->rows && ok; r++) {
            int s = r * p->cols + c;
            ok = alive(p, pl, s) && is_up(p, pl, s) && same(p, p->card[pl][s], p->card[pl][c]);
        }
        if (!ok) continue;
        for (int r = p->rows - 1; r >= 0; r--) {
            int s = r * p->cols + c;
            p->gone[pl] |= (uint16_t)(1u << s);
            p->pile[p->npile++] = p->card[pl][s];
        }
    }
}

/* the deck of the round, shuffled from the game's seed, the round and the reshuffles */
static uint32_t mix(uint32_t a, uint32_t b) { return (a ^ (b * 0x9e3779b9u)) * 0x85ebca6bu + b; }

static void deal(gpos *p)
{
    int n = cards_pack(p->variant == GOLF_NUMBER ? PACK_NUMBER : PACK_52, p->stock);
    if (p->variant == GOLF_CLASSIC && p->players > 4) n += cards_pack(PACK_52, p->stock + n);
    cards_shuffle(p->stock, n, mix(p->seed, 1000u + p->round));
    p->nstock = (int16_t)n;
    p->npile = 0;
    p->reshuffles = 0;
    for (int pl = 0; pl < p->players; pl++) {
        p->up[pl] = p->gone[pl] = 0;
        for (int s = 0; s < p->slots; s++) p->card[pl][s] = p->stock[--p->nstock];
    }
    p->pile[p->npile++] = p->stock[--p->nstock];
    p->first = (uint8_t)(p->round % p->players);
    p->turn = p->first;
    p->phase = GP_REVEAL;
    p->revealed = 0;
    p->ender = GOLF_NONE;
    p->held = CARD_NONE;
    p->taken = 0;
}

void golf_new(ggame *g, int variant, int players, uint32_t seed)
{
    memset(g, 0, sizeof *g);
    gpos *p = &g->pos;
    g->seed = p->seed = seed;
    g->variant = p->variant = (uint8_t)(variant == GOLF_NUMBER ? GOLF_NUMBER : GOLF_CLASSIC);
    g->players = p->players = (uint8_t)(players < 2 ? 2 : players > GOLF_MAXP ? GOLF_MAXP : players);
    p->rows = p->variant == GOLF_NUMBER ? 3 : 2;
    p->cols = p->variant == GOLF_NUMBER ? 4 : 3;
    p->slots = (uint8_t)(p->rows * p->cols);
    p->deck = p->variant == GOLF_NUMBER ? DECK_NUMBER : DECK_FRENCH;
    deal(p);
}

int golf_legal(const gpos *p, int act)
{
    int k = GA_KIND(act), s = GA_SLOT(act), pl = p->turn, slot = s < p->slots && alive(p, pl, s);
    switch (p->phase) {
    case GP_REVEAL: return k == GA_REVEAL && slot && !is_up(p, pl, s);
    case GP_DRAW: return (k == GA_STOCK && (p->nstock > 0 || p->npile > 1)) || (k == GA_TAKE && p->npile > 0);
    case GP_HOLD: return (k == GA_SWAP && slot) || (k == GA_DISCARD && !p->taken);
    case GP_FLIP: return k == GA_FLIP && slot && !is_up(p, pl, s);
    case GP_ROUND: return k == GA_NEXT;
    default: return 0;
    }
}

static void round_end(gpos *p)
{
    int score[GOLF_MAXP], low = 1 << 30;
    for (int pl = 0; pl < p->players; pl++) {
        p->up[pl] = (uint16_t)((1u << p->slots) - 1);
        columns(p, pl);
        score[pl] = golf_score(p, pl);
    }
    p->doubled = 0;
    if (p->variant == GOLF_NUMBER && p->ender != GOLF_NONE) {   /* the ender doubles, unless strictly the lowest */
        for (int pl = 0; pl < p->players; pl++)
            if (pl != p->ender && score[pl] < low) low = score[pl];
        if (score[p->ender] > 0 && score[p->ender] >= low) score[p->ender] *= 2, p->doubled = 1;
    }
    int top = -100000;
    for (int pl = 0; pl < p->players; pl++) {
        p->last[pl] = (int16_t)score[pl];
        p->total[pl] = (int16_t)(p->total[pl] + score[pl]);
        if (p->total[pl] > top) top = p->total[pl];
    }
    p->round++;
    p->held = CARD_NONE;
    int over = p->variant == GOLF_NUMBER ? top >= GOLF_LIMIT : p->round >= GOLF_HOLES;
    p->phase = over ? GP_OVER : GP_ROUND;
}

static void end_turn(gpos *p)
{
    int pl = p->turn;
    columns(p, pl);
    p->held = CARD_NONE;
    p->taken = 0;
    if (p->ender == GOLF_NONE && golf_down(p, pl) == 0) p->ender = (uint8_t)pl;
    int next = (pl + 1) % p->players;
    if (p->ender != GOLF_NONE && next == p->ender) { round_end(p); return; }
    p->turn = (uint8_t)next;
    p->phase = GP_DRAW;
}

/* the stock is empty: the discard, but its top card, shuffled into it */
static void reshuffle(gpos *p)
{
    uint8_t top = p->pile[p->npile - 1];
    int n = p->npile - 1;
    for (int i = 0; i < n; i++) p->stock[i] = p->pile[i];
    p->reshuffles++;
    cards_shuffle(p->stock, n, mix(p->seed, 7u * p->round + 131u * p->reshuffles));
    p->nstock = (int16_t)n;
    p->pile[0] = top;
    p->npile = 1;
}

static void apply(gpos *p, int act)
{
    int k = GA_KIND(act), s = GA_SLOT(act), pl = p->turn;
    switch (k) {
    case GA_REVEAL:
        p->up[pl] |= (uint16_t)(1u << s);
        if (++p->revealed < 2) break;
        p->revealed = 0;
        p->turn = (uint8_t)((pl + 1) % p->players);
        if (p->turn != p->first) break;
        p->phase = GP_DRAW;                     /* everyone has turned two: the first player starts... */
        if (p->variant == GOLF_NUMBER) {        /* ...or, number golf, the highest sum showing */
            int best = -1000;
            for (int i = 0; i < p->players; i++) {
                int q = (p->first + i) % p->players, v = golf_score(p, q);
                if (v > best) best = v, p->turn = (uint8_t)q;
            }
        }
        break;
    case GA_STOCK:
        if (!p->nstock) reshuffle(p);
        p->held = p->stock[--p->nstock];
        p->taken = 0;
        p->phase = GP_HOLD;
        break;
    case GA_TAKE:
        p->held = p->pile[--p->npile];
        p->taken = 1;
        p->phase = GP_HOLD;
        break;
    case GA_SWAP:
        p->pile[p->npile++] = p->card[pl][s];
        p->card[pl][s] = p->held;
        p->up[pl] |= (uint16_t)(1u << s);
        end_turn(p);
        break;
    case GA_DISCARD:
        p->pile[p->npile++] = p->held;
        p->held = CARD_NONE;
        p->phase = GP_FLIP;
        break;
    case GA_FLIP:
        p->up[pl] |= (uint16_t)(1u << s);
        end_turn(p);
        break;
    case GA_NEXT:
        deal(p);
        break;
    }
}

int golf_play(ggame *g, int act)
{
    if (g->n >= GOLF_MAX_ACTS || !golf_legal(&g->pos, act)) return 0;
    apply(&g->pos, act);
    g->act[g->n++] = (uint8_t)act;
    return 1;
}

int golf_undo(ggame *g)
{
    if (!g->n) return 0;
    int n = g->n - 1;
    uint8_t acts[GOLF_MAX_ACTS];
    memcpy(acts, g->act, (size_t)n);
    golf_new(g, g->variant, g->players, g->seed);
    for (int i = 0; i < n; i++) golf_play(g, acts[i]);
    return 1;
}

int golf_winner(const gpos *p)
{
    if (p->phase != GP_OVER) return -1;
    int best = 0, tie = 0;
    for (int pl = 1; pl < p->players; pl++) {
        if (p->total[pl] < p->total[best]) best = pl, tie = 0;
        else if (p->total[pl] == p->total[best]) tie = 1;
    }
    return tie ? -1 : best;
}

static int first_down(const gpos *p)
{
    for (int s = 0; s < p->slots; s++)
        if (alive(p, p->turn, s) && !is_up(p, p->turn, s)) return s;
    for (int s = 0; s < p->slots; s++)
        if (alive(p, p->turn, s)) return s;
    return 0;
}

int golf_auto(const gpos *p)
{
    switch (p->phase) {
    case GP_REVEAL: return GA_REVEAL | first_down(p);
    case GP_DRAW: return GA_STOCK;
    case GP_HOLD: return GA_SWAP | first_down(p);
    case GP_FLIP: return GA_FLIP | first_down(p);
    case GP_ROUND: return GA_NEXT;
    default: return 0;
    }
}

int golf_script(ggame *g, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        char c = *s++;
        int slot = 0, act;
        while (*s >= '0' && *s <= '9') slot = slot * 10 + (*s++ - '0');
        if (c == '#') {                         /* plain moves to the end of the round */
            while (g->pos.phase != GP_ROUND && g->pos.phase != GP_OVER)
                if (!golf_play(g, golf_auto(&g->pos))) return 0;
            continue;
        }
        switch (c) {
        case 'r': act = GA_REVEAL | slot; break;
        case 's': act = GA_STOCK; break;
        case 't': act = GA_TAKE; break;
        case 'w': act = GA_SWAP | slot; break;
        case 'd': act = GA_DISCARD; break;
        case 'f': act = GA_FLIP | slot; break;
        case 'n': act = GA_NEXT; break;
        default: return 0;
        }
        if (slot > 15 || !golf_play(g, act)) return 0;
    }
    return 1;
}

/* what one seat may see (online play later: a server sends each player only this): the position with every card that
 * is face down (the players' and the stock's) turned to CARD_NONE, and no seed (it would tell the deal). The discard,
 * the cards face up or gone, the drawn card and the counts stay. Golf hides the same from every seat (seat unused:
 * the hidden-hand games' views differ by seat). */
void golf_view(const gpos *p, int seat, gpos *out)
{
    (void)seat;
    *out = *p;
    for (int pl = 0; pl < out->players; pl++)
        for (int s = 0; s < out->slots; s++)
            if (!is_up(out, pl, s) && !(out->gone[pl] >> s & 1)) out->card[pl][s] = CARD_NONE;
    for (int i = 0; i < out->nstock; i++) out->stock[i] = CARD_NONE;
    out->seed = 0;
}
