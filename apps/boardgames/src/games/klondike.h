/*
 * Klondike, the classic patience, for one player: the rules. No SDL; the deal comes from a seed (cards.h's shuffle),
 * the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 52 cards: seven columns of 1 to 7 cards, the top one face up; the other 24 are the stock. Build the four
 * foundations up by suit from the ace to the king. In the tableau, a card goes on one a rank higher of the other
 * colour; any run of face-up cards moves together; only a king (or a run from a king) fills an empty column. A
 * face-down card left on top of a column turns up. The stock deals to the waste one card (KL_DRAW1) or three
 * (KL_DRAW3) at a time, the waste's top card playable; an empty stock takes the waste back, turned over, as many
 * times as you like. A card on a foundation may come back down to the tableau. Once the stock and the waste are
 * empty and every card is face up, the cards go up to the foundations by themselves.
 * Piles: 0..6 the columns, 7..10 the foundations, 11 the stock, 12 the waste.
 */
#ifndef KLONDIKE_H
#define KLONDIKE_H

#include "games/solitaire.h"

enum { KL_DRAW1, KL_DRAW3, KL_VARIANTS };
enum { KL_TAB = 0, KL_FOUND = 7, KL_STOCK = 11, KL_WASTE = 12, KL_PILES = 13 };

extern const solrules kl_rules;

void kl_new(solrec *r, int variant, uint32_t seed);
int  kl_legal(const solpos *p, solmove m);
int  kl_play(solrec *r, solmove m);              /* 0: not legal */
int  kl_won(const solpos *p);
solmove kl_hint(const solpos *p);
int  kl_script(solrec *r, const char *moves);    /* solitaire.h's notation; 0 at the first bad move */

#endif
