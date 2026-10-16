/*
 * Baker's Dozen: the rules (bakersdozen.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/bakersdozen.h"

void bkd_new(solrec *r, uint32_t seed)
{
    uint8_t deck[2 * 52];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, seed);
    bkd_deal_cards(r, 0, deck);
    r->seed = seed;
}

void bkd_deal_cards(solrec *r, int variant, const uint8_t *deck)
{
    (void)variant;
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    p->npiles = BKD_PILES;
    for (int i = 0; i < BKD_PILES; i++) p->kind[i] = i < BKD_FOUND ? SP_TAB : SP_FOUND;
    uint8_t col[13][4];
    for (int row = 0, k = 0; row < 4; row++)    /* four rows of thirteen, the bottom one first */
        for (int c = 0; c < 13; c++) col[c][row] = deck[k++];
    for (int c = 0; c < 13; c++) {              /* the kings to the bottom, the others' order kept */
        for (int i = 0; i < 4; i++)
            if (CARD_RANK(col[c][i]) == RANK_KING) p->c[c][p->n[c]++] = col[c][i];
        for (int i = 0; i < 4; i++)
            if (CARD_RANK(col[c][i]) != RANK_KING) p->c[c][p->n[c]++] = col[c][i];
    }
    sol_begin(r);
}

int bkd_won(const solpos *p) { return sol_on_found(p) == 52; }

int bkd_legal(const solpos *p, solmove m)
{
    int a = m.from, b = m.to;
    if (SM_KIND(m.kind) != SM_MOVE || a >= BKD_FOUND || b >= BKD_PILES || a == b || m.n != 1 || !p->n[a]) return 0;
    int card = sol_top(p, a);
    if (p->kind[b] == SP_FOUND) return sol_found_takes(p, b, card);
    return p->n[b] && CARD_RANK(card) + 1 == CARD_RANK(sol_top(p, b));
}

static int found_for(const solpos *p, int card) { return sol_found_for(p, card); }

int bkd_play(solrec *r, solmove m)
{
    if (r->n >= SOL_MOVES - 60 || !bkd_legal(&r->pos, m)) return 0;
    sol_push(r, m);
    sol_finish(r, 52, found_for);               /* the rest up by itself, if it all can go */
    return 1;
}

static solmove none(void) { return sol_mv(SM_NONE, 0, 0, 0); }

static solmove stock(const solpos *p)
{
    (void)p;
    return none();
}

static int pickable(const solpos *p, int pile, int i) { return pile < BKD_FOUND && p->n[pile] && i == p->n[pile] - 1; }

/* the lowest rank a column holds (14: none): a card wanted soon buried under the card put there */
static int lowest(const solpos *p, int c)
{
    int low = 14;
    for (int k = 0; k < p->n[c]; k++)
        if (CARD_RANK(p->c[c][k]) < low) low = CARD_RANK(p->c[c][k]);
    return low;
}

/* the best column for the top card of column a: one whose top is a rank higher, burying the fewest low cards; -1 */
static int column_for(const solpos *p, int a)
{
    int best = -1, bl = -1;
    for (int b = 0; b < BKD_FOUND; b++) {
        if (b == a || !bkd_legal(p, sol_mv(SM_MOVE, a, b, 1))) continue;
        int l = lowest(p, b);
        if (l > bl) bl = l, best = b;
    }
    return best;
}

static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    if (!pickable(p, pile, i)) return none();
    int f = sol_found_for(p, p->c[pile][i]);
    if (f >= 0) return sol_mv(SM_MOVE, pile, f, 1);
    if (found_only) return none();
    int b = column_for(p, pile);
    return b >= 0 ? sol_mv(SM_MOVE, pile, b, 1) : none();
}

/* how many cards lie on the next card a foundation wants in column c (-1: it holds none) */
static int depth_wanted(const solpos *p, int c)
{
    for (int k = p->n[c] - 1; k >= 0; k--) {
        int card = p->c[c][k];
        if (CARD_RANK(card) == sol_found_rank(p, CARD_SUIT(card)) + 1) return p->n[c] - 1 - k;
    }
    return -1;
}

