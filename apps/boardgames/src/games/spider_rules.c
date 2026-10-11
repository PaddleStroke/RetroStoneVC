/*
 * Spider: the rules (spider.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/spider.h"

void sp_new(solrec *r, int variant, uint32_t seed)
{
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    variant = variant < 0 || variant >= SP_VARIANTS ? SP_1SUIT : variant;
    r->seed = seed;
    r->variant = p->variant = (uint8_t)variant;
    p->npiles = SPI_PILES;
    for (int i = 0; i < SPI_PILES; i++) p->kind[i] = i < SPI_FOUND ? SP_TAB : i < SPI_STOCK ? SP_FOUND : SP_STOCK;
    static const uint8_t suits[SP_VARIANTS][8] = {
        {SUIT_SPADES, SUIT_SPADES, SUIT_SPADES, SUIT_SPADES, SUIT_SPADES, SUIT_SPADES, SUIT_SPADES, SUIT_SPADES},
        {SUIT_SPADES, SUIT_HEARTS, SUIT_SPADES, SUIT_HEARTS, SUIT_SPADES, SUIT_HEARTS, SUIT_SPADES, SUIT_HEARTS},
        {SUIT_SPADES, SUIT_HEARTS, SUIT_DIAMONDS, SUIT_CLUBS, SUIT_SPADES, SUIT_HEARTS, SUIT_DIAMONDS, SUIT_CLUBS}};
    uint8_t deck[104];
    int n = 0, k = 0;
    for (int s = 0; s < 8; s++)
        for (int rank = 1; rank <= 13; rank++) deck[n++] = CARD(suits[variant][s], rank);
    cards_shuffle(deck, n, seed);
    for (int row = 0; row < 6; row++)
        for (int col = 0; col < 10; col++)
            if (row < (col < 4 ? 6 : 5)) p->c[col][p->n[col]++] = deck[k++];
    for (int col = 0; col < 10; col++) p->down[col] = (uint8_t)(p->n[col] - 1);
    while (k < n) p->c[SPI_STOCK][p->n[SPI_STOCK]++] = deck[k++];
    p->down[SPI_STOCK] = p->n[SPI_STOCK];
    sol_begin(r);
}

int sp_won(const solpos *p) { return sol_on_found(p) == 104; }

static int any_empty(const solpos *p)
{
    for (int c = 0; c < 10; c++)
        if (!p->n[c]) return 1;
    return 0;
}

int sp_legal(const solpos *p, solmove m)
{
    int a = m.from, b = m.to, n = m.n;
    if (SM_KIND(m.kind) == SM_DEAL)
        return a == SPI_STOCK && b == 0 && n == 10 && p->n[SPI_STOCK] >= 10 && !any_empty(p);
    if (SM_KIND(m.kind) != SM_MOVE || a >= SPI_FOUND || b >= SPI_STOCK || a == b || n < 1 || n > sol_up(p, a)) return 0;
    if (!sol_run(p, a, p->n[a] - n, 1)) return 0;
    int card = p->c[a][p->n[a] - n];
    if (p->kind[b] == SP_FOUND) return n == 13 && CARD_RANK(card) == RANK_KING && !p->n[b];
    return !p->n[b] || sol_fits(card, sol_top(p, b), 0);
}

/* a run of one suit from king to ace on top of a column: to a foundation, by itself */
static void collect(solrec *r)
{
    solpos *p = &r->pos;
    for (int c = 0; c < 10; c++) {
        if (sol_up(p, c) < 13 || CARD_RANK(p->c[c][p->n[c] - 13]) != RANK_KING || !sol_run(p, c, p->n[c] - 13, 1))
            continue;
        for (int f = SPI_FOUND; f < SPI_STOCK; f++)
            if (!p->n[f]) {
                sol_push(r, sol_mv(SM_MOVE | SM_AUTO, c, f, 13));
                break;
            }
    }
}

int sp_play(solrec *r, solmove m)
{
    if (r->n >= SOL_MOVES - 20 || !sp_legal(&r->pos, m)) return 0;
    sol_push(r, m);
    collect(r);
    return 1;
}

static solmove stock(const solpos *p)
{
    solmove m = sol_mv(SM_DEAL, SPI_STOCK, 0, 10);
    return sp_legal(p, m) ? m : sol_mv(SM_NONE, 0, 0, 0);
}

static int pickable(const solpos *p, int pile, int i)
{
    return pile < SPI_FOUND && sol_run(p, pile, i, 1);
}

/* where a tap sends the run from i: onto its own suit, onto any card a rank higher, to an empty column (not a whole
 * column already); from the next column round */
static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    solmove none = sol_mv(SM_NONE, 0, 0, 0);
    if (found_only || !pickable(p, pile, i)) return none;
    int n = p->n[pile] - i, card = p->c[pile][i];
    for (int pass = 0; pass < 3; pass++)
        for (int k = 1; k < 10; k++) {
            int b = (pile + k) % 10, t = sol_top(p, b);
            if (pass < 2 ? t == CARD_NONE : t != CARD_NONE || i == 0) continue;
            if (pass == 0 && CARD_SUIT(t) != CARD_SUIT(card)) continue;
            solmove m = sol_mv(SM_MOVE, pile, b, n);
            if (sp_legal(p, m)) return m;
        }
    return none;
}

/* the hint: every move of a whole run scored (onto its own suit; uncovering a face-down card; emptying a column),
 * the best one; else the stock */
solmove sp_hint(const solpos *p)
{
    solmove best = sol_mv(SM_NONE, 0, 0, 0);
    int top = 0;
    for (int a = 0; a < 10; a++) {
        if (!sol_up(p, a)) continue;
        int i = p->n[a] - 1;
        while (i > p->down[a] && sol_fits(p->c[a][i], p->c[a][i - 1], 0) &&
               CARD_SUIT(p->c[a][i]) == CARD_SUIT(p->c[a][i - 1]))
            i--;
        int card = p->c[a][i], under = i > 0 ? p->c[a][i - 1] : CARD_NONE;
        int sits = i > p->down[a] && sol_fits(card, under, 0);   /* already on a card a rank higher */
        for (int b = 0; b < 10; b++) {
            solmove m = sol_mv(SM_MOVE, a, b, p->n[a] - i);
            if (b == a || !sp_legal(p, m)) continue;
            int t = sol_top(p, b), score;
            if (t == CARD_NONE) score = i > 0 && !sits ? 15 + 40 * (i == p->down[a]) : 0;
            else if (CARD_SUIT(t) == CARD_SUIT(card)) score = 100 + p->n[a] - i;
            else if (sits) score = 0;
            else score = i == p->down[a] && i > 0 ? 60 : i == 0 ? 30 : 20;
            if (score > top) top = score, best = m;
        }
    }
    return top ? best : stock(p);
}

static void deal(solrec *r, uint32_t n) { sp_new(r, r->variant, n); }

const solrules sp_rules = {sp_legal, sp_play, stock, sp_hint, tap, pickable, sp_won, deal};

int sp_script(solrec *r, const char *moves) { return sol_script(r, moves, &sp_rules); }
