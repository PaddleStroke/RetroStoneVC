/*
 * Forty Thieves, the patience of two packs and forty cards, for one player: the rules. No SDL; the deal comes from a
 * seed (cards.h's shuffle), the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 104 cards (two packs): ten columns of four cards, all face up; the other 64 are the stock. Build the eight
 * foundations up by suit from the ace to the king (two for each suit). In the tableau a card goes on the next higher
 * card of its own suit, and any card on an empty column; one card moves at a time, never a run. The stock turns one
 * card at a time onto the waste, whose top card is playable; it is gone through once, with no redeal. A card on a
 * foundation stays there. Once the stock and the waste are empty and the cards left can all go up in turn, they go up
 * by themselves.
 * Piles: 0..9 the columns, 10..17 the foundations, 18 the stock, 19 the waste. The notation: solitaire.h's.
 */
#ifndef FORTYTHIEVES_H
#define FORTYTHIEVES_H

#include "games/solitaire.h"

enum { FT_TAB = 0, FT_FOUND = 10, FT_STOCK = 18, FT_WASTE = 19, FT_PILES = 20 };

extern const solrules ft_rules;

void ft_new(solrec *r, uint32_t seed);
int  ft_found_for(const solpos *p, int card);   /* the foundation that takes this card (-1: none) */
int  ft_legal(const solpos *p, solmove m);
int  ft_play(solrec *r, solmove m);              /* 0: not legal */
int  ft_won(const solpos *p);
solmove ft_hint(const solpos *p);                /* SM_NONE: nothing useful seen */
int  ft_script(solrec *r, const char *moves);    /* 0 at the first bad move */

#endif
