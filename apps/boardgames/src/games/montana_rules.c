/*
 * Montana: the rules (montana.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/montana.h"

void mt_new(solrec *r, uint32_t seed)
{
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    r->seed = seed;
    p->npiles = MT_PILES;
    for (int i = 0; i < MT_PILES; i++) p->kind[i] = i < MT_HAND ? SP_FOUND : SP_STOCK;
    uint8_t deck[CARDS_MAX];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, seed);
    for (int k = 0; k < n; k++)                 /* the aces out: the gaps */
        if (CARD_RANK(deck[k]) != RANK_ACE) p->c[k][p->n[k]++] = deck[k];
    sol_begin(r);
}

int mt_run(const solpos *p, int row)
{
    int k = 0, first = p->n[13 * row] ? p->c[13 * row][0] : CARD_NONE;
    if (first == CARD_NONE || CARD_RANK(first) != 2) return 0;
    while (k < 12 && p->n[13 * row + k] && p->c[13 * row + k][0] == CARD(CARD_SUIT(first), k + 2)) k++;
    return k;
}

int mt_won(const solpos *p)
{
    for (int r = 0; r < 4; r++)
        if (mt_run(p, r) < 12) return 0;
    return 1;
}

int mt_redeals_left(const solpos *p) { return p->redeals < MT_REDEALS ? MT_REDEALS - p->redeals : 0; }

int mt_card_for(const solpos *p, int gap)
{
    if (gap < 0 || gap >= MT_HAND || p->n[gap]) return CARD_NONE;
    if (gap % 13 == 0) return CARD(SUIT_SPADES, 2);
    int left = sol_top(p, gap - 1);
    return left == CARD_NONE || CARD_RANK(left) == RANK_KING ? CARD_NONE : left + 1;
}

int mt_legal(const solpos *p, solmove m)
{
    int a = m.from, b = m.to;
    if (SM_KIND(m.kind) == SM_REDEAL) return a == MT_HAND && b == MT_HAND && !m.n && mt_redeals_left(p) && !mt_won(p);
    if (SM_KIND(m.kind) != SM_MOVE || m.n != 1 || a >= MT_HAND || b >= MT_HAND || !p->n[a] || p->n[b]) return 0;
    int want = mt_card_for(p, b), card = sol_top(p, a);
    if (want == CARD_NONE) return 0;
    return b % 13 == 0 ? CARD_RANK(card) == 2 && a % 13 : card == want;
}

int mt_play(solrec *r, solmove m)
{
    solpos *p = &r->pos;
    if (r->n >= SOL_MOVES - 120 || !mt_legal(p, m)) return 0;
    sol_push(r, m);
    if (SM_KIND(m.kind) != SM_REDEAL) return 1;
    int run[4];                                 /* the cards out of order gathered, dealt after each row's run */
    for (int row = 0; row < 4; row++) run[row] = mt_run(p, row);
    for (int i = 0; i < MT_HAND; i++)
        if (p->n[i] && i % 13 >= run[i / 13]) sol_push(r, sol_mv(SM_MOVE | SM_AUTO, i, MT_HAND, 1));
    uint8_t to[52];
    int k = 0;
    for (int row = 0; row < 4; row++)
        for (int c = run[row] + 1; c < 13; c++) to[k++] = (uint8_t)(13 * row + c);
    sol_deal_out(r, MT_HAND, to, k, sol_reseed(r->seed, p->redeals));
    return 1;
}

static solmove none(void) { return sol_mv(SM_NONE, 0, 0, 0); }

static solmove stock(const solpos *p)
{
    solmove m = sol_mv(SM_REDEAL, MT_HAND, MT_HAND, 0);
    return mt_legal(p, m) ? m : none();
}

static int pickable(const solpos *p, int pile, int i) { return pile < MT_HAND && i == 0 && p->n[pile]; }

/* the card that goes into a gap: the one that follows the card to its left; in the first column, a 2 (one that does
 * not stand there already, the first in the rows' order) */
solmove mt_fill(const solpos *p, int gap)
{
    for (int a = 0; a < MT_HAND; a++)
        if (mt_legal(p, sol_mv(SM_MOVE, a, gap, 1))) return sol_mv(SM_MOVE, a, gap, 1);
    return none();
}

/* a tap on a card: to the gap it fills */
static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    (void)found_only;
    if (!pickable(p, pile, i)) return none();
    for (int b = 0; b < MT_HAND; b++)
        if (mt_legal(p, sol_mv(SM_MOVE, pile, b, 1))) return sol_mv(SM_MOVE, pile, b, 1);
    return none();
}

/* a useful move: a card that makes a row's run longer (a 2 to the first column of a row with no run); else a move
 * that leaves its gap where it can be filled (not after a king, nor after a gap); else any move; else a redeal */
solmove mt_hint(const solpos *p)
{
    solmove best = none();
    int top = 0;
    for (int b = 0; b < MT_HAND; b++) {
        if (p->n[b]) continue;
        for (int a = 0; a < MT_HAND; a++) {
            solmove m = sol_mv(SM_MOVE, a, b, 1);
            if (!mt_legal(p, m)) continue;
            int score = b % 13 == mt_run(p, b / 13) ? 100 : 10;
            if (a % 13 && p->n[a - 1] && CARD_RANK(sol_top(p, a - 1)) != RANK_KING && a - 1 != b) score += 20;
            if (a % 13 == 0) score += 20;
            if (score > top) top = score, best = m;
        }
    }
    return top ? best : stock(p);
}

int mt_stuck(const solpos *p) { return !mt_won(p) && mt_hint(p).kind == SM_NONE; }

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static void deal(solrec *r, uint32_t n) { mt_new(r, n); }

const solrules mt_rules = {mt_legal, mt_play, stock, mt_hint, tap, pickable, mt_won, deal};

static int read_place(const char **s)           /* "b7": its place (-1: none) */
{
    const char *t = *s;
    if (*t < 'a' || *t > 'd') return -1;
    int row = *t++ - 'a', col = 0;
    if (*t < '1' || *t > '9') return -1;
    while (*t >= '0' && *t <= '9') col = col * 10 + (*t++ - '0');
    if (col < 1 || col > 13) return -1;
    *s = t;
    return 13 * row + col - 1;
}

int mt_script(solrec *r, const char *s)
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
            m = *s++ == 's' ? stock(&r->pos) : mt_hint(&r->pos);
        } else {
            int a = read_place(&s), b = a < 0 ? -1 : read_place(&s);
            if (b < 0) return 0;
            m = sol_mv(SM_MOVE, a, b, 1);
        }
        if (m.kind == SM_NONE || !mt_play(r, m)) return 0;
    }
    return 1;
}
