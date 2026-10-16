/*
 * Scorpion: the rules (scorpion.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/scorpion.h"

void scp_new(solrec *r, uint32_t seed)
{
    uint8_t deck[2 * 52];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, seed);
    scp_deal_cards(r, 0, deck);
    r->seed = seed;
}

void scp_deal_cards(solrec *r, int variant, const uint8_t *deck)
{
    (void)variant;
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    p->npiles = SCP_PILES;
    for (int i = 0; i < SCP_PILES; i++) p->kind[i] = i < SCP_FOUND ? SP_TAB : i < SCP_STOCK ? SP_FOUND : SP_STOCK;
    int k = 0;
    for (int row = 0; row < 7; row++)           /* seven rows of seven, the bottom one first */
        for (int col = 0; col < 7; col++) p->c[col][p->n[col]++] = deck[k++];
    for (int col = 0; col < 4; col++) p->down[col] = 3;
    while (k < 52) p->c[SCP_STOCK][p->n[SCP_STOCK]++] = deck[k++];
    p->down[SCP_STOCK] = p->n[SCP_STOCK];
    sol_begin(r);
}

int scp_won(const solpos *p) { return sol_on_found(p) == 52; }

/* the cards from i of column c: a whole suit from the king down to the ace, face up */
static int whole_suit(const solpos *p, int c, int i)
{
    return p->n[c] - i == 13 && i >= p->down[c] && CARD_RANK(p->c[c][i]) == RANK_KING && sol_run(p, c, i, 1);
}

int scp_legal(const solpos *p, solmove m)
{
    int a = m.from, b = m.to, n = m.n;
    if (SM_KIND(m.kind) == SM_DEAL) return a == SCP_STOCK && b == 0 && n == 3 && p->n[SCP_STOCK] == 3;
    if (SM_KIND(m.kind) != SM_MOVE || a >= SCP_FOUND || b >= SCP_STOCK || a == b || n < 1 || n > sol_up(p, a))
        return 0;
    int card = p->c[a][p->n[a] - n];
    if (p->kind[b] == SP_FOUND) return !p->n[b] && whole_suit(p, a, p->n[a] - n);
    if (!p->n[b]) return CARD_RANK(card) == RANK_KING;
    int on = sol_top(p, b);
    return CARD_SUIT(on) == CARD_SUIT(card) && CARD_RANK(card) + 1 == CARD_RANK(on);
}

/* a whole suit on top of a column: off to a foundation, by itself */
static void collect(solrec *r)
{
    solpos *p = &r->pos;
    for (int c = 0; c < 7; c++) {
        if (p->n[c] < 13 || !whole_suit(p, c, p->n[c] - 13)) continue;
        for (int f = SCP_FOUND; f < SCP_STOCK; f++)
            if (!p->n[f]) {
                sol_push(r, sol_mv(SM_MOVE | SM_AUTO, c, f, 13));
                break;
            }
    }
}

int scp_play(solrec *r, solmove m)
{
    if (r->n >= SOL_MOVES - 20 || !scp_legal(&r->pos, m)) return 0;
    sol_push(r, m);
    collect(r);
    return 1;
}

static solmove none(void) { return sol_mv(SM_NONE, 0, 0, 0); }

static solmove stock(const solpos *p)
{
    solmove m = sol_mv(SM_DEAL, SCP_STOCK, 0, 3);
    return scp_legal(p, m) ? m : none();
}

static int pickable(const solpos *p, int pile, int i)
{
    return pile < SCP_FOUND && i >= p->down[pile] && i < p->n[pile];
}

/* where the cards from i of column a can go: the column whose top card follows the card (there is only one), or an
 * empty one for a king (not a whole column already); -1: none */
static int column_for(const solpos *p, int a, int i)
{
    int n = p->n[a] - i;
    for (int b = 0; b < 7; b++) {
        if (b == a || (!p->n[b] && i == 0)) continue;
        if (scp_legal(p, sol_mv(SM_MOVE, a, b, n))) return b;
    }
    return -1;
}

static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    if (found_only || !pickable(p, pile, i)) return none();
    int b = column_for(p, pile, i);
    return b >= 0 ? sol_mv(SM_MOVE, pile, b, p->n[pile] - i) : none();
}

/* how good moving the cards from i of column a looks: a face-down card uncovered (the more under it, the better), a
 * column emptied, a king under other cards moved to an empty column, any other move (the card never sat on the one
 * it follows: that one is a column's top); 0: not worth it (a king already at the bottom of its column) */
static int worth(const solpos *p, int a, int i)
{
    int card = p->c[a][i];
    if (i == p->down[a] && i > 0) return 60 + p->down[a];
    if (i == 0) return CARD_RANK(card) == RANK_KING ? 0 : 45;
    if (CARD_RANK(card) == RANK_KING) return 30;
    int under = p->c[a][i - 1];                 /* the card left on top: does the one that follows it come free? */
    for (int c = 0; c < 7; c++)
        for (int k = p->down[c]; k < p->n[c]; k++)
            if (CARD_SUIT(p->c[c][k]) == CARD_SUIT(under) && CARD_RANK(p->c[c][k]) + 1 == CARD_RANK(under) &&
                !(c == a && k >= i))
                return 25;
    return 20;
}

/* the moves worth trying, best first (the reserve's deal last) */
int scp_cands(const solpos *p, solmove *list)
{
    solmove m[200];
    int v[200], n = 0, empty = -1;
    for (int b = 0; b < 7; b++)
        if (!p->n[b] && empty < 0) empty = b;
    for (int a = 0; a < 7; a++)
        for (int i = p->down[a]; i < p->n[a] && n < 190; i++) {
            int b = column_for(p, a, i), score = b >= 0 ? worth(p, a, i) : 0;
            if (score > 0 && (p->n[b] || b == empty)) m[n] = sol_mv(SM_MOVE, a, b, p->n[a] - i), v[n++] = score;
        }
    solmove s = stock(p);
    if (s.kind != SM_NONE) m[n] = s, v[n++] = 8;
    return sol_best_first(m, v, n, list);
}

/* a useful move: the best of the candidates; the reserve's deal once nothing else is left */
solmove scp_hint(const solpos *p)
{
    solmove list[SOLVE_BRANCH];
    return scp_cands(p, list) ? list[0] : none();
}

static void deal(solrec *r, uint32_t n) { scp_new(r, n); }

const solrules scp_rules = {scp_legal, scp_play, stock, scp_hint, tap, pickable, scp_won, deal};

int scp_script(solrec *r, const char *moves) { return sol_script(r, moves, &scp_rules); }

const solsearch scp_search = {&scp_rules, scp_cands, NULL, NULL, NULL};
