/*
 * Clock: the rules (clock.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/clock.h"

void ck_new(solrec *r, uint32_t seed)
{
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    r->seed = seed;
    p->npiles = CK_PILES;
    for (int i = 0; i < CK_PILES; i++) p->kind[i] = i < CK_DOWN ? SP_FOUND : SP_STOCK;
    uint8_t deck[CARDS_MAX];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, seed);
    for (int k = 0; k < n; k++) p->c[CK_DOWN + k % 13][p->n[CK_DOWN + k % 13]++] = deck[k];
    for (int i = CK_DOWN; i < CK_PILES; i++) p->down[i] = p->n[i];
    sol_begin(r);
}

/* the position says it: each card turned sends the next turn to its rank's pile, so a pile has been turned from as
 * often as cards of its rank have come up, but the one whose turn it is (the kings' pile: once more, the first) */
int ck_current(const solpos *p)
{
    for (int r = 1; r <= 13; r++) {
        int up = p->n[CK_UP + r - 1], taken = 4 - p->n[CK_DOWN + r - 1];
        if (up + (r == 13) - taken == 1) return r;
    }
    return 13;
}

int ck_over(const solpos *p) { return !p->n[CK_DOWN + ck_current(p) - 1]; }

int ck_won(const solpos *p)
{
    for (int i = CK_DOWN; i < CK_PILES; i++)
        if (p->n[i]) return 0;
    return 1;
}

int ck_legal(const solpos *p, solmove m)
{
    int a = CK_DOWN + ck_current(p) - 1;
    return SM_KIND(m.kind) == SM_MOVE && m.from == a && m.n == 1 && p->n[a] &&
           m.to == CK_UP + CARD_RANK(sol_top(p, a)) - 1;
}

int ck_play(solrec *r, solmove m)
{
    if (r->n >= SOL_MOVES - 2 || !ck_legal(&r->pos, m)) return 0;
    sol_push(r, m);
    return 1;
}

solmove ck_next(const solpos *p)
{
    int a = CK_DOWN + ck_current(p) - 1;
    if (!p->n[a]) return sol_mv(SM_NONE, 0, 0, 0);
    return sol_mv(SM_MOVE, a, CK_UP + CARD_RANK(sol_top(p, a)) - 1, 1);
}

static int pickable(const solpos *p, int pile, int i)
{
    (void)p, (void)pile, (void)i;
    return 0;
}

/* any tap: the next card */
static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    (void)pile, (void)i, (void)found_only;
    return ck_next(p);
}

static void deal(solrec *r, uint32_t n) { ck_new(r, n); }

const solrules ck_rules = {ck_legal, ck_play, ck_next, ck_next, tap, pickable, ck_won, deal};

int ck_script(solrec *r, const char *s)
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
        if (*s != 's' && *s != '?') return 0;
        s++;
        solmove m = ck_next(&r->pos);
        if (m.kind == SM_NONE || !ck_play(r, m)) return 0;
    }
    return 1;
}
