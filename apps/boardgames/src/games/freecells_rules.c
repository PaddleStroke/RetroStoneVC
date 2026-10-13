/*
 * Free Cells: the rules (freecells.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/freecells.h"

void fc_deal(int deal, uint8_t out[52])
{
    static const uint8_t suit[4] = {SUIT_CLUBS, SUIT_DIAMONDS, SUIT_HEARTS, SUIT_SPADES};
    int t[52];
    uint32_t x = (uint32_t)deal;
    for (int i = 0; i < 52; i++) t[i] = 51 - i;
    for (int i = 0; i < 51; i++) {
        x = (x * 214013u + 2531011u) & 0x7fffffffu;
        int j = 51 - (int)((x >> 16) % (uint32_t)(52 - i)), s = t[i];
        t[i] = t[j];
        t[j] = s;
    }
    for (int i = 0; i < 52; i++) out[i] = CARD(suit[t[i] % 4], t[i] / 4 + 1);
}

void fc_new(solrec *r, int deal)
{
    uint8_t d[52];
    deal = deal < 1 ? 1 : deal > FC_DEALS ? FC_DEALS : deal;
    fc_deal(deal, d);
    fc_deal_cards(r, 0, d);
    r->seed = (uint32_t)deal;
}

void fc_deal_cards(solrec *r, int variant, const uint8_t *d)
{
    (void)variant;
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    p->npiles = FC_PILES;
    for (int i = 0; i < FC_PILES; i++) p->kind[i] = i < FC_CELL ? SP_TAB : i < FC_FOUND ? SP_CELL : SP_FOUND;
    for (int i = 0; i < 52; i++) p->c[i % 8][p->n[i % 8]++] = d[i];
    sol_begin(r);
}

int fc_won(const solpos *p) { return sol_on_found(p) == 52; }

int fc_most(const solpos *p, int to)
{
    if (p->kind[to] != SP_TAB) return 1;
    int cells = 0, empty = 0;
    for (int i = 0; i < FC_PILES; i++) {
        if (p->kind[i] == SP_CELL) cells += !p->n[i];
        if (p->kind[i] == SP_TAB) empty += !p->n[i] && i != to;
    }
    return (cells + 1) << empty;
}

int fc_legal(const solpos *p, solmove m)
{
    int a = m.from, b = m.to, n = m.n;
    if (SM_KIND(m.kind) != SM_MOVE || a >= FC_PILES || b >= FC_PILES || a == b || n < 1 || n > p->n[a]) return 0;
    switch (p->kind[a]) {
    case SP_TAB: if (!sol_run(p, a, p->n[a] - n, 0)) return 0; break;
    case SP_CELL: if (n != 1) return 0; break;
    default: return 0;
    }
    int card = p->c[a][p->n[a] - n];
    switch (p->kind[b]) {
    case SP_TAB: return n <= fc_most(p, b) && (!p->n[b] || sol_fits(card, sol_top(p, b), 1));
    case SP_CELL: return n == 1 && !p->n[b] && p->kind[a] != SP_CELL;
    case SP_FOUND: return n == 1 && sol_found_takes(p, b, card);
    default: return 0;
    }
}

/* a card no longer needed in the tableau: an ace or a two, or both foundations of the other colour one rank below
 * it or higher (nothing could go on it any more) */
static int safe(const solpos *p, int card)
{
    int r = CARD_RANK(card), red = sol_red(card);
    if (r <= 2) return 1;
    int a = sol_found_rank(p, red ? SUIT_SPADES : SUIT_HEARTS), b = sol_found_rank(p, red ? SUIT_CLUBS : SUIT_DIAMONDS);
    return a >= r - 1 && b >= r - 1;
}

static void tidy(solrec *r)
{
    solpos *p = &r->pos;
    for (int again = 1; again;) {
        again = 0;
        for (int a = 0; a < FC_FOUND; a++) {
            int t = sol_top(p, a), f = t == CARD_NONE ? -1 : sol_found_for(p, t);
            if (f < 0 || !safe(p, t)) continue;
            sol_push(r, sol_mv(SM_MOVE | SM_AUTO, a, f, 1));
            again = 1;
        }
    }
}

