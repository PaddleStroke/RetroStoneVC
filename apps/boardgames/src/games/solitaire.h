/*
 * The patience games (Klondike, FreeCell, Spider, Pyramid, TriPeaks): what their rules share. No SDL: the rules and
 * their tests use it.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * A position is piles of cards: the tableau's columns (face-down cards at the bottom, then face-up ones), the
 * foundations, the free cells, the stock and the waste. A move takes cards from the top of one pile to the top of
 * another (the order kept); a face-down card left on top of a column turns up at once. The stock's moves: draw to the
 * waste (Klondike), turn the waste back over (Klondike), deal a row (Spider). A game is its deal and its moves:
 * undo replays them from the deal. Moves the game makes by itself (the rest going up once the game is decided,
 * Spider's complete runs, FreeCell's safe cards) are marked SM_AUTO: undo takes them back with the player's move
 * before them, and the move counter leaves them out.
 *
 * The notation (script: the screenshots and the tests), moves separated by spaces:
 *   "13"  a move from column 1 to column 3 (columns 1..9, the tenth 0), as many cards as fit (the most, onto an
 *         empty column); "13:4" four cards;
 *   "1h"  to a foundation (the one that takes it); a foundation as a source: A..H; free cells a..d; the waste w;
 *   "s"   the stock (draw, turn the waste over, deal a row); "?" the hint's move; "#n" a new game, deal n (its seed;
 *         FreeCell: its number).
 *
 * The layouts of single cards (Pyramid, TriPeaks): a pile for each card of the layout, holding it or nothing, in rows,
 * each card covered by up to two cards of the next row (solslot); a card is free once they are gone. Their notation:
 * a card of the layout is its row (1 the top) and its place in the row (a, b, c...): "7a"; the games write their own
 * moves with it.
 */
#ifndef SOLITAIRE_H
#define SOLITAIRE_H

#include <stdint.h>
#include <string.h>

#include "cards.h"

#define SOL_PILES 32                            /* the most piles a game has (Pyramid, TriPeaks: a pile a card) */
#define SOL_CARDS 104                           /* the most a pile can hold (Spider's two packs) */
#define SOL_MOVES 6000                          /* a game's moves, the automatic ones too */

enum { SP_TAB, SP_FOUND, SP_CELL, SP_STOCK, SP_WASTE };
/* SM_TURN: the face-down card on top of pile `from` turns face up (TriPeaks: a card of a peak uncovered) */
enum { SM_NONE, SM_MOVE, SM_DRAW, SM_REDEAL, SM_DEAL, SM_TURN };
#define SM_AUTO 0x80                            /* a move the game made by itself */
#define SM_KIND(k) ((k) & 0x7f)

typedef struct { uint8_t kind, from, to, n; } solmove;

typedef struct {
    uint8_t npiles, variant, draw;              /* draw: Klondike's cards drawn at a time */
    uint8_t redeals;                            /* the times the waste has been turned over (SM_REDEAL) */
    uint8_t kind[SOL_PILES], n[SOL_PILES], down[SOL_PILES];   /* down: the face-down cards at the pile's bottom */
    uint8_t c[SOL_PILES][SOL_CARDS];            /* bottom first */
} solpos;

typedef struct {
    solpos pos, start;                          /* now; the deal */
    solmove mv[SOL_MOVES];
    int n, moves;                               /* moves: the player's (the counter) */
    uint32_t seed;                              /* the deal's seed (FreeCell: the deal's number) */
    uint8_t variant;
} solrec;

