/*
 * La Belle Lucie: the rules (bellelucie.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/bellelucie.h"

void lb_new(solrec *r, uint32_t seed)
{
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    r->seed = seed;
    p->npiles = LB_PILES;
    for (int i = 0; i < LB_PILES; i++)
        p->kind[i] = i < LB_FOUND ? SP_TAB : i < LB_MERCI ? SP_FOUND : i == LB_MERCI ? SP_CELL : SP_STOCK;
    uint8_t deck[CARDS_MAX];
    int n = cards_pack(PACK_52, deck);
    cards_shuffle(deck, n, seed);
    for (int k = 0; k < n; k++) p->c[k / 3][p->n[k / 3]++] = deck[k];
    sol_begin(r);
}

int lb_won(const solpos *p) { return sol_on_found(p) == 52; }
int lb_redeals_left(const solpos *p) { return p->redeals < LB_REDEALS ? LB_REDEALS - p->redeals : 0; }
int lb_merci_open(const solpos *p) { return p->redeals == LB_REDEALS; }

static int is_fan(int pile) { return pile >= LB_FAN && pile < LB_FAN + LB_FANS; }

int lb_legal(const solpos *p, solmove m)
{
    int a = m.from, b = m.to, n = m.n;
    if (SM_KIND(m.kind) == SM_REDEAL) {         /* a redeal: some cards still in the fans */
        if (a != LB_HAND || b != LB_HAND || n || !lb_redeals_left(p)) return 0;
        for (int f = 0; f < LB_FANS; f++)
            if (p->n[f]) return 1;
        return 0;
    }
    if (SM_KIND(m.kind) != SM_MOVE || a >= LB_PILES || b >= LB_PILES || a == b || n < 1 || n > p->n[a]) return 0;
    if (b == LB_MERCI) return is_fan(a) && n >= 2 && lb_merci_open(p) && !p->n[LB_MERCI];   /* the merci */
    if ((!is_fan(a) && a != LB_MERCI) || n != 1) return 0;
    int card = sol_top(p, a);
    if (p->kind[b] == SP_FOUND) return sol_found_takes(p, b, card);
    return is_fan(b) && p->n[b] && CARD_SUIT(card) == CARD_SUIT(sol_top(p, b)) && sol_fits(card, sol_top(p, b), 0);
}

static int found_for(const solpos *p, int card) { return sol_found_for(p, card); }

int lb_play(solrec *r, solmove m)
{
    solpos *p = &r->pos;
    if (r->n >= SOL_MOVES - 140 || !lb_legal(p, m)) return 0;
    if (SM_KIND(m.kind) == SM_REDEAL) {         /* gathered, shuffled, dealt again in threes */
        sol_push(r, m);
        for (int f = 0; f < LB_FANS; f++)
            if (p->n[f]) sol_push(r, sol_mv(SM_MOVE | SM_AUTO, f, LB_HAND, p->n[f]));
        uint8_t to[52];
        int k = p->n[LB_HAND];
        for (int j = 0; j < k; j++) to[j] = (uint8_t)(j / 3);
        sol_deal_out(r, LB_HAND, to, k, sol_reseed(r->seed, p->redeals));
    } else if (m.to == LB_MERCI && m.n >= 2) {  /* the merci: the card drawn out from under the others */
        int a = m.from, over = m.n - 1;
        sol_push(r, sol_mv(SM_REDEAL, LB_HAND, LB_HAND, 0));
        sol_push(r, sol_mv(SM_MOVE | SM_AUTO, a, LB_HAND, over));
        sol_push(r, sol_mv(SM_MOVE | SM_AUTO, a, LB_MERCI, 1));
        sol_push(r, sol_mv(SM_MOVE | SM_AUTO, LB_HAND, a, over));
    } else {
        sol_push(r, m);
    }
    sol_finish(r, 52, found_for);
    return 1;
}

static solmove none(void) { return sol_mv(SM_NONE, 0, 0, 0); }

static solmove stock(const solpos *p)
{
    solmove m = sol_mv(SM_REDEAL, LB_HAND, LB_HAND, 0);
    return lb_legal(p, m) ? m : none();
}

static int pickable(const solpos *p, int pile, int i)
{
    return (is_fan(pile) || pile == LB_MERCI) && i >= 0 && i == p->n[pile] - 1;
}

