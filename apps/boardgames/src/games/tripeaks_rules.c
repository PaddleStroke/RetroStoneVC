/*
 * TriPeaks: the rules (tripeaks.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/tripeaks.h"

/* the three tops (row 1), then rows 2 (two cards a peak), 3 (nine) and 4 (ten, face up) */
#define TOP(q) {0, q, {3 + 2 * (q), 4 + 2 * (q)}}
#define MID(q) {1, q, {9 + 3 * ((q) / 2) + (q) % 2, 10 + 3 * ((q) / 2) + (q) % 2}}
#define LOW(j) {2, j, {18 + (j), 19 + (j)}}
#define BASE(j) {3, j, {-1, -1}}
const solslot tp_slots[TP_SLOTS] = {
    TOP(0), TOP(1), TOP(2),
    MID(0), MID(1), MID(2), MID(3), MID(4), MID(5),
    LOW(0), LOW(1), LOW(2), LOW(3), LOW(4), LOW(5), LOW(6), LOW(7), LOW(8),
    BASE(0), BASE(1), BASE(2), BASE(3), BASE(4), BASE(5), BASE(6), BASE(7), BASE(8), BASE(9),
};
const float tp_slot_x[TP_SLOTS] = {
    1.5f, 4.5f, 7.5f,
    1, 2, 4, 5, 7, 8,
    0.5f, 1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
};

void tp_new(solrec *r, int variant, uint32_t seed)
{
    uint8_t deck[2 * 52];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, seed);
    tp_deal_cards(r, variant, deck);
    r->seed = seed;
}

void tp_deal_cards(solrec *r, int variant, const uint8_t *deck)
{
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    r->variant = p->variant = (uint8_t)(variant == TP_NOWRAP ? TP_NOWRAP : TP_WRAP);
    p->draw = 1;
    p->npiles = TP_PILES;
    for (int i = 0; i < TP_PILES; i++) p->kind[i] = i < TP_STOCK ? SP_TAB : i == TP_STOCK ? SP_STOCK : SP_FOUND;
    int n = 52, k = 0;
    for (int i = 0; i < TP_SLOTS; i++) {
        p->c[TP_SLOT + i][p->n[TP_SLOT + i]++] = deck[k++];
        p->down[TP_SLOT + i] = tp_slots[i].row < 3;
    }
    p->c[TP_WASTE][p->n[TP_WASTE]++] = deck[k++];
    while (k < n) p->c[TP_STOCK][p->n[TP_STOCK]++] = deck[k++];
    p->down[TP_STOCK] = p->n[TP_STOCK];
    sol_begin(r);
}

static int ranks_follow(const solpos *p, int a, int b)   /* one apart; wrap: a king and an ace too */
{
    return a == b + 1 || b == a + 1 ||
           (p->variant == TP_WRAP && ((a == RANK_ACE && b == RANK_KING) || (a == RANK_KING && b == RANK_ACE)));
}

int tp_follows(const solpos *p, int card, int on)
{
    return on == CARD_NONE || ranks_follow(p, CARD_RANK(card), CARD_RANK(on));
}

static int playable(const solpos *p, int pile)   /* a free card of the peaks, face up */
{
    return pile >= TP_SLOT && pile < TP_SLOT + TP_SLOTS && sol_slot_free(p, tp_slots, TP_SLOT, pile - TP_SLOT) &&
           !p->down[pile];
}

int tp_legal(const solpos *p, solmove m)
{
    switch (SM_KIND(m.kind)) {
    case SM_DRAW:
        return m.from == TP_STOCK && m.to == TP_WASTE && m.n == 1 && p->n[TP_STOCK];
    case SM_MOVE:
        return m.to == TP_WASTE && m.n == 1 && m.from < TP_PILES && playable(p, m.from) &&
               tp_follows(p, sol_top(p, m.from), sol_top(p, TP_WASTE));
    default:
        return 0;
    }
}

int tp_won(const solpos *p) { return sol_slots_left(p, TP_SLOT, TP_SLOTS) == 0; }

int tp_play(solrec *r, solmove m)
{
    solpos *p = &r->pos;
    if (r->n >= SOL_MOVES - 8 || !tp_legal(p, m)) return 0;
    sol_push(r, m);
    for (int i = 0; i < TP_SLOTS; i++)          /* the cards it uncovered turn up */
        if (p->n[TP_SLOT + i] && p->down[TP_SLOT + i] && sol_slot_free(p, tp_slots, TP_SLOT, i))
            sol_push(r, sol_mv(SM_TURN | SM_AUTO, TP_SLOT + i, TP_SLOT + i, 0));
    return 1;
}

static solmove none(void) { return sol_mv(SM_NONE, 0, 0, 0); }

static solmove stock(const solpos *p)
{
    return p->n[TP_STOCK] ? sol_mv(SM_DRAW, TP_STOCK, TP_WASTE, 1) : none();
}

static int pickable(const solpos *p, int pile, int i) { return i >= 0 && i == p->n[pile] - 1 && playable(p, pile); }

static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    (void)found_only;
    solmove m = sol_mv(SM_MOVE, pile, TP_WASTE, 1);
    return pickable(p, pile, i) && tp_legal(p, m) ? m : none();
}

/* ---- the hint: the card that starts the longest run of cards from the peaks, looking ahead only at the cards face
 * up now (the hint does not peek under the backs); on a tie, the one that turns up more cards ---------------------- */
