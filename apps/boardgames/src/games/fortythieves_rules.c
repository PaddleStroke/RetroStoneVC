/*
 * Forty Thieves: the rules (fortythieves.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/fortythieves.h"

void ft_new(solrec *r, uint32_t seed)
{
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    r->seed = seed;
    p->npiles = FT_PILES;
    p->draw = 1;
    for (int i = 0; i < FT_PILES; i++)
        p->kind[i] = i < FT_FOUND ? SP_TAB : i < FT_STOCK ? SP_FOUND : i == FT_STOCK ? SP_STOCK : SP_WASTE;
    uint8_t deck[2 * 52];
    int n = cards_pack(PACK_52, deck), k = 0;
    n += cards_pack(PACK_52, deck + n);
    cards_shuffle(deck, n, seed);
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 10; col++) p->c[col][p->n[col]++] = deck[k++];
    while (k < n) p->c[FT_STOCK][p->n[FT_STOCK]++] = deck[k++];
    p->down[FT_STOCK] = p->n[FT_STOCK];
    sol_begin(r);
}

int ft_won(const solpos *p) { return sol_on_found(p) == 104; }

int ft_found_for(const solpos *p, int card)
{
    int empty = -1;
    for (int f = FT_FOUND; f < FT_STOCK; f++) {
        if (!p->n[f]) { if (empty < 0) empty = f; continue; }
        int t = sol_top(p, f);
        if (CARD_SUIT(t) == CARD_SUIT(card) && CARD_RANK(card) == CARD_RANK(t) + 1) return f;
    }
    return CARD_RANK(card) == RANK_ACE ? empty : -1;
}

int ft_legal(const solpos *p, solmove m)
{
    int a = m.from, b = m.to;
    if (SM_KIND(m.kind) == SM_DRAW) return a == FT_STOCK && b == FT_WASTE && m.n == 1 && p->n[a];
    if (SM_KIND(m.kind) != SM_MOVE || a >= FT_PILES || b >= FT_PILES || a == b || m.n != 1 || !p->n[a]) return 0;
    if (p->kind[a] != SP_TAB && p->kind[a] != SP_WASTE) return 0;
    int card = sol_top(p, a);
    switch (p->kind[b]) {
    case SP_TAB:
        return !p->n[b] || (sol_fits(card, sol_top(p, b), 0) && CARD_SUIT(card) == CARD_SUIT(sol_top(p, b)));
    case SP_FOUND:
        return p->n[b] ? CARD_SUIT(card) == CARD_SUIT(sol_top(p, b)) && CARD_RANK(card) == CARD_RANK(sol_top(p, b)) + 1
                       : CARD_RANK(card) == RANK_ACE;
    default:
        return 0;
    }
}

int ft_play(solrec *r, solmove m)
{
    if (r->n >= SOL_MOVES - 120 || !ft_legal(&r->pos, m)) return 0;
    sol_push(r, m);
    if (!r->pos.n[FT_STOCK] && !r->pos.n[FT_WASTE]) sol_finish(r, 104, ft_found_for);
    return 1;
}

static solmove none(void) { return sol_mv(SM_NONE, 0, 0, 0); }

static solmove stock(const solpos *p) { return p->n[FT_STOCK] ? sol_mv(SM_DRAW, FT_STOCK, FT_WASTE, 1) : none(); }

static int pickable(const solpos *p, int pile, int i)
{
    return (p->kind[pile] == SP_TAB || p->kind[pile] == SP_WASTE) && i >= 0 && i == p->n[pile] - 1;
}

/* where a tap sends a top card: a foundation, the next card of its suit (from the next column round), an empty column
 * (not a column's only card) */
static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    if (!pickable(p, pile, i)) return none();
    int f = ft_found_for(p, sol_top(p, pile));
    if (f >= 0) return sol_mv(SM_MOVE, pile, f, 1);
    if (found_only) return none();
    int from = p->kind[pile] == SP_TAB ? pile + 1 : 0;
    for (int pass = 0; pass < 2; pass++)
        for (int k = 0; k < 10; k++) {
            int b = (from + k) % 10;
            if (b == pile || (pass == 0) != (p->n[b] > 0) || (pass == 1 && p->kind[pile] == SP_TAB && i == 0)) continue;
            if (ft_legal(p, sol_mv(SM_MOVE, pile, b, 1))) return sol_mv(SM_MOVE, pile, b, 1);
        }
    return none();
}

/* a useful move: a card up; else the best of the moves onto a column (the card under it can then go up; the column
 * left empty; the waste's card played; a card off one it does not follow), never a card off the one it follows
 * unless that one can then go up; else the stock */
solmove ft_hint(const solpos *p)
{
    for (int a = 0; a < FT_PILES; a++) {
        if (p->kind[a] != SP_TAB && p->kind[a] != SP_WASTE) continue;
        int t = sol_top(p, a), f = t == CARD_NONE ? -1 : ft_found_for(p, t);
        if (f >= 0) return sol_mv(SM_MOVE, a, f, 1);
    }
    solmove best = none();
    int top = 0;
    for (int a = 0; a < FT_PILES; a++) {
        if ((p->kind[a] != SP_TAB && p->kind[a] != SP_WASTE) || !p->n[a]) continue;
        int n = p->n[a], card = p->c[a][n - 1], under = n > 1 ? p->c[a][n - 2] : CARD_NONE;
        int sits = p->kind[a] == SP_TAB && under != CARD_NONE && sol_fits(card, under, 0) &&
                   CARD_SUIT(card) == CARD_SUIT(under);
        int frees = p->kind[a] == SP_TAB && under != CARD_NONE && ft_found_for(p, under) >= 0;
        for (int b = 0; b < FT_FOUND; b++) {
            if (!ft_legal(p, sol_mv(SM_MOVE, a, b, 1))) continue;
            int score;
            if (frees) score = 60;
            else if (sits) continue;
            else if (!p->n[b]) continue;        /* to an empty column only to free a card for a foundation */
            else if (p->kind[a] == SP_WASTE) score = 30;
            else score = n == 1 ? 40 : 20;
            if (!p->n[b] && n == 1) continue;
            if (score > top) top = score, best = sol_mv(SM_MOVE, a, b, 1);
        }
    }
    if (top) return best;
    for (int a = 0; a < FT_STOCK; a++)          /* a move after which a card can go up (two moves ahead) */
        for (int b = 0; b < FT_FOUND; b++) {
            solmove m = sol_mv(SM_MOVE, a, b, 1);
            if (!ft_legal(p, m) || (!p->n[b] && p->n[a] == 1)) continue;
            solpos q = *p;
            sol_apply(&q, m);
            for (int c = 0; c < FT_FOUND; c++)
                if (c != b && q.n[c] && ft_found_for(&q, sol_top(&q, c)) >= 0) return m;
        }
    return stock(p);
}

static void deal(solrec *r, uint32_t n) { ft_new(r, n); }

const solrules ft_rules = {ft_legal, ft_play, stock, ft_hint, tap, pickable, ft_won, deal};

int ft_script(solrec *r, const char *moves) { return sol_script(r, moves, &ft_rules); }
