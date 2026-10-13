/*
 * Canfield: the rules (canfield.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/canfield.h"

void cn_new(solrec *r, uint32_t seed)
{
    uint8_t deck[2 * 52];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, seed);
    cn_deal_cards(r, 0, deck);
    r->seed = seed;
}

void cn_deal_cards(solrec *r, int variant, const uint8_t *deck)
{
    (void)variant;
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    p->npiles = CN_PILES;
    p->draw = 3;
    for (int i = 0; i < CN_PILES; i++)
        p->kind[i] = i < CN_FOUND ? SP_TAB : i < CN_STOCK ? SP_FOUND : i == CN_STOCK ? SP_STOCK : SP_WASTE;
    int n = 52, k = 0;
    while (p->n[CN_RESERVE] < 13) p->c[CN_RESERVE][p->n[CN_RESERVE]++] = deck[k++];
    p->down[CN_RESERVE] = 12;
    p->c[CN_FOUND][p->n[CN_FOUND]++] = deck[k++];
    for (int t = 0; t < 4; t++) p->c[CN_TAB + t][p->n[CN_TAB + t]++] = deck[k++];
    while (k < n) p->c[CN_STOCK][p->n[CN_STOCK]++] = deck[k++];
    p->down[CN_STOCK] = p->n[CN_STOCK];
    sol_begin(r);
}

int cn_won(const solpos *p) { return sol_on_found(p) == 52; }

int cn_base(const solpos *p) { return p->n[CN_FOUND] ? CARD_RANK(p->c[CN_FOUND][0]) : RANK_ACE; }

static int next(int rank) { return rank == RANK_KING ? RANK_ACE : rank + 1; }   /* round the corner */

static int takes(const solpos *p, int f, int card)
{
    if (!p->n[f]) return CARD_RANK(card) == cn_base(p);
    int t = sol_top(p, f);
    return CARD_SUIT(t) == CARD_SUIT(card) && CARD_RANK(card) == next(CARD_RANK(t));
}

int cn_found_for(const solpos *p, int card)
{
    for (int f = CN_FOUND; f < CN_STOCK; f++)
        if (p->n[f] && CARD_SUIT(sol_top(p, f)) == CARD_SUIT(card)) return takes(p, f, card) ? f : -1;
    for (int f = CN_FOUND; f < CN_STOCK; f++)
        if (takes(p, f, card)) return f;
    return -1;
}

static int follows(int card, int on) { return next(CARD_RANK(card)) == CARD_RANK(on) && sol_red(card) != sol_red(on); }

int cn_legal(const solpos *p, solmove m)
{
    int a = m.from, b = m.to, n = m.n;
    switch (SM_KIND(m.kind)) {
    case SM_DRAW:
        return a == CN_STOCK && b == CN_WASTE && p->n[a] && n == (p->n[a] < 3 ? p->n[a] : 3);
    case SM_REDEAL:
        return a == CN_WASTE && b == CN_STOCK && !p->n[b] && p->n[a] && n == p->n[a];
    case SM_MOVE:
        break;
    default:
        return 0;
    }
    if (a >= CN_FOUND && a != CN_WASTE) return 0;
    if (b >= CN_PILES || a == b || n < 1 || n > p->n[a] || (n > 1 && (a >= CN_RESERVE || n != p->n[a]))) return 0;
    for (int k = p->n[a] - n + 1; k < p->n[a]; k++)    /* a pile is always in order; checked all the same */
        if (!follows(p->c[a][k], p->c[a][k - 1])) return 0;
    int card = p->c[a][p->n[a] - n];
    if (b < CN_RESERVE) return p->n[b] ? follows(card, sol_top(p, b)) : 1;
    if (b >= CN_FOUND && b < CN_STOCK) return n == 1 && takes(p, b, card);
    return 0;
}

int cn_play(solrec *r, solmove m)
{
    solpos *p = &r->pos;
    if (r->n >= SOL_MOVES - 80 || !cn_legal(p, m)) return 0;
    sol_push(r, m);
    for (int t = 0; t < 4; t++)                 /* a pile left empty: the reserve's card, at once */
        if (!p->n[t] && p->n[CN_RESERVE]) sol_push(r, sol_mv(SM_MOVE | SM_AUTO, CN_RESERVE, t, 1));
    if (!p->n[CN_STOCK] && !p->n[CN_WASTE] && !p->n[CN_RESERVE]) sol_finish(r, 52, cn_found_for);
    return 1;
}

static solmove none(void) { return sol_mv(SM_NONE, 0, 0, 0); }

static solmove stock(const solpos *p)
{
    if (p->n[CN_STOCK]) return sol_mv(SM_DRAW, CN_STOCK, CN_WASTE, p->n[CN_STOCK] < 3 ? p->n[CN_STOCK] : 3);
    if (p->n[CN_WASTE]) return sol_mv(SM_REDEAL, CN_WASTE, CN_STOCK, p->n[CN_WASTE]);
    return none();
}

