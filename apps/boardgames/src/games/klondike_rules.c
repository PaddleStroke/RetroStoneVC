/*
 * Klondike: the rules (klondike.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/klondike.h"

void kl_new(solrec *r, int variant, uint32_t seed)
{
    uint8_t deck[CARDS_MAX];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, seed);
    kl_deal_cards(r, variant, deck);
    r->seed = seed;
}

void kl_deal_cards(solrec *r, int variant, const uint8_t *deck)
{
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    r->variant = p->variant = (uint8_t)(variant == KL_DRAW3 ? KL_DRAW3 : KL_DRAW1);
    p->draw = p->variant == KL_DRAW3 ? 3 : 1;
    p->npiles = KL_PILES;
    for (int i = 0; i < KL_PILES; i++)
        p->kind[i] = i < KL_FOUND ? SP_TAB : i < KL_STOCK ? SP_FOUND : i == KL_STOCK ? SP_STOCK : SP_WASTE;
    int n = 52, k = 0;
    for (int row = 0; row < 7; row++)           /* dealt as at the table: a row at a time, one more column each */
        for (int col = row; col < 7; col++) p->c[col][p->n[col]++] = deck[k++];
    for (int col = 0; col < 7; col++) p->down[col] = (uint8_t)(p->n[col] - 1);
    while (k < n) p->c[KL_STOCK][p->n[KL_STOCK]++] = deck[k++];
    p->down[KL_STOCK] = p->n[KL_STOCK];
    sol_begin(r);
}

int kl_won(const solpos *p) { return sol_on_found(p) == 52; }

int kl_legal(const solpos *p, solmove m)
{
    int a = m.from, b = m.to, n = m.n;
    switch (SM_KIND(m.kind)) {
    case SM_DRAW:
        return a == KL_STOCK && b == KL_WASTE && p->n[a] && n == (p->n[a] < p->draw ? p->n[a] : p->draw);
    case SM_REDEAL:
        return a == KL_WASTE && b == KL_STOCK && !p->n[b] && p->n[a] && n == p->n[a];
    case SM_MOVE:
        break;
    default:
        return 0;
    }
    if (a >= KL_PILES || b >= KL_PILES || a == b || n < 1 || n > sol_up(p, a)) return 0;
    switch (p->kind[a]) {
    case SP_TAB: if (!sol_run(p, a, p->n[a] - n, 0)) return 0; break;
    case SP_WASTE: case SP_FOUND: if (n != 1) return 0; break;
    default: return 0;
    }
    int card = p->c[a][p->n[a] - n];
    switch (p->kind[b]) {
    case SP_TAB: return p->n[b] ? sol_fits(card, sol_top(p, b), 1) : CARD_RANK(card) == RANK_KING;
    case SP_FOUND: return n == 1 && p->kind[a] != SP_FOUND && sol_found_takes(p, b, card);
    default: return 0;
    }
}

/* the stock and the waste empty, every card face up: the rest goes up by itself (the lowest card first) */
static void finish(solrec *r)
{
    solpos *p = &r->pos;
    if (p->n[KL_STOCK] || p->n[KL_WASTE]) return;
    for (int c = 0; c < 7; c++)
        if (p->down[c]) return;
    while (!kl_won(p)) {
        int best = -1, rank = 99;
        for (int c = 0; c < 7; c++) {
            int t = sol_top(p, c);
            if (t != CARD_NONE && CARD_RANK(t) < rank && sol_found_for(p, t) >= 0) best = c, rank = CARD_RANK(t);
        }
        if (best < 0) return;
        sol_push(r, sol_mv(SM_MOVE | SM_AUTO, best, sol_found_for(p, sol_top(p, best)), 1));
    }
}

int kl_play(solrec *r, solmove m)
{
    if (r->n >= SOL_MOVES - 60 || !kl_legal(&r->pos, m)) return 0;
    sol_push(r, m);
    finish(r);
    return 1;
}

static solmove stock(const solpos *p)
{
    if (p->n[KL_STOCK]) return sol_mv(SM_DRAW, KL_STOCK, KL_WASTE, p->n[KL_STOCK] < p->draw ? p->n[KL_STOCK] : p->draw);
    if (p->n[KL_WASTE]) return sol_mv(SM_REDEAL, KL_WASTE, KL_STOCK, p->n[KL_WASTE]);
    return sol_mv(SM_NONE, 0, 0, 0);
}

static int pickable(const solpos *p, int pile, int i)
{
    if (i < 0 || i >= p->n[pile]) return 0;
    switch (p->kind[pile]) {
    case SP_TAB: return i >= p->down[pile];
    case SP_WASTE: case SP_FOUND: return i == p->n[pile] - 1;
    default: return 0;
    }
}

/* a column for the cards from i of pile a: one that is not empty first (from the next column round), then an empty
 * one (a king; not the whole of a column already) */
