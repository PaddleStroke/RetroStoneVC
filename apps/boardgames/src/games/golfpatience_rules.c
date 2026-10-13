/*
 * Golf, the patience: the rules (golfpatience.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/golfpatience.h"

void pg_new(solrec *r, int variant, uint32_t seed)
{
    uint8_t deck[2 * 52];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, seed);
    pg_deal_cards(r, variant, deck);
    r->seed = seed;
}

void pg_deal_cards(solrec *r, int variant, const uint8_t *deck)
{
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    r->variant = p->variant = (uint8_t)(variant == PG_WRAP ? PG_WRAP : PG_NOWRAP);
    p->draw = 1;
    p->npiles = PG_PILES;
    for (int i = 0; i < PG_PILES; i++) p->kind[i] = i < PG_STOCK ? SP_TAB : i == PG_STOCK ? SP_STOCK : SP_FOUND;
    int n = 52, k = 0;
    for (int row = 0; row < 5; row++)
        for (int col = 0; col < 7; col++) p->c[col][p->n[col]++] = deck[k++];
    p->c[PG_WASTE][p->n[PG_WASTE]++] = deck[k++];
    while (k < n) p->c[PG_STOCK][p->n[PG_STOCK]++] = deck[k++];
    p->down[PG_STOCK] = p->n[PG_STOCK];
    sol_begin(r);
}

static int ranks_follow(int variant, int a, int b)
{
    return a == b + 1 || b == a + 1 ||
           (variant == PG_WRAP && ((a == RANK_ACE && b == RANK_KING) || (a == RANK_KING && b == RANK_ACE)));
}

int pg_follows(const solpos *p, int card, int on)
{
    return on == CARD_NONE || ranks_follow(p->variant, CARD_RANK(card), CARD_RANK(on));
}

int pg_legal(const solpos *p, solmove m)
{
    switch (SM_KIND(m.kind)) {
    case SM_DRAW:
        return m.from == PG_STOCK && m.to == PG_WASTE && m.n == 1 && p->n[PG_STOCK];
    case SM_MOVE:
        return m.from < PG_STOCK && m.to == PG_WASTE && m.n == 1 && p->n[m.from] &&
               pg_follows(p, sol_top(p, m.from), sol_top(p, PG_WASTE));
    default:
        return 0;
    }
}

int pg_won(const solpos *p)
{
    for (int c = 0; c < 7; c++)
        if (p->n[c]) return 0;
    return 1;
}

int pg_play(solrec *r, solmove m)
{
    if (r->n >= SOL_MOVES - 4 || !pg_legal(&r->pos, m)) return 0;
    sol_push(r, m);
    return 1;
}

int pg_stuck(const solpos *p)
{
    if (pg_won(p) || p->n[PG_STOCK]) return 0;
    for (int c = 0; c < 7; c++)
        if (pg_legal(p, sol_mv(SM_MOVE, c, PG_WASTE, 1))) return 0;
    return 1;
}

int pg_score(const solpos *p)
{
    int k = 0;
    for (int c = 0; c < 7; c++) k += p->n[c];
    return k ? k : -p->n[PG_STOCK];
}

static solmove none(void) { return sol_mv(SM_NONE, 0, 0, 0); }

static solmove stock(const solpos *p) { return p->n[PG_STOCK] ? sol_mv(SM_DRAW, PG_STOCK, PG_WASTE, 1) : none(); }

static int pickable(const solpos *p, int pile, int i) { return pile < PG_STOCK && i >= 0 && i == p->n[pile] - 1; }

static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    (void)found_only;
    solmove m = sol_mv(SM_MOVE, pile, PG_WASTE, 1);
    return pickable(p, pile, i) && pg_legal(p, m) ? m : none();
}

/* ---- the hint: the column whose card starts the longest run of cards to the waste (every card is face up, so the
 * look ahead sees them all; it stops at 150000 positions a card) ------------------------------------------ */
typedef struct { const solpos *p; uint8_t h[7]; int nodes; } search;

static int longest(search *s, int on)
{
    int best = 0;
    if (++s->nodes > 150000) return 0;
    for (int c = 0; c < 7; c++) {
        if (!s->h[c]) continue;
        int card = s->p->c[c][s->h[c] - 1];
        if (!ranks_follow(s->p->variant, CARD_RANK(card), on)) continue;
        s->h[c]--;
        int v = 1 + longest(s, CARD_RANK(card));
        s->h[c]++;
        if (v > best) best = v;
        if (best == 35) break;
    }
    return best;
}

solmove pg_hint(const solpos *p)
{
    search s = {p, {0}, 0};
    for (int c = 0; c < 7; c++) s.h[c] = p->n[c];
    int best = -1, pick = -1;
    for (int c = 0; c < 7; c++) {
        if (!pg_legal(p, sol_mv(SM_MOVE, c, PG_WASTE, 1))) continue;
        s.h[c]--;
        s.nodes = 0;
        int v = longest(&s, CARD_RANK(p->c[c][p->n[c] - 1]));
        s.h[c]++;
        if (v > best) best = v, pick = c;
    }
    if (pick >= 0) return sol_mv(SM_MOVE, pick, PG_WASTE, 1);
    return stock(p);
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static void deal(solrec *r, uint32_t n) { pg_new(r, r->variant, n); }

const solrules pg_rules = {pg_legal, pg_play, stock, pg_hint, tap, pickable, pg_won, deal};

int pg_script(solrec *r, const char *s)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == '#') {
            uint32_t n = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) n = n * 10 + (uint32_t)(*s - '0');
            deal(r, n);
            continue;
        }
        solmove m;
        if (*s == 's' || *s == '?') m = *s == 's' ? stock(&r->pos) : pg_hint(&r->pos);
        else if (*s >= '1' && *s <= '7') m = sol_mv(SM_MOVE, *s - '1', PG_WASTE, 1);
        else return 0;
        s++;
        if (m.kind == SM_NONE || !pg_play(r, m)) return 0;
    }
    return 1;
}

/* ---- the search (sollevels.h): the moves worth trying, best first ----------------------------------------------- */
/* a column's card to the waste, the longest column first; the stock last */
int pg_cands(const solpos *p, solmove *list)
{
    solmove m[8];
    int v[8], n = 0;
    for (int c = 0; c < 7; c++)
        if (pg_legal(p, sol_mv(SM_MOVE, c, PG_WASTE, 1))) m[n] = sol_mv(SM_MOVE, c, PG_WASTE, 1), v[n++] = 50 + p->n[c];
    solmove s = stock(p);
    if (s.kind != SM_NONE) m[n] = s, v[n++] = 1;
    return sol_best_first(m, v, n, list);
}

const solsearch pg_search = {&pg_rules, pg_cands, NULL, NULL, NULL};