int fc_play(solrec *r, solmove m)
{
    if (r->n >= SOL_MOVES - 60 || !fc_legal(&r->pos, m)) return 0;
    sol_push(r, m);
    tidy(r);
    return 1;
}

static solmove stock(const solpos *p)
{
    (void)p;
    return sol_mv(SM_NONE, 0, 0, 0);
}

static int pickable(const solpos *p, int pile, int i)
{
    if (i < 0 || i >= p->n[pile]) return 0;
    if (p->kind[pile] == SP_TAB) return sol_run(p, pile, i, 0);
    return p->kind[pile] == SP_CELL;
}

/* where a tap sends the cards from i: a foundation, a column that is not empty (from the next one round), an empty
 * column (not a whole column already), a free cell */
static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    solmove none = sol_mv(SM_NONE, 0, 0, 0);
    if (!pickable(p, pile, i)) return none;
    int n = p->n[pile] - i;
    if (n == 1) {
        int f = sol_found_for(p, p->c[pile][i]);
        if (f >= 0) return sol_mv(SM_MOVE, pile, f, 1);
    }
    if (found_only) return none;
    int from = p->kind[pile] == SP_TAB ? pile + 1 : 0;
    for (int pass = 0; pass < 2; pass++)
        for (int k = 0; k < 8; k++) {
            int b = (from + k) % 8;
            if (b == pile || (pass == 0) != (p->n[b] > 0) || (pass == 1 && p->kind[pile] == SP_TAB && i == 0)) continue;
            solmove m = sol_mv(SM_MOVE, pile, b, n);
            if (fc_legal(p, m)) return m;
        }
    if (n == 1 && p->kind[pile] == SP_TAB)
        for (int c = FC_CELL; c < FC_FOUND; c++)
            if (!p->n[c]) return sol_mv(SM_MOVE, pile, c, 1);
    return none;
}

/* ---- the hint: a look two moves ahead -------------------------------------------------------------------------- */
/* the moves worth looking at: each source to each place, the most cards that go (one n per pair) */
static int moves(const solpos *p, solmove *out)
{
    int k = 0;
    for (int a = 0; a < FC_FOUND; a++)
        for (int b = 0; b < FC_PILES; b++)
            for (int n = p->n[a]; n >= 1; n--)
                if (fc_legal(p, sol_mv(SM_MOVE, a, b, n))) {
                    out[k++] = sol_mv(SM_MOVE, a, b, n);
                    break;
                }
    return k;
}

static void settle(solpos *p)                   /* the safe cards up, as after a move */
{
    for (int again = 1; again;) {
        again = 0;
        for (int a = 0; a < FC_FOUND; a++) {
            int t = sol_top(p, a), f = t == CARD_NONE ? -1 : sol_found_for(p, t);
            if (f >= 0 && safe(p, t)) sol_apply(p, sol_mv(SM_MOVE, a, f, 1)), again = 1;
        }
    }
}

/* how good a position looks: cards up, free cells and columns empty, cards in order, the next cards each suit
 * needs near the top */
static int eval(const solpos *p)
{
    int v = 100 * sol_on_found(p);
    for (int i = 0; i < FC_PILES; i++) {
        if (p->kind[i] == SP_CELL) v += p->n[i] ? 0 : 22;
        if (p->kind[i] != SP_TAB) continue;
        if (!p->n[i]) v += 45;
        for (int k = 1; k < p->n[i]; k++) v += 5 * sol_fits(p->c[i][k], p->c[i][k - 1], 1);
        for (int k = 0; k < p->n[i]; k++) {
            int c = p->c[i][k];
            if (CARD_RANK(c) <= sol_found_rank(p, CARD_SUIT(c)) + 2)
                v -= (p->n[i] - 1 - k) * (CARD_RANK(c) == sol_found_rank(p, CARD_SUIT(c)) + 1 ? 9 : 4);
        }
    }
    return v;
}