static int column_for(const solpos *p, int a, int i)
{
    int n = p->n[a] - i, from = p->kind[a] == SP_TAB ? a + 1 : 0;
    for (int pass = 0; pass < 2; pass++)
        for (int k = 0; k < 7; k++) {
            int b = (from + k) % 7;
            if (b == a || (pass == 0) != (p->n[b] > 0)) continue;
            if (pass == 1 && p->kind[a] == SP_TAB && i == 0) continue;
            if (kl_legal(p, sol_mv(SM_MOVE, a, b, n))) return b;
        }
    return -1;
}

static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    solmove none = sol_mv(SM_NONE, 0, 0, 0);
    if (!pickable(p, pile, i)) return none;
    int n = p->n[pile] - i;
    if (n == 1 && p->kind[pile] != SP_FOUND) {
        int f = sol_found_for(p, p->c[pile][i]);
        if (f >= 0) return sol_mv(SM_MOVE, pile, f, 1);
    }
    if (found_only) return none;
    int b = column_for(p, pile, i);
    return b >= 0 ? sol_mv(SM_MOVE, pile, b, n) : none;
}

/* a useful move: a card up to a foundation; a run that uncovers a face-down card or frees a card for a foundation;
 * the waste's card to the tableau; else the stock */
solmove kl_hint(const solpos *p)
{
    for (int a = 0; a < KL_PILES; a++) {
        if (p->kind[a] != SP_TAB && p->kind[a] != SP_WASTE) continue;
        int t = sol_top(p, a);
        if (t != CARD_NONE && sol_found_for(p, t) >= 0) return sol_mv(SM_MOVE, a, sol_found_for(p, t), 1);
    }
    for (int a = 0; a < 7; a++) {
        for (int i = p->down[a]; i < p->n[a]; i++) {
            int useful = i == p->down[a] ? p->down[a] > 0 : sol_found_for(p, p->c[a][i - 1]) >= 0;
            if (!useful) continue;
            for (int b = 0; b < 7; b++) {
                if (b == a || (!p->n[b] && i == 0)) continue;
                solmove m = sol_mv(SM_MOVE, a, b, p->n[a] - i);
                if (kl_legal(p, m)) return m;
            }
        }
    }
    if (p->n[KL_WASTE]) {
        int b = column_for(p, KL_WASTE, p->n[KL_WASTE] - 1);
        if (b >= 0) return sol_mv(SM_MOVE, KL_WASTE, b, 1);
    }
    return stock(p);
}

static void deal(solrec *r, uint32_t n) { kl_new(r, r->variant, n); }

const solrules kl_rules = {kl_legal, kl_play, stock, kl_hint, tap, pickable, kl_won, deal};

/* ---- the search (sollevels.h): the moves worth trying, best first --------------------------------------------- */
/* up to a foundation; a run that uncovers a face-down card (or frees a card for a foundation); the waste's card to
 * the tableau; the stock; a card back down from a foundation. A whole column never moves to an empty one. */
int kl_cands(const solpos *p, solmove *list)
{
    int n = 0;
    for (int a = 0; a < KL_PILES; a++) {
        int t = p->kind[a] == SP_TAB || a == KL_WASTE ? sol_top(p, a) : CARD_NONE;
        if (t != CARD_NONE && sol_found_for(p, t) >= 0) list[n++] = sol_mv(SM_MOVE, a, sol_found_for(p, t), 1);
    }
    for (int a = 0; a < 7; a++)
        for (int i = p->down[a]; i < p->n[a]; i++) {
            int useful = i == p->down[a] ? p->down[a] > 0 : sol_found_for(p, p->c[a][i - 1]) >= 0;
            for (int b = 0; b < 7 && useful; b++)
                if (b != a && kl_legal(p, sol_mv(SM_MOVE, a, b, p->n[a] - i)))
                    list[n++] = sol_mv(SM_MOVE, a, b, p->n[a] - i);
        }
    for (int b = 0; b < 7; b++)
        if (kl_legal(p, sol_mv(SM_MOVE, KL_WASTE, b, 1))) list[n++] = sol_mv(SM_MOVE, KL_WASTE, b, 1);
    solmove s = stock(p);
    if (s.kind != SM_NONE) list[n++] = s;
    for (int a = KL_FOUND; a < KL_STOCK; a++)
        for (int b = 0; b < 7; b++)
            if (kl_legal(p, sol_mv(SM_MOVE, a, b, 1))) list[n++] = sol_mv(SM_MOVE, a, b, 1);
    return n;
}

/* the waste turned over as often as you like: the key leaves the count out */
static uint64_t key(const solpos *p) { return sol_key_of(p, 0); }

const solsearch kl_search = {&kl_rules, kl_cands, NULL, key, NULL};

int kl_script(solrec *r, const char *moves) { return sol_script(r, moves, &kl_rules); }