/* what a game's rules give the shared code */
typedef struct {
    int (*legal)(const solpos *p, solmove m);
    int (*play)(solrec *r, solmove m);          /* the move, then the game's own; 0: not legal */
    solmove (*stock)(const solpos *p);          /* what a tap on the stock does (SM_NONE: nothing) */
    solmove (*hint)(const solpos *p);           /* a useful move (SM_NONE: none) */
    /* a tap on card i of pile p: the move it makes (to the best place; found_only: a foundation or nothing) */
    solmove (*tap)(const solpos *p, int pile, int i, int found_only);
    int (*pickable)(const solpos *p, int pile, int i);      /* 1: the cards from i up can be lifted together */
    int (*won)(const solpos *p);
    void (*deal)(solrec *r, uint32_t n);        /* a new game, the same variant: deal n (the notation's "#n") */
} solrules;

static inline solmove sol_mv(int kind, int from, int to, int n)
{
    solmove m = {(uint8_t)kind, (uint8_t)from, (uint8_t)to, (uint8_t)n};
    return m;
}

static inline int sol_red(int card) { return CARD_SUIT(card) == SUIT_HEARTS || CARD_SUIT(card) == SUIT_DIAMONDS; }
static inline int sol_top(const solpos *p, int pile) { return p->n[pile] ? p->c[pile][p->n[pile] - 1] : CARD_NONE; }
static inline int sol_up(const solpos *p, int pile) { return p->n[pile] - p->down[pile]; }   /* face up */

static inline int sol_first(const solpos *p, int kind)       /* the first pile of a kind (-1: none) */
{
    for (int i = 0; i < p->npiles; i++)
        if (p->kind[i] == kind) return i;
    return -1;
}

static inline int sol_count(const solpos *p, int kind)
{
    int k = 0;
    for (int i = 0; i < p->npiles; i++) k += p->kind[i] == kind;
    return k;
}

static inline int sol_on_found(const solpos *p)              /* cards on the foundations */
{
    int k = 0;
    for (int i = 0; i < p->npiles; i++)
        if (p->kind[i] == SP_FOUND) k += p->n[i];
    return k;
}

/* the cards from i up: face up, each one rank lower than the one under it, colours alternating (suit: one suit) */
static inline int sol_run(const solpos *p, int pile, int i, int suit)
{
    if (i < p->down[pile] || i >= p->n[pile]) return 0;
    for (int k = i + 1; k < p->n[pile]; k++) {
        int a = p->c[pile][k - 1], b = p->c[pile][k];
        if (CARD_RANK(b) + 1 != CARD_RANK(a)) return 0;
        if (suit ? CARD_SUIT(a) != CARD_SUIT(b) : sol_red(a) == sol_red(b)) return 0;
    }
    return 1;
}

/* the lowest card of a run that can go on `card` in the tableau: one rank lower (alt: of the other colour) */
static inline int sol_fits(int card, int on, int alt)
{
    return CARD_RANK(card) + 1 == CARD_RANK(on) && (!alt || sol_red(card) != sol_red(on));
}

/* the foundation (built up by suit from the ace) that takes this card; -1: none */
static inline int sol_found_for(const solpos *p, int card)
{
    int empty = -1;
    for (int f = 0; f < p->npiles; f++) {
        if (p->kind[f] != SP_FOUND) continue;
        if (!p->n[f]) { if (empty < 0) empty = f; continue; }
        int t = sol_top(p, f);
        if (CARD_SUIT(t) == CARD_SUIT(card)) return CARD_RANK(card) == CARD_RANK(t) + 1 ? f : -1;
    }
    return CARD_RANK(card) == RANK_ACE ? empty : -1;
}

/* foundation f takes this card: an ace when empty, else the next card of its suit */
static inline int sol_found_takes(const solpos *p, int f, int card)
{
    if (p->kind[f] != SP_FOUND) return 0;
    if (!p->n[f]) return CARD_RANK(card) == RANK_ACE;
    int t = sol_top(p, f);
    return CARD_SUIT(t) == CARD_SUIT(card) && CARD_RANK(card) == CARD_RANK(t) + 1;
}

