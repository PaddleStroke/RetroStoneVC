/*
 * Yukon, the patience without a stock, for one player: the rules. No SDL; the deal comes from a seed (cards.h's
 * shuffle), the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 52 cards, all dealt: seven columns, the first one a single card face up, the others 1 to 6 cards face down under 5
 * face up. Build the four foundations up by suit from the ace to the king. In the tableau a card goes on one a rank
 * higher of the other colour, and ANY face-up card moves with all the cards lying on it, in order or not; only a king
 * (with what lies on it) fills an empty column. A face-down card left on top of a column turns up. A card on a
 * foundation may come back down. Once every card is face up and they can all go up in turn, they go up by themselves.
 * Piles: 0..6 the columns, 7..10 the foundations. The notation: solitaire.h's.
 */
#ifndef YUKON_H
#define YUKON_H

#include "games/solitaire.h"

enum { YK_TAB = 0, YK_FOUND = 7, YK_PILES = 11 };

extern const solrules yk_rules;

void yk_new(solrec *r, uint32_t seed);
int  yk_legal(const solpos *p, solmove m);
int  yk_play(solrec *r, solmove m);              /* 0: not legal */
int  yk_won(const solpos *p);
solmove yk_hint(const solpos *p);                /* SM_NONE: nothing useful seen */
int  yk_script(solrec *r, const char *moves);    /* 0 at the first bad move */

#endif