/* where the top card of a pile can go: its foundation, else the fan it follows (-1: nowhere) */
static int place_for(const solpos *p, int a, int found_only)
{
    int f = sol_found_for(p, sol_top(p, a));
    if (f >= 0 || found_only) return f;
    for (int b = 0; b < LB_FANS; b++)
        if (lb_legal(p, sol_mv(SM_MOVE, a, b, 1))) return b;
    return -1;
}

/* a tap: a top card to its place; a buried card, while the merci is open: the merci */
static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    if (i < 0 || i >= p->n[pile]) return none();
    if (pickable(p, pile, i)) {
        int b = place_for(p, pile, found_only);
        return b >= 0 ? sol_mv(SM_MOVE, pile, b, 1) : none();
    }
    solmove m = sol_mv(SM_MOVE, pile, LB_MERCI, p->n[pile] - i);
    return !found_only && lb_legal(p, m) ? m : none();
}

/* a useful move: a card up; a card onto its fan (the one that uncovers a card that can then go up first); the
 * merci's card; a redeal; the merci on the buried card that can go up, or else onto a fan, or else the lowest one */
solmove lb_hint(const solpos *p)
{
    for (int a = 0; a <= LB_MERCI; a++)
        if ((is_fan(a) || a == LB_MERCI) && p->n[a] && sol_found_for(p, sol_top(p, a)) >= 0)
            return sol_mv(SM_MOVE, a, sol_found_for(p, sol_top(p, a)), 1);
    solmove best = none();
    int top = 0;
    for (int a = 0; a <= LB_MERCI; a++) {
        if ((!is_fan(a) && a != LB_MERCI) || !p->n[a]) continue;
        int b = place_for(p, a, 0);
        if (b < 0) continue;
        int score = a == LB_MERCI ? 40 : p->n[a] > 1 && sol_found_for(p, p->c[a][p->n[a] - 2]) >= 0 ? 50 : 20;
        if (score > top) top = score, best = sol_mv(SM_MOVE, a, b, 1);
    }
    if (top) return best;
    if (stock(p).kind != SM_NONE) return stock(p);
    if (!lb_merci_open(p) || p->n[LB_MERCI]) return none();
    int rank = 1000;
    for (int a = 0; a < LB_FANS; a++)
        for (int i = 0; i + 1 < p->n[a]; i++) {
            int c = p->c[a][i], score = sol_found_for(p, c) >= 0 ? 200 : 100 - CARD_RANK(c);
            for (int b = 0; b < LB_FANS && score < 100; b++)
                if (b != a && p->n[b] && CARD_SUIT(c) == CARD_SUIT(sol_top(p, b)) && sol_fits(c, sol_top(p, b), 0))
                    score = 150;
            if (200 - score < rank) rank = 200 - score, best = sol_mv(SM_MOVE, a, LB_MERCI, p->n[a] - i);
        }
    return best;
}

int lb_stuck(const solpos *p) { return !lb_won(p) && lb_hint(p).kind == SM_NONE; }

/* ---- the notation ------------------------------------------------------------------------------------------------ */
static void deal(solrec *r, uint32_t n) { lb_new(r, n); }

const solrules lb_rules = {lb_legal, lb_play, stock, lb_hint, tap, pickable, lb_won, deal};

static int pile_of(char ch)
{
    if (ch >= 'a' && ch < 'a' + LB_FANS) return LB_FAN + ch - 'a';
    return ch == 'x' ? LB_MERCI : -1;
}

int lb_script(solrec *r, const char *s)
{
    const solpos *p = &r->pos;
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
            m = *s++ == 's' ? stock(p) : lb_hint(p);
        } else if (*s == '!') {
            int a = pile_of(s[1]), i = 0;
            if (a < 0 || a == LB_MERCI) return 0;
            for (s += 2; *s >= '0' && *s <= '9'; s++) i = i * 10 + (*s - '0');
            m = sol_mv(SM_MOVE, a, LB_MERCI, p->n[a] - i);
        } else {
            int a = pile_of(s[0]), b = s[1] == 'h' ? sol_found_for(p, sol_top(p, a < 0 ? 0 : a)) : pile_of(s[1]);
            if (a < 0 || b < 0) return 0;
            s += 2;
            m = sol_mv(SM_MOVE, a, b, 1);
        }
        if (m.kind == SM_NONE || !lb_play(r, m)) return 0;
    }
    return 1;
}