static int pickable(const solpos *p, int pile, int i)
{
    if (i < 0 || i >= p->n[pile]) return 0;
    if (pile < CN_RESERVE) return i == 0 || i == p->n[pile] - 1;
    return (pile == CN_RESERVE || pile == CN_WASTE) && i == p->n[pile] - 1;
}

/* where a tap sends the cards from i: a foundation (one card), a pile that is not empty (from the next one round),
 * an empty pile (not a whole pile already) */
static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    if (!pickable(p, pile, i)) return none();
    int n = p->n[pile] - i;
    if (n == 1) {
        int f = cn_found_for(p, sol_top(p, pile));
        if (f >= 0) return sol_mv(SM_MOVE, pile, f, 1);
    }
    if (found_only) return none();
    int from = pile < CN_RESERVE ? pile + 1 : 0;
    for (int pass = 0; pass < 2; pass++)
        for (int k = 0; k < 4; k++) {
            int b = (from + k) % 4;
            if (b == pile || (pass == 0) != (p->n[b] > 0) || (pass == 1 && pile < CN_RESERVE && i == 0)) continue;
            if (cn_legal(p, sol_mv(SM_MOVE, pile, b, n))) return sol_mv(SM_MOVE, pile, b, n);
        }
    return none();
}

/* a useful move: a card up (the reserve's first); else the best of: the reserve's card onto the tableau, the waste's,
 * a whole pile onto another one (the reserve fills its place); else the stock */
solmove cn_hint(const solpos *p)
{
    static const int from[6] = {CN_RESERVE, 0, 1, 2, 3, CN_WASTE};
    for (int k = 0; k < 6; k++) {
        int a = from[k], f = p->n[a] ? cn_found_for(p, sol_top(p, a)) : -1;
        if (f >= 0) return sol_mv(SM_MOVE, a, f, 1);
    }
    solmove best = none();
    int top = 0;
    for (int k = 0; k < 6; k++) {
        int a = from[k];
        if (!p->n[a]) continue;
        for (int b = 0; b < 4; b++) {
            int whole = a < CN_RESERVE, n = whole ? p->n[a] : 1, score;
            solmove m = sol_mv(SM_MOVE, a, b, n);
            if (b == a || !cn_legal(p, m)) continue;
            if (a == CN_RESERVE) score = 50;
            else if (a == CN_WASTE) score = p->n[b] ? 30 : 10;
            else score = p->n[b] && p->n[CN_RESERVE] ? 20 : 0;
            if (score > top) top = score, best = m;
        }
    }
    return top ? best : stock(p);
}

static void deal(solrec *r, uint32_t n) { cn_new(r, n); }

const solrules cn_rules = {cn_legal, cn_play, stock, cn_hint, tap, pickable, cn_won, deal};

int cn_script(solrec *r, const char *moves) { return sol_script(r, moves, &cn_rules); }

/* ---- the search (sollevels.h): the moves worth trying, best first ----------------------------------------------- */
/* up to a foundation (the reserve first); the reserve's card down; the waste's onto a pile; a whole pile onto
 * another; the stock; the waste's to an empty pile last */
int cn_cands(const solpos *p, solmove *list)
{
    static const int from[6] = {CN_RESERVE, 0, 1, 2, 3, CN_WASTE};
    solmove m[64];
    int v[64], n = 0, empty = -1;
    for (int b = 0; b < 4; b++)
        if (!p->n[b] && empty < 0) empty = b;
    for (int k = 0; k < 6; k++) {
        int a = from[k], f = p->n[a] ? cn_found_for(p, sol_top(p, a)) : -1;
        if (f >= 0) m[n] = sol_mv(SM_MOVE, a, f, 1), v[n++] = a == CN_RESERVE ? 110 : 100;
    }
    for (int k = 0; k < 6; k++) {
        int a = from[k];
        if (!p->n[a]) continue;
        for (int b = 0; b < 4; b++) {
            int whole = a < CN_RESERVE, cnt = whole ? p->n[a] : 1, score;
            solmove mv = sol_mv(SM_MOVE, a, b, cnt);
            if (b == a || (!p->n[b] && b != empty) || !cn_legal(p, mv)) continue;
            if (a == CN_RESERVE) score = 50;
            else if (a == CN_WASTE) score = p->n[b] ? 30 : 4;
            else score = p->n[b] ? 20 : 0;
            m[n] = mv, v[n++] = score;
        }
    }
    solmove s = stock(p);
    if (s.kind != SM_NONE) m[n] = s, v[n++] = 8;
    return sol_best_first(m, v, n, list);
}

/* the waste turned over as often as you like: the key leaves the count out */
static uint64_t key(const solpos *p) { return sol_key_of(p, 0); }

const solsearch cn_search = {&cn_rules, cn_cands, NULL, key, NULL};
