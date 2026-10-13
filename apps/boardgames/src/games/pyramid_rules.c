/*
 * Pyramid: the rules (pyramid.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/pyramid.h"

#define AT(r, c) ((r) * ((r) + 1) / 2 + (c))
#define SLOT(r, c) {r, c, {(r) < 6 ? AT((r) + 1, c) : -1, (r) < 6 ? AT((r) + 1, (c) + 1) : -1}}
const solslot py_slots[PY_SLOTS] = {
    SLOT(0, 0),
    SLOT(1, 0), SLOT(1, 1),
    SLOT(2, 0), SLOT(2, 1), SLOT(2, 2),
    SLOT(3, 0), SLOT(3, 1), SLOT(3, 2), SLOT(3, 3),
    SLOT(4, 0), SLOT(4, 1), SLOT(4, 2), SLOT(4, 3), SLOT(4, 4),
    SLOT(5, 0), SLOT(5, 1), SLOT(5, 2), SLOT(5, 3), SLOT(5, 4), SLOT(5, 5),
    SLOT(6, 0), SLOT(6, 1), SLOT(6, 2), SLOT(6, 3), SLOT(6, 4), SLOT(6, 5), SLOT(6, 6),
};

void py_new(solrec *r, int variant, uint32_t seed)
{
    uint8_t deck[2 * 52];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, seed);
    py_deal_cards(r, variant, deck);
    r->seed = seed;
}

void py_deal_cards(solrec *r, int variant, const uint8_t *deck)
{
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    r->variant = p->variant = (uint8_t)(variant == PY_THRICE ? PY_THRICE : PY_ONCE);
    p->draw = 1;
    p->npiles = PY_PILES;
    for (int i = 0; i < PY_PILES; i++)
        p->kind[i] = i < PY_STOCK ? SP_TAB : i == PY_STOCK ? SP_STOCK : i == PY_TURN ? SP_CELL :
                     i == PY_WASTE ? SP_WASTE : SP_FOUND;
    int n = 52, k = 0;
    for (int i = 0; i < PY_SLOTS; i++) p->c[PY_SLOT + i][p->n[PY_SLOT + i]++] = deck[k++];   /* row by row */
    while (k < n) p->c[PY_STOCK][p->n[PY_STOCK]++] = deck[k++];
    p->down[PY_STOCK] = p->n[PY_STOCK];
    sol_begin(r);
}

int py_free(const solpos *p, int pile)
{
    if (pile >= PY_SLOT && pile < PY_SLOT + PY_SLOTS) return sol_slot_free(p, py_slots, PY_SLOT, pile - PY_SLOT);
    return (pile == PY_TURN || pile == PY_WASTE) && p->n[pile];
}

int py_passes(const solpos *p) { return (p->variant == PY_THRICE ? 3 : 1) - p->redeals; }

int py_won(const solpos *p) { return sol_slots_left(p, PY_SLOT, PY_SLOTS) == 0; }

static int rank_of(const solpos *p, int pile) { return CARD_RANK(sol_top(p, pile)); }

int py_legal(const solpos *p, solmove m)
{
    int a = m.from, b = m.to;
    switch (SM_KIND(m.kind)) {
    case SM_DRAW:
        return a == PY_STOCK && b == PY_TURN && m.n == 1 && p->n[PY_STOCK];
    case SM_REDEAL:
        return a == PY_WASTE && b == PY_STOCK && !p->n[PY_STOCK] && py_passes(p) > 1 &&
               m.n == p->n[PY_WASTE] + p->n[PY_TURN] && m.n > 0;
    case SM_MOVE:
        if (a >= PY_PILES || b >= PY_PILES || a == b || m.n != 1 || !py_free(p, a)) return 0;
        if (b == PY_OUT) return rank_of(p, a) == RANK_KING;          /* a king alone */
        return py_free(p, b) && rank_of(p, a) + rank_of(p, b) == 13;  /* a pair */
    default:
        return 0;
    }
}

int py_play(solrec *r, solmove m)
{
    solpos *p = &r->pos;
    if (r->n >= SOL_MOVES - 4 || !py_legal(p, m)) return 0;
    switch (SM_KIND(m.kind)) {
    case SM_MOVE:
        sol_push(r, sol_mv(SM_MOVE, m.from, PY_OUT, 1));
        if (m.to != PY_OUT) sol_push(r, sol_mv(SM_MOVE | SM_AUTO, m.to, PY_OUT, 1));
        break;
    case SM_DRAW:
    case SM_REDEAL: {
        int auto_ = 0;
        if (p->n[PY_TURN]) {                    /* the turned card goes on the waste first */
            sol_push(r, sol_mv(SM_MOVE, PY_TURN, PY_WASTE, 1));
            auto_ = SM_AUTO;
        }
        if (SM_KIND(m.kind) == SM_DRAW) sol_push(r, sol_mv(SM_DRAW | auto_, PY_STOCK, PY_TURN, 1));
        else sol_push(r, sol_mv(SM_REDEAL | auto_, PY_WASTE, PY_STOCK, p->n[PY_WASTE]));
        break;
    }
    }
    return 1;
}

static solmove none(void) { return sol_mv(SM_NONE, 0, 0, 0); }

static solmove stock(const solpos *p)
{
    if (p->n[PY_STOCK]) return sol_mv(SM_DRAW, PY_STOCK, PY_TURN, 1);
    solmove m = sol_mv(SM_REDEAL, PY_WASTE, PY_STOCK, p->n[PY_WASTE] + p->n[PY_TURN]);
    return py_legal(p, m) ? m : none();
}

static int pickable(const solpos *p, int pile, int i) { return i >= 0 && i == p->n[pile] - 1 && py_free(p, pile); }