static int after(const solpos *p, solmove m, solpos *q)
{
    *q = *p;
    sol_apply(q, m);
    settle(q);
    return eval(q);
}

solmove fc_hint(const solpos *p)
{
    solmove list[256], list2[256], best = sol_mv(SM_NONE, 0, 0, 0);
    int n = moves(p, list), top = -(1 << 30);
    for (int i = 0; i < n; i++) {
        solpos q, r;
        int v = after(p, list[i], &q), n2 = moves(&q, list2);
        if (fc_won(&q)) return list[i];
        for (int j = 0; j < n2; j++) {
            int w = after(&q, list2[j], &r) - 2;
            if (w > v) v = w;
        }
        if (v > top) top = v, best = list[i];
    }
    return best;
}

static void deal(solrec *r, uint32_t n) { fc_new(r, n < 1 ? 1 : n > FC_DEALS ? FC_DEALS : (int)n); }

const solrules fc_rules = {fc_legal, fc_play, stock, fc_hint, tap, pickable, fc_won, deal};

int fc_script(solrec *r, const char *moves) { return sol_script(r, moves, &fc_rules); }

/* ---- the search (sollevels.h): the moves worth trying, the best looking first ----------------------------------- */
/* each source to each place (the most cards that go), but one empty free cell and one empty column of each (they are
 * all alike), and never a whole column onto an empty one; sorted by how good the position looks after them */
int fc_cands(const solpos *p, solmove *list)
{
    solmove m[256];
    int n = 0, cell = -1, col = -1, v[256];
    for (int b = 0; b < FC_FOUND; b++)
        if (!p->n[b]) {
            if (p->kind[b] == SP_CELL && cell < 0) cell = b;
            if (p->kind[b] == SP_TAB && col < 0) col = b;
        }
    for (int a = 0; a < FC_FOUND; a++)
        for (int b = 0; b < FC_PILES; b++) {
            if (!p->n[b] && b != cell && b != col && p->kind[b] != SP_FOUND) continue;
            for (int k = p->n[a]; k >= 1; k--) {
                if (p->kind[a] == SP_TAB && k == p->n[a] && !p->n[b] && p->kind[b] == SP_TAB) continue;
                if (fc_legal(p, sol_mv(SM_MOVE, a, b, k))) {
                    m[n++] = sol_mv(SM_MOVE, a, b, k);
                    break;
                }
            }
        }
    for (int i = 0; i < n; i++) {
        solpos q;
        v[i] = after(p, m[i], &q);
    }
    for (int k = 0; k < n && k < SOLVE_BRANCH; k++) {   /* the best first */
        int best = k;
        for (int i = k + 1; i < n; i++)
            if (v[i] > v[best]) best = i;
        solmove t = m[k];
        int tv = v[k];
        m[k] = m[best], v[k] = v[best];
        m[best] = t, v[best] = tv;
        list[k] = m[k];
    }
    return n < SOLVE_BRANCH ? n : SOLVE_BRANCH;
}

/* the columns and the free cells as sets (their order does not matter): a position's key */
static uint64_t key(const solpos *p)
{
    uint64_t h = 0;
    for (int i = 0; i < FC_FOUND; i++) {
        uint64_t c = 1469598103934665603ull ^ (uint64_t)p->kind[i];
        for (int k = 0; k < p->n[i]; k++) c = (c ^ p->c[i][k]) * 1099511628211ull;
        c ^= c >> 29;
        c *= 0xbf58476d1ce4e5b9ull;
        c ^= c >> 32;
        h += c;
    }
    for (int i = FC_FOUND; i < FC_PILES; i++) h = (h ^ p->n[i]) * 1099511628211ull;
    return h | 1;
}

const solsearch fc_search = {&fc_rules, fc_cands, NULL, key, NULL};