/* the moves worth trying, best first: a card up to its foundation; a card moved off one that can then go up; off a
 * column holding a card the foundations want soon (the fewer cards on it, the better); any other last */
int bkd_cands(const solpos *p, solmove *list)
{
    solmove m[200];
    int v[200], n = 0;
    for (int a = 0; a < BKD_FOUND; a++) {
        if (!p->n[a]) continue;
        int card = sol_top(p, a), f = sol_found_for(p, card);
        if (f >= 0) { m[n] = sol_mv(SM_MOVE, a, f, 1), v[n++] = 100; continue; }
        int score = 2, d = depth_wanted(p, a);
        if (p->n[a] > 1 && sol_found_for(p, p->c[a][p->n[a] - 2]) >= 0) score = 60;
        else if (d > 0) score = 50 - d;
        int best = column_for(p, a);
        for (int b = 0; b < BKD_FOUND && n < 200; b++)
            if (b != a && bkd_legal(p, sol_mv(SM_MOVE, a, b, 1)))
                m[n] = sol_mv(SM_MOVE, a, b, 1), v[n++] = score + (b == best);
    }
    return sol_best_first(m, v, n, list);
}

/* a useful move: a card up, else the best move freeing a card the foundations want (never a card shuffled to and
 * fro between two cards of the same rank: the hint then has nothing) */
solmove bkd_hint(const solpos *p)
{
    solmove list[SOLVE_BRANCH];
    int n = bkd_cands(p, list);
    for (int i = 0; i < n; i++) {
        int a = list[i].from;
        if (p->kind[list[i].to] == SP_FOUND) return list[i];
        if (p->n[a] < 2) continue;
        int under = p->c[a][p->n[a] - 2];
        if (sol_found_for(p, under) >= 0) return list[i];
        if (CARD_RANK(under) == CARD_RANK(sol_top(p, a)) + 1) continue;   /* it already sits on a card it may */
        if (depth_wanted(p, a) > 0) return list[i];
    }
    return none();
}

static void deal(solrec *r, uint32_t n) { bkd_new(r, n); }

const solrules bkd_rules = {bkd_legal, bkd_play, stock, bkd_hint, tap, pickable, bkd_won, deal};

/* the notation's columns: 1..9, 0, x, y, z; a foundation "h" as a target */
static int column_of(char ch)
{
    if (ch >= '1' && ch <= '9') return ch - '1';
    if (ch == '0') return 9;
    if (ch >= 'x' && ch <= 'z') return 10 + ch - 'x';
    return -1;
}

int bkd_script(solrec *r, const char *s)
{
    while (*s) {
        if (*s == ' ') { s++; continue; }
        if (*s == '#') {
            uint32_t n = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) n = n * 10 + (uint32_t)(*s - '0');
            bkd_new(r, n);
            continue;
        }
        solmove m = none();
        if (*s == '?') m = bkd_hint(&r->pos), s++;
        else {
            int a = column_of(*s);
            if (a < 0 || !s[1]) return 0;
            int b = s[1] == 'h' ? (r->pos.n[a] ? sol_found_for(&r->pos, sol_top(&r->pos, a)) : -1) : column_of(s[1]);
            if (b < 0) return 0;
            m = sol_mv(SM_MOVE, a, b, 1);
            s += 2;
        }
        if (m.kind == SM_NONE || !bkd_play(r, m)) return 0;
    }
    return 1;
}

/* how promising a position looks (the search takes the best first): the cards up, less the cards lying on the ones
 * the foundations want next, and on the low cards */
static int eval(const solpos *p)
{
    int v = 100 * sol_on_found(p);
    for (int c = 0; c < BKD_FOUND; c++)
        for (int k = 0; k < p->n[c]; k++) {
            int card = p->c[c][k], need = CARD_RANK(card) - sol_found_rank(p, CARD_SUIT(card));
            if (need <= 3) v -= (4 - need) * 6 * (p->n[c] - 1 - k);
        }
    return v;
}

const solsearch bkd_search = {&bkd_rules, bkd_cands, NULL, NULL, eval};