typedef struct { const solpos *p; uint8_t rank[TP_SLOTS]; uint32_t up; int nodes; } search;

static int free_in(uint32_t left, int i)
{
    for (int k = 0; k < 2; k++)
        if (tp_slots[i].cover[k] >= 0 && (left >> tp_slots[i].cover[k] & 1)) return 0;
    return 1;
}

/* the best (cards played x 64 + cards uncovered) from here */
static int longest(search *s, uint32_t left, int on, int uncovered)
{
    int best = uncovered;
    if (++s->nodes > 60000) return best;
    for (int i = 0; i < TP_SLOTS; i++) {
        if (!(left >> i & 1) || !(s->up >> i & 1) || !free_in(left, i) || !ranks_follow(s->p, s->rank[i], on))
            continue;
        uint32_t next = left & ~(1u << i);
        int more = 0;
        for (int j = 0; j < TP_SLOTS; j++)
            if ((next >> j & 1) && !(s->up >> j & 1) && free_in(next, j) && !free_in(left, j)) more++;
        int v = 64 + longest(s, next, s->rank[i], uncovered + more);
        if (v > best) best = v;
    }
    return best;
}

solmove tp_hint(const solpos *p)
{
    search s = {p, {0}, 0, 0};
    uint32_t left = 0;
    for (int i = 0; i < TP_SLOTS; i++) {
        if (!p->n[TP_SLOT + i]) continue;
        left |= 1u << i;
        s.rank[i] = (uint8_t)CARD_RANK(sol_top(p, TP_SLOT + i));
        if (!p->down[TP_SLOT + i]) s.up |= 1u << i;
    }
    int best = -1, pick = -1;
    for (int i = 0; i < TP_SLOTS; i++) {
        if (!tp_legal(p, sol_mv(SM_MOVE, TP_SLOT + i, TP_WASTE, 1))) continue;
        s.nodes = 0;
        uint32_t next = left & ~(1u << i);
        int more = 0;
        for (int j = 0; j < TP_SLOTS; j++)
            if ((next >> j & 1) && !(s.up >> j & 1) && free_in(next, j) && !free_in(left, j)) more++;
        int v = longest(&s, next, s.rank[i], more);
        if (v > best) best = v, pick = i;
    }
    if (pick >= 0) return sol_mv(SM_MOVE, TP_SLOT + pick, TP_WASTE, 1);
    return stock(p);
}

int tp_stuck(const solpos *p)
{
    if (tp_won(p) || p->n[TP_STOCK]) return 0;
    for (int a = TP_SLOT; a < TP_SLOT + TP_SLOTS; a++)
        if (tp_legal(p, sol_mv(SM_MOVE, a, TP_WASTE, 1))) return 0;
    return 1;
}

int tp_streak(const solrec *r)
{
    int k = 0;
    for (int i = r->n - 1; i >= 0; i--) {
        solmove m = r->mv[i];
        if (m.kind & SM_AUTO) continue;
        if (SM_KIND(m.kind) != SM_MOVE) break;
        k++;
    }
    return k;
}

int tp_best_streak(const solrec *r)
{
    int k = 0, best = 0;
    for (int i = 0; i < r->n; i++) {
        solmove m = r->mv[i];
        if (m.kind & SM_AUTO) continue;
        k = SM_KIND(m.kind) == SM_MOVE ? k + 1 : 0;
        if (k > best) best = k;
    }
    return best;
}

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static void deal(solrec *r, uint32_t n) { tp_new(r, r->variant, n); }

const solrules tp_rules = {tp_legal, tp_play, stock, tp_hint, tap, pickable, tp_won, deal};

int tp_script(solrec *r, const char *s)
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
        if (*s == 's' || *s == '?') {
            m = *s++ == 's' ? stock(&r->pos) : tp_hint(&r->pos);
        } else {
            int i = sol_slot_read(tp_slots, TP_SLOTS, s);
            if (i < 0) return 0;
            s += 2;
            m = sol_mv(SM_MOVE, TP_SLOT + i, TP_WASTE, 1);
        }
        if (m.kind == SM_NONE || !tp_play(r, m)) return 0;
    }
    return 1;
}

/* ---- the search (sollevels.h): the moves worth trying, best first ----------------------------------------------- */
/* the cards that uncover a face-down one first, the lower rows first; the stock last (what a player sees: the
 * face-down cards are not looked at) */
int tp_cands(const solpos *p, solmove *list)
{
    solmove m[TP_SLOTS + 1];
    int v[TP_SLOTS + 1], n = 0;
    for (int i = 0; i < TP_SLOTS; i++) {
        solmove mv = sol_mv(SM_MOVE, TP_SLOT + i, TP_WASTE, 1);
        if (!tp_legal(p, mv)) continue;
        solpos q = *p;
        q.n[TP_SLOT + i] = 0;
        int opens = 0;
        for (int j = 0; j < TP_SLOTS; j++)
            opens += p->n[TP_SLOT + j] && p->down[TP_SLOT + j] && sol_slot_free(&q, tp_slots, TP_SLOT, j);
        m[n] = mv, v[n++] = 50 + 30 * opens + 5 * tp_slots[i].row;
    }
    solmove s = stock(p);
    if (s.kind != SM_NONE) m[n] = s, v[n++] = 1;
    return sol_best_first(m, v, n, list);
}

const solsearch tp_search = {&tp_rules, tp_cands, NULL, NULL, NULL};
