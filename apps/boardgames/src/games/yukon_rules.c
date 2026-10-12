/*
 * Yukon: the rules (yukon.h). No SDL.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 */
#include <string.h>

#include "games/yukon.h"

void yk_new(solrec *r, uint32_t seed)
{
    memset(r, 0, sizeof *r);
    solpos *p = &r->pos;
    r->seed = seed;
    p->npiles = YK_PILES;
    for (int i = 0; i < YK_PILES; i++) p->kind[i] = i < YK_FOUND ? SP_TAB : SP_FOUND;
    uint8_t deck[CARDS_MAX];
    int n = cards_pack(PACK_52, deck), k = 0;
    cards_shuffle(deck, n, seed);
    for (int row = 0; row < 7; row++)           /* Klondike's triangle, the last of each column face up... */
        for (int col = row; col < 7; col++) p->c[col][p->n[col]++] = deck[k++];
    for (int row = 0; row < 4; row++)           /* ...then four more face up on each column but the first */
        for (int col = 1; col < 7; col++) p->c[col][p->n[col]++] = deck[k++];
    for (int col = 0; col < 7; col++) p->down[col] = (uint8_t)col;
    sol_begin(r);
}

int yk_won(const solpos *p) { return sol_on_found(p) == 52; }

int yk_legal(const solpos *p, solmove m)
{
    int a = m.from, b = m.to, n = m.n;
    if (SM_KIND(m.kind) != SM_MOVE || a >= YK_PILES || b >= YK_PILES || a == b || n < 1 || n > sol_up(p, a)) return 0;
    if (p->kind[a] == SP_FOUND && (n != 1 || p->kind[b] != SP_TAB)) return 0;
    int card = p->c[a][p->n[a] - n];
    if (p->kind[b] == SP_FOUND) return n == 1 && sol_found_takes(p, b, card);
    return p->n[b] ? sol_fits(card, sol_top(p, b), 1) : CARD_RANK(card) == RANK_KING;
}

static int found_for(const solpos *p, int card) { return sol_found_for(p, card); }

int yk_play(solrec *r, solmove m)
{
    if (r->n >= SOL_MOVES - 60 || !yk_legal(&r->pos, m)) return 0;
    sol_push(r, m);
    for (int c = 0; c < 7; c++)
        if (r->pos.down[c]) return 1;
    sol_finish(r, 52, found_for);               /* every card face up: up by themselves, if they all can */
    return 1;
}

static solmove none(void) { return sol_mv(SM_NONE, 0, 0, 0); }

static solmove stock(const solpos *p)
{
    (void)p;
    return none();
}

static int pickable(const solpos *p, int pile, int i)
{
    if (i < 0 || i >= p->n[pile]) return 0;
    return p->kind[pile] == SP_TAB ? i >= p->down[pile] : i == p->n[pile] - 1;
}

/* a column for the cards from i of pile a: one that is not empty first (from the next column round), then an empty
 * one (a king; not the whole of a column already) */
static int column_for(const solpos *p, int a, int i)
{
    int n = p->n[a] - i, from = p->kind[a] == SP_TAB ? a + 1 : 0;
    for (int pass = 0; pass < 2; pass++)
        for (int k = 0; k < 7; k++) {
            int b = (from + k) % 7;
            if (b == a || (pass == 0) != (p->n[b] > 0)) continue;
            if (pass == 1 && p->kind[a] == SP_TAB && i == 0) continue;
            if (yk_legal(p, sol_mv(SM_MOVE, a, b, n))) return b;
        }
    return -1;
}

static solmove tap(const solpos *p, int pile, int i, int found_only)
{
    if (!pickable(p, pile, i)) return none();
    int n = p->n[pile] - i;
    if (n == 1 && p->kind[pile] == SP_TAB) {
        int f = sol_found_for(p, p->c[pile][i]);
        if (f >= 0) return sol_mv(SM_MOVE, pile, f, 1);
    }
    if (found_only) return none();
    int b = column_for(p, pile, i);
    return b >= 0 ? sol_mv(SM_MOVE, pile, b, n) : none();
}

/* a useful move: a card up to a foundation; else the best of: cards moved off a face-down card (the more cards still
 * face down there, the better), cards moved off a card that can then go up, a card moved off one it does not follow
 * onto one it does (never a card that already sits where it should, so the hints never go round in circles) */
static solmove direct(const solpos *p, int *score)
{
    *score = 100;
    for (int a = 0; a < 7; a++) {
        int t = sol_top(p, a);
        if (t != CARD_NONE && sol_found_for(p, t) >= 0) return sol_mv(SM_MOVE, a, sol_found_for(p, t), 1);
    }
    solmove best = none();
    int top = 0;
    for (int a = 0; a < 7; a++)
        for (int i = p->down[a]; i < p->n[a]; i++) {
            int score;
            if (i == p->down[a] && i > 0) score = 50 + p->down[a];
            else if (i > p->down[a] && sol_found_for(p, p->c[a][i - 1]) >= 0) score = 40;
            else if (i > p->down[a] && !sol_fits(p->c[a][i], p->c[a][i - 1], 1)) score = 10;
            else continue;
            if (score <= top) continue;
            for (int b = 0; b < 7; b++) {
                if (b == a || (!p->n[b] && (i == 0 || score == 10))) continue;
                solmove m = sol_mv(SM_MOVE, a, b, p->n[a] - i);
                if (yk_legal(p, m)) {
                    best = m, top = score;
                    break;
                }
            }
        }
    *score = top;
    return best;
}

/* else a move after which a card can go up or a face-down card can be uncovered (a card that already sits where it
 * should moved onto the other card it could sit on, a king to an empty column) */
solmove yk_hint(const solpos *p)
{
    int score;
    solmove m = direct(p, &score);
    if (score) return m;
    for (int a = 0; a < 7; a++)
        for (int i = p->down[a]; i < p->n[a]; i++)
            for (int b = 0; b < 7; b++) {
                m = sol_mv(SM_MOVE, a, b, p->n[a] - i);
                if (b == a || (i == 0 && !p->n[b]) || !yk_legal(p, m)) continue;
                solpos q = *p;
                sol_apply(&q, m);
                direct(&q, &score);
                if (score >= 40) return m;
            }
    return none();
}

static void deal(solrec *r, uint32_t n) { yk_new(r, n); }

const solrules yk_rules = {yk_legal, yk_play, stock, yk_hint, tap, pickable, yk_won, deal};

int yk_script(solrec *r, const char *moves) { return sol_script(r, moves, &yk_rules); }