/* a tap: a king goes off at once; any other card waits for its pair (solitaire_ui.h's pairs) */
static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    (void)found_only;
    solmove m = sol_mv(SM_MOVE, pile, PY_OUT, 1);
    return pickable(p, pile, i) && py_legal(p, m) ? m : none();
}

/* a move: the pyramid's kings, its pairs, a card of it with the turned card or the waste's, then those two alone,
 * then the stock */
solmove py_hint(const solpos *p)
{
    static const int extra[2] = {PY_TURN, PY_WASTE};
    for (int a = PY_SLOT; a < PY_SLOT + PY_SLOTS; a++)
        if (py_legal(p, sol_mv(SM_MOVE, a, PY_OUT, 1))) return sol_mv(SM_MOVE, a, PY_OUT, 1);
    for (int a = PY_SLOT; a < PY_SLOT + PY_SLOTS; a++)
        for (int b = a + 1; b < PY_SLOT + PY_SLOTS; b++)
            if (py_legal(p, sol_mv(SM_MOVE, a, b, 1))) return sol_mv(SM_MOVE, a, b, 1);
    for (int a = PY_SLOT; a < PY_SLOT + PY_SLOTS; a++)
        for (int k = 0; k < 2; k++)
            if (py_legal(p, sol_mv(SM_MOVE, a, extra[k], 1))) return sol_mv(SM_MOVE, a, extra[k], 1);
    for (int k = 0; k < 2; k++)
        if (py_legal(p, sol_mv(SM_MOVE, extra[k], PY_OUT, 1))) return sol_mv(SM_MOVE, extra[k], PY_OUT, 1);
    if (py_legal(p, sol_mv(SM_MOVE, PY_TURN, PY_WASTE, 1))) return sol_mv(SM_MOVE, PY_TURN, PY_WASTE, 1);
    return stock(p);
}

int py_stuck(const solpos *p) { return !py_won(p) && py_hint(p).kind == SM_NONE; }

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static void name(int pile, char *out)
{
    if (pile == PY_TURN || pile == PY_WASTE) out[0] = pile == PY_TURN ? 't' : 'w', out[1] = 0;
    else sol_slot_write(py_slots, pile - PY_SLOT, out);
}

void py_write(const solpos *p, solmove m, char *out)
{
    (void)p;
    if (SM_KIND(m.kind) != SM_MOVE) { strcpy(out, "s"); return; }
    name(m.from, out);
    if (m.to == PY_OUT) return;
    size_t k = strlen(out);
    out[k++] = '+';
    name(m.to, out + k);
}

static int read_card(const char *s, int *pile)  /* the characters read (0: none) */
{
    if (*s == 't' || *s == 'w') { *pile = *s == 't' ? PY_TURN : PY_WASTE; return 1; }
    int i = sol_slot_read(py_slots, PY_SLOTS, s);
    if (i < 0) return 0;
    *pile = PY_SLOT + i;
    return 2;
}

static void deal(solrec *r, uint32_t n) { py_new(r, r->variant, n); }

const solrules py_rules = {py_legal, py_play, stock, py_hint, tap, pickable, py_won, deal};

int py_script(solrec *r, const char *s)
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
            m = *s++ == 's' ? stock(&r->pos) : py_hint(&r->pos);
        } else {
            int a, b = PY_OUT, k = read_card(s, &a);
            if (!k) return 0;
            s += k;
            if (*s == '+') {
                if (!(k = read_card(s + 1, &b))) return 0;
                s += 1 + k;
            }
            m = sol_mv(SM_MOVE, a, b, 1);
        }
        if (m.kind == SM_NONE || !py_play(r, m)) return 0;
    }
    return 1;
}

/* ---- the search (sollevels.h): the moves worth trying, best first ----------------------------------------------- */
/* the pyramid's kings; its pairs, the lower rows first; a card of it with the turned card or the waste's; those two
 * alone or together; the stock last */
int py_cands(const solpos *p, solmove *list)
{
    static const int extra[2] = {PY_TURN, PY_WASTE};
    solmove m[400];
    int v[400], n = 0;
    for (int a = PY_SLOT; a < PY_SLOT + PY_SLOTS; a++) {
        int ra = py_slots[a - PY_SLOT].row;
        if (py_legal(p, sol_mv(SM_MOVE, a, PY_OUT, 1))) m[n] = sol_mv(SM_MOVE, a, PY_OUT, 1), v[n++] = 100 + ra;
        for (int b = a + 1; b < PY_SLOT + PY_SLOTS; b++)
            if (py_legal(p, sol_mv(SM_MOVE, a, b, 1)))
                m[n] = sol_mv(SM_MOVE, a, b, 1), v[n++] = 80 + ra + py_slots[b - PY_SLOT].row;
        for (int k = 0; k < 2; k++)
            if (py_legal(p, sol_mv(SM_MOVE, a, extra[k], 1))) m[n] = sol_mv(SM_MOVE, a, extra[k], 1), v[n++] = 60 + ra;
    }
    for (int k = 0; k < 2; k++)
        if (py_legal(p, sol_mv(SM_MOVE, extra[k], PY_OUT, 1))) m[n] = sol_mv(SM_MOVE, extra[k], PY_OUT, 1), v[n++] = 40;
    if (py_legal(p, sol_mv(SM_MOVE, PY_TURN, PY_WASTE, 1))) m[n] = sol_mv(SM_MOVE, PY_TURN, PY_WASTE, 1), v[n++] = 30;
    solmove s = stock(p);
    if (s.kind != SM_NONE) m[n] = s, v[n++] = 1;
    return sol_best_first(m, v, n, list);
}

const solsearch py_search = {&py_rules, py_cands, NULL, NULL, NULL};