/* how high the foundation of a suit has been built (0: not started) */
static inline int sol_found_rank(const solpos *p, int suit)
{
    for (int f = 0; f < p->npiles; f++)
        if (p->kind[f] == SP_FOUND && p->n[f] && CARD_SUIT(p->c[f][0]) == suit) return CARD_RANK(sol_top(p, f));
    return 0;
}

static inline void sol_apply(solpos *p, solmove m)
{
    int a = m.from, b = m.to, n = m.n;
    switch (SM_KIND(m.kind)) {
    case SM_MOVE:
        for (int k = 0; k < n; k++) p->c[b][p->n[b] + k] = p->c[a][p->n[a] - n + k];
        p->n[b] = (uint8_t)(p->n[b] + n);
        p->n[a] = (uint8_t)(p->n[a] - n);
        if (p->kind[a] == SP_TAB && p->down[a] >= p->n[a] && p->n[a]) p->down[a] = (uint8_t)(p->n[a] - 1);
        if (p->down[a] > p->n[a]) p->down[a] = p->n[a];
        break;
    case SM_DRAW:                               /* n cards from the stock to the waste, one at a time */
        for (int k = 0; k < n; k++) p->c[b][p->n[b]++] = p->c[a][--p->n[a]];
        p->down[a] = p->n[a];
        break;
    case SM_REDEAL:                             /* the waste turned over: the stock again */
        for (int k = 0; k < n; k++) p->c[b][p->n[b]++] = p->c[a][--p->n[a]];
        p->down[b] = p->n[b];
        p->redeals++;
        break;
    case SM_DEAL:                               /* one card face up on each of n columns from `to` */
        for (int k = 0; k < n; k++) p->c[b + k][p->n[b + k]++] = p->c[a][--p->n[a]];
        p->down[a] = p->n[a];
        break;
    case SM_TURN:
        if (p->n[a] && p->down[a] >= p->n[a]) p->down[a] = (uint8_t)(p->n[a] - 1);
        break;
    }
}

static inline void sol_push(solrec *r, solmove m)
{
    if (r->n >= SOL_MOVES) return;
    sol_apply(&r->pos, m);
    r->mv[r->n++] = m;
    if (!(m.kind & SM_AUTO)) r->moves++;
}

static inline void sol_begin(solrec *r)         /* the deal is in pos: the game starts there */
{
    r->start = r->pos;
    r->n = r->moves = 0;
}

/* back one move of the player's (and what the game did by itself after it): replayed from the deal */
static inline int sol_undo(solrec *r)
{
    int n = r->n;
    while (n > 0 && (r->mv[n - 1].kind & SM_AUTO)) n--;
    if (n <= 0) return 0;
    n--;
    r->pos = r->start;
    r->n = r->moves = 0;
    for (int i = 0; i < n; i++) sol_push(r, r->mv[i]);
    return 1;
}

/* the notation: a pile's letter, and back */
static inline int sol_pile_of(const solpos *p, char ch)
{
    int kind, k;
    if (ch >= '1' && ch <= '9') kind = SP_TAB, k = ch - '1';
    else if (ch == '0') kind = SP_TAB, k = 9;
    else if (ch >= 'a' && ch <= 'd') kind = SP_CELL, k = ch - 'a';
    else if (ch >= 'A' && ch <= 'H') kind = SP_FOUND, k = ch - 'A';
    else if (ch == 'w') kind = SP_WASTE, k = 0;
    else return -1;
    for (int i = 0; i < p->npiles; i++)
        if (p->kind[i] == kind && k-- == 0) return i;
    return -1;
}

static inline char sol_name(const solpos *p, int pile)
{
    int k = 0;
    for (int i = 0; i < pile; i++) k += p->kind[i] == p->kind[pile];
    switch (p->kind[pile]) {
    case SP_TAB: return k == 9 ? '0' : (char)('1' + k);
    case SP_CELL: return (char)('a' + k);
    case SP_FOUND: return (char)('A' + k);
    case SP_WASTE: return 'w';
    default: return 's';
    }
}

