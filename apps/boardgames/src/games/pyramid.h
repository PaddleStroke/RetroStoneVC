/*
 * Pyramid, the patience of pairs making thirteen, for one player: the rules. No SDL; the deal comes from a seed
 * (cards.h's shuffle), the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 52 cards: 28 face up in a pyramid of seven rows (1 to 7 cards), each row lying over half of the one above; the
 * other 24 are the stock, face down. A card is free when no card lies over it any more. Take off two free cards
 * whose ranks make 13 (an ace 1, a jack 11, a queen 12: a queen and an ace, a jack and a 2, a 10 and a 3...); a king
 * (13) goes off alone. The stock turns up one card at a time (the turned card); turning the next one puts it on the
 * waste. The turned card and the waste's top card are free too, and may pair with each other. The stock is gone
 * through once (PY_ONCE) or three times (PY_THRICE: the turned card and the waste go back, face down, as the stock,
 * twice). Won when the pyramid is cleared (the stock and the waste may keep cards); lost when no move is left.
 * The deals are random: not every one can be won.
 * Piles: 0..27 the pyramid (py_slots: row by row, from the top), 28 the stock, 29 the turned card, 30 the waste,
 * 31 the cards taken off.
 *
 * The notation (script), moves separated by spaces: a king "7a" (a card of the pyramid: its row 1..7, its place
 * a..g; the turned card t, the waste's top w); a pair "7a+6b", "7c+w", "t+w"; "s" the stock (turn a card, or turn
 * the waste over); "?" the hint's move; "#n" a new game, deal n (its seed). As solmoves: a king goes to pile 31, a
 * pair is the move from the first card's pile to the second's; played, both go to pile 31 (the second one marked
 * SM_AUTO). The stock's move (SM_DRAW, SM_REDEAL) puts the turned card on the waste first.
 */
#ifndef PYRAMID_H
#define PYRAMID_H

#include "games/sollevels.h"

enum { PY_ONCE, PY_THRICE, PY_VARIANTS };
enum { PY_SLOT = 0, PY_STOCK = 28, PY_TURN, PY_WASTE, PY_OUT, PY_PILES };
#define PY_SLOTS 28

extern const solrules py_rules;
extern const solslot py_slots[PY_SLOTS];

void py_new(solrec *r, int variant, uint32_t seed);
void py_deal_cards(solrec *r, int variant, const uint8_t *deck);   /* the pack in the order dealt (seed 0) */
int  py_free(const solpos *p, int pile);         /* the card on top of pile can be taken off */
int  py_passes(const solpos *p);                 /* the passes through the stock left, this one included */
int  py_legal(const solpos *p, solmove m);
int  py_play(solrec *r, solmove m);              /* 0: not legal */
int  py_won(const solpos *p);
int  py_stuck(const solpos *p);                  /* no move left */
solmove py_hint(const solpos *p);                /* SM_NONE: no move left */
void py_write(const solpos *p, solmove m, char *out);   /* a move in the notation (out: 8 chars) */
int  py_script(solrec *r, const char *moves);    /* 0 at the first bad move */


/* the search (sollevels.h): the moves worth trying, best first; the search itself */
int  py_cands(const solpos *p, solmove *list);
extern const solsearch py_search;

#endif
