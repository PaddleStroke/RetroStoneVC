/*
 * Canfield, the patience of the reserve of thirteen, for one player: the rules. No SDL; the deal comes from a seed
 * (cards.h's shuffle), the moves are solitaire.h's.
 * MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).
 *
 * 52 cards: a reserve of 13 (only its top card face up), one card face up on the first foundation, four tableau piles
 * of one card each; the other 34 are the stock. The first foundation's card sets the base: the other foundations
 * start with the other three cards of that rank, and all build up by suit, round the corner (a king, then an ace...).
 * The tableau builds down in alternating colours, round the corner too (a king on an ace); its top card moves, or a
 * whole pile at once (the classic rule: never part of one). A pile left empty is filled at once from the reserve;
 * once the reserve is gone, any card (or pile) may fill it. The stock turns three cards at a time onto the waste
 * (fewer at its end), whose top card is playable; empty, it takes the waste back, turned over, as often as you like.
 * A card on a foundation stays there. Once the stock, the waste and the reserve are empty and the cards left can all go
 * up in turn, they go up by themselves.
 * Piles: 0..3 the tableau, 4 the reserve, 5..8 the foundations, 9 the stock, 10 the waste.
 * The notation: solitaire.h's (the reserve is the fifth column, 5).
 */
#ifndef CANFIELD_H
#define CANFIELD_H

#include "games/sollevels.h"

enum { CN_TAB = 0, CN_RESERVE = 4, CN_FOUND = 5, CN_STOCK = 9, CN_WASTE = 10, CN_PILES = 11 };

extern const solrules cn_rules;

void cn_new(solrec *r, uint32_t seed);
void cn_deal_cards(solrec *r, int variant, const uint8_t *deck);   /* the pack in the order dealt (seed 0) */
int  cn_base(const solpos *p);                   /* the foundations' first rank */
int  cn_found_for(const solpos *p, int card);   /* the foundation that takes this card (-1: none) */
int  cn_legal(const solpos *p, solmove m);
int  cn_play(solrec *r, solmove m);              /* 0: not legal */
int  cn_won(const solpos *p);
solmove cn_hint(const solpos *p);                /* SM_NONE: nothing useful seen */
int  cn_script(solrec *r, const char *moves);    /* 0 at the first bad move */


/* the search (sollevels.h): the moves worth trying, best first; the search itself */
int  cn_cands(const solpos *p, solmove *list);
extern const solsearch cn_search;

#endif