/* a move in the notation (out: 8 chars) */
static inline void sol_write(const solpos *p, solmove m, char *out)
{
    int k = 0;
    if (SM_KIND(m.kind) != SM_MOVE) out[k++] = 's';
    else {
        out[k++] = sol_name(p, m.from);
        out[k++] = p->kind[m.to] == SP_FOUND ? 'h' : sol_name(p, m.to);
        if (m.n > 1) {
            out[k++] = ':';
            if (m.n >= 10) out[k++] = (char)('0' + m.n / 10);
            out[k++] = (char)('0' + m.n % 10);
        }
    }
    out[k] = 0;
}

/* plays one move of the notation, s pointing at it; returns the characters read (0: a bad move) */
static inline int sol_script_one(solrec *r, const char *s, const solrules *R)
{
    const solpos *p = &r->pos;
    const char *s0 = s;
    if (*s == 's' || *s == '?') {
        solmove m = *s == 's' ? R->stock(p) : R->hint(p);
        return m.kind != SM_NONE && R->play(r, m) ? 1 : 0;
    }
    int a = sol_pile_of(p, *s++), to = -1;
    if (a < 0 || !*s) return 0;
    char t = *s++;
    if (t != 'h' && (to = sol_pile_of(p, t)) < 0) return 0;
    int n = 0;
    if (*s == ':')
        for (s++; *s >= '0' && *s <= '9'; s++) n = n * 10 + (*s - '0');
    for (int k = n ? n : sol_up(p, a); k >= (n ? n : 1); k--)
        for (int b = 0; b < p->npiles; b++) {
            if (t == 'h' ? p->kind[b] != SP_FOUND : b != to) continue;
            solmove m = sol_mv(SM_MOVE, a, b, k);
            if (R->legal(p, m)) return R->play(r, m) ? (int)(s - s0) : 0;
        }
    return 0;
}

static inline int sol_script(solrec *r, const char *s, const solrules *R)
{
    while (*s) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == '#') {
            uint32_t n = 0;
            for (s++; *s >= '0' && *s <= '9'; s++) n = n * 10 + (uint32_t)(*s - '0');
            R->deal(r, n);
            continue;
        }
        int k = sol_script_one(r, s, R);
        if (!k) return 0;
        s += k;
    }
    return 1;
}

/* ---- the layouts of single cards (Pyramid, TriPeaks) ------------------------------------------------------------- */
/* a place of the layout: its row (0 the top) and place in the row, the places of the two cards lying over it (-1:
 * none); its card is pile first + its index */
typedef struct { int8_t row, col, cover[2]; } solslot;

/* the card of place i is there and free: nothing lies over it any more */
static inline int sol_slot_free(const solpos *p, const solslot *s, int first, int i)
{
    if (!p->n[first + i]) return 0;
    for (int k = 0; k < 2; k++)
        if (s[i].cover[k] >= 0 && p->n[first + s[i].cover[k]]) return 0;
    return 1;
}

static inline int sol_slots_left(const solpos *p, int first, int n)   /* the cards still in the layout */
{
    int k = 0;
    for (int i = 0; i < n; i++) k += p->n[first + i] > 0;
    return k;
}

/* the notation: place i as "7a" (out: 3 chars) */
static inline void sol_slot_write(const solslot *s, int i, char *out)
{
    out[0] = (char)('1' + s[i].row);
    out[1] = (char)('a' + s[i].col);
    out[2] = 0;
}

/* the place written at t ("7a"), one of n; -1: none */
static inline int sol_slot_read(const solslot *s, int n, const char *t)
{
    if (t[0] < '1' || t[0] > '9' || t[1] < 'a' || t[1] > 'z') return -1;
    for (int i = 0; i < n; i++)
        if (s[i].row == t[0] - '1' && s[i].col == t[1] - 'a') return i;
    return -1;
}

#endif
